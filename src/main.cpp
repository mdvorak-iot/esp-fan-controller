#include <Arduino.h>
#include <WiFi.h>
#include <mdns.h>
#include <atomic>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <EspWifiSetup.h>
#include "Pwm.h"
#include "Rpm.h"
#include "Average.h"
#include "Values.h"

// Config
const auto LO_THERSHOLD_TEMP_C = 28;
const auto HI_THERSHOLD_TEMP_C = 38;
const auto LO_THERSHOLD_DUTY = 30u;
const auto HI_THERSHOLD_DUTY = 99u;

const auto PWM_PIN = GPIO_NUM_33;
const auto PWM_FREQ = 25000u;
const auto PWM_RESOLUTION = LEDC_TIMER_9_BIT;

const auto RPM_PIN = GPIO_NUM_39;
const auto RPM_SAMPLES = 10;
const auto RPM_AVERAGE = 10;
const auto RPM_INTERVAL = 100;

const auto MAIN_LOOP_INTERVAL = 1000;

// Devices
static Pwm pwm(PWM_PIN, LEDC_TIMER_0, LEDC_CHANNEL_0, PWM_FREQ, PWM_RESOLUTION);
static Rpm rpm(RPM_PIN, RPM_SAMPLES);
static OneWire wire(GPIO_NUM_15);
static DallasTemperature temp(&wire);
static std::vector<uint64_t> sensors;
static Average<uint16_t, RPM_AVERAGE> rpmAvg;

// Setup
void loopRpm(void *);
void setupHttp();

esp_err_t setDuty(uint8_t dutyPercent)
{
  // NOTE inverted output via transistor
  return pwm.duty(pwm.maxDuty() - dutyPercent * pwm.maxDuty() / 100U);
}

void setup()
{
  // Enable ISR
  ESP_ERROR_CHECK(gpio_install_isr_service(0));

  // Initialize LEDC
  ESP_ERROR_CHECK(ledc_fade_func_install(0));

  // Power PWM
  ESP_ERROR_CHECK(pwm.begin());
  ESP_ERROR_CHECK(setDuty(HI_THERSHOLD_DUTY));

  // Init Sensors
  temp.begin();

  // Enumerate and store all sensors
  DeviceAddress addr = {};
  while (wire.search(addr))
  {
    if (temp.validAddress(addr) && temp.validFamily(addr))
    {
      uint64_t addr64 = 0;
      memcpy(&addr64, addr, 8);
      log_i("found temp sensor: 0x%016X", addr64);
      sensors.push_back(addr64);
    }
  }

  if (sensors.empty())
  {
    log_e("no temperature sensors found!");
  }

  // RPM
  auto err = rpm.begin();
  if (err != ESP_OK)
  {
    log_e("rpm.begin failed: %d %s", err, esp_err_to_name(err));
  }
  xTaskCreate(loopRpm, "loopRpm", 10000, nullptr, tskIDLE_PRIORITY, nullptr);

  // WiFi
  WiFiSetup(WIFI_SETUP_WPS);

  // mDNS
  mdns_init();
  mdns_hostname_set("rad");
  mdns_service_add(nullptr, "_http", "_tcp", 80, nullptr, 0);

  // HTTP
  setupHttp();

  // Done
  pinMode(0, OUTPUT);
  log_i("started");
  delay(500);
}

void loop()
{
  // Read temperatures
  float highestTemp = DEVICE_DISCONNECTED_C;

  if (!sensors.empty())
  {
    temp.requestTemperatures();

    for (uint64_t addr : sensors)
    {
      float c = temp.getTempC((uint8_t *)&addr);
      if (c > highestTemp)
      {
        highestTemp = c;
      }
    }
  }

  // Calculate fan speed
  uint32_t dutyPercent = HI_THERSHOLD_DUTY;
  if (highestTemp != DEVICE_DISCONNECTED_C && highestTemp != 85.0)
  {
    auto value = constrain(highestTemp, LO_THERSHOLD_TEMP_C, HI_THERSHOLD_TEMP_C);
    dutyPercent = map(value, LO_THERSHOLD_TEMP_C, HI_THERSHOLD_TEMP_C, LO_THERSHOLD_DUTY, HI_THERSHOLD_DUTY);

    Values::temperature.store(highestTemp);
  }
  Values::duty.store(dutyPercent);

  // Control PWM
  setDuty(dutyPercent);

  // Status LED
  static auto status = false;
  status = !status;
  digitalWrite(0, status ? HIGH : LOW);

  // Wait
  static auto lastLoop = millis();
  auto elapsed = millis() - lastLoop;

  delay(elapsed >= MAIN_LOOP_INTERVAL ? 1 : MAIN_LOOP_INTERVAL - elapsed);
  lastLoop = millis();
}

void loopRpm(void *)
{
  for (;;)
  {
    auto r = rpm.measure();
    rpmAvg.add(r);
    Values::rpm.store(rpmAvg.value());
    delay(RPM_INTERVAL);
  }
}

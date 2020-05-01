#include <Arduino.h>
#include <WiFi.h>
#include <mdns.h>
#include <esp_https_ota.h>
#include <atomic>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <EspWifiSetup.h>
#include "version.h"
#include "config.h"
#include "Pwm.h"
#include "Rpm.h"
#include "Values.h"

// Devices
static Pwm pwm(PWM_PIN, LEDC_TIMER_0, LEDC_CHANNEL_0, PWM_FREQ, PWM_RESOLUTION);
static Rpm rpm(RPM_PIN);
static OneWire wire(GPIO_NUM_15);
static DallasTemperature temp(&wire);
static std::vector<uint64_t> sensors;

// Setup
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
  temp.setResolution(12);

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

  Rpm::start(rpm);

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
  log_i("started %s", VERSION);
  delay(1000);
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
      if (c > highestTemp && c != 85.0)
      {
        highestTemp = c;
      }
    }
  }

  // Calculate fan speed
  uint32_t dutyPercent = Values::duty.load();
  if (highestTemp != DEVICE_DISCONNECTED_C && highestTemp != 85.0)
  {
    auto value = constrain(highestTemp, LO_THERSHOLD_TEMP_C, HI_THERSHOLD_TEMP_C);
    // NOTE *100 to have precision while using long arithmetics
    dutyPercent = map(value * 100, LO_THERSHOLD_TEMP_C * 100, HI_THERSHOLD_TEMP_C * 100, LO_THERSHOLD_DUTY, HI_THERSHOLD_DUTY);

    Values::duty.store(dutyPercent);
    Values::temperature.store(highestTemp);
    Values::temperatureReadout.store(millis());
  }

  // Control PWM
  setDuty(dutyPercent);
  Values::rpm.store(rpm.value());

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

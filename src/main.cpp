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
static Pwm pwm(PWM_PIN, LEDC_TIMER_0, LEDC_CHANNEL_0, PWM_FREQ, PWM_RESOLUTION, PWM_INVERTED);
static Rpm rpm(RPM_PIN);
static OneWire wire(GPIO_NUM_15);
static DallasTemperature temp(&wire);
static std::vector<uint64_t> sensors;
static std::uint64_t primarySensor;

// Setup
void setupHttp();

void setup()
{
  // Enable ISR
  ESP_ERROR_CHECK(gpio_install_isr_service(0));

  // Initialize LEDC
  ESP_ERROR_CHECK(ledc_fade_func_install(0));

  // Power PWM
  ESP_ERROR_CHECK(pwm.begin());
  ESP_ERROR_CHECK(pwm.duty((float)HI_THERSHOLD_DUTY * pwm.maxDuty() / 100));

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

      // WARNING esp32 does not support printing of 64-bit integer, and trying to do so corrupts heap!
      log_i("found temp sensor: 0x%08X%08X", (uint32_t)(addr64 >> 32), (uint32_t)addr64);

      if (sensors.empty())
        primarySensor = addr64;

      sensors.push_back(addr64);
    }
  }

  if (sensors.empty())
  {
    log_e("no temperature sensors found!");
  }
  else
  {
    log_i("found %d sensors", sensors.size());
  }

  // Request temperature, so sensors won't have 85C stored as initial value
  temp.requestTemperatures();

  // RPM
  auto err = rpm.begin();
  if (err != ESP_OK)
  {
    log_e("rpm.begin failed: %d %s", err, esp_err_to_name(err));
  }

  Rpm::start(rpm);

  // WiFi
  WiFiSetup(WIFI_SETUP_WPS);

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
  float primaryTemp = DEVICE_DISCONNECTED_C;

  if (!sensors.empty())
  {
    temp.requestTemperatures();

    int i = 0;
    for (uint64_t addr : sensors)
    {
      float c = temp.getTempC(reinterpret_cast<uint8_t *>(&addr));
      // WARNING esp32 does not support printing of 64-bit integer, and trying to do so corrupts heap!
      log_d("temp [%d] 0x%08X%08X: %f", i, (uint32_t)(addr >> 32), (uint32_t)addr, c);

      // Store temperature

      // Use temperature for fan duty
      if (addr == primarySensor)
      {
        primaryTemp = c;
      }

      i++;
    }
  }

  // Calculate fan speed
  float dutyPercent = Values::duty.load();
  if (primaryTemp != DEVICE_DISCONNECTED_C)
  {
    float value = constrain(primaryTemp, LO_THERSHOLD_TEMP_C, HI_THERSHOLD_TEMP_C);
    // map temperature range to duty cycle
    dutyPercent = (value - LO_THERSHOLD_TEMP_C) * (HI_THERSHOLD_DUTY - LO_THERSHOLD_DUTY) / (HI_THERSHOLD_TEMP_C - LO_THERSHOLD_TEMP_C) + LO_THERSHOLD_DUTY;

    Values::duty.store(dutyPercent);
    Values::temperature.store(primaryTemp);
    Values::temperatureReadout.store(millis());
  }

  // Control PWM
  pwm.duty(dutyPercent * pwm.maxDuty() / 100);
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

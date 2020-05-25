#include <Arduino.h>
#include <WiFi.h>
#include <esp_https_ota.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <memory>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <EspWifiSetup.h>
#include "app_config.h"
#include "rpm_counter.h"
#include "app_temps.h"
#include "cpu_temp.h"
#include "version.h"
#include "Pwm.h"

using namespace appconfig;
using namespace rpmcounter;
using namespace apptemps;
using namespace cputemp;

// TODO
const auto RPM_PIN = GPIO_NUM_25;
const auto PWM_PIN = GPIO_NUM_2;

// Configuration
const auto PWM_FREQ = 25000u;
const auto PWM_RESOLUTION = LEDC_TIMER_10_BIT;
const auto PWM_INVERTED = true;
const auto MAIN_LOOP_INTERVAL = 1000;

// Devices
static app_config config;
static float dutyPercent;
static Pwm pwm;
static RpmCounter rpm;

// Setup
void setupHttp(const appconfig::app_config &config, const rpmcounter::RpmCounter &rpm, const Pwm &pwm);

void setup()
{
  esp_err_t err;

  // Enable ISR
  ESP_ERROR_CHECK(gpio_install_isr_service(0));

  // Initialize LEDC
  ESP_ERROR_CHECK(ledc_fade_func_install(0));

  // Initialize NVS
  err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);

  // Init config
  ESP_ERROR_CHECK(app_config_init(config));

  // Power PWM
  // NOTE do this ASAP
  if (config.data.control_pin != APP_CONFIG_PIN_DISABLED)
  {
    ESP_ERROR_CHECK_WITHOUT_ABORT(pwm.begin(config.data.control_pin, LEDC_TIMER_0, LEDC_CHANNEL_0, PWM_FREQ, PWM_RESOLUTION, PWM_INVERTED));
    ESP_ERROR_CHECK_WITHOUT_ABORT(pwm.dutyPercent(config.data.high_threshold_duty_percent / 100.0f));
  }

  // Init Sensors
  if (config.data.sensors_pin != APP_CONFIG_PIN_DISABLED)
  {
    ESP_ERROR_CHECK_WITHOUT_ABORT(temperature_sensors_init(config.data.sensors_pin));
    for (const auto &s : config.data.sensors)
    {
      temperature_assign_name(s.address, s.name);
    }
  }

  // RPM
  for (size_t i = 0; i < APP_CONFIG_MAX_RPM; i++)
  {
    if (config.data.rpm_pins[i] != APP_CONFIG_PIN_DISABLED)
    {
      ESP_ERROR_CHECK_WITHOUT_ABORT(rpm.add(config.data.rpm_pins[i]));
    }
  }

  ESP_ERROR_CHECK_WITHOUT_ABORT(rpm.begin());

  // WiFi
  WiFiSetup(WIFI_SETUP_WPS);

  // HTTP
  setupHttp(config, rpm, pwm);

  // CPU temperature client
  if (!config.cpu_query_url.empty())
  {
    ESP_ERROR_CHECK_WITHOUT_ABORT(cpu_temp_init(config.cpu_query_url, config.data.cpu_poll_interval_seconds * 1000));
  }

  // Done
  pinMode(0, OUTPUT);
  log_i("started %s", VERSION);
  delay(1000);
}

void loop()
{
  // Read temperatures
  temperature_request();

  // Control PWM
  if (pwm.configured())
  {
    // Config
    auto lowThresTemp = config.data.low_threshold_celsius;
    auto hiThresTemp = config.data.high_threshold_celsius;
    auto cpuThresTemp = config.data.cpu_threshold_celsius;
    auto lowThresDuty = config.data.low_threshold_duty_percent / 100.0f;
    auto hiThresDuty = config.data.high_threshold_duty_percent / 100.0f;

    // Find control temperature
    float primaryTemp = temperature_value(config.data.primary_sensor_address);

    // Calculate fan speed
    if (primaryTemp != DEVICE_DISCONNECTED_C)
    {
      // limit to range
      float tempInRange = constrain(primaryTemp, lowThresTemp, hiThresTemp);
      // map temperature range to duty cycle
      dutyPercent = (tempInRange - lowThresTemp) * (hiThresDuty - lowThresDuty) / (hiThresTemp - lowThresTemp) + lowThresDuty;
    }

    // Override when CPU is overheating
    auto cpuTemp = cpu_temp_value();
    if (cpuThresTemp > 0 && cpuTemp > cpuThresTemp)
    {
      dutyPercent = hiThresDuty;
    }

    // Update PWM
    ESP_ERROR_CHECK_WITHOUT_ABORT(pwm.dutyPercent(dutyPercent));
  }

  // Status LED
  static auto status = false;
  status = !status;
  digitalWrite(0, status ? HIGH : LOW);

  // Wait
  static auto previousWakeTime = xTaskGetTickCount();
  vTaskDelayUntil(&previousWakeTime, MAIN_LOOP_INTERVAL);
}

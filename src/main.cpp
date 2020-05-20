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
#include "version.h"
#include "Pwm.h"

using namespace appconfig;
using namespace rpmcounter;
using namespace apptemps;

// TODO
const auto LO_THERSHOLD_TEMP_C = 30;
const auto HI_THERSHOLD_TEMP_C = 35;
const auto LO_THERSHOLD_DUTY = 30;
const auto HI_THERSHOLD_DUTY = 99;
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
static std::unique_ptr<Pwm> pwm;

// Setup
void setupHttp(const appconfig::app_config &config);

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
    pwm = std::unique_ptr<Pwm>(new Pwm(config.data.control_pin, LEDC_TIMER_0, LEDC_CHANNEL_0, PWM_FREQ, PWM_RESOLUTION, PWM_INVERTED));
    ESP_ERROR_CHECK_WITHOUT_ABORT(pwm->begin());
    ESP_ERROR_CHECK_WITHOUT_ABORT(pwm->duty((float)HI_THERSHOLD_DUTY * pwm->maxDuty() / 100));
  }

  // Init Sensors
  if (config.data.sensors_pin != APP_CONFIG_PIN_DISABLED)
  {
    ESP_ERROR_CHECK_WITHOUT_ABORT(temperature_sensors_init(config.data.sensors_pin));
  }

  // RPM
  for (size_t i = 0; i < APP_CONFIG_MAX_RPM; i++)
  {
    if (config.data.rpm_pins[i] != APP_CONFIG_PIN_DISABLED)
    {
      ESP_ERROR_CHECK_WITHOUT_ABORT(rpm_counter_add(config.data.rpm_pins[i]));
    }
  }

  ESP_ERROR_CHECK_WITHOUT_ABORT(rpm_counter_init());

  // WiFi
  WiFiSetup(WIFI_SETUP_WPS);

  // HTTP
  setupHttp(config);

  // Done
  pinMode(0, OUTPUT);
  log_i("started %s", VERSION);
  delay(1000);
}

void loop()
{
  // Read temperatures
  temperature_request();
  // Find important one
  float primaryTemp = temperature_value(config.data.primary_sensor_address);

  // Calculate fan speed
  if (primaryTemp != DEVICE_DISCONNECTED_C)
  {
    // limit to range
    float value = constrain(primaryTemp, LO_THERSHOLD_TEMP_C, HI_THERSHOLD_TEMP_C);
    // map temperature range to duty cycle
    dutyPercent = (value - LO_THERSHOLD_TEMP_C) * (HI_THERSHOLD_DUTY - LO_THERSHOLD_DUTY) / (HI_THERSHOLD_TEMP_C - LO_THERSHOLD_TEMP_C) + LO_THERSHOLD_DUTY;
  }

  // Control PWM
  if (pwm)
  {
    ESP_ERROR_CHECK_WITHOUT_ABORT(pwm->duty(dutyPercent * pwm->maxDuty() / 100));
  }

  // Status LED
  static auto status = false;
  status = !status;
  digitalWrite(0, status ? HIGH : LOW);

  // Wait
  static auto previousWakeTime = xTaskGetTickCount();
  vTaskDelayUntil(&previousWakeTime, MAIN_LOOP_INTERVAL);
}

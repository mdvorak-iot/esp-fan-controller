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
#include "version.h"
#include "Pwm.h"
#include "state.h"

using namespace appconfig;
using namespace rpmcounter;

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
static std::unique_ptr<OneWire> wire;
static std::unique_ptr<DallasTemperature> temp;
static std::vector<uint64_t> sensors;
static std::uint64_t primarySensor;

// Setup
void setupSensors();
void setupHttp(const appconfig::app_config &config, const std::vector<std::string> &sensorNames);
std::vector<std::string> getSensorNames();

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

  // TODO
  config.data.control_pin = PWM_PIN;
  config.data.sensors_pin = GPIO_NUM_15;

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
    wire = std::unique_ptr<OneWire>(new OneWire(config.data.sensors_pin));
    temp = std::unique_ptr<DallasTemperature>(new DallasTemperature(wire.get()));

    temp->begin();
    temp->setResolution(12);

    // Enumerate and store all sensors
    setupSensors();

    // Request temperature, so sensors won't have 85C stored as initial value
    temp->requestTemperatures();
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
  std::vector<std::string> sensorNames = getSensorNames();
  setupHttp(config, sensorNames);

  // Done
  pinMode(0, OUTPUT);
  log_i("started %s", VERSION);
  delay(1000);
}

void loop()
{
  // Read temperatures
  float primaryTemp = DEVICE_DISCONNECTED_C;

  if (temp && !sensors.empty())
  {
    temp->requestTemperatures();

    int i = 0;
    for (uint64_t addr : sensors)
    {
      float c = temp->getTempC(reinterpret_cast<uint8_t *>(&addr));
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
  if (primaryTemp != DEVICE_DISCONNECTED_C)
  {
    float value = constrain(primaryTemp, LO_THERSHOLD_TEMP_C, HI_THERSHOLD_TEMP_C);
    // map temperature range to duty cycle
    dutyPercent = (value - LO_THERSHOLD_TEMP_C) * (HI_THERSHOLD_DUTY - LO_THERSHOLD_DUTY) / (HI_THERSHOLD_TEMP_C - LO_THERSHOLD_TEMP_C) + LO_THERSHOLD_DUTY;

    // TODO
    // Values::duty.store(dutyPercent);
    // Values::temperature.store(primaryTemp);
    // Values::temperatureReadout.store(millis());
  }

  // Control PWM
  if (pwm)
  {
    ESP_ERROR_CHECK_WITHOUT_ABORT(pwm->duty(dutyPercent * pwm->maxDuty() / 100));
  }

  // TODO
  //Values::rpm.store(rpm.value());

  // Status LED
  static auto status = false;
  status = !status;
  digitalWrite(0, status ? HIGH : LOW);

  // Wait
  static auto previousWakeTime = xTaskGetTickCount();
  vTaskDelayUntil(&previousWakeTime, MAIN_LOOP_INTERVAL);
}

void setupSensors()
{
  sensors.clear();

  DeviceAddress addr = {};
  while (wire->search(addr))
  {
    if (temp->validAddress(addr) && temp->validFamily(addr))
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
}

std::vector<std::string> getSensorNames()
{
  std::vector<std::string> names;
  names.reserve(sensors.size());

  for (auto addr : sensors)
  {
    // Find
    std::string name;
    for (auto &c : config.data.sensors)
    {
      if (c.address == addr)
      {
        name = c.name;
        break;
      }
    }

    // Default
    if (name.empty())
    {
      char s[APP_CONFIG_MAX_NAME_LENGHT] = {0};
      snprintf(s, APP_CONFIG_MAX_NAME_LENGHT - 1, "%08X%08X", (uint32_t)(addr >> 32), (uint32_t)addr);
      name = s;
    }

    // Add
    names.push_back(name);
  }

  assert(names.size() == sensors.size());
  return names;
}
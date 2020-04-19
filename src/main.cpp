#include <Arduino.h>
#include <esp_bt.h>
#include <esp_bt_device.h>
#include <esp_bt_main.h>
#include <esp_gap_ble_api.h>
#include <esp32-hal-bt.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "Pwm.h"
#include "Rpm.h"
#include "Average.h"

// Config
const auto LO_THERSHOLD_TEMP_C = 30;
const auto HI_THERSHOLD_TEMP_C = 60;
const auto LO_THERSHOLD_DUTY = 50u;
const auto HI_THERSHOLD_DUTY = 99u;

const auto PWM_PIN = GPIO_NUM_14;
const auto PWM_FREQ = 50000u;
const auto PWM_RESOLUTION = LEDC_TIMER_8_BIT;
const auto RPM_PIN = GPIO_NUM_12;

const auto BLE_DEVICE_NAME = "PSU";
const auto BLE_ADV_INTERVAL = 500;

// Data
const size_t SENSOR_DATA_TEMP_LEN = 4;
struct SensorData
{
  uint8_t duty;
  uint16_t rpm;
  uint16_t temperatures[SENSOR_DATA_TEMP_LEN];
};

// Devices
static Pwm pwm(PWM_PIN, LEDC_TIMER_0, LEDC_CHANNEL_0, PWM_FREQ, PWM_RESOLUTION);
static Rpm rpm(RPM_PIN);
static OneWire wire(GPIO_NUM_23);
static DallasTemperature temp(&wire);
static std::vector<uint64_t> sensors;
static Average<uint16_t, 10> rpmAvg;

// BLE
void setupBLE(const char *deviceName, uint32_t minIntervalMs, uint32_t maxIntervalMs);
void updateBLE(void *data, size_t dataLen);

// Setup
void setup()
{
  // Enable ISR
  ESP_ERROR_CHECK(gpio_install_isr_service(0));

  // Initialize LEDC
  ESP_ERROR_CHECK(ledc_fade_func_install(0));

  // Power PWM
  ESP_ERROR_CHECK(pwm.begin());
  ESP_ERROR_CHECK(pwm.duty(pwm.maxDuty() - 1));

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

  // Init BLE
  setupBLE(BLE_DEVICE_NAME, BLE_ADV_INTERVAL - 10, BLE_ADV_INTERVAL + 10);

  // Done
  log_i("started");
  delay(500);
}

void loop()
{
  SensorData sensorData = {};

  // Read temperatures
  temp.requestTemperatures();

  float highestTemp = DEVICE_DISCONNECTED_C;
  int tempIndex = 0;
  for (uint64_t addr : sensors)
  {
    float c = temp.getTempC((uint8_t *)&addr);
    if (c > highestTemp)
    {
      highestTemp = c;
    }
    if (tempIndex < SENSOR_DATA_TEMP_LEN)
    {
      sensorData.temperatures[tempIndex++] = (uint16_t)(c * 10);
    }
  }

  // Calculate fan speed
  uint32_t dutyPercent = 99;
  if (highestTemp > DEVICE_DISCONNECTED_C)
  {
    dutyPercent = map(highestTemp, LO_THERSHOLD_TEMP_C, HI_THERSHOLD_TEMP_C, LO_THERSHOLD_DUTY, HI_THERSHOLD_TEMP_C);
  }

  // TODO
  dutyPercent = 50; //((millis() / 1000U) % (99 - 30)) + 30;
  // TODO END

  // Control PWM
  pwm.duty(dutyPercent * pwm.maxDuty() / 100U);

  // Readout
  sensorData.duty = pwm.duty() * 100U / pwm.maxDuty();

  auto rpmValue = rpm.measure();
  rpmAvg.add(rpmValue);
  sensorData.rpm = rpmAvg.value();

  // Update BLE
  updateBLE(&sensorData, sizeof(sensorData));

  // Wait
  delay(100);
}

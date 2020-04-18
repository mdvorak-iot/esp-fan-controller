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
const auto PWM_PIN = GPIO_NUM_14;
const auto PWM_FREQ = 50000U;
const auto PWM_RESOLUTION = LEDC_TIMER_8_BIT;
const auto RPM_PIN = GPIO_NUM_12;

// Devices
static Pwm pwm(PWM_PIN, LEDC_TIMER_0, LEDC_CHANNEL_0, PWM_FREQ, PWM_RESOLUTION);
static Rpm rpm(RPM_PIN);
static OneWire wire(GPIO_NUM_23);
static DallasTemperature temp(&wire);
static std::vector<uint64_t> sensors;

// Data
const char *DEVICE_NAME = "PSU";
const uint8_t SERVICE_UUID[16] = {0xa4, 0x86, 0xb7, 0xd8, 0x56, 0x33, 0x71, 0x4b, 0x81, 0xa4, 0x4e, 0x8e, 0xae, 0x63, 0x89, 0xd6};

const size_t SENSOR_DATA_TEMP_LEN = 4;
struct SensorData
{
  uint16_t rpm;
  uint8_t duty;
  uint16_t temperatures[SENSOR_DATA_TEMP_LEN];
};

static SensorData sensorData = {};
static esp_ble_adv_data_t advData = {
    .set_scan_rsp = false,
    .include_name = true,
    .include_txpower = false,
    .min_interval = 32,
    .max_interval = 64,
    .appearance = 0x00,
    .manufacturer_len = 0,
    .p_manufacturer_data = NULL,
    .service_data_len = sizeof(SensorData),
    .p_service_data = (uint8_t *)(&sensorData),
    .service_uuid_len = sizeof(SERVICE_UUID),
    .p_service_uuid = const_cast<uint8_t *>(SERVICE_UUID), // NOTE it is never modified
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};
static esp_ble_adv_params_t advParams = {
    .adv_int_min = 80, // N * 0.625ms
    .adv_int_max = 160,
    .adv_type = ADV_TYPE_NONCONN_IND,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .peer_addr = {0},
    .peer_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

void readoutLoop(void *);

// Setup
void setupBLE()
{
  esp_err_t err;

  // For some reason, we have to link this for BT to work
  btStarted();

  // Disable classic BT
  err = esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
  if (err != ESP_OK)
  {
    log_e("esp_bt_controller_mem_release failed: %d %s", err, esp_err_to_name(err));
    return;
  }

  // Init
  esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
  bt_cfg.mode = ESP_BT_MODE_BLE;
  err = esp_bt_controller_init(&bt_cfg);
  if (err != ESP_OK)
  {
    log_e("esp_bt_controller_init failed: %d %s", err, esp_err_to_name(err));
    return;
  }

  // Wait for ready
  while (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_IDLE)
  {
    delay(10);
  }

  // Enable controller
  err = esp_bt_controller_enable(ESP_BT_MODE_BLE);
  if (err != ESP_OK)
  {
    log_e("esp_bt_controller_enable failed: %d %s", err, esp_err_to_name(err));
    return;
  }

  err = esp_bluedroid_init();
  if (err != ESP_OK)
  {
    log_e("esp_bluedroid_init failed: %d %s", err, esp_err_to_name(err));
    return;
  }
  err = esp_bluedroid_enable();
  if (err != ESP_OK)
  {
    log_e("esp_bluedroid_enable failed: %d %s", err, esp_err_to_name(err));
    return;
  }

  err = esp_ble_gap_config_adv_data(&advData);
  if (err != ESP_OK)
  {
    log_e("esp_ble_gap_config_adv_data failed: %d %s", err, esp_err_to_name(err));
    return;
  }

  err = esp_ble_gap_set_device_name(DEVICE_NAME);
  if (err != ESP_OK)
  {
    log_e("esp_ble_gap_set_device_name failed: %d %s", err, esp_err_to_name(err));
    return;
  }

  err = esp_ble_gap_start_advertising(&advParams);
  if (err != ESP_OK)
  {
    log_e("esp_ble_gap_start_advertising failed: %d %s", err, esp_err_to_name(err));
    return;
  }
}

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
  //setupBLE();

  // Start background tasks
  xTaskCreate(readoutLoop, "readoutLoop", 10000, nullptr, tskIDLE_PRIORITY, nullptr);

  // Done
  log_i("started");
}

void loop()
{
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
    dutyPercent = map(highestTemp, 30, 60, 40, 99); // TODO range to constants
  }

  // TODO
  dutyPercent = 50; //((millis() / 1000U) % (99 - 30)) + 30;
  // TODO END

  // Control PWM
  pwm.duty(dutyPercent * pwm.maxDuty() / 100U);

  // Wait
  delay(100);
}

void readoutLoop(void *)
{
  Average<uint16_t, 10> rpmAvg;

  while (true)
  {
    sensorData.duty = pwm.duty() * 100U / pwm.maxDuty();
    rpmAvg.add(rpm.rpm());
    sensorData.rpm = rpmAvg.value();
    log_e("duty=%d rpm=%d", sensorData.duty, sensorData.rpm);
    delay(100);
  }
}

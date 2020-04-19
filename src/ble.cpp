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

// Data
static esp_ble_adv_data_t advData = {
    .set_scan_rsp = false,
    .include_name = true,
    .include_txpower = false,
    .min_interval = 32,
    .max_interval = 64,
    .appearance = 0x00,
    .manufacturer_len = 0,
    .p_manufacturer_data = NULL,
    .service_data_len = 0,     // Set in setup
    .p_service_data = nullptr, // Set in setup
    .service_uuid_len = 0,     // Set in setup
    .p_service_uuid = nullptr, // Set in setup
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

// Setup
void setupBLE(const char *deviceName, const uint8_t serviceUUID[16], void *data, size_t dataLen, uint32_t minIntervalMs, uint32_t maxIntervalMs)
{
  esp_err_t err;

  // Configure
  advData.p_service_uuid = const_cast<uint8_t *>(serviceUUID); // NOTE it is never modified
  advData.service_uuid_len = 16;
  advData.p_service_data = static_cast<uint8_t *>(data);
  advData.service_data_len = dataLen;

  advParams.adv_int_min = 0.625 * minIntervalMs;
  advParams.adv_int_max = 0.625 * maxIntervalMs;

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

  err = esp_ble_gap_set_device_name(deviceName);
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

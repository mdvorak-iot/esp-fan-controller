#include <esp_bt.h>
#include <esp_bt_device.h>
#include <esp_bt_main.h>
#include <esp_gap_ble_api.h>
#include <esp32-hal-bt.h>

// Data
static esp_ble_adv_params_t advParams = {
    .adv_int_min = 0, // Set in setup
    .adv_int_max = 0, // Set in setup
    .adv_type = ADV_TYPE_NONCONN_IND,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .peer_addr = {0},
    .peer_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

// Setup
void setupBLE(uint32_t minIntervalMs, uint32_t maxIntervalMs)
{
  esp_err_t err;

  // Configure
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

  err = esp_ble_gap_start_advertising(&advParams);
  if (err != ESP_OK)
  {
    log_e("esp_ble_gap_start_advertising failed: %d %s", err, esp_err_to_name(err));
    return;
  }

  log_i("ble started");
}

void updateBLE(const uint8_t uuid[ESP_UUID_LEN_128], void *data, size_t dataLen)
{
  if (dataLen > 8)
  {
    log_e("adv data too long (%d bytes), max is 8 bytes", dataLen);
    return;
  }

  // Prepare
  static uint8_t advData[ESP_BLE_ADV_DATA_LEN_MAX] = {0};

  uint8_t *p = advData;

  // Flag (3 bytes)
  {
    *p++ = 2;
    *p++ = ESP_BLE_AD_TYPE_FLAG;
    *p++ = ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT;
  }

  // UUID (18 bytes)
  {
    *p++ = 1 + ESP_UUID_LEN_128;
    *p++ = ESP_BLE_AD_TYPE_128SRV_CMPL;
    memcpy(p, uuid, ESP_UUID_LEN_128);
    p += ESP_UUID_LEN_128;
  }

  // Data (2+len bytes)
  {
    *p++ = 1 + dataLen;
    *p++ = ESP_BLE_AD_TYPE_SERVICE_DATA;
    memcpy(p, data, dataLen);
    p += dataLen;
  }

  // Update
  uint32_t advDataLen = p - advData;

  auto err = esp_ble_gap_config_adv_data_raw(advData, advDataLen);
  if (err != ESP_OK)
  {
    log_e("esp_ble_gap_config_adv_data_raw failed: %d %s (%d bytes) %d", err, esp_err_to_name(err), advDataLen, dataLen);
    return;
  }
}

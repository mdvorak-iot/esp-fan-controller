#include <esp_wifi.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <driver/gpio.h>
#include <driver/ledc.h>
#include <double_reset.h>
#include <wps_config.h>
#include <wifi_reconnect.h>
#include "sdkconfig.h"
#include "version.h"
// #include <memory>
// #include "app_config.h"
// #include "rpm_counter.h"
// #include "app_temps.h"
// #include "cpu_temp.h"
// #include "Pwm.h"

static const char TAG[] = "main";

const gpio_num_t STATUS_LED_GPIO = (gpio_num_t)CONFIG_STATUS_LED_GPIO;
const uint8_t STATUS_LED_ON = CONFIG_STATUS_LED_ON;
const uint8_t STATUS_LED_OFF = (~CONFIG_STATUS_LED_ON & 1);

// TODO
const auto RPM_PIN = GPIO_NUM_25;
const auto PWM_PIN = GPIO_NUM_2;

// Configuration
const auto PWM_FREQ = 25000u;
const auto PWM_RESOLUTION = LEDC_TIMER_10_BIT;
const auto PWM_INVERTED = true;
const auto MAIN_LOOP_INTERVAL = 1000;

// Devices
// static app_config config;
// static float dutyPercent;
// static Pwm pwm;

// Setup
// void setupHttp(const appconfig::app_config &config, const Pwm &pwm);

void setup()
{
  // Initialize NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  // Check double reset
  // NOTE this should be called as soon as possible, ideally right after nvs init
  bool reconfigure = false;
  ESP_ERROR_CHECK(double_reset_start(&reconfigure, 5000));

  // Status LED
  ESP_ERROR_CHECK(gpio_reset_pin(STATUS_LED_GPIO));
  ESP_ERROR_CHECK(gpio_set_direction(STATUS_LED_GPIO, GPIO_MODE_OUTPUT));
  ESP_ERROR_CHECK(gpio_set_level(STATUS_LED_GPIO, STATUS_LED_ON));

  // Enable ISR
  ESP_ERROR_CHECK(gpio_install_isr_service(0));

  // Initialize LEDC
  ESP_ERROR_CHECK(ledc_fade_func_install(0));

  // Initalize WiFi
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
  assert(sta_netif);
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_start());

  ESP_ERROR_CHECK(wifi_reconnect_start()); // NOTE this must be called before connect, otherwise it might miss connected event

  // TODO custom config
  // // Init config
  // ESP_ERROR_CHECK(app_config_init(config));

  // // Power PWM
  // // NOTE do this ASAP
  // if (config.data.control_pin != APP_CONFIG_PIN_DISABLED)
  // {
  //   ESP_ERROR_CHECK_WITHOUT_ABORT(pwm.begin(config.data.control_pin, LEDC_TIMER_0, LEDC_CHANNEL_0, PWM_FREQ, PWM_RESOLUTION, PWM_INVERTED));
  //   ESP_ERROR_CHECK_WITHOUT_ABORT(pwm.dutyPercent(config.data.high_threshold_duty_percent / 100.0f));
  // }

  // // Init Sensors
  // if (config.data.sensors_pin != APP_CONFIG_PIN_DISABLED)
  // {
  //   ESP_ERROR_CHECK_WITHOUT_ABORT(temperature_sensors_init(config.data.sensors_pin));
  //   for (const auto &s : config.data.sensors)
  //   {
  //     temperature_assign_name(s.address, s.name);
  //   }
  // }

  // // RPM
  // for (size_t i = 0; i < APP_CONFIG_MAX_RPM; i++)
  // {
  //   if (config.data.rpm_pins[i] != APP_CONFIG_PIN_DISABLED)
  //   {
  //     ESP_ERROR_CHECK_WITHOUT_ABORT(Rpm.add(config.data.rpm_pins[i]));
  //   }
  // }

  // ESP_ERROR_CHECK_WITHOUT_ABORT(Rpm.begin());

  // Start WPS if WiFi is not configured, or reconfiguration was requested
  if (!wifi_reconnect_is_ssid_stored() || reconfigure)
  {
    ESP_LOGI(TAG, "reconfigure request detected, starting WPS");
    ESP_ERROR_CHECK(wps_config_start());
  }
  else
  {
    // Connect now
    ESP_ERROR_CHECK(esp_wifi_connect());
  }

  // Wait for WiFi
  ESP_LOGI(TAG, "waiting for wifi");
  if (!wifi_reconnect_wait_for_connection(AUTO_WPS_TIMEOUT_MS + WIFI_RECONNECT_CONNECT_TIMEOUT_MS))
  {
    ESP_LOGE(TAG, "failed to connect to wifi!");
    wifi_reconnect_resume();
  }

  // HTTP
  // TODO setupHttp(config, pwm);

  // CPU temperature client
  // if (!config.cpu_query_url.empty())
  // {
  //   ESP_ERROR_CHECK_WITHOUT_ABORT(cpu_temp_init(config.cpu_query_url, config.data.cpu_poll_interval_seconds * 1000));
  // }

  // Setup complete
  ESP_LOGI(TAG, "started %s", VERSION);
  vTaskDelay(1000 / portTICK_PERIOD_MS);
}

void loop()
{
  vTaskDelay(1); // TODO
  // // Read temperatures
  // temperature_request();

  // // Control PWM
  // if (pwm.configured())
  // {
  //   // Config
  //   auto lowThresTemp = config.data.low_threshold_celsius;
  //   auto hiThresTemp = config.data.high_threshold_celsius;
  //   auto cpuThresTemp = config.data.cpu_threshold_celsius;
  //   auto lowThresDuty = config.data.low_threshold_duty_percent / 100.0f;
  //   auto hiThresDuty = config.data.high_threshold_duty_percent / 100.0f;

  //   // Find control temperature
  //   float primaryTemp = temperature_value(config.data.primary_sensor_address);

  //   // Calculate fan speed
  //   if (primaryTemp != DEVICE_DISCONNECTED_C)
  //   {
  //     // limit to range
  //     float tempInRange = constrain(primaryTemp, lowThresTemp, hiThresTemp);
  //     // map temperature range to duty cycle
  //     dutyPercent = (tempInRange - lowThresTemp) * (hiThresDuty - lowThresDuty) / (hiThresTemp - lowThresTemp) + lowThresDuty;
  //   }

  //   // Override when CPU is overheating
  //   auto cpuTemp = cpu_temp_value();
  //   if (cpuThresTemp > 0 && cpuTemp > cpuThresTemp)
  //   {
  //     dutyPercent = hiThresDuty;
  //   }

  //   // Update PWM
  //   ESP_ERROR_CHECK_WITHOUT_ABORT(pwm.dutyPercent(dutyPercent));
  // }

  // Toggle Status LED
  static auto status = false;
  ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_set_level(STATUS_LED_GPIO, (status = !status) ? STATUS_LED_ON : STATUS_LED_OFF));

  // Wait
  static auto previousWakeTime = xTaskGetTickCount();
  vTaskDelayUntil(&previousWakeTime, MAIN_LOOP_INTERVAL / portTICK_PERIOD_MS);
}

extern "C" _Noreturn void app_main()
{
  setup();
  for (;;)
  {
    loop();
  }
}

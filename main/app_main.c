#include "app_status.h"
#include "web_server.h"
#include <app_rainmaker.h>
#include <app_wifi.h>
#include <double_reset.h>
#include <driver/ledc.h>
#include <ds18b20_group.h>
#include <esp_log.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_params.h>
#include <esp_wifi.h>
#include <nvs_flash.h>
#include <pc_fan_control.h>
#include <string.h>
#include <wifi_reconnect.h>

#define APP_DEVICE_NAME CONFIG_APP_DEVICE_NAME
#define APP_DEVICE_TYPE CONFIG_APP_DEVICE_TYPE
#define HW_PWM_PIN CONFIG_HW_PWM_PIN
#define HW_PWM_INVERTED CONFIG_HW_PWM_INVERTED
#define HW_RPM_PIN CONFIG_HW_RPM_PIN
#define HW_PWM_TIMER LEDC_TIMER_0
#define HW_PWM_CHANNEL LEDC_CHANNEL_0
#define HW_DS18B20_PIN CONFIG_HW_DS18B20_PIN
#define SENSORS_RMT_CHANNEL_TX RMT_CHANNEL_0
#define SENSORS_RMT_CHANNEL_RX RMT_CHANNEL_1

static const char TAG[] = "app_main";

// State
static owb_rmt_driver_info owb_driver = {};
static ds18b20_group_handle_t sensors = NULL;
static volatile float fan_duty_percent = 0;

// Program
static void app_devices_init(esp_rmaker_node_t *node);

static esp_err_t set_fan_duty(float duty_percent)
{
    // Invert
    esp_err_t err = pc_fan_control_set_duty(HW_PWM_CHANNEL, HW_PWM_INVERTED ? 1 - duty_percent : duty_percent);
    if (err == ESP_OK)
    {
        fan_duty_percent = duty_percent;
    }
    return err;
}

void setup()
{
    // Initialize NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // System services
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(ledc_fade_func_install(0));

    // Check double reset
    // NOTE this should be called as soon as possible, ideally right after nvs init
    bool reconfigure = false;
    ESP_ERROR_CHECK_WITHOUT_ABORT(double_reset_start(&reconfigure, DOUBLE_RESET_DEFAULT_TIMEOUT));

    // Setup
    app_status_init();

    struct app_wifi_config wifi_cfg = {
        .security = WIFI_PROV_SECURITY_1,
        .wifi_connect = wifi_reconnect_resume,
    };
    ESP_ERROR_CHECK(app_wifi_init(&wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_MAX_MODEM));
    ESP_ERROR_CHECK(wifi_reconnect_start());

    // Fans
    ESP_ERROR_CHECK_WITHOUT_ABORT(pc_fan_control_config(HW_PWM_PIN, HW_PWM_TIMER, HW_PWM_CHANNEL));
    ESP_ERROR_CHECK_WITHOUT_ABORT(set_fan_duty(0.9));

    // Initialize OneWireBus
    owb_rmt_initialize(&owb_driver, HW_DS18B20_PIN, SENSORS_RMT_CHANNEL_TX, SENSORS_RMT_CHANNEL_RX);
    owb_use_crc(&owb_driver.bus, true);

    // Temperature sensors
    ESP_ERROR_CHECK_WITHOUT_ABORT(ds18b20_group_create(&owb_driver.bus, &sensors));
    ESP_ERROR_CHECK_WITHOUT_ABORT(ds18b20_group_find(sensors));
    ESP_ERROR_CHECK_WITHOUT_ABORT(ds18b20_group_use_crc(sensors, true));
    ESP_ERROR_CHECK_WITHOUT_ABORT(ds18b20_group_set_resolution(sensors, DS18B20_RESOLUTION_12_BIT));

    // RainMaker
    char node_name[APP_RMAKER_NODE_NAME_LEN] = {};
    ESP_ERROR_CHECK(app_rmaker_node_name(node_name, sizeof(node_name)));

    esp_rmaker_node_t *node = NULL;
    ESP_ERROR_CHECK(app_rmaker_init(node_name, &node));

    app_devices_init(node);

    // HTTP Server
    ESP_ERROR_CHECK_WITHOUT_ABORT(web_server_start());
    //    ESP_ERROR_CHECK_WITHOUT_ABORT(web_server_register_handler("/metrics", HTTP_GET, metrics_http_handler, NULL));

    // Start
    ESP_ERROR_CHECK(tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_STA, node_name)); // NOTE this isn't available before WiFi init
    ESP_ERROR_CHECK(esp_rmaker_start());
    ESP_ERROR_CHECK(app_wifi_start(reconfigure));

    // Done
    ESP_LOGI(TAG, "setup complete");
}

_Noreturn void app_main()
{
    setup();

    // Run
    TickType_t start = xTaskGetTickCount();
    uint8_t blink_counter = 0;

    for (;;)
    {
        // Blink every n seconds (approx)
        // TODO
        //        if ((++blink_counter % 8) == 0 && !status_led_is_active(status_led))
        //        {
        //            status_led_set_interval_for(status_led, 0, true, 20, false);
        //        }

        // Throttle
        vTaskDelayUntil(&start, 1000 / portTICK_PERIOD_MS);

        // Read temperatures
        if (sensors)
        {
            ESP_ERROR_CHECK_WITHOUT_ABORT(ds18b20_group_convert(sensors));
            ESP_ERROR_CHECK_WITHOUT_ABORT(ds18b20_group_wait_for_conversion(sensors));

            for (size_t i = 0; i < sensors->count; i++)
            {
                float temp_c = -127;

                // TODO
                //                if (ds18b20_group_read(sensors, i, &temp_c) == ESP_OK)
                //                {
                //                    temp_c += sensor_configs[i].offset_c;
                //
                //                    ESP_LOGI(TAG, "read temperature %s: %.3f C", sensor_configs[i].address.c_str(), temp_c);
                //                    sensor_values_c[i] = temp_c;
                //                }
                //                else
                //                {
                //                    ESP_LOGW(TAG, "failed to read from %s", sensor_configs[i].address.c_str());
                //                }
            }
        }

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
    }
}

static esp_err_t device_write_cb(__unused const esp_rmaker_device_t *device, const esp_rmaker_param_t *param,
                                 const esp_rmaker_param_val_t val, __unused void *private_data,
                                 __unused esp_rmaker_write_ctx_t *ctx)
{
    //    char *param_name = esp_rmaker_param_get_name(param);
    //    if (strcmp(param_name, "TODO") == 0)
    //    {
    //        // TODO handle
    //        esp_rmaker_param_update_and_report(param, val);
    //    }
    return ESP_OK;
}

static void app_devices_init(esp_rmaker_node_t *node)
{
    // Prepare device
    esp_rmaker_device_t *device = esp_rmaker_device_create(APP_DEVICE_NAME, APP_DEVICE_TYPE, NULL);
    assert(device);

    ESP_ERROR_CHECK(esp_rmaker_device_add_cb(device, device_write_cb, NULL));
    ESP_ERROR_CHECK(esp_rmaker_device_add_param(device, esp_rmaker_name_param_create(ESP_RMAKER_DEF_NAME_PARAM, APP_DEVICE_NAME)));
    ESP_ERROR_CHECK(esp_rmaker_node_add_device(node, device));

    // Register buttons, sensors, etc
}

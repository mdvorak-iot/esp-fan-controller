#include "app_config.h"
#include "ds18b20_group.h"
#include "fan_control.h"
#include "metrics.h"
#include "web_server.h"

#include <new>

#include <aws_iot_mqtt_config.h>
#include <aws_iot_shadow.h>
#include <aws_iot_shadow_mqtt_error.h>
#include <double_reset.h>
#include <driver/ledc.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_wifi.h>
#include <freertos/event_groups.h>
#include <mqtt_client.h>
#include <nvs_flash.h>
#include <rapidjson/document.h>
#include <rapidjson/pointer.h>
#include <rapidjson/writer.h>
#include <status_led.h>
#include <wifi_reconnect.h>
#include <wps_config.h>

static const char TAG[] = "main";

static const auto STATUS_LED_CONNECTING_INTERVAL = 500u;
static const auto PWM_TIMER = LEDC_TIMER_0;
static const auto PWM_CHANNEL = LEDC_CHANNEL_0;
static const auto SENSORS_RMT_CHANNEL_TX = RMT_CHANNEL_0;
static const auto SENSORS_RMT_CHANNEL_RX = RMT_CHANNEL_1;

static const auto STATE_BIT_RECONFIGURE = BIT0;
static const auto STATE_BIT_MQTT_STARTED = BIT1;
static const auto STATE_BIT_SHADOW_READY = BIT2;

// Configuration
static EventGroupHandle_t state;
static status_led_handle_ptr status_led;
static esp_mqtt_client_handle_t mqtt_client = nullptr;
static aws_iot_shadow_handle_ptr shadow_client = nullptr;
static owb_rmt_driver_info owb_driver = {};
static ds18b20_group_handle_t sensors = nullptr;
static struct ds18b20_config
{
    std::string address;
    std::string name;
    float offset_c = 0;
} sensor_configs[DS18B20_GROUP_MAX_SIZE];
static volatile float sensor_values_c[DS18B20_GROUP_MAX_SIZE] = {0};
static volatile float fan_duty_percent = 0;

static hw_config hw_config;
static auto *hw_config_state = hw_config::state();

static void do_mqtt_connect()
{
    if (xEventGroupGetBits(state) & STATE_BIT_MQTT_STARTED)
    {
        ESP_LOGI(TAG, "reconnect mqtt client");
        esp_err_t err = esp_mqtt_client_reconnect(mqtt_client);
        if (err != ESP_OK)
        {
            ESP_LOGW(TAG, "esp_mqtt_client_reconnect failed: %d (%s)", err, esp_err_to_name(err));
        }
    }
    else
    {
        // Initial connection
        ESP_LOGI(TAG, "start mqtt client");
        esp_err_t err = esp_mqtt_client_start(mqtt_client);
        if (err == ESP_OK)
        {
            xEventGroupSetBits(state, STATE_BIT_MQTT_STARTED);
        }
        else
        {
            ESP_LOGE(TAG, "esp_mqtt_client_start failed: %d (%s)", err, esp_err_to_name(err));
        }
    }
}

static void restart_task(__unused void *p)
{
    esp_mqtt_client_stop(mqtt_client);
    esp_restart();
}

static void do_restart()
{
    ESP_LOGI(TAG, "scheduling restart...");

    // Note: This must be done from outside handlers, so execute it in own task
    auto r = xTaskCreate(restart_task, "restart_task", 1500, nullptr, tskIDLE_PRIORITY, nullptr);
    if (r != pdTRUE)
    {
        ESP_LOGE(TAG, "failed to create restart_task, restarting immediately");
        esp_restart();
    }
}

static esp_err_t set_fan_duty(float duty_percent)
{
    // Ignore disabled
    if (hw_config.pwm_pin == GPIO_NUM_NC)
    {
        return ESP_OK;
    }

    // Invert
    esp_err_t err = fan_control_set_duty(PWM_CHANNEL, hw_config.pwm_inverted_duty ? 1 - duty_percent : duty_percent);
    if (err == ESP_OK)
    {
        fan_duty_percent = duty_percent;
    }
    return err;
}

static void disconnected_handler(__unused void *arg, esp_event_base_t event_base,
                                 __unused int32_t event_id, __unused void *event_data)
{
    ESP_LOGD(TAG, "disconnected %s", event_base);
    xEventGroupClearBits(state, STATE_BIT_SHADOW_READY);
    status_led_set_interval(status_led, STATUS_LED_CONNECTING_INTERVAL, true);
}

static void shadow_ready_handler(__unused void *arg, __unused esp_event_base_t event_base,
                                 __unused int32_t event_id, __unused void *event_data)
{
    ESP_LOGD(TAG, "shadow ready");
    xEventGroupSetBits(state, STATE_BIT_SHADOW_READY);
    status_led_set_interval_for(status_led, 200, false, 700, false);
}

static void wps_event_handler(__unused void *arg, esp_event_base_t event_base,
                              int32_t event_id, __unused void *event_data)
{
    ESP_LOGD(TAG, "wps event %s %d", event_base, event_id);

    if (event_base == WIFI_EVENT
        && (event_id == WIFI_EVENT_STA_WPS_ER_SUCCESS || event_id == WIFI_EVENT_STA_WPS_ER_TIMEOUT || event_id == WIFI_EVENT_STA_WPS_ER_FAILED))
    {
        status_led_set_interval(status_led, STATUS_LED_CONNECTING_INTERVAL, true);
        wifi_reconnect_resume();
    }
    else if (event_base == WPS_CONFIG_EVENT && event_id == WPS_CONFIG_EVENT_START)
    {
        status_led_set_interval(status_led, 100, true);
    }
}

static void dump_json(const rapidjson::Value &val)
{
    ESP_LOGE(TAG, "heap2: %ud", esp_get_free_heap_size());
    rapidjson::StringBuffer dump;
    rapidjson::Writer<rapidjson::StringBuffer> dump_writer(dump);
    ESP_LOGE(TAG, "heap3: %ud", esp_get_free_heap_size());
    val.Accept(dump_writer);
    ESP_LOGE(TAG, "heap4: %ud", esp_get_free_heap_size());

    ESP_LOGI(TAG, "hw_config:\n%s", dump.GetString());
}

static void dump_hw_config()
{
    ESP_LOGE(TAG, "heap1: %ud", esp_get_free_heap_size());
    rapidjson::Document dump_doc;
    hw_config_state->write(hw_config, dump_doc, dump_doc.GetAllocator());
    dump_json(dump_doc);
}

static void setup_init()
{
    // State
    state = xEventGroupCreate();
    assert(state);

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
    ESP_ERROR_CHECK(double_reset_start(&reconfigure, DOUBLE_RESET_DEFAULT_TIMEOUT));
    if (reconfigure)
    {
        xEventGroupSetBits(state, STATE_BIT_RECONFIGURE);
    }

    // Load app_config
    {
        auto handle = nvs::open_nvs_handle(APP_CONFIG_NVS_NAME, NVS_READONLY, &err);
        if (err == ESP_OK)
        {
            hw_config_state->load(hw_config, *handle);
        }
        else
        {
            ESP_LOGW(TAG, "failed to load hw_config, using defaults: %d %s", err, esp_err_to_name(err));
        }
    }

    // Status LED
    ESP_ERROR_CHECK_WITHOUT_ABORT(status_led_create(hw_config.status_led_pin, hw_config.status_led_on_state, &status_led));
    ESP_ERROR_CHECK_WITHOUT_ABORT(status_led_set_interval(status_led, STATUS_LED_CONNECTING_INTERVAL, true));

    // Events
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wps_event_handler, nullptr);
    esp_event_handler_register(WPS_CONFIG_EVENT, WPS_CONFIG_EVENT_START, wps_event_handler, nullptr);
    esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, disconnected_handler, nullptr);

    // Dump current config JSON
#ifndef NDEBUG
    ESP_LOGE(TAG, "heap0: %ud", esp_get_free_heap_size());
    dump_hw_config();
    ESP_LOGE(TAG, "heap6: %ud", esp_get_free_heap_size());
#endif
}

static void setup_fans()
{
    // Configure fan pwm
    ESP_ERROR_CHECK_WITHOUT_ABORT(fan_control_config(hw_config.pwm_pin, PWM_TIMER, PWM_CHANNEL));
    ESP_ERROR_CHECK_WITHOUT_ABORT(set_fan_duty(0.9));

    // Fan metrics
    metrics_register_callback([](std::ostream &m) {
        metric_type(m, "esp_pwm_duty", METRIC_TYPE_GAUGE);
        metric_value(m, "esp_pwm_duty").label("hardware", "TODO").label("device", "fan").is() << std::setprecision(3) << fan_duty_percent << '\n';

        // TODO metric type?
        // metric_type(m "esp_rpm", METRIC_TYPE_COUNTER);
    });
}

static void setup_sensors()
{
    // Skip if not configured
    if (hw_config.sensors_pin < 0 || !GPIO_IS_VALID_GPIO(hw_config.sensors_pin))
    {
        ESP_LOGW(TAG, "sensor_pin not valid, skipping sensors config");
        return;
    }

    // Initialize OneWireBus
    owb_rmt_initialize(&owb_driver, hw_config.sensors_pin, SENSORS_RMT_CHANNEL_TX, SENSORS_RMT_CHANNEL_RX);
    owb_use_crc(&owb_driver.bus, true);

    // Temperature sensors
    ESP_ERROR_CHECK_WITHOUT_ABORT(ds18b20_group_create(&owb_driver.bus, &sensors));
    ESP_ERROR_CHECK_WITHOUT_ABORT(ds18b20_group_find(sensors));
    ESP_ERROR_CHECK_WITHOUT_ABORT(ds18b20_group_use_crc(sensors, true));
    ESP_ERROR_CHECK_WITHOUT_ABORT(ds18b20_group_set_resolution(sensors, DS18B20_RESOLUTION_12_BIT));

    // Initialize config
    // TODO into separate method
    char addr_str[17] = {};
    for (size_t i = 0; i < sensors->count; i++)
    {
        uint64_t addr = *(uint64_t *)sensors->devices[i].rom_code.bytes;
        app_config_print_address(addr_str, sizeof(addr_str), addr);
        sensor_configs[i].address = addr_str;
        sensor_configs[i].name = addr_str;
        sensor_configs[i].offset_c = 0;

        for (auto &sensor : hw_config.sensors)
        {
            if (sensor.address == addr_str)
            {
                ESP_LOGD(TAG, "found sensor %s config: '%s' %.3f", addr_str, sensor.name.c_str(), sensor.offset_c);
                sensor_configs[i].name = !sensor.name.empty() ? sensor.name : addr_str; // configured name or address as a name
                sensor_configs[i].offset_c = sensor.offset_c;
            }
        }
    }

    // Temperature metrics
    metrics_register_callback([](std::ostream &m) {
        metric_type(m, "esp_celsius", METRIC_TYPE_GAUGE);
        for (size_t i = 0; i < sensors->count; i++)
        {
            metric_value(m, "esp_celsius").label("address", sensor_configs[i].address).label("hardware", "TODO").label("sensor", sensor_configs[i].name).is() << sensor_values_c[i] << '\n';
        }
    });
}

static void mqtt_event_handler(__unused void *handler_args, __unused esp_event_base_t event_base, __unused int32_t event_id, void *event_data)
{
    auto event = (const esp_mqtt_event_t *)event_data;

    switch (event->event_id)
    {
    case MQTT_EVENT_BEFORE_CONNECT:
        ESP_LOGI(TAG, "connecting to mqtt...");
        break;

    case MQTT_EVENT_ERROR:
        aws_iot_shadow_log_mqtt_error(TAG, event->error_handle);
        break;
    default:
        break;
    }
}

static const rapidjson::Pointer JSON_PTR_STATE_DESIRED("/state/desired");
static const rapidjson::Pointer JSON_PTR_CFG("/cfg");
static const rapidjson::Pointer JSON_PTR_STATE_REPORT_CFG("/state/reported/cfg");
static const rapidjson::Pointer JSON_PTR_STATE_REPORT_SENSORS("/state/reported/sensors");

static void shadow_event_handler_state_accepted(__unused void *handler_args, __unused esp_event_base_t event_base,
                                                int32_t event_id, void *event_data)
{
    ESP_LOGE(TAG, "heapA1: %u", esp_get_free_heap_size());

    ESP_LOGD(TAG, "shadow accepted event %d", event_id);
    auto *event = (const struct aws_iot_shadow_event_data *)event_data;

    // Parse
    rapidjson::Document doc;
    doc.Parse(event->data, event->data_len);
    ESP_LOGE(TAG, "heapA2: %u", esp_get_free_heap_size());

    auto *desired = JSON_PTR_STATE_DESIRED.Get(doc);

    // Ignore if desired is missing in an update - nothing to do here
    if (event->event_id == AWS_IOT_SHADOW_EVENT_UPDATE_ACCEPTED && (!desired || !desired->IsObject() || desired->ObjectEmpty()))
    {
        return;
    }

    // Handle change
    uint64_t version = doc.HasMember("version") && doc["version"].IsUint64() ? doc["version"].GetUint64() : 0;
    auto *desired_cfg = JSON_PTR_CFG.Get(*desired);

    if (desired_cfg && desired_cfg->IsObject())
    {
        bool should_restart = false;
        esp_err_t err = ESP_OK;
        auto handle = nvs::open_nvs_handle(APP_CONFIG_NVS_NAME, NVS_READWRITE, &err);

        if (err == ESP_OK)
        {
            dump_hw_config();

            uint64_t current_version = 0;
            handle->get_item("$version", current_version);

            // Ignore same version
            if (version == 0 || version != current_version)
            {
                ESP_LOGI(TAG, "parsing hw_config");
                if ((should_restart = hw_config_state->read(hw_config, *desired_cfg)))
                {
                    ESP_LOGI(TAG, "parsed hw_config");
                    dump_hw_config();
                    ESP_LOGI(TAG, "storing hw_config");
                    err = hw_config_state->store(hw_config, *handle);
                    if (err == ESP_OK)
                    {
                        ESP_LOGI(TAG, "hw_config stored successfully");
                    }
                    else
                    {
                        ESP_LOGW(TAG, "failed to store hw_config: %d %s", err, esp_err_to_name(err));
                    }
                }

                // TODO rest of the config

                // Commit
                handle->set_item("$version", version);
                handle->commit();
            }
            else
            {
                ESP_LOGI(TAG, "same hw_config version %" PRIu64 ", skipping", version);
            }
        }
        else
        {
            ESP_LOGW(TAG, "failed to store hw_config: %d %s", err, esp_err_to_name(err));
        }

        // And restart, since we cannot re-initialize some of the services
        if (should_restart)
        {
            do_restart();
            return;
        }
    }

    // Re-use document
    rapidjson::Value(rapidjson::kObjectType).Swap(doc); // Erase document

    auto &to_report_cfg = JSON_PTR_STATE_REPORT_CFG.Create(doc, doc.GetAllocator());

    // Report always
    // NOTE this is needed, since we restart on config change
    hw_config_state->write(hw_config, to_report_cfg, doc.GetAllocator());

    if (event->event_id == AWS_IOT_SHADOW_EVENT_GET_ACCEPTED && sensors != nullptr)
    {
        auto &sensors_obj = JSON_PTR_STATE_REPORT_SENSORS.Create(doc, doc.GetAllocator()).SetArray();

        for (size_t i = 0; i < sensors->count; i++)
        {
            sensors_obj.PushBack(rapidjson::StringRef(sensor_configs[i].address), doc.GetAllocator());
        }
    }

    // Serialize
    rapidjson::StringBuffer json;
    rapidjson::Writer<rapidjson::StringBuffer> json_writer(json);
    doc.Accept(json_writer);

    // Publish event
    esp_err_t err = aws_iot_shadow_request_update(event->handle, json.GetString(), json.GetLength());
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "failed to publish update: %d (%s)", err, esp_err_to_name(err));
    }
}

static const rapidjson::Pointer JSON_PTR_CODE("/code");

static void shadow_event_handler_error(__unused void *handler_args, __unused esp_event_base_t event_base,
                                       __unused int32_t event_id, void *event_data)
{
    const auto *event = (const struct aws_iot_shadow_event_data *)event_data;

    // Parse
    rapidjson::Document doc;
    doc.Parse<rapidjson::kParseInsituFlag>(const_cast<char *>(event->data), event->data_len);

    int code = JSON_PTR_CODE.GetWithDefault(doc, -1).GetInt();

    const char *message = nullptr;
    if (doc.HasMember(AWS_IOT_SHADOW_JSON_MESSAGE) && doc[AWS_IOT_SHADOW_JSON_MESSAGE].IsString())
    {
        message = doc[AWS_IOT_SHADOW_JSON_MESSAGE].GetString();
    }

    // Log
    ESP_LOGW(TAG, "shadow error %d: %d %s", event->event_id, code, message ? message : "");
}

static void setup_aws()
{
    // MQTT
    esp_mqtt_client_config_t mqtt_cfg = {};
    mqtt_cfg.cert_pem = AWS_IOT_ROOT_CA;
    mqtt_cfg.cert_len = AWS_IOT_ROOT_CA_LEN;
    ESP_ERROR_CHECK_WITHOUT_ABORT(aws_iot_mqtt_config_load(&mqtt_cfg));
    ESP_LOGI(TAG, "mqtt host=%s, port=%u, client_id=%s", mqtt_cfg.host ? mqtt_cfg.host : "", mqtt_cfg.port, mqtt_cfg.client_id ? mqtt_cfg.client_id : "");

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (!mqtt_client)
    {
        ESP_LOGE(TAG, "failed to init mqtt client");
        return;
    }

    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_mqtt_client_register_event(mqtt_client, MQTT_EVENT_ANY, mqtt_event_handler, nullptr));

    // Connect when WiFi connects
    esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, [](void *, esp_event_base_t, int32_t, void *) { do_mqtt_connect(); },
        nullptr, nullptr);

    // Shadow
    ESP_ERROR_CHECK_WITHOUT_ABORT(aws_iot_shadow_init(mqtt_client, aws_iot_shadow_thing_name(mqtt_cfg.client_id), nullptr, &shadow_client));
    ESP_ERROR_CHECK_WITHOUT_ABORT(aws_iot_shadow_handler_register(shadow_client, AWS_IOT_SHADOW_EVENT_DISCONNECTED, disconnected_handler, nullptr));
    ESP_ERROR_CHECK_WITHOUT_ABORT(aws_iot_shadow_handler_register(shadow_client, AWS_IOT_SHADOW_EVENT_READY, shadow_ready_handler, nullptr));
    ESP_ERROR_CHECK_WITHOUT_ABORT(aws_iot_shadow_handler_register(shadow_client, AWS_IOT_SHADOW_EVENT_GET_ACCEPTED, shadow_event_handler_state_accepted, nullptr));
    ESP_ERROR_CHECK_WITHOUT_ABORT(aws_iot_shadow_handler_register(shadow_client, AWS_IOT_SHADOW_EVENT_UPDATE_ACCEPTED, shadow_event_handler_state_accepted, nullptr));
    ESP_ERROR_CHECK_WITHOUT_ABORT(aws_iot_shadow_handler_register(shadow_client, AWS_IOT_SHADOW_EVENT_GET_REJECTED, shadow_event_handler_error, nullptr));
    ESP_ERROR_CHECK_WITHOUT_ABORT(aws_iot_shadow_handler_register(shadow_client, AWS_IOT_SHADOW_EVENT_UPDATE_REJECTED, shadow_event_handler_error, nullptr));
}

static void setup_wifi()
{
    // Get app info
    esp_app_desc_t app_info = {};
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_ota_get_partition_description(esp_ota_get_running_partition(), &app_info));

    // Initalize WiFi
    ESP_ERROR_CHECK(esp_netif_init());
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_MAX_MODEM));
    ESP_ERROR_CHECK(esp_wifi_start());
    if (strlen(app_info.project_name))
    {
        ESP_ERROR_CHECK(tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_STA, app_info.project_name));
    }

    ESP_ERROR_CHECK(wifi_reconnect_start()); // NOTE this must be called before connect, otherwise it might miss connected event

    // Start WPS if WiFi is not configured, or reconfiguration was requested
    bool reconfigure = xEventGroupGetBits(state) & STATE_BIT_RECONFIGURE;
    if (!wifi_reconnect_is_ssid_stored() || reconfigure)
    {
        ESP_LOGI(TAG, "reconfigure request detected, starting WPS");
        ESP_ERROR_CHECK(wps_config_start());
        // NOTE wifi_reconnect_resume will be called when WPS finishes
    }
    else
    {
        // Connect right now
        wifi_reconnect_resume();
    }
}

// TODO not here
esp_err_t root_handler_get(httpd_req_t *req)
{
    return httpd_resp_sendstr(req, "Hello world");
}

static void setup_final()
{
    // HTTP Server
    ESP_ERROR_CHECK_WITHOUT_ABORT(web_server_start());
    ESP_ERROR_CHECK_WITHOUT_ABORT(web_server_register_handler("/", HTTP_GET, root_handler_get, nullptr));
    ESP_ERROR_CHECK_WITHOUT_ABORT(web_server_register_handler("/metrics", HTTP_GET, metrics_http_handler, nullptr));

    // Ready
    esp_app_desc_t app_info = {};
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_ota_get_partition_description(esp_ota_get_running_partition(), &app_info));
    ESP_LOGI(TAG, "started %s %s", app_info.project_name, app_info.version);
}

_Noreturn static void run()
{
    TickType_t start = xTaskGetTickCount();
    uint8_t blink_counter = 0;

    for (;;)
    {
        // Blink every n seconds (approx)
        if ((++blink_counter % 8) == 0 && !status_led_is_active(status_led))
        {
            status_led_set_interval_for(status_led, 0, true, 20, false);
        }

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

                if (ds18b20_group_read(sensors, i, &temp_c) == ESP_OK)
                {
                    temp_c += sensor_configs[i].offset_c;

                    ESP_LOGI(TAG, "read temperature %s: %.3f C", sensor_configs[i].address.c_str(), temp_c);
                    sensor_values_c[i] = temp_c;
                }
                else
                {
                    ESP_LOGW(TAG, "failed to read from %s", sensor_configs[i].address.c_str());
                }
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

extern "C" void app_main()
{
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("main", ESP_LOG_DEBUG);
    esp_log_level_set("aws_iot_shadow", ESP_LOG_DEBUG);
    esp_log_level_set("ds18b20_group", ESP_LOG_INFO);
    esp_log_level_set("fan_control", ESP_LOG_DEBUG);
    esp_log_level_set("metrics", ESP_LOG_DEBUG);
    esp_log_level_set("web_server", ESP_LOG_DEBUG);

    // Setup
    setup_init();
    setup_fans();
    setup_sensors();
    setup_aws();
    setup_wifi();
    setup_final();

    // Run
    run();
}

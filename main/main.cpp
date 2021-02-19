#include "ds18b20_group.h"
#include "fan_control.h"
#include "metrics.h"
#include "web_server.h"
#include <aws_shadow.h>
#include <aws_shadow_mqtt_error.h>
#include <double_reset.h>
#include <driver/ledc.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_wifi.h>
#include <mqtt_client.h>
#include <nvs_flash.h>
#include <status_led.h>
#include <wifi_reconnect.h>
#include <wps_config.h>

// #include "app_config.h"
// #include "rpm_counter.h"
// #include "cpu_temp.h"

static const char TAG[] = "main";

// TODO configurable
const auto OWB_PIN = GPIO_NUM_15;
const auto RPM_PIN = GPIO_NUM_25;
const auto PWM_TIMER = LEDC_TIMER_0;
const auto PWM_CHANNEL = LEDC_CHANNEL_0;

static gpio_num_t pwm_pin = GPIO_NUM_2;

// Configuration
static bool reconfigure = false;
static bool mqtt_started = true;
static esp_mqtt_client_handle_t mqtt_client = nullptr;
static aws_shadow_handle_t shadow_client = nullptr;
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

static esp_err_t set_fan_duty(float duty_percent)
{
    // Invert TODO configurable
    esp_err_t err = fan_control_set_duty(PWM_CHANNEL, 1 - duty_percent);
    if (err == ESP_OK)
    {
        fan_duty_percent = duty_percent;
    }
    return err;
}

static void do_mqtt_connect()
{
    if (!mqtt_started)
    {
        // Initial connection
        ESP_LOGI(TAG, "start mqtt client");
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_mqtt_client_start(mqtt_client));
        mqtt_started = true;
    }
    else
    {
        // Ignore error here
        ESP_LOGI(TAG, "reconnect mqtt client");
        esp_mqtt_client_reconnect(mqtt_client);
    }
}

static void setup_init()
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
    ESP_ERROR_CHECK(double_reset_start(&reconfigure, DOUBLE_RESET_DEFAULT_TIMEOUT));

    // Status LED
    ESP_ERROR_CHECK_WITHOUT_ABORT(status_led_create_default());
    ESP_ERROR_CHECK_WITHOUT_ABORT(status_led_set_interval(STATUS_LED_DEFAULT, 500, true));

    // Events
    esp_event_handler_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, [](void *, esp_event_base_t, int32_t event_id, void *) {
            switch (event_id)
            {
            case WIFI_EVENT_STA_DISCONNECTED:
                status_led_set_interval(STATUS_LED_DEFAULT, 500, true);
                break;

            case WIFI_EVENT_STA_WPS_ER_SUCCESS:
            case WIFI_EVENT_STA_WPS_ER_TIMEOUT:
            case WIFI_EVENT_STA_WPS_ER_FAILED:
                status_led_set_interval(STATUS_LED_DEFAULT, 500, true);
                wifi_reconnect_resume();
                break;
            default:
                break;
            }
        },
        nullptr);
    esp_event_handler_register(
        WPS_CONFIG_EVENT, WPS_CONFIG_EVENT_START, [](void *, esp_event_base_t, int32_t, void *) { status_led_set_interval(STATUS_LED_DEFAULT, 100, true); }, nullptr);
    esp_event_handler_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, [](void *, esp_event_base_t, int32_t, void *) { status_led_set_interval_for(STATUS_LED_DEFAULT, 200, false, 700, false); }, nullptr);
}

static void setup_fans()
{
    // Configure fan pwm
    ESP_ERROR_CHECK(fan_control_config(pwm_pin, PWM_TIMER, PWM_CHANNEL));
    ESP_ERROR_CHECK(set_fan_duty(0.9));

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
    // Initialize OneWireBus
    owb_rmt_initialize(&owb_driver, OWB_PIN, RMT_CHANNEL_0, RMT_CHANNEL_1);
    owb_use_crc(&owb_driver.bus, true);

    // Temperature sensors
    ESP_ERROR_CHECK(ds18b20_group_create(&owb_driver.bus, &sensors));
    ESP_ERROR_CHECK(ds18b20_group_find(sensors));
    ESP_ERROR_CHECK_WITHOUT_ABORT(ds18b20_group_use_crc(sensors, true));
    ESP_ERROR_CHECK_WITHOUT_ABORT(ds18b20_group_set_resolution(sensors, DS18B20_RESOLUTION_12_BIT));

    // Initialize config
    for (size_t i = 0; i < sensors->count; i++)
    {
        char addr[17];
        owb_string_from_rom_code(sensors->devices[i].rom_code, addr, sizeof(addr));
        sensor_configs[i].address = addr;
        sensor_configs[i].name = addr;
        sensor_configs[i].offset_c = 0;
    }

    // TODO test
    sensor_configs[0].offset_c = 0.45;
    sensor_configs[1].offset_c = -0.45;

    // Temperature metrics
    metrics_register_callback([](std::ostream &m) {
        metric_type(m, "esp_celsius", METRIC_TYPE_GAUGE);
        for (size_t i = 0; i < sensors->count; i++)
        {
            metric_value(m, "esp_celsius").label("address", sensor_configs[i].address).label("hardware", "TODO").label("sensor", sensor_configs[i].name).is() << sensor_values_c[i] << '\n';
        }
    });

    // Custom devices and other init, that needs to be done before waiting for wifi connection
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
}

static void mqtt_event_handler(__unused void *handler_args, __unused esp_event_base_t event_base, __unused int32_t event_id, void *event_data)
{
    auto event = (esp_mqtt_event_handle_t)event_data;

    switch (event->event_id)
    {
    case MQTT_EVENT_BEFORE_CONNECT:
        ESP_LOGI(TAG, "connecting to mqtt...");
        break;

    case MQTT_EVENT_ERROR:
        aws_shadow_log_mqtt_error(TAG, event->error_handle);
        break;
    default:
        break;
    }
}

inline int cJSON_GetIntValue_(const cJSON *object, const char *key, int default_value)
{
    cJSON *value = cJSON_GetObjectItemCaseSensitive(object, key);
    return cJSON_IsNumber(value) ? value->valueint : default_value;
}

static void shadow_updated(const cJSON *state)
{
    int pwm_pin_value = cJSON_GetIntValue_(state, "pwm_pin", pwm_pin);
    if (pwm_pin_value > 0 && pwm_pin_value < GPIO_NUM_MAX)
    {
        if (pwm_pin_value != pwm_pin)
        {
            // Persist value to nvs and restart
            pwm_pin = (gpio_num_t)pwm_pin_value;
            ESP_LOGI(TAG, "pwm_pin=>%d", pwm_pin);
            // TODO
        }
    }
    else
    {
        // Fix desired value
    }
}

static void shadow_event_handler(__unused void *handler_args, __unused esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    auto *event = (aws_shadow_event_data_t *)event_data;

    if (event->event_id == AWS_SHADOW_EVENT_UPDATE_ACCEPTED || event->event_id == AWS_SHADOW_EVENT_UPDATE_DELTA)
    {
        if (event->desired)
        {
            shadow_updated(event->desired);
        }
        if (event->delta)
        {
            shadow_updated(event->delta);
            aws_shadow_request_update_reported(event->handle, event->delta, nullptr);
        }
    }
    else if (event->event_id == AWS_SHADOW_EVENT_ERROR)
    {
        ESP_LOGW(TAG, "shadow error %d %s", event->error->code, event->error->message);
    }
}

static void setup_aws()
{
    // Read config
    nvs_handle_t aws_nvs = 0;
    if (nvs_open("aws", NVS_READONLY, &aws_nvs) != ESP_OK)
    {
        ESP_LOGE(TAG, "no aws config found, cannot start");
        return;
    }

    // Host
    char host[255] = {};
    size_t host_len = sizeof(host);
    nvs_get_str(aws_nvs, "host", host, &host_len);
    if (host_len == 0)
    {
        ESP_LOGE(TAG, "no aws host, cannot start");
        return;
    }

    // Port
    uint32_t port = 0;
    nvs_get_u32(aws_nvs, "port", &port);
    if (port == 0)
    {
        // Use default
        port = 8883;
    }

    // client_id
    char client_id[255] = {};
    size_t client_id_len = sizeof(client_id);
    nvs_get_str(aws_nvs, "client_id", client_id, &client_id_len);
    if (client_id_len == 0)
    {
        ESP_LOGE(TAG, "no aws client_id, cannot start");
        return;
    }

    // Parse thing_name
    const char *thing_name = strstr(client_id, ":thing/");
    if (thing_name == nullptr)
    {
        ESP_LOGE(TAG, "unable to extract thing_name from client_id '%s'", client_id);
        return;
    }
    thing_name += strlen(":thing/");

    // client_key
    size_t client_key_len = 0;
    nvs_get_str(aws_nvs, "client_key", nullptr, &client_key_len);
    if (client_key_len == 0)
    {
        ESP_LOGE(TAG, "no aws client_key, cannot start");
        return;
    }
    char *client_key = new char[client_key_len];
    nvs_get_str(aws_nvs, "client_key", client_key, &client_key_len);

    // client_cert
    size_t client_cert_len = 0;
    nvs_get_str(aws_nvs, "client_cert", nullptr, &client_cert_len);
    if (client_cert_len == 0)
    {
        ESP_LOGE(TAG, "no aws client_key, cannot start");
        free(client_key);
        return;
    }
    char *client_cert = new char[client_cert_len];
    nvs_get_str(aws_nvs, "client_key", client_cert, &client_cert_len);

    // Config done
    nvs_close(aws_nvs);

    // MQTT
    esp_mqtt_client_config_t mqtt_cfg = {};
    mqtt_cfg.host = host;
    mqtt_cfg.port = port;
    mqtt_cfg.client_id = client_id;
    mqtt_cfg.transport = MQTT_TRANSPORT_OVER_SSL;
    mqtt_cfg.protocol_ver = MQTT_PROTOCOL_V_3_1_1;
    mqtt_cfg.cert_pem = AWS_ROOT_CA_PEM;
    mqtt_cfg.client_cert_pem = client_cert;
    mqtt_cfg.client_key_pem = client_key;

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);

    // Release dynamic configs
    free(client_key);
    free(client_cert);

    ESP_ERROR_CHECK(esp_mqtt_client_register_event(mqtt_client, MQTT_EVENT_ANY, mqtt_event_handler, NULL));

    esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, [](void *, esp_event_base_t, int32_t, void *) { do_mqtt_connect(); },
        nullptr, nullptr);

    // Shadow
    ESP_ERROR_CHECK(aws_shadow_init(mqtt_client, thing_name, NULL, &shadow_client));
    ESP_ERROR_CHECK(aws_shadow_handler_register(shadow_client, AWS_SHADOW_EVENT_ANY, shadow_event_handler, NULL));
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
    ESP_ERROR_CHECK(tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_STA, app_info.project_name));

    ESP_ERROR_CHECK(wifi_reconnect_start()); // NOTE this must be called before connect, otherwise it might miss connected event

    // Start WPS if WiFi is not configured, or reconfiguration was requested
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
    ESP_ERROR_CHECK(web_server_start());
    ESP_ERROR_CHECK(web_server_register_handler("/", HTTP_GET, root_handler_get, nullptr));
    ESP_ERROR_CHECK(web_server_register_handler("/metrics", HTTP_GET, metrics_http_handler, nullptr));

    // CPU temperature client
    // if (!config.cpu_query_url.empty())
    // {
    //   ESP_ERROR_CHECK_WITHOUT_ABORT(cpu_temp_init(config.cpu_query_url, config.data.cpu_poll_interval_seconds * 1000));
    // }

    // Ready
    esp_app_desc_t app_info = {};
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_ota_get_partition_description(esp_ota_get_running_partition(), &app_info));
    ESP_LOGI(TAG, "started %s %s", app_info.project_name, app_info.version);
}

_Noreturn static void run()
{
    for (;;)
    {
        // TODO wait until again, when readout fails
        vTaskDelay(1);

        // Read temperatures
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

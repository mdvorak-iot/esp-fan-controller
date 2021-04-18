#include "app_status.h"
#include "util/util_append.h"
#include <app_rainmaker.h>
#include <app_wifi.h>
#include <double_reset.h>
#include <driver/ledc.h>
#include <ds18b20_group.h>
#include <esp_http_server.h>
#include <esp_log.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_params.h>
#include <esp_rmaker_standard_types.h>
#include <esp_wifi.h>
#include <nvs_flash.h>
#include <pc_fan_control.h>
#include <pc_fan_rpm.h>
#include <status_led.h>
#include <string.h>
#include <wifi_reconnect.h>

static const char TAG[] = "app_main";

#define APP_DEVICE_NAME CONFIG_APP_DEVICE_NAME
#define APP_DEVICE_TYPE CONFIG_APP_DEVICE_TYPE
#define APP_CONTROL_LOOP_INTERVAL CONFIG_APP_CONTROL_LOOP_INTERVAL
#define HW_PWM_PIN CONFIG_HW_PWM_PIN
#define HW_PWM_INVERTED CONFIG_HW_PWM_INVERTED
#define HW_PWM_TIMER LEDC_TIMER_0
#define HW_PWM_CHANNEL LEDC_CHANNEL_0
#define HW_RPM_PIN CONFIG_HW_RPM_PIN
#define HW_RPM_UNIT PCNT_UNIT_0
#define HW_RPM_SAMPLES CONFIG_HW_RPM_SAMPLES
#define HW_RPM_SAMPLING_INTERVAL CONFIG_HW_RPM_SAMPLING_INTERVAL
#define HW_DS18B20_PIN CONFIG_HW_DS18B20_PIN
#define SENSORS_RMT_CHANNEL_TX RMT_CHANNEL_0
#define SENSORS_RMT_CHANNEL_RX RMT_CHANNEL_1
#define SENSORS_NVS_NAME "sensors"

// Params
#define APP_RMAKER_DEF_MAX_SPEED_NAME "Max Speed"
#define APP_RMAKER_DEF_LOW_SPEED_NAME "Low Speed"
#define APP_RMAKER_DEF_HIGH_SPEED_NAME "High Speed"
#define APP_RMAKER_DEF_LOW_TEMP_NAME "Low Temperature"
#define APP_RMAKER_DEF_HIGH_TEMP_NAME "High Temperature"
#define APP_RMAKER_DEF_PRIMARY_SENSOR_NAME "Primary Sensor"
#define APP_RMAKER_DEF_SENSOR_NAME_NAME_F "Sensor %s Name"
#define APP_RMAKER_DEF_SENSOR_OFFSET_NAME_F "Sensor %s Offset"

esp_rmaker_param_t *max_speed_param = NULL;
esp_rmaker_param_t *low_speed_param = NULL;
esp_rmaker_param_t *high_speed_param = NULL;
esp_rmaker_param_t *low_temperature_param = NULL;
esp_rmaker_param_t *high_temperature_param = NULL;
esp_rmaker_param_t *primary_sensor_param = NULL;

// State
static httpd_handle_t httpd = NULL;
static owb_rmt_driver_info owb_driver = {};
static ds18b20_group_handle_t sensors = NULL;
static pc_fan_rpm_sampling_ptr rpm = NULL;
static esp_timer_handle_t rpm_timer = NULL;
static float fan_duty_percent = 0.9f;
static struct app_sensor_config
{
    char address[17];
    char name[33];
    float offset_c;

    char name_param_name[40];
    char offset_param_name[40];
} sensors_config[DS18B20_GROUP_MAX_SIZE] = {};
static float temperatures[DS18B20_GROUP_MAX_SIZE] = {};
static size_t sensor_errors = 0;

// Config
static bool force_max_duty = false;
static float low_duty_percent = 0.5f;
static float high_duty_percent = 0.9f;
static float low_temperature_threshold = 25.0f;
static float high_temperature_threshold = 35.0f;
static size_t primary_sensor_index = 0;

// Program
static void app_devices_init(esp_rmaker_node_t *node);
static void app_hw_init();
static esp_err_t metrics_http_handler(httpd_req_t *r);

static void set_fan_duty(float duty_percent)
{
    // Log only on change
    if (duty_percent != fan_duty_percent)
    {
        ESP_LOGI(TAG, "changing fan duty to %0.1f", fan_duty_percent * 100.0f);
    }

    // Invert if needed
    float value = duty_percent;
#if HW_PWM_INVERTED
    value = 1.0f - value;
#endif

    // Change
    esp_err_t err = pc_fan_control_set_duty(HW_PWM_CHANNEL, value);
    if (err == ESP_OK)
    {
        fan_duty_percent = duty_percent;
    }
    else
    {
        ESP_LOGW(TAG, "failed to control fan: %d %s", err, esp_err_to_name(err));
    }
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
    app_hw_init();

    struct app_wifi_config wifi_cfg = {
        .security = WIFI_PROV_SECURITY_1,
        .wifi_connect = wifi_reconnect_resume,
    };
    ESP_ERROR_CHECK(app_wifi_init(&wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_MAX_MODEM));
    ESP_ERROR_CHECK(wifi_reconnect_start());

    // RainMaker
    char node_name[APP_RMAKER_NODE_NAME_LEN] = {};
    ESP_ERROR_CHECK(app_rmaker_node_name(node_name, sizeof(node_name)));

    esp_rmaker_node_t *node = NULL;
    ESP_ERROR_CHECK(app_rmaker_init(node_name, &node));

    app_devices_init(node);

    // HTTP Server
    httpd_config_t httpd_config = HTTPD_DEFAULT_CONFIG();
    ESP_ERROR_CHECK_WITHOUT_ABORT(httpd_start(&httpd, &httpd_config));
    httpd_uri_t metrics_handler_uri = {.uri = "/metrics", .method = HTTP_GET, .handler = metrics_http_handler};
    ESP_ERROR_CHECK_WITHOUT_ABORT(httpd_register_uri_handler(httpd, &metrics_handler_uri));

    // Start
    ESP_ERROR_CHECK(tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_STA, node_name)); // NOTE this isn't available before WiFi init
    ESP_ERROR_CHECK(esp_rmaker_start());
    ESP_ERROR_CHECK(app_wifi_start(reconfigure));

    // Done
    ESP_LOGI(TAG, "setup complete");
}

void app_hw_init()
{
    // Fans
    ESP_ERROR_CHECK_WITHOUT_ABORT(pc_fan_control_init(HW_PWM_PIN, HW_PWM_TIMER, HW_PWM_CHANNEL));
    set_fan_duty(fan_duty_percent);

    struct pc_fan_rpm_config rpm_cfg = {
        .pin = (gpio_num_t)HW_RPM_PIN,
        .unit = (pcnt_unit_t)HW_RPM_UNIT,
    };
    pc_fan_rpm_handle_ptr rpm_handle = NULL;
    ESP_ERROR_CHECK_WITHOUT_ABORT(pc_fan_rpm_create(&rpm_cfg, &rpm_handle));
    ESP_ERROR_CHECK_WITHOUT_ABORT(pc_fan_rpm_sampling_create(HW_RPM_SAMPLES, rpm_handle, &rpm));

    // Start sampling timer
    ESP_ERROR_CHECK_WITHOUT_ABORT(pc_fan_rpm_sampling_timer_create(rpm, &rpm_timer));
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_timer_start_periodic(rpm_timer, HW_RPM_SAMPLING_INTERVAL * 1000)); // ms to us

    // Initialize OneWireBus
    owb_rmt_initialize(&owb_driver, HW_DS18B20_PIN, SENSORS_RMT_CHANNEL_TX, SENSORS_RMT_CHANNEL_RX);
    owb_use_crc(&owb_driver.bus, true);

    // Temperature sensors
    ESP_ERROR_CHECK_WITHOUT_ABORT(ds18b20_group_create(&owb_driver.bus, &sensors));
    if (sensors)
    {
        ESP_ERROR_CHECK_WITHOUT_ABORT(ds18b20_group_find(sensors));
        ESP_ERROR_CHECK_WITHOUT_ABORT(ds18b20_group_use_crc(sensors, true));
        ESP_ERROR_CHECK_WITHOUT_ABORT(ds18b20_group_set_resolution(sensors, DS18B20_RESOLUTION_12_BIT));

        for (size_t i = 0; i < sensors->count; i++)
        {
            // Print address as string so we don't have to do that every time
            snprintf(sensors_config[i].address, sizeof(sensors_config[i].address), "%llx", *(uint64_t *)sensors->devices[i].rom_code.bytes);
            strcpy(sensors_config[i].name, sensors_config[i].address); // Default name is address

            snprintf(sensors_config[i].name_param_name, sizeof(sensors_config[i].name_param_name), APP_RMAKER_DEF_SENSOR_NAME_NAME_F, sensors_config[i].address);
            snprintf(sensors_config[i].offset_param_name, sizeof(sensors_config[i].offset_param_name), APP_RMAKER_DEF_SENSOR_OFFSET_NAME_F, sensors_config[i].address);
        }
    }
}

static esp_err_t primary_sensor_param_handler(const esp_rmaker_param_t *param, const char *val)
{
    // Find primary sensor
    if (sensors && sensors->count > 0)
    {
        for (size_t i = 0; i < sensors->count; i++)
        {
            if (strcmp(sensors_config[i].address, val) == 0)
            {
                // Found
                primary_sensor_index = i;
                return esp_rmaker_param_update_and_report(param, esp_rmaker_str(val));
            }
        }
    }

    // Not found or no sensors connected, ignore
    return ESP_ERR_INVALID_STATE;
}

static esp_err_t sensor_name_param_handler(const esp_rmaker_param_t *param, const char *val, struct app_sensor_config *sensor_cfg)
{
    assert(param);
    assert(val);
    assert(sensor_cfg);

    // NOTE this will actually trim last two chars from address, which are always 28
    char nvs_key[16] = {};
    snprintf(nvs_key, sizeof(nvs_key), "n%.14s", sensor_cfg->address);

    char value[sizeof(sensor_cfg->name)] = {};
    strlcpy(value, val, sizeof(value));

    // Custom NVS store
    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open(SENSORS_NVS_NAME, NVS_READWRITE, &handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "nvs_open(%s) failed: %d %s", SENSORS_NVS_NAME, err, esp_err_to_name(err));
        return err;
    }
    err = nvs_set_str(handle, nvs_key, value);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "nvs_set_str(%s) failed: %d %s", nvs_key, err, esp_err_to_name(err));
        return err;
    }
    err = nvs_commit(handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "nvs_commit(%s): %d %s", SENSORS_NVS_NAME, err, esp_err_to_name(err));
        return err;
    }
    nvs_close(handle);

    // Store state
    strlcpy(sensor_cfg->name, value, sizeof(sensor_cfg->name));

    // Report
    return esp_rmaker_param_update_and_report(param, esp_rmaker_str(sensor_cfg->name));
}

static esp_err_t sensor_offset_param_handler(const esp_rmaker_param_t *param, float val, struct app_sensor_config *sensor_cfg)
{
    assert(param);
    assert(sensor_cfg);

    // NOTE this will actually trim last two chars from address, which are always 28
    char nvs_key[16] = {};
    snprintf(nvs_key, sizeof(nvs_key), "o%.14s", sensor_cfg->address);

    // Custom NVS store
    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open(SENSORS_NVS_NAME, NVS_READWRITE, &handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "nvs_open(%s) failed: %d %s", SENSORS_NVS_NAME, err, esp_err_to_name(err));
        return err;
    }
    // float is not support it directly, so store it as multiplication of desired precision
    int32_t val_int = (int32_t)(val * 1000.0f);
    err = nvs_set_i32(handle, nvs_key, val_int);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "nvs_set_i32(%s) failed: %d %s", nvs_key, err, esp_err_to_name(err));
        return err;
    }
    err = nvs_commit(handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "nvs_commit(%s): %d %s", SENSORS_NVS_NAME, err, esp_err_to_name(err));
        return err;
    }
    nvs_close(handle);

    // Store state
    sensor_cfg->offset_c = (float)val_int / 1000.0f;

    // Report
    return esp_rmaker_param_update_and_report(param, esp_rmaker_float(sensor_cfg->offset_c));
}

static esp_err_t device_write_cb(__unused const esp_rmaker_device_t *device, const esp_rmaker_param_t *param,
                                 const esp_rmaker_param_val_t val, __unused void *private_data,
                                 __unused esp_rmaker_write_ctx_t *ctx)
{
    char *name = esp_rmaker_param_get_name(param);
    if (strcmp(name, APP_RMAKER_DEF_MAX_SPEED_NAME) == 0)
    {
        force_max_duty = val.val.b;
        return esp_rmaker_param_update_and_report(param, val);
    }
    if (strcmp(name, APP_RMAKER_DEF_LOW_SPEED_NAME) == 0)
    {
        float value = (float)val.val.i / 100.0f;
        if (value >= 0 && value <= 1)
        {
            low_duty_percent = value;
            return esp_rmaker_param_update_and_report(param, val);
        }
        return ESP_ERR_INVALID_ARG;
    }
    if (strcmp(name, APP_RMAKER_DEF_HIGH_SPEED_NAME) == 0)
    {
        float value = (float)val.val.i / 100.0f;
        if (value >= 0 && value <= 1)
        {
            high_duty_percent = value;
            return esp_rmaker_param_update_and_report(param, val);
        }
        return ESP_ERR_INVALID_ARG;
    }
    if (strcmp(name, APP_RMAKER_DEF_LOW_TEMP_NAME) == 0)
    {
        low_temperature_threshold = val.val.f;
        return esp_rmaker_param_update_and_report(param, val);
    }
    if (strcmp(name, APP_RMAKER_DEF_HIGH_TEMP_NAME) == 0)
    {
        high_temperature_threshold = val.val.f;
        return esp_rmaker_param_update_and_report(param, val);
    }
    if (strcmp(name, APP_RMAKER_DEF_PRIMARY_SENSOR_NAME) == 0)
    {
        return primary_sensor_param_handler(param, val.val.s);
    }

    if (sensors)
    {
        for (size_t i = 0; i < sensors->count; i++)
        {
            if (strcmp(name, sensors_config[i].name_param_name) == 0)
            {
                return sensor_name_param_handler(param, val.val.s, &sensors_config[i]);
            }
            if (strcmp(name, sensors_config[i].offset_param_name) == 0)
            {
                return sensor_offset_param_handler(param, val.val.f, &sensors_config[i]);
            }
        }
    }
    return ESP_OK;
}

static void app_devices_init(esp_rmaker_node_t *node)
{
    // Prepare device
    esp_rmaker_device_t *device = esp_rmaker_device_create(APP_DEVICE_NAME, APP_DEVICE_TYPE, NULL);
    assert(device);

    ESP_ERROR_CHECK(esp_rmaker_node_add_device(node, device));
    ESP_ERROR_CHECK(esp_rmaker_device_add_cb(device, device_write_cb, NULL));
    ESP_ERROR_CHECK(esp_rmaker_device_add_param(device, esp_rmaker_name_param_create(ESP_RMAKER_DEF_NAME_PARAM, APP_DEVICE_NAME)));

    // Register buttons, sensors, etc
    max_speed_param = esp_rmaker_param_create(APP_RMAKER_DEF_MAX_SPEED_NAME, ESP_RMAKER_PARAM_SPEED, esp_rmaker_bool(false), PROP_FLAG_READ | PROP_FLAG_WRITE);
    ESP_ERROR_CHECK(esp_rmaker_param_add_ui_type(max_speed_param, ESP_RMAKER_UI_TOGGLE));
    ESP_ERROR_CHECK(esp_rmaker_device_add_param(device, max_speed_param));

    low_speed_param = esp_rmaker_param_create(APP_RMAKER_DEF_LOW_SPEED_NAME, ESP_RMAKER_PARAM_SPEED, esp_rmaker_int((int)(low_duty_percent * 100.0f)), PROP_FLAG_READ | PROP_FLAG_WRITE | PROP_FLAG_PERSIST);
    ESP_ERROR_CHECK(esp_rmaker_param_add_ui_type(low_speed_param, ESP_RMAKER_UI_TEXT));
    ESP_ERROR_CHECK(esp_rmaker_param_add_bounds(low_speed_param, esp_rmaker_int(0), esp_rmaker_int(100), esp_rmaker_int(1)));
    ESP_ERROR_CHECK(esp_rmaker_device_add_param(device, low_speed_param));

    high_speed_param = esp_rmaker_param_create(APP_RMAKER_DEF_HIGH_SPEED_NAME, ESP_RMAKER_PARAM_SPEED, esp_rmaker_int((int)(high_duty_percent * 100.0f)), PROP_FLAG_READ | PROP_FLAG_WRITE | PROP_FLAG_PERSIST);
    ESP_ERROR_CHECK(esp_rmaker_param_add_ui_type(high_speed_param, ESP_RMAKER_UI_TEXT));
    ESP_ERROR_CHECK(esp_rmaker_param_add_bounds(high_speed_param, esp_rmaker_int(0), esp_rmaker_int(100), esp_rmaker_int(1)));
    ESP_ERROR_CHECK(esp_rmaker_device_add_param(device, high_speed_param));

    low_temperature_param = esp_rmaker_param_create(APP_RMAKER_DEF_LOW_TEMP_NAME, ESP_RMAKER_PARAM_TEMPERATURE, esp_rmaker_float(low_temperature_threshold), PROP_FLAG_READ | PROP_FLAG_WRITE | PROP_FLAG_PERSIST);
    ESP_ERROR_CHECK(esp_rmaker_param_add_ui_type(low_temperature_param, ESP_RMAKER_UI_TEXT));
    ESP_ERROR_CHECK(esp_rmaker_param_add_bounds(low_temperature_param, esp_rmaker_float(0), esp_rmaker_float(50), esp_rmaker_float(0.5f)));
    ESP_ERROR_CHECK(esp_rmaker_device_add_param(device, low_temperature_param));

    high_temperature_param = esp_rmaker_param_create(APP_RMAKER_DEF_HIGH_TEMP_NAME, ESP_RMAKER_PARAM_TEMPERATURE, esp_rmaker_float(high_temperature_threshold), PROP_FLAG_READ | PROP_FLAG_WRITE | PROP_FLAG_PERSIST);
    ESP_ERROR_CHECK(esp_rmaker_param_add_ui_type(high_temperature_param, ESP_RMAKER_UI_TEXT));
    ESP_ERROR_CHECK(esp_rmaker_param_add_bounds(high_temperature_param, esp_rmaker_float(0), esp_rmaker_float(50), esp_rmaker_float(0.5f)));
    ESP_ERROR_CHECK(esp_rmaker_device_add_param(device, high_temperature_param));

    size_t sensor_count = sensors ? sensors->count : 0;
    if (sensor_count > 0)
    {
        // NOTE this is never deallocated, since RainMaker is using it during its lifetime and it never changes anyway
        const char **sensor_addresses = calloc(sensor_count, sizeof(const char *));
        // Reference config values
        for (size_t i = 0; i < sensor_count; i++)
        {
            sensor_addresses[i] = sensors_config[i].address;
        }

        primary_sensor_param = esp_rmaker_param_create(APP_RMAKER_DEF_PRIMARY_SENSOR_NAME, NULL, esp_rmaker_str(sensor_addresses[0]), PROP_FLAG_READ | PROP_FLAG_WRITE | PROP_FLAG_PERSIST);
        ESP_ERROR_CHECK(esp_rmaker_param_add_ui_type(primary_sensor_param, ESP_RMAKER_UI_DROPDOWN));
        ESP_ERROR_CHECK(esp_rmaker_param_add_valid_str_list(primary_sensor_param, sensor_addresses, sensor_count));
        ESP_ERROR_CHECK(esp_rmaker_device_add_param(device, primary_sensor_param));

        // Custom NVS store
        // NOTE we ignore errors here, because it might not be initialized yet on first boot
        nvs_handle_t handle = 0;
        ESP_ERROR_CHECK_WITHOUT_ABORT(nvs_open(SENSORS_NVS_NAME, NVS_READONLY, &handle));

        // Sensors config
        for (size_t i = 0; i < sensor_count; i++)
        {
            // NOTE this will actually trim last two chars from address, which are always 28
            char nvs_name_key[16] = {};
            char nvs_offset_key[16] = {};
            snprintf(nvs_name_key, sizeof(nvs_name_key), "n%.14s", sensors_config[i].address);
            snprintf(nvs_offset_key, sizeof(nvs_offset_key), "o%.14s", sensors_config[i].address);

            size_t nvs_name_len = sizeof(sensors_config[i].name);
            nvs_get_str(handle, nvs_name_key, sensors_config[i].name, &nvs_name_len);

            int32_t offset_int = (int32_t)(sensors_config[i].offset_c * 1000.0f);
            nvs_get_i32(handle, nvs_offset_key, &offset_int);
            sensors_config[i].offset_c = (float)offset_int / 1000.0f;

            esp_rmaker_param_t *sensor_name_param = esp_rmaker_param_create(sensors_config[i].name_param_name, NULL, esp_rmaker_str(sensors_config[i].name), PROP_FLAG_READ | PROP_FLAG_WRITE);
            ESP_ERROR_CHECK(esp_rmaker_param_add_ui_type(sensor_name_param, ESP_RMAKER_UI_TEXT));
            ESP_ERROR_CHECK(esp_rmaker_device_add_param(device, sensor_name_param));

            esp_rmaker_param_t *sensor_offset_param = esp_rmaker_param_create(sensors_config[i].offset_param_name, NULL, esp_rmaker_float(sensors_config[i].offset_c), PROP_FLAG_READ | PROP_FLAG_WRITE);
            ESP_ERROR_CHECK(esp_rmaker_param_add_ui_type(sensor_offset_param, ESP_RMAKER_UI_SLIDER));
            ESP_ERROR_CHECK(esp_rmaker_param_add_bounds(sensor_offset_param, esp_rmaker_float(-1.0f), esp_rmaker_float(1.0f), esp_rmaker_float(0.05f)));
            ESP_ERROR_CHECK(esp_rmaker_device_add_param(device, sensor_offset_param));
        }

        // Close NVS
        nvs_close(handle);
    }
}

static esp_err_t metrics_http_handler(httpd_req_t *r)
{
    // Read device name from NVS, since rainmaker provides absolutely no means to get it directly
    nvs_handle_t handle;
    esp_err_t err = nvs_open(APP_DEVICE_NAME, NVS_READONLY, &handle);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "failed to open nvs storage");
        return err;
    }
    char name[100] = {};
    size_t name_len = sizeof(name);
    nvs_get_str(handle, ESP_RMAKER_DEF_NAME_PARAM, name, &name_len);
    nvs_close(handle);

    // Build metrics string
    char buf[1024] = {};
    char *ptr = buf;
    const char *end = ptr + sizeof(buf);

    // Sensors
    if (sensors)
    {
        ptr = util_append(ptr, end, "# TYPE esp_celsius gauge\n");
        for (size_t i = 0; i < sensors->count; i++)
        {
            ptr = util_append(ptr, end, "esp_celsius{address=\"%s\",hardware=\"%s\",sensor=\"%s\"} %0.3f\n", sensors_config[i].address, name, sensors_config[i].name, temperatures[i]);
        }
    }

    // Sensor errors
    ptr = util_append(ptr, end, "# TYPE esp_errors counter\n");
    if (sensor_errors)
    {
        ptr = util_append(ptr, end, "esp_celsius{hardware=\"%s\"} %zu\n", name, sensor_errors);
    }

    // Fan
    ptr = util_append(ptr, end, "# TYPE esp_rpm gauge\n");
    ptr = util_append(ptr, end, "esp_rpm{hardware=\"%s\",sensor=\"Fan\"} %u\n", name, pc_fan_rpm_last_value(rpm));

    ptr = util_append(ptr, end, "# TYPE esp_duty gauge\n");
    ptr = util_append(ptr, end, "esp_duty{hardware=\"%s\",sensor=\"Fan\"} %d\n", name, (int)(fan_duty_percent * 100.0f));

    // Send result
    if (ptr != NULL)
    {
        // Send data
        httpd_resp_set_type(r, "text/plain");
        return httpd_resp_send(r, buf, (ssize_t)(ptr - buf));
    }
    else
    {
        // Buffer overflow
        return ESP_FAIL;
    }
}

static void loop()
{
    // Read temperatures
    if (sensors && sensors->count > 0)
    {
        ESP_ERROR_CHECK_WITHOUT_ABORT(ds18b20_group_convert(sensors));
        ESP_ERROR_CHECK_WITHOUT_ABORT(ds18b20_group_wait_for_conversion(sensors));

        for (size_t i = 0; i < sensors->count; i++)
        {
            float temp_c = -127;
            if (ds18b20_group_read_single(sensors, i, &temp_c) == ESP_OK && temp_c > -70)
            {
                temp_c += sensors_config[i].offset_c;
                temperatures[i] = temp_c;
                ESP_LOGI(TAG, "read temperature %s: %.3f C", sensors_config[i].address, temp_c);
            }
            else
            {
                ++sensor_errors;
                ESP_LOGW(TAG, "failed to read from %s", sensors_config[i].address);
            }
        }

        // Find primary temperature
        float primary_temp_c = temperatures[primary_sensor_index < sensors->count ? primary_sensor_index : 0];
        ESP_LOGI(TAG, "primary temperature: %.3f C", primary_temp_c);

        // Limit to range
        float temp_in_range = primary_temp_c < low_temperature_threshold ? low_temperature_threshold : (primary_temp_c > high_temperature_threshold ? high_temperature_threshold : primary_temp_c);
        // Map temperature range to duty cycle
        float duty_percent = (temp_in_range - low_temperature_threshold) * (high_duty_percent - low_duty_percent) / (high_temperature_threshold - low_temperature_threshold) + low_duty_percent;

        // Control fan
        set_fan_duty(force_max_duty ? high_duty_percent : duty_percent);
    }
    else
    {
        // Fallback mode
        set_fan_duty(high_duty_percent);
    }

    ESP_LOGI(TAG, "rpm: %d", pc_fan_rpm_last_value(rpm));
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
        if ((++blink_counter % 8) == 0 && !status_led_is_active(STATUS_LED_DEFAULT))
        {
            status_led_set_interval_for(STATUS_LED_DEFAULT, 0, true, 20, false);
        }

        // Throttle
        vTaskDelayUntil(&start, APP_CONTROL_LOOP_INTERVAL / portTICK_PERIOD_MS);

        // Run control loop
        loop();
    }
}

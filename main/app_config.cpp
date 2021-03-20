#include "app_config.h"
#include <esp_log.h>

static const char TAG[] = "app_config";

void app_config_init_defaults(app_config_t *cfg)
{
    cfg->status_led_pin = GPIO_NUM_NC;
    cfg->status_led_on_state = true;
    cfg->pwm_pin = GPIO_NUM_NC;
    cfg->pwm_inverted_duty = false;
    for (size_t i = 0; i < APP_CONFIG_RPM_MAX_LENGTH; i++)
        cfg->rpm_pins[i] = GPIO_NUM_NC;
    cfg->sensors_pin = GPIO_NUM_NC;
    cfg->primary_sensor_address = 0;
    memset(cfg->sensors, 0, sizeof(cfg->sensors));
    cfg->low_threshold_celsius = 20;
    cfg->high_threshold_celsius = 70;
    cfg->low_threshold_duty_percent = 50;
    cfg->high_threshold_duty_percent = 100;
}

inline static bool is_valid_gpio_num(int pin)
{
    return pin == GPIO_NUM_NC || (pin >= 0 && GPIO_IS_VALID_GPIO(pin));
}

//
//esp_err_t app_config_load(app_config_t *cfg)
//{
//    if (cfg == NULL)
//    {
//        return ESP_ERR_INVALID_ARG;
//    }
//
//    // Open
//    nvs_handle_t handle = 0;
//    esp_err_t err = nvs_open(APP_CONFIG_NVS_NAME, NVS_READONLY, &handle);
//    if (err != ESP_OK)
//    {
//        return err;
//    }
//
//    // Prepare
//    size_t rpm_pins_len = APP_CONFIG_RPM_MAX_LENGTH;
//    int8_t rpm_pins[APP_CONFIG_RPM_MAX_LENGTH];
//    for (size_t i = 0; i < APP_CONFIG_RPM_MAX_LENGTH; i++)
//        rpm_pins[i] = GPIO_NUM_NC;
//
//    char key_buf[16] = {}; // max 15 chars
//
//    // Load
//    nvs_helper_get_gpio_num(handle, APP_CONFIG_KEY_STATUS_LED_PIN, &cfg->status_led_pin);
//    nvs_helper_get_bool(handle, APP_CONFIG_KEY_STATUS_LED_ON_STATE, &cfg->status_led_on_state);
//    nvs_helper_get_gpio_num(handle, APP_CONFIG_KEY_PWM_PIN, &cfg->pwm_pin);
//    nvs_helper_get_bool(handle, APP_CONFIG_KEY_PWM_INVERTED_DUTY, &cfg->pwm_inverted_duty);
//
//    nvs_get_blob(handle, APP_CONFIG_KEY_RPM_PINS, rpm_pins, &rpm_pins_len);
//    for (size_t i = 0; i < APP_CONFIG_RPM_MAX_LENGTH; i++)
//        cfg->rpm_pins[i] = (gpio_num_t)rpm_pins[i];
//
//    nvs_helper_get_gpio_num(handle, APP_CONFIG_KEY_SENSORS_PIN, &cfg->sensors_pin);
//    nvs_get_u64(handle, APP_CONFIG_KEY_PRIMARY_SENSOR_ADDRESS, &cfg->primary_sensor_address);
//
//    for (size_t i = 0; i < APP_CONFIG_SENSORS_MAX_LENGTH; i++)
//    {
//        nvs_get_u64(handle, nvs_helper_indexed_key(key_buf, sizeof(key_buf), "s%uz" APP_CONFIG_KEY_SENSOR_ADDRESS, i), &cfg->sensors[i].address);
//        size_t addr_len = sizeof(cfg->sensors[i].name);
//        nvs_get_str(handle, nvs_helper_indexed_key(key_buf, sizeof(key_buf), "s%uz" APP_CONFIG_KEY_SENSOR_NAME, i), cfg->sensors[i].name, &addr_len);
//        nvs_helper_get_float(handle, nvs_helper_indexed_key(key_buf, sizeof(key_buf), "s%uz" APP_CONFIG_KEY_SENSOR_OFFSET_C, i), CALIBRATION_PRECISION, &cfg->sensors[i].offset_c);
//    }
//
//    nvs_helper_get_float(handle, APP_CONFIG_KEY_LOW_THRESHOLD_CELSIUS, CELSIUS_PRECISION, &cfg->low_threshold_celsius);
//    nvs_helper_get_float(handle, APP_CONFIG_KEY_HIGH_THRESHOLD_CELSIUS, CELSIUS_PRECISION, &cfg->high_threshold_celsius);
//    nvs_get_u8(handle, APP_CONFIG_KEY_LOW_THRESHOLD_DUTY_PERCENT, &cfg->low_threshold_duty_percent);
//    nvs_get_u8(handle, APP_CONFIG_KEY_HIGH_THRESHOLD_DUTY_PERCENT, &cfg->high_threshold_duty_percent);
//
//    // Close and exit
//    nvs_close(handle);
//    return err;
//}
//
//static esp_err_t app_config_store_sensors(nvs_handle_t handle, const app_config_t *cfg)
//{
//    esp_err_t err = ESP_FAIL;
//    char key_buf[16] = {}; // max 15 chars
//
//    for (size_t i = 0; i < APP_CONFIG_SENSORS_MAX_LENGTH; i++)
//    {
//        const app_config_sensor_t *s = &cfg->sensors[i];
//
//        // Address
//        nvs_helper_indexed_key(key_buf, sizeof(key_buf), "s%uz" APP_CONFIG_KEY_SENSOR_ADDRESS, i);
//        if (s->address != 0)
//        {
//            HANDLE_ERROR(err = nvs_set_u64(handle, key_buf, s->address), return err);
//        }
//        else
//        {
//            HANDLE_ERROR_ERASE(err = nvs_erase_key(handle, key_buf), return err);
//        }
//
//        // Name
//        nvs_helper_indexed_key(key_buf, sizeof(key_buf), "s%uz" APP_CONFIG_KEY_SENSOR_NAME, i);
//        if (strlen(s->name))
//        {
//            HANDLE_ERROR(err = nvs_set_str(handle, key_buf, s->name), return err);
//        }
//        else
//        {
//            HANDLE_ERROR_ERASE(err = nvs_erase_key(handle, key_buf), return err);
//        }
//
//        // Calibration
//        nvs_helper_indexed_key(key_buf, sizeof(key_buf), "s%uz" APP_CONFIG_KEY_SENSOR_OFFSET_C, i);
//        if (s->offset_c != 0.0f)
//        {
//            HANDLE_ERROR(err = nvs_helper_set_float(handle, key_buf, CALIBRATION_PRECISION, s->offset_c), return err);
//        }
//        else
//        {
//            HANDLE_ERROR_ERASE(err = nvs_erase_key(handle, key_buf), return err);
//        }
//    }
//
//    return ESP_OK;
//}
//
//esp_err_t app_config_store(const app_config_t *cfg)
//{
//    if (cfg == NULL)
//    {
//        return ESP_ERR_INVALID_ARG;
//    }
//
//    // Open
//    nvs_handle_t handle = 0;
//    esp_err_t err = nvs_open(APP_CONFIG_NVS_NAME, NVS_READWRITE, &handle);
//    if (err != ESP_OK)
//    {
//        return err;
//    }
//
//    // Prepare data
//    int8_t rpm_pins[APP_CONFIG_RPM_MAX_LENGTH];
//    for (size_t i = 0; i < APP_CONFIG_RPM_MAX_LENGTH; i++)
//    {
//        rpm_pins[i] = cfg->rpm_pins[i];
//    }
//
//    // Store
//    HANDLE_ERROR(err = nvs_set_i8(handle, APP_CONFIG_KEY_STATUS_LED_PIN, cfg->status_led_pin), goto exit);
//    HANDLE_ERROR(err = nvs_set_u8(handle, APP_CONFIG_KEY_STATUS_LED_ON_STATE, cfg->status_led_on_state), goto exit);
//    HANDLE_ERROR(err = nvs_set_i8(handle, APP_CONFIG_KEY_PWM_PIN, cfg->pwm_pin), goto exit);
//    HANDLE_ERROR(err = nvs_set_u8(handle, APP_CONFIG_KEY_PWM_INVERTED_DUTY, cfg->pwm_inverted_duty), goto exit);
//    HANDLE_ERROR(err = nvs_set_blob(handle, APP_CONFIG_KEY_RPM_PINS, rpm_pins, sizeof(rpm_pins)), goto exit);
//    HANDLE_ERROR(err = nvs_set_i8(handle, APP_CONFIG_KEY_SENSORS_PIN, cfg->sensors_pin), goto exit);
//    HANDLE_ERROR(err = nvs_set_u64(handle, APP_CONFIG_KEY_PRIMARY_SENSOR_ADDRESS, cfg->primary_sensor_address), goto exit);
//    HANDLE_ERROR(err = app_config_store_sensors(handle, cfg), goto exit);
//    HANDLE_ERROR(err = nvs_helper_set_float(handle, APP_CONFIG_KEY_LOW_THRESHOLD_CELSIUS, CELSIUS_PRECISION, cfg->low_threshold_celsius), goto exit);
//    HANDLE_ERROR(err = nvs_helper_set_float(handle, APP_CONFIG_KEY_HIGH_THRESHOLD_CELSIUS, CELSIUS_PRECISION, cfg->high_threshold_celsius), goto exit);
//    HANDLE_ERROR(err = nvs_set_u8(handle, APP_CONFIG_KEY_LOW_THRESHOLD_DUTY_PERCENT, cfg->low_threshold_duty_percent), goto exit);
//    HANDLE_ERROR(err = nvs_set_u8(handle, APP_CONFIG_KEY_HIGH_THRESHOLD_DUTY_PERCENT, cfg->high_threshold_duty_percent), goto exit);
//
//    // Commit
//    err = nvs_commit(handle);
//    ESP_LOGI("app_config", "nvs_commit=%d", err);
//
//exit:
//    // Close and exit
//    nvs_close(handle);
//    return err;
//}

gpio_num_t rapidjson_get_gpionum(const rapidjson::GenericValue<rapidjson::UTF8<>> &val)
{
}

esp_err_t app_config_read(app_config_t *cfg, const rapidjson::GenericValue<rapidjson::UTF8<>> &data)
{
    if (cfg == nullptr)
    {
        return ESP_ERR_INVALID_ARG;
    }

    rapidjson_get_gpionum(data[APP_CONFIG_KEY_STATUS_LED_PIN]);

    cfg->status_led_pin = static_cast<gpio_num_t>(data[APP_CONFIG_KEY_STATUS_LED_PIN].GetInt());

    /*
    const char *value = NULL;
    size_t value_len = 0;

    if (JSON_SearchConst_Simple(data, data_len, APP_CONFIG_KEY_STATUS_LED_PIN, &value, &value_len, NULL) == JSONSuccess)
        cfg->status_led_pin = strtol(value, NULL, 10);

    json_helper_set_gpio_num(data, APP_CONFIG_KEY_STATUS_LED_PIN, changed, cfg, &cfg->status_led_pin);
    json_helper_set_bool(data, APP_CONFIG_KEY_STATUS_LED_ON_STATE, changed, &cfg->status_led_on_state);
    json_helper_set_gpio_num(data, APP_CONFIG_KEY_PWM_PIN, changed, cfg, &cfg->pwm_pin);
    json_helper_set_bool(data, APP_CONFIG_KEY_PWM_INVERTED_DUTY, changed, &cfg->pwm_inverted_duty);
    json_helper_set_gpio_num_array(data, APP_CONFIG_KEY_RPM_PINS, changed, cfg, cfg->rpm_pins, APP_CONFIG_RPM_MAX_LENGTH);
    json_helper_set_gpio_num(data, APP_CONFIG_KEY_SENSORS_PIN, changed, cfg, &cfg->sensors_pin);
    json_helper_set_address(data, APP_CONFIG_KEY_PRIMARY_SENSOR_ADDRESS, changed, &cfg->primary_sensor_address);

    cJSON *sensors = cJSON_GetObjectItemCaseSensitive(data, APP_CONFIG_KEY_SENSORS);
    if (cJSON_IsArray(sensors))
    {
        for (size_t i = 0; i < APP_CONFIG_SENSORS_MAX_LENGTH; i++)
        {
            cJSON *sensor = cJSON_GetArrayItem(sensors, i);
            json_helper_set_address(sensor, APP_CONFIG_KEY_SENSOR_ADDRESS, changed, &cfg->sensors[i].address);
            json_helper_set_string(sensor, APP_CONFIG_KEY_SENSOR_NAME, changed, cfg->sensors[i].name, sizeof(cfg->sensors[i].name));
            json_helper_set_float(sensor, APP_CONFIG_KEY_SENSOR_OFFSET_C, changed, &cfg->sensors[i].offset_c);
        }
    }

    json_helper_set_float(data, APP_CONFIG_KEY_LOW_THRESHOLD_CELSIUS, changed, &cfg->low_threshold_celsius);
    json_helper_set_float(data, APP_CONFIG_KEY_HIGH_THRESHOLD_CELSIUS, changed, &cfg->high_threshold_celsius);
    json_helper_set_u8(data, APP_CONFIG_KEY_LOW_THRESHOLD_DUTY_PERCENT, changed, &cfg->low_threshold_duty_percent);
    json_helper_set_u8(data, APP_CONFIG_KEY_HIGH_THRESHOLD_DUTY_PERCENT, changed, &cfg->high_threshold_duty_percent);
*/
    return ESP_OK;
}

esp_err_t app_config_write(const app_config_t *cfg, rapidjson::Writer<rapidjson::StringBuffer> &w)
{
    if (cfg == nullptr)
    {
        return ESP_ERR_INVALID_ARG;
    }

    char addr_str[17] = {};

    // Set data
    assert(w.Key(APP_CONFIG_KEY_STATUS_LED_PIN));
    assert(w.Int(cfg->status_led_pin));

    assert(w.Key(APP_CONFIG_KEY_STATUS_LED_ON_STATE));
    assert(w.Bool(cfg->status_led_on_state));

    assert(w.Key(APP_CONFIG_KEY_PWM_PIN));
    assert(w.Int(cfg->pwm_pin));

    assert(w.Key(APP_CONFIG_KEY_PWM_INVERTED_DUTY));
    assert(w.Bool(cfg->pwm_inverted_duty));

    //
    //    // Trim the array, don't report NC pins at the end
    //    size_t rpm_pins_actual_len = json_helper_gpio_num_count(cfg->rpm_pins, APP_CONFIG_RPM_MAX_LENGTH);
    //    cJSON *rpm_pins_array = cJSON_AddArrayToObject(data, APP_CONFIG_KEY_RPM_PINS);
    //    for (size_t i = 0; i < rpm_pins_actual_len; i++)
    //    {
    //        cJSON_AddItemToArray(rpm_pins_array, cJSON_CreateNumber(cfg->rpm_pins[i]));
    //    }

    assert(w.Key(APP_CONFIG_KEY_SENSORS_PIN));
    assert(w.Int(cfg->sensors_pin));

    assert(w.Key(APP_CONFIG_KEY_PRIMARY_SENSOR_ADDRESS));
    assert(w.String(app_config_print_address(addr_str, sizeof(addr_str), cfg->primary_sensor_address)));

    assert(w.Key(APP_CONFIG_KEY_SENSORS));
    assert(w.StartArray());

    for (const auto &sensor : cfg->sensors)
    {
        if (sensor.address == 0 && strlen(sensor.name) == 0 && sensor.offset_c == 0.0f)
        {
            continue;
        }

        assert(w.StartObject());

        assert(w.Key(APP_CONFIG_KEY_SENSOR_ADDRESS));
        assert(w.String(app_config_print_address(addr_str, sizeof(addr_str), sensor.address)));

        assert(w.Key(APP_CONFIG_KEY_SENSOR_NAME));
        assert(w.String(sensor.name));

        assert(w.Key(APP_CONFIG_KEY_SENSOR_OFFSET_C));
        assert(w.Double(sensor.offset_c));

        assert(w.EndObject());
    }

    assert(w.EndArray());

    //
    //    cJSON_AddNumberToObject(data, APP_CONFIG_KEY_LOW_THRESHOLD_CELSIUS, cfg->low_threshold_celsius);
    //    cJSON_AddNumberToObject(data, APP_CONFIG_KEY_HIGH_THRESHOLD_CELSIUS, cfg->high_threshold_celsius);
    //    cJSON_AddNumberToObject(data, APP_CONFIG_KEY_LOW_THRESHOLD_DUTY_PERCENT, cfg->low_threshold_duty_percent);
    //    cJSON_AddNumberToObject(data, APP_CONFIG_KEY_HIGH_THRESHOLD_DUTY_PERCENT, cfg->high_threshold_duty_percent);

    return ESP_OK;
}

char *app_config_print_address(char *buf, size_t buf_len, uint64_t value)
{
    int c = snprintf(buf, buf_len, "%08x%08x", (uint32_t)(value >> 32), (uint32_t)value);
    assert(c >= 0 && c < buf_len);
    return buf;
}

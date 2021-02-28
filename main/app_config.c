#include "app_config.h"
#include <esp_log.h>
#include <nvs.h>
#include <string.h>

static const char TAG[] = "app_config";

#define HANDLE_ERROR(expr, action)  \
    {                               \
        esp_err_t err_ = (expr);    \
        if (err_ != ESP_OK) action; \
    }                               \
    (void)0

#define HANDLE_ERROR_ERASE(expr, action)                             \
    {                                                                \
        esp_err_t err_ = (expr);                                     \
        if (err_ != ESP_OK && err_ != ESP_ERR_NVS_NOT_FOUND) action; \
    }                                                                \
    (void)0

static const float CELSIUS_PRECISION = 100;
static const float CALIBRATION_PRECISION = 1000;

static char *nvs_helper_indexed_key(char *buf, size_t buf_len, const char *format, size_t index)
{
    int c = snprintf(buf, buf_len, format, index);
    assert(c >= 0 && c < buf_len);
    return buf;
}

static void nvs_helper_get_gpio_num(nvs_handle_t handle, const char *key, gpio_num_t *out_value)
{
    int8_t value;
    if (nvs_get_i8(handle, key, &value) == ESP_OK)
    {
        *out_value = (gpio_num_t)value;
        ESP_LOGD(TAG, "nvs get gpio_num %s: %d", key, value);
        return;
    }
}

static void nvs_helper_get_float(nvs_handle_t handle, char *key, float precision_factor, float *out_value)
{
    int32_t value;
    if (nvs_get_i32(handle, key, &value) == ESP_OK)
    {
        *out_value = (float)value / precision_factor;
        ESP_LOGD(TAG, "nvs get float %s: %f", key, *out_value);
        return;
    }
}

static esp_err_t nvs_helper_set_float(nvs_handle_t handle, char *key, float precision_factor, float in_value)
{
    int32_t value = in_value * precision_factor;
    ESP_LOGD(TAG, "nvs set float %s: %d / %f", key, value, precision_factor);
    return nvs_set_i32(handle, key, value);
}

inline static bool is_valid_gpio_num(int pin)
{
    return pin == GPIO_NUM_NC || (pin >= 0 && GPIO_IS_VALID_GPIO(pin));
}

inline static bool is_pin_used(const app_config_t *cfg, gpio_num_t pin)
{
    // NC is special case, it is always "free"
    if (pin == GPIO_NUM_NC)
    {
        return false;
    }

    if (cfg->status_led_pin == pin) return true;
    if (cfg->pwm_pin == pin) return true;
    if (cfg->sensors_pin == pin) return true;

    for (size_t i = 0; i < APP_CONFIG_RPM_MAX_LENGTH; i++)
    {
        if (cfg->rpm_pins[i] == pin) return true;
    }

    return false;
}

static void json_helper_obj_gpio_num(const cJSON *value_obj, bool *changed, const app_config_t *cfg, gpio_num_t *out_value)
{
    if (cJSON_IsNumber(value_obj) && is_valid_gpio_num(value_obj->valueint))
    {
        if (*out_value != (gpio_num_t)value_obj->valueint)
        {
            // Validate - this must be called only if change is detected
            if (is_pin_used(cfg, (gpio_num_t)value_obj->valueint))
            {
                // Don't store incompatible value
                return;
            }

            // Value changed
            *changed = true;
            *out_value = (gpio_num_t)value_obj->valueint;
        }
    }
}

inline static void json_helper_set_gpio_num(const cJSON *data, char *key, bool *changed, const app_config_t *cfg, gpio_num_t *out_value)
{
    cJSON *value_obj = cJSON_GetObjectItemCaseSensitive(data, key);
    json_helper_obj_gpio_num(value_obj, changed, cfg, out_value);
}

static size_t json_helper_gpio_num_count(const gpio_num_t *pins, size_t pin_len)
{
    size_t actual_len = 0;
    for (size_t l = pin_len; l > 0; l--)
    {
        if (pins[l - 1] != GPIO_NUM_NC)
        {
            actual_len = l;
            break;
        }
    }
    return actual_len;
}

static void json_helper_set_gpio_num_array(const cJSON *data, char *key, bool *changed, const app_config_t *cfg, gpio_num_t *out_value, size_t out_value_len)
{
    cJSON *array_obj = cJSON_GetObjectItemCaseSensitive(data, key);
    if (cJSON_IsArray(array_obj))
    {
        int array_len = cJSON_GetArraySize(array_obj);
        for (size_t i = 0; i < out_value_len; i++)
        {
            if (i < array_len)
            {
                // Reset to false if any pin is invalid
                json_helper_obj_gpio_num(cJSON_GetArrayItem(array_obj, i), changed, cfg, &out_value[i]);
            }
            else
            {
                out_value[i] = GPIO_NUM_NC;
            }
        }
    }
}

static void json_helper_set_bool(const cJSON *data, char *key, bool *changed, bool *out_value)
{
    cJSON *value_obj = cJSON_GetObjectItemCaseSensitive(data, key);
    if (cJSON_IsBool(value_obj))
    {
        bool value_bool = cJSON_IsTrue(value_obj);
        if (*out_value != value_bool)
        {
            // Value changed
            *out_value = value_bool;
            *changed = true;
        }
    }
}

static void json_helper_set_u8(const cJSON *data, char *key, bool *changed, uint8_t *out_value)
{
    cJSON *value_obj = cJSON_GetObjectItemCaseSensitive(data, key);
    if (cJSON_IsNumber(value_obj) && value_obj->valueint >= 0 && value_obj->valueint <= 255)
    {
        if (*out_value != (uint8_t)value_obj->valueint)
        {
            // Value changed
            *out_value = (uint8_t)value_obj->valueint;
            *changed = true;
        }
    }
}

static void json_helper_set_float(const cJSON *data, char *key, bool *changed, float *out_value)
{
    cJSON *value_obj = cJSON_GetObjectItemCaseSensitive(data, key);
    if (cJSON_IsNumber(value_obj))
    {
        float value = (float)value_obj->valuedouble;
        if (*out_value != value)
        {
            // Value changed
            *out_value = value;
            *changed = true;
        }
    }
}

static cJSON *json_helper_add_address_to_object(cJSON *obj, const char *key, uint64_t address)
{
    char addr_str[17] = {};
    return cJSON_AddStringToObject(obj, key, app_config_print_address(addr_str, sizeof(addr_str), address));
}

static void json_helper_set_address(const cJSON *data, char *key, bool *changed, uint64_t *out_value)
{
    cJSON *value_obj = cJSON_GetObjectItemCaseSensitive(data, key);
    if (cJSON_IsString(value_obj))
    {
        uint64_t value = strtoull(value_obj->valuestring, NULL, 16);
        if (*out_value != value)
        {
            // Value changed
            *out_value = value;
            *changed = true;
        }
    }
}

static void json_helper_set_string(const cJSON *data, char *key, bool *changed, char *out_value, size_t out_value_len)
{
    cJSON *value_obj = cJSON_GetObjectItemCaseSensitive(data, key);
    if (cJSON_IsString(value_obj))
    {
        if (strncmp(out_value, value_obj->valuestring, out_value_len) != 0)
        {
            // Value changed
            // NOTE strncpy skips null terminator if max len is reached, so -1
            strncpy(out_value, value_obj->valuestring, out_value_len - 1);
            *changed = true;
        }
    }
}

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

esp_err_t app_config_load(const struct serialization_context *ctx, app_config_t *cfg)
{
    if (cfg == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    // Open
    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open(APP_CONFIG_NVS_NAME, NVS_READONLY, &handle);
    if (err != ESP_OK)
    {
        return err;
    }

    // Prepare
    size_t rpm_pins_len = APP_CONFIG_RPM_MAX_LENGTH;
    int8_t rpm_pins[APP_CONFIG_RPM_MAX_LENGTH];
    for (size_t i = 0; i < APP_CONFIG_RPM_MAX_LENGTH; i++)
        rpm_pins[i] = GPIO_NUM_NC;

    char key_buf[16] = {}; // max 15 chars

    // Load
    serialization_get_i32(ctx, APP_CONFIG_KEY_STATUS_LED_PIN, &cfg->status_led_pin);
    serialization_get_bool(ctx, APP_CONFIG_KEY_STATUS_LED_ON_STATE, &cfg->status_led_on_state);
    serialization_get_i32(ctx, APP_CONFIG_KEY_PWM_PIN, &cfg->pwm_pin);
    serialization_get_bool(ctx, APP_CONFIG_KEY_PWM_INVERTED_DUTY, &cfg->pwm_inverted_duty);

    nvs_get_blob(handle, APP_CONFIG_KEY_RPM_PINS, rpm_pins, &rpm_pins_len);
    for (size_t i = 0; i < APP_CONFIG_RPM_MAX_LENGTH; i++)
        cfg->rpm_pins[i] = (gpio_num_t)rpm_pins[i];

    nvs_helper_get_gpio_num(handle, APP_CONFIG_KEY_SENSORS_PIN, &cfg->sensors_pin);
    nvs_get_u64(handle, APP_CONFIG_KEY_PRIMARY_SENSOR_ADDRESS, &cfg->primary_sensor_address);

    for (size_t i = 0; i < APP_CONFIG_SENSORS_MAX_LENGTH; i++)
    {
        nvs_get_u64(handle, nvs_helper_indexed_key(key_buf, sizeof(key_buf), "s%uz" APP_CONFIG_KEY_SENSOR_ADDRESS, i), &cfg->sensors[i].address);
        size_t addr_len = sizeof(cfg->sensors[i].name);
        nvs_get_str(handle, nvs_helper_indexed_key(key_buf, sizeof(key_buf), "s%uz" APP_CONFIG_KEY_SENSOR_NAME, i), cfg->sensors[i].name, &addr_len);
        nvs_helper_get_float(handle, nvs_helper_indexed_key(key_buf, sizeof(key_buf), "s%uz" APP_CONFIG_KEY_SENSOR_OFFSET_C, i), CALIBRATION_PRECISION, &cfg->sensors[i].offset_c);
    }

    nvs_helper_get_float(handle, APP_CONFIG_KEY_LOW_THRESHOLD_CELSIUS, CELSIUS_PRECISION, &cfg->low_threshold_celsius);
    nvs_helper_get_float(handle, APP_CONFIG_KEY_HIGH_THRESHOLD_CELSIUS, CELSIUS_PRECISION, &cfg->high_threshold_celsius);
    nvs_get_u8(handle, APP_CONFIG_KEY_LOW_THRESHOLD_DUTY_PERCENT, &cfg->low_threshold_duty_percent);
    nvs_get_u8(handle, APP_CONFIG_KEY_HIGH_THRESHOLD_DUTY_PERCENT, &cfg->high_threshold_duty_percent);

    // Close and exit
    nvs_close(handle);
    return err;
}

static esp_err_t app_config_store_sensors(nvs_handle_t handle, const app_config_t *cfg)
{
    esp_err_t err = ESP_FAIL;
    char key_buf[16] = {}; // max 15 chars

    for (size_t i = 0; i < APP_CONFIG_SENSORS_MAX_LENGTH; i++)
    {
        const app_config_sensor_t *s = &cfg->sensors[i];

        // Address
        nvs_helper_indexed_key(key_buf, sizeof(key_buf), "s%uz" APP_CONFIG_KEY_SENSOR_ADDRESS, i);
        if (s->address != 0)
        {
            HANDLE_ERROR(err = nvs_set_u64(handle, key_buf, s->address), return err);
        }
        else
        {
            HANDLE_ERROR_ERASE(err = nvs_erase_key(handle, key_buf), return err);
        }

        // Name
        nvs_helper_indexed_key(key_buf, sizeof(key_buf), "s%uz" APP_CONFIG_KEY_SENSOR_NAME, i);
        if (strlen(s->name))
        {
            HANDLE_ERROR(err = nvs_set_str(handle, key_buf, s->name), return err);
        }
        else
        {
            HANDLE_ERROR_ERASE(err = nvs_erase_key(handle, key_buf), return err);
        }

        // Calibration
        nvs_helper_indexed_key(key_buf, sizeof(key_buf), "s%uz" APP_CONFIG_KEY_SENSOR_OFFSET_C, i);
        if (s->offset_c != 0.0f)
        {
            HANDLE_ERROR(err = nvs_helper_set_float(handle, key_buf, CALIBRATION_PRECISION, s->offset_c), return err);
        }
        else
        {
            HANDLE_ERROR_ERASE(err = nvs_erase_key(handle, key_buf), return err);
        }
    }

    return ESP_OK;
}

esp_err_t app_config_store(const app_config_t *cfg)
{
    if (cfg == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    // Open
    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open(APP_CONFIG_NVS_NAME, NVS_READWRITE, &handle);
    if (err != ESP_OK)
    {
        return err;
    }

    // Prepare data
    int8_t rpm_pins[APP_CONFIG_RPM_MAX_LENGTH];
    for (size_t i = 0; i < APP_CONFIG_RPM_MAX_LENGTH; i++)
    {
        rpm_pins[i] = cfg->rpm_pins[i];
    }

    // Store
    HANDLE_ERROR(err = nvs_set_i8(handle, APP_CONFIG_KEY_STATUS_LED_PIN, cfg->status_led_pin), goto exit);
    HANDLE_ERROR(err = nvs_set_u8(handle, APP_CONFIG_KEY_STATUS_LED_ON_STATE, cfg->status_led_on_state), goto exit);
    HANDLE_ERROR(err = nvs_set_i8(handle, APP_CONFIG_KEY_PWM_PIN, cfg->pwm_pin), goto exit);
    HANDLE_ERROR(err = nvs_set_u8(handle, APP_CONFIG_KEY_PWM_INVERTED_DUTY, cfg->pwm_inverted_duty), goto exit);
    HANDLE_ERROR(err = nvs_set_blob(handle, APP_CONFIG_KEY_RPM_PINS, rpm_pins, sizeof(rpm_pins)), goto exit);
    HANDLE_ERROR(err = nvs_set_i8(handle, APP_CONFIG_KEY_SENSORS_PIN, cfg->sensors_pin), goto exit);
    HANDLE_ERROR(err = nvs_set_u64(handle, APP_CONFIG_KEY_PRIMARY_SENSOR_ADDRESS, cfg->primary_sensor_address), goto exit);
    HANDLE_ERROR(err = app_config_store_sensors(handle, cfg), goto exit);
    HANDLE_ERROR(err = nvs_helper_set_float(handle, APP_CONFIG_KEY_LOW_THRESHOLD_CELSIUS, CELSIUS_PRECISION, cfg->low_threshold_celsius), goto exit);
    HANDLE_ERROR(err = nvs_helper_set_float(handle, APP_CONFIG_KEY_HIGH_THRESHOLD_CELSIUS, CELSIUS_PRECISION, cfg->high_threshold_celsius), goto exit);
    HANDLE_ERROR(err = nvs_set_u8(handle, APP_CONFIG_KEY_LOW_THRESHOLD_DUTY_PERCENT, cfg->low_threshold_duty_percent), goto exit);
    HANDLE_ERROR(err = nvs_set_u8(handle, APP_CONFIG_KEY_HIGH_THRESHOLD_DUTY_PERCENT, cfg->high_threshold_duty_percent), goto exit);

    // Commit
    err = nvs_commit(handle);
    ESP_LOGI("app_config", "nvs_commit=%d", err);

exit:
    // Close and exit
    nvs_close(handle);
    return err;
}

esp_err_t app_config_update_from(app_config_t *cfg, const cJSON *data, bool *changed)
{
    if (cfg == NULL || data == NULL || changed == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

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

    return ESP_OK;
}

static void app_config_add_sensor_to(cJSON *sensors, const app_config_sensor_t *s)
{
    // Ignore empty
    if (s->address == 0 && strlen(s->name) == 0 && s->offset_c == 0.0f)
    {
        return;
    }

    cJSON *sensor_obj = cJSON_CreateObject();
    cJSON_AddItemToArray(sensors, sensor_obj);

    json_helper_add_address_to_object(sensor_obj, APP_CONFIG_KEY_SENSOR_ADDRESS, s->address);
    cJSON_AddStringToObject(sensor_obj, APP_CONFIG_KEY_SENSOR_NAME, s->name);
    cJSON_AddNumberToObject(sensor_obj, APP_CONFIG_KEY_SENSOR_OFFSET_C, s->offset_c);
}

esp_err_t app_config_add_to(const app_config_t *cfg, cJSON *data)
{
    if (cfg == NULL || data == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    // Set data
    cJSON_AddNumberToObject(data, APP_CONFIG_KEY_STATUS_LED_PIN, cfg->status_led_pin);
    cJSON_AddBoolToObject(data, APP_CONFIG_KEY_STATUS_LED_ON_STATE, cfg->status_led_on_state);
    cJSON_AddNumberToObject(data, APP_CONFIG_KEY_PWM_PIN, cfg->pwm_pin);
    cJSON_AddBoolToObject(data, APP_CONFIG_KEY_PWM_INVERTED_DUTY, cfg->pwm_inverted_duty);

    // Trim the array, don't report NC pins at the end
    size_t rpm_pins_actual_len = json_helper_gpio_num_count(cfg->rpm_pins, APP_CONFIG_RPM_MAX_LENGTH);
    cJSON *rpm_pins_array = cJSON_AddArrayToObject(data, APP_CONFIG_KEY_RPM_PINS);
    for (size_t i = 0; i < rpm_pins_actual_len; i++)
    {
        cJSON_AddItemToArray(rpm_pins_array, cJSON_CreateNumber(cfg->rpm_pins[i]));
    }

    cJSON_AddNumberToObject(data, APP_CONFIG_KEY_SENSORS_PIN, cfg->sensors_pin);
    json_helper_add_address_to_object(data, APP_CONFIG_KEY_PRIMARY_SENSOR_ADDRESS, cfg->primary_sensor_address);

    cJSON *sensors = cJSON_AddArrayToObject(data, APP_CONFIG_KEY_SENSORS);
    if (sensors) // NULL if already exists, therefore add failed
    {
        for (size_t i = 0; i < APP_CONFIG_SENSORS_MAX_LENGTH; i++)
        {
            app_config_add_sensor_to(sensors, &cfg->sensors[i]);
        }
    }

    cJSON_AddNumberToObject(data, APP_CONFIG_KEY_LOW_THRESHOLD_CELSIUS, cfg->low_threshold_celsius);
    cJSON_AddNumberToObject(data, APP_CONFIG_KEY_HIGH_THRESHOLD_CELSIUS, cfg->high_threshold_celsius);
    cJSON_AddNumberToObject(data, APP_CONFIG_KEY_LOW_THRESHOLD_DUTY_PERCENT, cfg->low_threshold_duty_percent);
    cJSON_AddNumberToObject(data, APP_CONFIG_KEY_HIGH_THRESHOLD_DUTY_PERCENT, cfg->high_threshold_duty_percent);

    return ESP_OK;
}

char *app_config_print_address(char *buf, size_t buf_len, uint64_t value)
{
    int c = snprintf(buf, buf_len, "%08x%08x", (uint32_t)(value >> 32), (uint32_t)value);
    assert(c >= 0 && c < buf_len);
    return buf;
}

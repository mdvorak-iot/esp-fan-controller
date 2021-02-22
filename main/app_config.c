#include "app_config.h"
#include <esp_log.h>
#include <nvs.h>
#include <string.h>

#define HANDLE_ERROR(expr, action)  \
    {                               \
        esp_err_t err_ = (expr);    \
        if (err_ != ESP_OK) action; \
    }                               \
    (void)0

static void nvs_helper_get_gpio_num(nvs_handle_t handle, const char *key, gpio_num_t *out_value)
{
    uint8_t value;
    if (nvs_get_u8(handle, key, &value) == ESP_OK)
    {
        *out_value = value;
    }
}

static void nvs_helper_get_bool(nvs_handle_t handle, const char *key, bool *out_value)
{
    uint8_t value;
    if (nvs_get_u8(handle, key, &value) == ESP_OK)
    {
        *out_value = value;
    }
}

inline static bool is_valid_gpio_num(int pin)
{
    return pin == GPIO_NUM_NC || (pin >= GPIO_NUM_0 && pin < GPIO_NUM_MAX);
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

static void json_helper_get_gpio_num(char *key, const cJSON *desired, bool *changed, cJSON *reported, const app_config_t *cfg, gpio_num_t *out_value)
{
    cJSON *value_obj = cJSON_GetObjectItemCaseSensitive(desired, key);
    if (cJSON_IsNumber(value_obj) && is_valid_gpio_num(value_obj->valueint))
    {
        if (*out_value != (gpio_num_t)value_obj->valueint)
        {
            // Validate - this must be called only if change is detected
            if (is_pin_used(cfg, (gpio_num_t)value_obj->valueint))
            {
                // Don't store or report incompatible value
                return;
            }

            // Value changed
            *out_value = (gpio_num_t)value_obj->valueint;
            *changed = true;
        }
        if (reported)
        {
            // Report, regardless whether value has changed
            cJSON_AddNumberToObject(reported, key, *out_value);
        }
    }
}

static void json_helper_get_bool(char *key, const cJSON *desired, bool *changed, cJSON *reported, bool *out_value)
{
    cJSON *value_obj = cJSON_GetObjectItemCaseSensitive(desired, key);
    if (cJSON_IsBool(value_obj))
    {
        bool value_bool = cJSON_IsTrue(value_obj);
        if (*out_value != value_bool)
        {
            // Value changed
            *out_value = value_bool;
            *changed = true;
        }
        if (reported)
        {
            // Report, regardless whether value has changed
            cJSON_AddBoolToObject(reported, key, *out_value);
        }
    }
}

static void json_helper_get_u8(char *key, const cJSON *desired, bool *changed, cJSON *reported, uint8_t *out_value)
{
    cJSON *value_obj = cJSON_GetObjectItemCaseSensitive(desired, key);
    if (cJSON_IsNumber(value_obj) && value_obj->valueint >= 0 && value_obj->valueint <= 255)
    {
        if (*out_value != (uint8_t)value_obj->valueint)
        {
            // Value changed
            *out_value = (uint8_t)value_obj->valueint;
            *changed = true;
        }
        if (reported)
        {
            // Report, regardless whether value has changed
            cJSON_AddNumberToObject(reported, key, *out_value);
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
    cfg->low_threshold_celsius = 0;
    cfg->high_threshold_celsius = 100;
    cfg->low_threshold_duty_percent = 0;
    cfg->high_threshold_duty_percent = 100;
}

esp_err_t app_config_load(app_config_t *cfg)
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

    // Load
    nvs_helper_get_gpio_num(handle, APP_CONFIG_KEY_STATUS_LED_PIN, &cfg->status_led_pin);
    nvs_helper_get_bool(handle, APP_CONFIG_KEY_STATUS_LED_ON_STATE_SHORT, &cfg->status_led_on_state);
    nvs_helper_get_gpio_num(handle, APP_CONFIG_KEY_PWM_PIN, &cfg->pwm_pin);
    nvs_helper_get_bool(handle, APP_CONFIG_KEY_PWM_INVERTED_DUTY_SHORT, &cfg->pwm_inverted_duty);
    nvs_helper_get_gpio_num(handle, APP_CONFIG_KEY_SENSORS_PIN, &cfg->sensors_pin);

    // Close and exit
    nvs_close(handle);
    return err;
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

    // Store
    HANDLE_ERROR(err = nvs_set_u8(handle, APP_CONFIG_KEY_STATUS_LED_PIN, cfg->status_led_pin), goto exit);
    HANDLE_ERROR(err = nvs_set_u8(handle, APP_CONFIG_KEY_STATUS_LED_ON_STATE_SHORT, cfg->status_led_on_state), goto exit);
    HANDLE_ERROR(err = nvs_set_u8(handle, APP_CONFIG_KEY_PWM_PIN, cfg->pwm_pin), goto exit);
    HANDLE_ERROR(err = nvs_set_u8(handle, APP_CONFIG_KEY_PWM_INVERTED_DUTY_SHORT, cfg->pwm_inverted_duty), goto exit);
    HANDLE_ERROR(err = nvs_set_u8(handle, APP_CONFIG_KEY_SENSORS_PIN, cfg->sensors_pin), goto exit);

    // Commit
    err = nvs_commit(handle);

exit:
    // Close and exit
    nvs_close(handle);
    return err;
}

esp_err_t app_config_update_from(app_config_t *cfg, const cJSON *data, bool *changed, cJSON *reported)
{
    if (cfg == NULL || data == NULL || changed == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    json_helper_get_gpio_num(APP_CONFIG_KEY_STATUS_LED_PIN, data, changed, reported, cfg, &cfg->status_led_pin);
    json_helper_get_bool(APP_CONFIG_KEY_STATUS_LED_ON_STATE, data, changed, reported, &cfg->status_led_on_state);
    json_helper_get_gpio_num(APP_CONFIG_KEY_PWM_PIN, data, changed, reported, cfg, &cfg->pwm_pin);
    json_helper_get_bool(APP_CONFIG_KEY_PWM_INVERTED_DUTY, data, changed, reported, &cfg->pwm_inverted_duty);
    json_helper_get_gpio_num(APP_CONFIG_KEY_SENSORS_PIN, data, changed, reported, cfg, &cfg->sensors_pin);

    return ESP_OK;
}

esp_err_t app_config_write_to(const app_config_t *cfg, cJSON *data)
{
    if (cfg == NULL || data == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    cJSON_AddNumberToObject(data, APP_CONFIG_KEY_STATUS_LED_PIN, cfg->status_led_pin);
    cJSON_AddBoolToObject(data, APP_CONFIG_KEY_STATUS_LED_ON_STATE, cfg->status_led_on_state);
    cJSON_AddNumberToObject(data, APP_CONFIG_KEY_PWM_PIN, cfg->pwm_pin);
    cJSON_AddBoolToObject(data, APP_CONFIG_KEY_PWM_INVERTED_DUTY, cfg->pwm_inverted_duty);
    cJSON_AddNumberToObject(data, APP_CONFIG_KEY_SENSORS_PIN, cfg->sensors_pin);

    return ESP_OK;
}

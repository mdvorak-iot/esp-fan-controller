#include "app_config.h"
#include <esp_log.h>
#include <nvs.h>

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

inline static bool validate_pin(const app_config_t *cfg, gpio_num_t pin)
{
    if (cfg->status_led_pin == pin) return false;
    if (cfg->pwm_pin == pin) return false;
    if (cfg->sensors_pin == pin) return false;

    for (size_t i = 0; i < APP_CONFIG_RPM_MAX_LENGTH; i++)
    {
        if (cfg->rpm_pins[i] == pin) return false;
    }

    return true;
}

static void json_helper_get_gpio_num(char *key, const cJSON *desired, bool *changed, cJSON *reported, const app_config_t *cfg, gpio_num_t *out_value)
{
    cJSON *value_obj = cJSON_GetObjectItemCaseSensitive(desired, key);
    if (cJSON_IsNumber(value_obj) && value_obj->valueint > 0 && value_obj->valueint < GPIO_NUM_MAX)
    {
        if (*out_value != (gpio_num_t)value_obj->valueint)
        {
            // Validate - this must be called only if change is detected
            if (!validate_pin(cfg, (gpio_num_t)value_obj->valueint))
            {
                // Don't store or report invalid value
                return;
            }

            // Value changed
            *out_value = (gpio_num_t)value_obj->valueint;
            *changed = true;
        }
        if (desired)
        {
            // Report, regardless whether value has changed
            cJSON_AddNumberToObject(reported, key, *out_value);
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
        if (desired)
        {
            // Report, regardless whether value has changed
            cJSON_AddNumberToObject(reported, key, *out_value);
        }
    }
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
    nvs_helper_get_gpio_num(handle, APP_CONFIG_KEY_PWM_PIN, &cfg->pwm_pin);
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
    HANDLE_ERROR(err = nvs_set_u8(handle, APP_CONFIG_KEY_PWM_PIN, cfg->pwm_pin), goto exit);
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
    json_helper_get_gpio_num(APP_CONFIG_KEY_PWM_PIN, data, changed, reported, cfg, &cfg->pwm_pin);
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
    cJSON_AddNumberToObject(data, APP_CONFIG_KEY_PWM_PIN, cfg->pwm_pin);
    cJSON_AddNumberToObject(data, APP_CONFIG_KEY_SENSORS_PIN, cfg->sensors_pin);

    return ESP_OK;
}

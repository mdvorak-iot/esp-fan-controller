#pragma once

#include <cJSON.h>
#include <driver/gpio.h>

#ifdef __cplusplus
extern "C" {
#endif

// TODO use Kconfig for constants

/**
 * @brief Max supported rpm sensor count.
 */
#define APP_CONFIG_RPM_MAX_LENGTH 12
/**
 * @brief Max supported temperature sensor count.
 */
#define APP_CONFIG_SENSORS_MAX_LENGTH 10
/**
 * @brief Maximum sensor name length, including terminating char.
 */
#define APP_CONFIG_MAX_NAME_LENGHT 33

struct __packed app_config_sensor_t
{
    uint64_t address;
    char name[APP_CONFIG_MAX_NAME_LENGHT];
};

struct __packed app_config_t
{
    gpio_num_t status_led_pin;
    bool status_led_on_state;
    gpio_num_t pwm_pin;
    bool pwm_inverted_duty;
    gpio_num_t rpm_pins[APP_CONFIG_RPM_MAX_LENGTH];
    gpio_num_t sensors_pin;
    uint64_t primary_sensor_address;
    app_config_sensor_t sensors[APP_CONFIG_SENSORS_MAX_LENGTH];
    uint8_t low_threshold_celsius;
    uint8_t high_threshold_celsius;
    uint8_t low_threshold_duty_percent;
    uint8_t high_threshold_duty_percent;
};

esp_err_t app_config_load(app_config_t *cfg);

esp_err_t app_config_store(const app_config_t *cfg);

esp_err_t app_config_update_from(app_config_t *cfg, const cJSON *desired, bool *changed, cJSON *reported);

const auto APP_CONFIG_NVS_NAME = "appconfig";
const auto APP_CONFIG_NVS_KEY_STATUS_LED_PIN = "status_led_pin";

#ifdef __cplusplus
}
#endif

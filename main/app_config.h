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

typedef struct app_config_sensor
{
    uint64_t address;
    char name[APP_CONFIG_MAX_NAME_LENGHT];
    float offset_c;
} app_config_sensor_t;

typedef struct app_config
{
    gpio_num_t status_led_pin;
    bool status_led_on_state;
    gpio_num_t pwm_pin;
    bool pwm_inverted_duty;
    gpio_num_t rpm_pins[APP_CONFIG_RPM_MAX_LENGTH];
    gpio_num_t sensors_pin;
    uint64_t primary_sensor_address;
    app_config_sensor_t sensors[APP_CONFIG_SENSORS_MAX_LENGTH];
    float low_threshold_celsius;
    float high_threshold_celsius;
    uint8_t low_threshold_duty_percent;
    uint8_t high_threshold_duty_percent;
} app_config_t;

void app_config_init_defaults(app_config_t *cfg);

esp_err_t app_config_load(app_config_t *cfg);

esp_err_t app_config_store(const app_config_t *cfg);

esp_err_t app_config_update_from(app_config_t *cfg, const cJSON *data, bool *changed);

esp_err_t app_config_add_to(const app_config_t *cfg, cJSON *data);

char *app_config_print_address(char *buf, size_t buf_len, uint64_t value);

#define APP_CONFIG_NVS_NAME "app_config"
#define APP_CONFIG_KEY_STATUS_LED_PIN "status_led_pin"
#define APP_CONFIG_KEY_STATUS_LED_ON_STATE "status_led_on"
#define APP_CONFIG_KEY_PWM_PIN "pwm_pin"
#define APP_CONFIG_KEY_PWM_INVERTED_DUTY "pwm_inverted"
#define APP_CONFIG_KEY_RPM_PINS "rpm_pins"
#define APP_CONFIG_KEY_SENSORS_PIN "sensors_pin"
#define APP_CONFIG_KEY_PRIMARY_SENSOR_ADDRESS "primary_addr"
#define APP_CONFIG_KEY_SENSORS "sensors"
#define APP_CONFIG_KEY_SENSOR_ADDRESS "addr"
#define APP_CONFIG_KEY_SENSOR_NAME "name"
#define APP_CONFIG_KEY_SENSOR_OFFSET_C "offset_c"
#define APP_CONFIG_KEY_LOW_THRESHOLD_CELSIUS "lo_thresh_c"
#define APP_CONFIG_KEY_HIGH_THRESHOLD_CELSIUS "hi_thresh_c"
#define APP_CONFIG_KEY_LOW_THRESHOLD_DUTY_PERCENT "lo_thresh_duty"
#define APP_CONFIG_KEY_HIGH_THRESHOLD_DUTY_PERCENT "hi_thresh_duty"

#ifdef __cplusplus
}
#endif

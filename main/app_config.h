#pragma once

#include "shadow_state.h"
#include "shadow_state_gpio.h"
#include <driver/gpio.h>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>

// TODO use Kconfig for constants

struct hw_config_sensor
{
    ShadowStateSet state;

    ShadowState<std::string> address;
    ShadowState<std::string> name;
    ShadowState<double> offset_c;

    hw_config_sensor()
        : address(state, "/addr", ""),
          name(state, "/name", ""),
          offset_c(state, "/offsetC", 0.0)
    {
    }
};

struct hw_config
{
    ShadowStateSet state;

    ShadowState<gpio_num_t> status_led_pin;
    ShadowState<bool> status_led_on_state;
    ShadowStateList sensors;

    hw_config()
        : status_led_pin(state, "/statusLedPin", GPIO_NUM_NC),
          status_led_on_state(state, "/statusLedOnState", true),
          sensors(state, "/sensors", &hw_config_sensor::hw_config_sensor)
    {
    }
};

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

esp_err_t app_config_read(app_config_t *cfg, const rapidjson::GenericValue<rapidjson::UTF8<>> &data);

esp_err_t app_config_write(const app_config_t *cfg, rapidjson::Writer<rapidjson::StringBuffer> &w);

char *app_config_print_address(char *buf, size_t buf_len, uint64_t value);

#define APP_CONFIG_NVS_NAME "app_config"
#define APP_CONFIG_KEY_STATUS_LED_PIN "statusLedPin"
#define APP_CONFIG_KEY_STATUS_LED_ON_STATE "statusLedOnState"
#define APP_CONFIG_KEY_PWM_PIN "pwmPin"
#define APP_CONFIG_KEY_PWM_INVERTED_DUTY "pwmInverted"
#define APP_CONFIG_KEY_RPM_PINS "rpmPins"
#define APP_CONFIG_KEY_SENSORS_PIN "sensorsPin"
#define APP_CONFIG_KEY_PRIMARY_SENSOR_ADDRESS "primaryAddr"
#define APP_CONFIG_KEY_SENSORS "sensors"
#define APP_CONFIG_KEY_SENSOR_ADDRESS "addr"
#define APP_CONFIG_KEY_SENSOR_NAME "name"
#define APP_CONFIG_KEY_SENSOR_OFFSET_C "offsetC"
#define APP_CONFIG_KEY_LOW_THRESHOLD_CELSIUS "lowThreshC"
#define APP_CONFIG_KEY_HIGH_THRESHOLD_CELSIUS "highThreshC"
#define APP_CONFIG_KEY_LOW_THRESHOLD_DUTY_PERCENT "lowThreshDuty"
#define APP_CONFIG_KEY_HIGH_THRESHOLD_DUTY_PERCENT "highThreshDuty"

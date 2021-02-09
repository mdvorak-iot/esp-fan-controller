#pragma once

#include <string>
#include <driver/gpio.h>
#include <nvs.h>

// Max supported RPM sensors for fans
const size_t APP_CONFIG_MAX_RPM = 7; // Must be <= rpmcounter::RPM_MAX_COUNTERS
// Max supported temperature sensors
const size_t APP_CONFIG_MAX_SENSORS = 10;
// Maximum sensor name length, including terminating char
const size_t APP_CONFIG_MAX_NAME_LENGHT = 20;
// Unused pin
const gpio_num_t APP_CONFIG_PIN_DISABLED = static_cast<gpio_num_t>(-1);

// Name of the DS20B18 temperature sensors
struct __packed app_config_sensor
{
    uint64_t address;
    char name[APP_CONFIG_MAX_NAME_LENGHT];
};

// Structure for module configurations, excluding URL for CPU temperature polling
struct __packed app_config_data
{
    uint8_t magic_byte;
    gpio_num_t control_pin;
    gpio_num_t rpm_pins[APP_CONFIG_MAX_RPM];
    gpio_num_t sensors_pin;
    uint64_t primary_sensor_address;
    app_config_sensor sensors[APP_CONFIG_MAX_SENSORS];
    uint8_t low_threshold_celsius;
    uint8_t high_threshold_celsius;
    uint8_t cpu_threshold_celsius;
    uint16_t cpu_poll_interval_seconds;
    uint8_t low_threshold_duty_percent;
    uint8_t high_threshold_duty_percent;
    char hardware_name[APP_CONFIG_MAX_NAME_LENGHT];
};

struct app_config
{
    app_config_data data;
    std::string cpu_query_url;
};

const uint8_t APP_CONFIG_MAGIC_BYTE = 0x92;

// Defaults for struct app_config_data
#define APP_CONFIG_DATA_DEFAULT()                                                                                                                                                                    \
    {                                                                                                                                                                                                \
        .magic_byte = APP_CONFIG_MAGIC_BYTE,                                                                                                                                                         \
        .control_pin = APP_CONFIG_PIN_DISABLED,                                                                                                                                                      \
        .rpm_pins = {APP_CONFIG_PIN_DISABLED, APP_CONFIG_PIN_DISABLED, APP_CONFIG_PIN_DISABLED, APP_CONFIG_PIN_DISABLED, APP_CONFIG_PIN_DISABLED, APP_CONFIG_PIN_DISABLED, APP_CONFIG_PIN_DISABLED}, \
        .sensors_pin = APP_CONFIG_PIN_DISABLED,                                                                                                                                                      \
        .primary_sensor_address = 0,                                                                                                                                                                 \
        .sensors = {},                                                                                                                                                                               \
        .low_threshold_celsius = 25,                                                                                                                                                                 \
        .high_threshold_celsius = 35,                                                                                                                                                                \
        .cpu_threshold_celsius = 75,                                                                                                                                                                 \
        .cpu_poll_interval_seconds = 10,                                                                                                                                                             \
        .low_threshold_duty_percent = 30,                                                                                                                                                            \
        .high_threshold_duty_percent = 99,                                                                                                                                                           \
        .hardware_name = {},                                                                                                                                                                         \
    }

// Defaults for struct app_config
#define APP_CONFIG_DEFAULT()               \
    {                                      \
        .data = APP_CONFIG_DATA_DEFAULT(), \
    }

const auto APP_CONFIG_NVS_NAME = "appconfig";
const auto APP_CONFIG_NVS_DATA = "data";
const auto APP_CONFIG_NVS_CPU_QUERY_URL = "cpu_query_url";

esp_err_t app_config_init(app_config &cfg);

esp_err_t app_config_update(const app_config &cfg);

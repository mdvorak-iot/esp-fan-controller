#pragma once

#include <string>
#include <driver/gpio.h>
#include <nvs.h>

namespace appconfig
{

// Max supported RPM sensors for fans
const size_t APP_CONFIG_MAX_RPM = 7; // Must be <= rpmcounter::RPM_MAX_COUNTERS
// Max supported temperature sensors
const size_t APP_CONFIG_MAX_SENSORS = 10;
// Maximum sensor name length, including terminating char
const size_t MAX_NAME_LENGHT = 16;
// Unused pin
const auto APP_CONFIG_PIN_DISABLED = GPIO_NUM_MAX;

// Name of the DS20B18 temperature sensors
struct __packed app_config_sensor
{
    uint64_t address;
    char name[MAX_NAME_LENGHT];
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
    char hardware_name[MAX_NAME_LENGHT];
};

struct app_config
{
    nvs_handle handle;
    app_config_data data;
    std::string cpu_query_url;
};

const uint8_t APP_CONFIG_MAGIC_BYTE = 0x21;

// Defaults for struct config_data
#define CONFIG_DATA_DEFAULT()                   \
    {                                           \
        .magic_byte = APP_CONFIG_MAGIC_BYTE,    \
        .control_pin = APP_CONFIG_PIN_DISABLED, \
        .rpm_pins = {APP_CONFIG_PIN_DISABLED},  \
        .sensors_pin = APP_CONFIG_PIN_DISABLED, \
        .primary_sensor_address = 0,            \
        .sensors = {},                          \
        .low_threshold_celsius = 25,            \
        .high_threshold_celsius = 35,           \
        .cpu_threshold_celsius = 75,            \
        .cpu_poll_interval_seconds = 10,        \
        .hardware_name = {0},                   \
    }

esp_err_t app_config_init(app_config &cfg, const char *name = "config");

esp_err_t app_config_update(const app_config &cfg);

}; // namespace appconfig

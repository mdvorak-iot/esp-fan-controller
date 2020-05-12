#pragma once
#include <string>
#include <driver/gpio.h>
#include <nvs.h>
#include <memory>

// Max supported RPM sensors for fans
const size_t MAX_FANS = 14;
// Max supported temperature sensors
const size_t MAX_SENSORS = 10;
// Maximum sensor name length, including terminating char
const size_t MAX_NAME_LENGHT = 16;
// 0 is used as DISABLED, as it is unsupported pin (it is used by LED)
const auto GPIO_NUM_DISABLED = GPIO_NUM_0;

// Name of the DS20B18 temperature sensors
struct __packed sensor_config_data
{
    uint64_t address;
    char name[MAX_NAME_LENGHT];
};

// Structure for module configurations, excluding URL for CPU temperature polling
struct __packed config_data
{
    uint8_t magic_byte;
    gpio_num_t control_pin;
    gpio_num_t rpm_pins[MAX_FANS];
    gpio_num_t sensors_pin;
    uint64_t primary_sensor_address;
    sensor_config_data sensors[MAX_SENSORS];
    uint8_t low_threshold_celsius;
    uint8_t high_threshold_celsius;
    uint8_t cpu_threshold_celsius;
    uint16_t cpu_poll_interval_seconds;
};

const uint8_t CONFIG_DATA_MAGIC_BYTE = 0x21;

// Defaults for struct config_data
#define CONFIG_DATA_DEFAULT()                 \
    {                                         \
        .magic_byte = CONFIG_DATA_MAGIC_BYTE, \
        .control_pin = GPIO_NUM_DISABLED,     \
        .rpm_pins = {GPIO_NUM_DISABLED},      \
        .sensors_pin = GPIO_NUM_DISABLED,     \
        .primary_sensor_address = 0,          \
        .sensors = {},                        \
        .low_threshold_celsius = 25,          \
        .high_threshold_celsius = 35,         \
        .cpu_threshold_celsius = 75,          \
        .cpu_poll_interval_seconds = 10,      \
    }

class app_config
{
public:
    esp_err_t begin()
    {
        auto err = nvs_open("config", NVS_READWRITE, &_handle);
        if (err != ESP_OK)
        {
            return err;
        }

        // Read (ignore errors here)
        size_t size = sizeof(config_data);
        nvs_get_blob(_handle, "data", &_data, &size);

        if (_data.magic_byte != CONFIG_DATA_MAGIC_BYTE)
        {
            _data = CONFIG_DATA_DEFAULT();
        }

        return ESP_OK;
    }

    // TODO rework modification pattern
    esp_err_t update(config_data data)
    {
        auto err = nvs_set_blob(_handle, "data", &data, sizeof(config_data));
        if (err != ESP_OK)
        {
            return err;
        }

        err = nvs_commit(_handle);
        if (err != ESP_OK)
        {
            return err;
        }

        _data = data;
        return ESP_OK;
    }

    const config_data &data() const
    {
        return _data;
    }

    inline std::string cpu_query_url() const
    {
        char v[1024]{0};
        size_t s = sizeof(v);
        if (nvs_get_str(_handle, "cpu_query_url", v, &s) == ESP_OK)
        {
            return std::string(v, s);
        }
        return std::string();
    }

private:
    nvs_handle _handle = 0;
    config_data _data{};
};

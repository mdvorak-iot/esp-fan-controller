#include "temperature_sensors.h"
#include <esp_log.h>
#include <owb.h>
#include <owb_rmt.h>
#include <ds18b20.h>
#include <string.h>

static const char TAG[] = "temperature_sensors";

static const uint8_t DS18B20_FAMILY = 0x28;

struct temperature_sensors_handle
{
    owb_rmt_driver_info rmt;
    OneWireBus *owb;
    DS18B20_Info sensors[TEMPERATURE_SENSORS_MAX_COUNT];
    float calibrations[TEMPERATURE_SENSORS_MAX_COUNT];
    size_t sensor_count;
};

inline static bool ds18b20_check_family(const OneWireBus_ROMCode *rom_code)
{
    return rom_code->bytes[0] == DS18B20_FAMILY;
}

esp_err_t temperature_sensors_create(gpio_num_t pin, rmt_channel_t tx_channel, rmt_channel_t rx_channel, temperature_sensors_handle_t *handle)
{
    if (handle == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    temperature_sensors_handle_t result = (temperature_sensors_handle_t)malloc(sizeof(*result));
    if (result == NULL)
    {
        return ESP_ERR_NO_MEM;
    }

    // Reset
    memset(result, 0, sizeof(*result));

    // NOTE technically this points just to variable in owb_rmt_driver_info, but that is implementation detail,
    // lets rather rely on public api and store it explicitly
    result->owb = owb_rmt_initialize(&result->rmt, pin, tx_channel, rx_channel);
    configASSERT(result->owb); // Never null ATM, but better be sure for future

    // enable CRC check for ROM code
    owb_use_crc(result->owb, true);

    // Check for parasitic-powered devices
    bool parasitic_power = false;
    owb_status s = ds18b20_check_for_parasite_power(result->owb, &parasitic_power);
    if (s != OWB_STATUS_OK)
    {
        ESP_LOGW(TAG, "failed to check for parasitic power");
    }

    if (parasitic_power)
    {
        ESP_LOGI(TAG, "parasitic-powered devices detected");
    }

    // In parasitic-power mode, devices cannot indicate when conversions are complete,
    // so waiting for a temperature conversion must be done by waiting a prescribed duration
    owb_use_parasitic_power(result->owb, parasitic_power);

    // Success
    *handle = result;
    return ESP_OK;
}

void temperature_sensors_delete(temperature_sensors_handle_t handle)
{
    if (handle != NULL)
    {
        owb_status err = owb_uninitialize(handle->owb);
        if (err != OWB_STATUS_OK)
        {
            ESP_LOGE(TAG, "failed to uninitialize owb: %d", err);
        }

        free(handle);
    }
}

esp_err_t temperature_sensors_find(temperature_sensors_handle_t handle)
{
    if (handle == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    // Reset
    handle->sensor_count = 0;
    memset(handle->sensors, 0, sizeof(handle->sensors));

    // Search
    OneWireBus_SearchState search_state = {0};
    bool found = false;

    OneWireBus_ROMCode owb_devices[TEMPERATURE_SENSORS_MAX_COUNT];
    size_t device_count = 0; // Total count of all devices
    size_t sensor_count = 0; // Supported sensor devices

    owb_search_first(handle->owb, &search_state, &found);
    while (found)
    {
        // Increment total count
        device_count++;

        // For logging
        char rom_code_s[17];
        owb_string_from_rom_code(search_state.rom_code, rom_code_s, sizeof(rom_code_s));

        // Ignore if not correct family
        if (!ds18b20_check_family(&search_state.rom_code))
        {
            ESP_LOGI(TAG, "found unsupported device: %s", rom_code_s);
            break;
        }

        // Log
        ESP_LOGI(TAG, "found sensor %d: %s", sensor_count, rom_code_s);

        // Store and increment count
        owb_devices[sensor_count++] = search_state.rom_code;

        // Search next
        found = false;
        owb_search_next(handle->owb, &search_state, &found);

        // Buffer overflow
        if (found && device_count >= TEMPERATURE_SENSORS_MAX_COUNT)
        {
            ESP_LOGW(TAG, "found too many temperature sensors");
            break;
        }
    }

    // Special handling - if sensor is one and only device on the bus, we can skip addressing
    if (sensor_count == 1 && device_count == 1)
    {
        // Initialize and set count to 1
        ds18b20_init_solo(&handle->sensors[0], handle->owb);
        handle->sensor_count = 1;
    }
    else
    {
        // Initialize
        for (size_t i = 0; i < sensor_count; i++)
        {
            ds18b20_init(&handle->sensors[i], handle->owb, owb_devices[i]);
        }

        // Store count
        handle->sensor_count = sensor_count;
    }

    ESP_LOGI(TAG, "found %d sensors", handle->sensor_count);
    return ESP_OK;
}

esp_err_t temperature_sensors_configure(temperature_sensors_handle_t handle, uint8_t resolution, bool crc)
{
    if (handle == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    for (size_t i = 0; i < handle->sensor_count; i++)
    {
        ds18b20_use_crc(&handle->sensors[i], crc);
        ds18b20_set_resolution(&handle->sensors[i], (DS18B20_RESOLUTION)resolution);
    }

    return ESP_OK;
}

esp_err_t temperature_sensors_set_calibration(temperature_sensors_handle_t handle, size_t index, float value_c)
{
    if (handle == NULL || index >= handle->sensor_count)
    {
        return ESP_ERR_INVALID_ARG;
    }

    handle->calibrations[index] = value_c;

    char rom_code_s[17];
    owb_string_from_rom_code(handle->sensors[index].rom_code, rom_code_s, sizeof(rom_code_s));

    ESP_LOGI(TAG, "set calibration for %d %s: %.3f", index, rom_code_s, value_c);
    return ESP_OK;
}

esp_err_t temperature_sensors_convert(temperature_sensors_handle_t handle)
{
    if (handle == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    ds18b20_convert_all(handle->owb);
    return ESP_OK;
}

esp_err_t temperature_sensors_wait_for_conversion(temperature_sensors_handle_t handle)
{
    if (handle == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }
    if (handle->sensor_count == 0)
    {
        return ESP_ERR_INVALID_STATE;
    }

    // All should finish at same time
    ds18b20_wait_for_conversion(&handle->sensors[0]);
    return ESP_OK;
}

size_t temperature_sensors_count(temperature_sensors_handle_t handle)
{
    return handle != NULL ? handle->sensor_count : 0;
}

esp_err_t temperature_sensors_address(temperature_sensors_handle_t handle, size_t index, uint64_t *address)
{
    if (handle == NULL || address == NULL || index >= handle->sensor_count)
    {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(address, handle->sensors[index].rom_code.bytes, 8);
    return ESP_OK;
}

esp_err_t temperature_sensors_read(temperature_sensors_handle_t handle, size_t index, float *value_c)
{
    if (handle == NULL || value_c == NULL || index >= handle->sensor_count)
    {
        return ESP_ERR_INVALID_ARG;
    }

    float raw_value_c;
    DS18B20_ERROR err = ds18b20_read_temp(&handle->sensors[index], &raw_value_c);
    if (err != DS18B20_OK)
    {
        ESP_LOGW(TAG, "failed to read temperature for sensor %d: %d", index, err);
        return ESP_FAIL;
    }

    *value_c = raw_value_c + handle->calibrations[index];
    ESP_LOGD(TAG, "readout %d: %.3f => %.3f", index, raw_value_c, *value_c);
    return ESP_OK;
}

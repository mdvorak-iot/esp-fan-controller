#include "ds18b20_group.h"
#include <ds18b20.h>
#include <esp_log.h>
#include <owb.h>
#include <owb_rmt.h>
#include <string.h>

static const char TAG[] = "ds18b20_group";

inline static bool ds18b20_check_family(const OneWireBus_ROMCode *rom_code)
{
    return rom_code->bytes[0] == DS18B20_FAMILY;
}

esp_err_t ds18b20_group_create(OneWireBus *owb, ds18b20_group_handle_t *handle)
{
    if (owb == NULL || handle == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    ds18b20_group_handle_t result = (ds18b20_group_handle_t)malloc(sizeof(*result));
    if (result == NULL)
    {
        return ESP_ERR_NO_MEM;
    }

    // Init
    memset(result, 0, sizeof(*result));
    result->owb = owb;

    // Check for parasitic-powered devices
    bool parasitic_power = false;
    DS18B20_ERROR err = ds18b20_check_for_parasite_power(result->owb, &parasitic_power);
    if (err != DS18B20_OK)
    {
        ESP_LOGW(TAG, "failed to check for parasitic power: %d", err);
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

void ds18b20_group_delete(ds18b20_group_handle_t handle)
{
    if (handle != NULL)
    {
        free(handle);
    }
}

esp_err_t ds18b20_group_find(ds18b20_group_handle_t handle)
{
    if (handle == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    // Reset
    handle->count = 0;
    memset(handle->devices, 0, sizeof(handle->devices));

    // Search
    OneWireBus_SearchState search_state = {0};
    bool found = false;

    OneWireBus_ROMCode owb_devices[DS18B20_GROUP_MAX_SIZE] = {};
    uint8_t total_count = 0;  // Total count of all devices
    uint8_t device_count = 0; // Supported ds18b20 devices

    owb_search_first(handle->owb, &search_state, &found);
    while (found)
    {
        // Increment total count
        total_count++;

        // For logging
        char rom_code_s[17] = {};
        owb_string_from_rom_code(search_state.rom_code, rom_code_s, sizeof(rom_code_s));

        // Ignore if not correct family
        if (!ds18b20_check_family(&search_state.rom_code))
        {
            ESP_LOGI(TAG, "found unsupported device: %s", rom_code_s);
            break;
        }

        // Log
        ESP_LOGI(TAG, "found device %u: %s", device_count, rom_code_s);

        // Store and increment count
        owb_devices[device_count++] = search_state.rom_code;

        // Search next
        found = false;
        owb_search_next(handle->owb, &search_state, &found);

        // Buffer overflow
        if (found && device_count >= DS18B20_GROUP_MAX_SIZE)
        {
            ESP_LOGW(TAG, "found too many ds18b20 devices");
            break;
        }
    }

    // Special handling - if sensor is one and only device on the bus, we can skip addressing
    if (device_count == 1 && total_count == 1)
    {
        // Initialize and set count to 1
        ds18b20_init_solo(&handle->devices[0], handle->owb);
        handle->count = 1;
    }
    else
    {
        // Initialize
        for (size_t i = 0; i < device_count; i++)
        {
            ds18b20_init(&handle->devices[i], handle->owb, owb_devices[i]);
        }

        // Store count
        handle->count = device_count;
    }

    ESP_LOGI(TAG, "found %u ds18b20 devices", handle->count);
    return ESP_OK;
}

esp_err_t ds18b20_group_use_crc(ds18b20_group_handle_t handle, bool crc)
{
    if (handle == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    for (size_t i = 0; i < handle->count; i++)
    {
        ds18b20_use_crc(&handle->devices[i], crc);
    }

    return ESP_OK;
}

esp_err_t ds18b20_group_set_resolution(ds18b20_group_handle_t handle, DS18B20_RESOLUTION resolution)
{
    if (handle == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    for (size_t i = 0; i < handle->count; i++)
    {
        ds18b20_set_resolution(&handle->devices[i], resolution);
    }

    return ESP_OK;
}

esp_err_t ds18b20_group_convert(ds18b20_group_handle_t handle)
{
    if (handle == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    ds18b20_convert_all(handle->owb);
    return ESP_OK;
}

esp_err_t ds18b20_group_wait_for_conversion(ds18b20_group_handle_t handle)
{
    if (handle == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }
    if (handle->count == 0)
    {
        return ESP_ERR_INVALID_STATE;
    }

    // All should finish at same time
    ds18b20_wait_for_conversion(&handle->devices[0]);
    return ESP_OK;
}

esp_err_t ds18b20_group_read_single(ds18b20_group_handle_t handle, uint8_t index, float *value_c)
{
    if (handle == NULL || value_c == NULL || index >= handle->count)
    {
        return ESP_ERR_INVALID_ARG;
    }

    DS18B20_ERROR err = ds18b20_read_temp(&handle->devices[index], value_c);
    if (err != DS18B20_OK)
    {
        ESP_LOGW(TAG, "failed to read temperature for sensor %u: %d", index, err);
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "readout %u: %.3f", index, *value_c);
    return ESP_OK;
}

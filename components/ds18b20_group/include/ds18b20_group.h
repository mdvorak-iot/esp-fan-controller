#pragma once

#include <ds18b20.h>
#include <esp_err.h>
#include <owb.h>

#ifndef DS18B20_GROUP_MAX_SIZE
#define DS18B20_GROUP_MAX_SIZE CONFIG_DS18B20_GROUP_MAX_SIZE
#endif

#ifndef DS18B20_FAMILY
#define DS18B20_FAMILY 0x28
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct ds18b20_group_handle
{
    OneWireBus *owb;
    DS18B20_Info devices[DS18B20_GROUP_MAX_SIZE];
    uint8_t count;
};

/**
     * Handle for DS18B20 sensor group.
     */
typedef struct ds18b20_group_handle *ds18b20_group_handle_t;

/**
     * @brief Creates and initializes group of DS18B20 sensors on one bus.
     *
     * Creates new instance of ds18b20_group_handle. This have to be freed using ds18b20_group_delete().
     *
     * To populate the list, ds18b20_group_find() needs to be called before any other methods.
     *
     * @param owb Reference to initialized OneWireBus
     * @param handle Pointer where the handle reference should be stored
     * @return ESP_OK if operation succeded, error otherwise.
     */
esp_err_t ds18b20_group_create(OneWireBus *owb, ds18b20_group_handle_t *handle);

/**
     * Delete handle and release its memory.
     *
     * @param handle Handle or NULL.
     */
void ds18b20_group_delete(ds18b20_group_handle_t handle);

esp_err_t ds18b20_group_find(ds18b20_group_handle_t handle);

esp_err_t ds18b20_group_use_crc(ds18b20_group_handle_t handle, bool crc);

esp_err_t ds18b20_group_set_resolution(ds18b20_group_handle_t handle, DS18B20_RESOLUTION resolution);

esp_err_t ds18b20_group_convert(ds18b20_group_handle_t handle);

esp_err_t ds18b20_group_wait_for_conversion(ds18b20_group_handle_t handle);

esp_err_t ds18b20_group_read_single(ds18b20_group_handle_t handle, uint8_t index, float *value_c);

#ifdef __cplusplus
}
#endif

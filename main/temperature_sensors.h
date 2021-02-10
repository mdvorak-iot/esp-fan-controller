#pragma once

#include <esp_err.h>
#include <driver/gpio.h>
#include <hal/rmt_types.h>
#include <ds18b20.h>

#ifndef TEMPERATURE_SENSORS_MAX_COUNT
// TODO CONFIG_
#define TEMPERATURE_SENSORS_MAX_COUNT 10
#endif

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct temperature_sensors_handle *temperature_sensors_handle_t;

    esp_err_t temperature_sensors_create(gpio_num_t pin, rmt_channel_t tx_channel, rmt_channel_t rx_channel, temperature_sensors_handle_t *handle);

    void temperature_sensors_delete(temperature_sensors_handle_t handle);

    esp_err_t temperature_sensors_find(temperature_sensors_handle_t handle);

    esp_err_t temperature_sensors_configure(temperature_sensors_handle_t handle, DS18B20_RESOLUTION resolution, bool crc);

    esp_err_t temperature_sensors_convert(temperature_sensors_handle_t handle);

    esp_err_t temperature_sensors_wait_for_conversion(temperature_sensors_handle_t handle);

    size_t temperature_sensors_count(temperature_sensors_handle_t handle);

    esp_err_t temperature_sensors_address(temperature_sensors_handle_t handle, size_t index, uint8_t address[8]);

    esp_err_t temperature_sensors_read(temperature_sensors_handle_t handle, size_t index, float *value_c);

#ifdef __cplusplus
}
#endif

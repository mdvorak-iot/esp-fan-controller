#pragma once

#include <vector>
#include <string>
#include <esp_err.h>
#include <esp32-hal-gpio.h>

// TODO move from static to context struct

namespace apptemps
{

    struct temperature_sensor
    {
        uint64_t address;
        float value;
    };

    esp_err_t temperature_sensors_init(gpio_num_t pin);

    esp_err_t temperature_request();

    esp_err_t temperature_values(std::vector<temperature_sensor> &out);

}; // namespace apptemps

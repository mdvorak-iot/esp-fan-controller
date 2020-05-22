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
        uint32_t errors;
        std::string name;
    };

    esp_err_t temperature_sensors_init(gpio_num_t pin);

    void temperature_request();

    void temperature_assign_name(uint64_t address, std::string name);

    void temperature_values(std::vector<temperature_sensor> &out);

    float temperature_value(uint64_t address);

}; // namespace apptemps

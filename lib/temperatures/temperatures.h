#pragma once

#include <vector>
#include <string>
#include <atomic>
#include <esp_err.h>

class OneWire;
class DallasTemperature;

namespace temperatures
{

struct temperature_sensor
{
    uint64_t address;
    std::string name;
    std::atomic<float> value;
};

struct temperature_sensors
{
    std::vector<temperature_sensor> *list;
    OneWire *wire;
    DallasTemperature *dallas;
};

esp_err_t temperature_sensors_init();

}; // namespace temperatures

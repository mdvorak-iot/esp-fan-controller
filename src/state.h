#pragma once
#include <string>
#include <vector>
#include <atomic>

struct sensor_state
{
    uint64_t address;
    float temperature;
};

struct state
{
    std::string hardware();
    float duty();
    std::vector<uint16_t> rpms();
    std::vector<sensor_state> sensors();
};

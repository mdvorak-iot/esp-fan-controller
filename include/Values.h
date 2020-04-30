#pragma once
#include <atomic>

struct Values
{
    static std::atomic<uint16_t> rpm;
    static std::atomic<uint8_t> duty;
    static std::atomic<float> temperature;
    static std::atomic<unsigned long> temperatureReadout;
};

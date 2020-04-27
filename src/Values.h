#pragma once
#include <atomic>

struct Values
{
    static std::atomic<uint16_t> rpm;
    static std::atomic<float> temperature;
    static std::atomic<uint8_t> duty;
};

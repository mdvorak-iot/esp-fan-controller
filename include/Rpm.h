#pragma once

#include <stdint.h>
#include <Arduino.h>
#include "driver/pcnt.h"

class Rpm
{
public:
    Rpm(gpio_num_t pin, size_t samples, pcnt_unit_t unit = PCNT_UNIT_0, pcnt_channel_t channel = PCNT_CHANNEL_0)
        : _pin(pin),
          _unit(unit),
          _channel(channel),
          _index(0),
          _samples(samples)
    {
        _values = new Snapshot[samples];
    }

    ~Rpm()
    {
        delete[] _values;
        _values = nullptr;
    }

    esp_err_t begin();
    uint16_t measure();

private:
    struct Snapshot
    {
        unsigned long time;
        int16_t count;
    };

    gpio_num_t const _pin;
    pcnt_unit_t _unit;
    pcnt_channel_t _channel;
    size_t _index;
    size_t _samples;
    Snapshot *_values;
};

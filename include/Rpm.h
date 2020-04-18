#pragma once

#include <stdint.h>
#include <Arduino.h>
#include "driver/pcnt.h"

class Rpm
{
public:
    Rpm(gpio_num_t pin, pcnt_unit_t unit = PCNT_UNIT_0, pcnt_channel_t channel = PCNT_CHANNEL_0)
        : _pin(pin),
          _unit(unit),
          _channel(channel),
          _mutex(portMUX_INITIALIZER_UNLOCKED),
          _index(0),
          _values{}
    {
    }

    esp_err_t begin();
    uint16_t rpm();

private:
    static const size_t SAMPLES = 100;
    static const unsigned long INTERVAL = 10;

    struct Snapshot
    {
        unsigned long time;
        int16_t count;
    };

    gpio_num_t const _pin;
    pcnt_unit_t _unit;
    pcnt_channel_t _channel;
    portMUX_TYPE _mutex;
    size_t _index;
    Snapshot _values[SAMPLES];

    void measure();
    static void worker(void *p);
};

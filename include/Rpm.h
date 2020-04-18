#pragma once

#include <stdint.h>
#include <atomic>
#include <esp_attr.h>
#include <esp_timer.h>
#include <driver/gpio.h>
#include <Arduino.h>

static const size_t READINGS_LEN = 40;

class Rpm
{
public:
    Rpm(gpio_num_t pin)
        : _pin(pin),
          _mutex(portMUX_INITIALIZER_UNLOCKED),
          _index(0),
          _readings{0}
    {
    }

    esp_err_t begin();

    uint16_t rpm()
    {
        auto now = micros();
        uint16_t revs = 0;

        portENTER_CRITICAL(&_mutex);
        for (size_t i = 0; i < READINGS_LEN; i++)
        {
            auto index = (READINGS_LEN + _index - i) % READINGS_LEN;
            if (now - _readings[index] > 1000000UL)
            {
                revs = i;
                break;
            }
        }
        portEXIT_CRITICAL(&_mutex);

        return 30 * revs;
    }

private:
    gpio_num_t const _pin;
    portMUX_TYPE _mutex;
    size_t _index;
    unsigned long _readings[READINGS_LEN];

    void inline IRAM_ATTR onRpmInterrupt()
    {
        auto now = micros();
        portENTER_CRITICAL_ISR(&_mutex);
        _index = (_index + 1) % READINGS_LEN;
        _readings[_index] = now;
        portEXIT_CRITICAL_ISR(&_mutex);
    }

    static void IRAM_ATTR onRpmInterruptHandler(void *self);
};

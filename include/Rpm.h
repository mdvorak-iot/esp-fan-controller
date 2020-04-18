#pragma once

#include <stdint.h>
#include <atomic>
#include <esp_attr.h>
#include <esp_timer.h>
#include <driver/gpio.h>
#include <Arduino.h>

class Rpm
{
public:
    Rpm(gpio_num_t pin)
        : _pin(pin)
    {
    }

    esp_err_t begin();
    uint16_t rpm();

private:
    gpio_num_t const _pin;
};

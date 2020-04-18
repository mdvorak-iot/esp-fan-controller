#pragma once

#include <stdint.h>
#include <atomic>
#include <esp_attr.h>
#include <esp_timer.h>
#include <driver/gpio.h>

class Rpm
{
public:
    Rpm(gpio_num_t pin)
        : _pin(pin),
          _mutex(portMUX_INITIALIZER_UNLOCKED),
          _lastIndex(0),
          _readings{0}
    {
    }

    esp_err_t begin()
    {
        // Configure GPIO for RPM
        gpio_config_t io_conf = {};
        io_conf.pin_bit_mask = 1ULL << _pin;
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.intr_type = GPIO_INTR_NEGEDGE;
        io_conf.pull_up_en = GPIO_PULLUP_ENABLE;

        esp_err_t err = gpio_config(&io_conf);
        if (err != ESP_OK)
        {
            return err;
        }

        // Attach handler
        return gpio_isr_handler_add(_pin, &onRpmInterruptHandler, this);
    }

    uint16_t rpm()
    {
        portENTER_CRITICAL(&_mutex);
        size_t lastIndex = _lastIndex;
        size_t firstIndex = (lastIndex + 1) % READINGS_LEN;
        auto lastRead = _readings[lastIndex];
        auto firstRead = _readings[firstIndex];
        auto d = (lastRead - firstRead) / READINGS_LEN;
        uint16_t rpm = 30000000UL / d;
        portEXIT_CRITICAL(&_mutex);

        return rpm;
    }

private:
    static const size_t READINGS_LEN = 64;

    gpio_num_t const _pin;
    portMUX_TYPE _mutex;
    size_t _lastIndex;
    unsigned long _readings[READINGS_LEN];

    void inline IRAM_ATTR onRpmInterrupt()
    {
        portENTER_CRITICAL_ISR(&_mutex);
        _lastIndex = (_lastIndex + 1) % READINGS_LEN;
        _readings[_lastIndex] = micros();
        portEXIT_CRITICAL_ISR(&_mutex);
    }

    static void IRAM_ATTR onRpmInterruptHandler(void *self)
    {
        static_cast<Rpm *>(self)->onRpmInterrupt();
    }
};

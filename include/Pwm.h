#pragma once

#include <stdint.h>
#include <driver/gpio.h>
#include <driver/ledc.h>

class Pwm
{
public:
    Pwm(gpio_num_t pin, ledc_timer_t timer, ledc_channel_t channel, uint32_t frequency, ledc_timer_bit_t resolution)
        : _timer(timer),
          _channel(channel),
          _pin(pin),
          _frequency(frequency),
          _resolution(resolution),
          _maxDuty((1UL << _resolution) - 1)
    {
    }

    esp_err_t begin()
    {
        // Power PWM
        ledc_timer_config_t timerConfig = {};
        timerConfig.timer_num = _timer;
        timerConfig.speed_mode = LEDC_HIGH_SPEED_MODE;
        timerConfig.duty_resolution = _resolution;
        timerConfig.freq_hz = _frequency;
        auto r = ledc_timer_config(&timerConfig);
        if (r != ESP_OK)
        {
            return r;
        }

        // Power Channel
        ledc_channel_config_t channelConfig = {};
        channelConfig.timer_sel = _timer;
        channelConfig.channel = _channel;
        channelConfig.gpio_num = _pin;
        channelConfig.speed_mode = LEDC_HIGH_SPEED_MODE;
        channelConfig.duty = 0;
        r = ledc_channel_config(&channelConfig);
        return r;
    }

    uint32_t maxDuty() const
    {
        return _maxDuty;
    }

    uint32_t duty() const
    {
        return _duty;
    }

    esp_err_t duty(uint32_t duty)
    {
        // Avoid overflow
        if (duty > _maxDuty)
        {
            duty = _maxDuty;
        }
        // NOOP
        if (duty == _duty)
        {
            return ESP_OK;
        }

        // Update
        auto r = ledc_set_duty_and_update(LEDC_HIGH_SPEED_MODE, _channel, duty, 0);
        if (r == ESP_OK)
        {
            _duty = duty;
        }
        return r;
    }

    uint32_t frequency() const
    {
        return _frequency;
    }

private:
    ledc_timer_t _timer;
    ledc_channel_t _channel;
    gpio_num_t _pin;
    uint32_t _frequency;
    uint32_t _duty;
    ledc_timer_bit_t _resolution;
    uint32_t _maxDuty;
};

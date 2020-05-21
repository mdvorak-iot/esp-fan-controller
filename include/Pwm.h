#pragma once

#include <stdint.h>
#include <atomic>
#include <driver/gpio.h>
#include <driver/ledc.h>

class FanPwm
{
public:
    esp_err_t begin(gpio_num_t pin, ledc_timer_t timer, ledc_channel_t channel, uint32_t frequency, ledc_timer_bit_t resolution, bool inverted = false)
    {
        uint32_t maxDuty = (1UL << resolution) - 1;

        // Timer
        ledc_timer_config_t timerConfig = {};
        timerConfig.timer_num = timer;
        timerConfig.speed_mode = LEDC_HIGH_SPEED_MODE;
        timerConfig.duty_resolution = resolution;
        timerConfig.freq_hz = frequency;
        auto r = ledc_timer_config(&timerConfig);
        if (r != ESP_OK)
        {
            return r;
        }

        // Channel
        ledc_channel_config_t channelConfig = {};
        channelConfig.timer_sel = timer;
        channelConfig.channel = channel;
        channelConfig.gpio_num = pin;
        channelConfig.speed_mode = LEDC_HIGH_SPEED_MODE;
        channelConfig.duty = inverted ? maxDuty : 0;
        r = ledc_channel_config(&channelConfig);
        if (r != ESP_OK)
        {
            return r;
        }

        // Store config
        channel_ = channel;
        inverted_ = inverted;
        maxDuty_ = maxDuty;

        // Success
        return ESP_OK;
    }

    esp_err_t stop(uint32_t idleLevel = 0)
    {
        return ledc_stop(LEDC_HIGH_SPEED_MODE, channel_, idleLevel);
    }

    ledc_mode_t mode() const
    {
        return mode_;
    }

    ledc_channel_t channel() const
    {
        return channel_;
    }

    bool configured() const
    {
        return channel_ != LEDC_CHANNEL_MAX;
    }

    inline uint32_t maxDuty() const
    {
        return maxDuty_;
    }

    inline uint32_t duty() const
    {
        return duty_;
    }

    esp_err_t duty(uint32_t duty)
    {
        // Avoid overflow
        if (duty > maxDuty_)
        {
            duty = maxDuty_;
        }
        // NOOP
        if (duty == duty_)
        {
            return ESP_OK;
        }

        // Update
        auto r = ledc_set_duty_and_update(LEDC_HIGH_SPEED_MODE, channel_, inverted_ ? maxDuty_ - duty : duty, 0);
        if (r == ESP_OK)
        {
            duty_ = duty;
        }
        return r;
    }

    // Sets duty from percentage range 0.0-1.0
    esp_err_t dutyPercent(float percent)
    {
        return duty(percent * maxDuty_);
    }

private:
    ledc_mode_t mode_ = LEDC_HIGH_SPEED_MODE;
    ledc_channel_t channel_ = LEDC_CHANNEL_MAX;
    std::atomic<uint32_t> duty_;
    uint32_t maxDuty_ = 0;
    bool inverted_ = false;
};

#include "fan_control.h"
#include <cmath>
#include <driver/ledc.h>
#include <esp_log.h>

static const char TAG[] = "fan_control";

static const uint32_t FAN_CONTROL_FREQ_HZ = 25000;
static const ledc_timer_bit_t FAN_CONTROL_RESOLUTION = (ledc_timer_bit_t)log2(LEDC_APB_CLK_HZ / FAN_CONTROL_FREQ_HZ);
static const uint32_t FAN_CONTROL_MAX_DUTY = (1u << FAN_CONTROL_RESOLUTION) - 1;

esp_err_t fan_control_config(gpio_num_t pin, ledc_timer_t timer, ledc_channel_t channel)
{
    ESP_LOGI(TAG, "initializing ledc timer %d channel %d on pin %d to frequency %d with resolution %d and max duty %d", timer, channel, pin, FAN_CONTROL_FREQ_HZ, FAN_CONTROL_RESOLUTION, FAN_CONTROL_MAX_DUTY);

    // Timer
    ledc_timer_config_t timerConfig = {};
    timerConfig.timer_num = timer;
    timerConfig.speed_mode = LEDC_HIGH_SPEED_MODE;
    timerConfig.duty_resolution = FAN_CONTROL_RESOLUTION;
    timerConfig.freq_hz = FAN_CONTROL_FREQ_HZ;
    esp_err_t err = ledc_timer_config(&timerConfig);
    if (err != ESP_OK)
    {
        return err;
    }

    // Channel
    ledc_channel_config_t channelConfig = {};
    channelConfig.timer_sel = timer;
    channelConfig.channel = channel;
    channelConfig.gpio_num = pin;
    channelConfig.speed_mode = LEDC_HIGH_SPEED_MODE;
    channelConfig.duty = 0;
    return ledc_channel_config(&channelConfig);
}

esp_err_t fan_control_set_duty(ledc_channel_t channel, float duty_percent)
{
    // TODO constrain
    uint32_t duty = duty_percent * FAN_CONTROL_MAX_DUTY;
    return ledc_set_duty_and_update(LEDC_HIGH_SPEED_MODE, channel, duty, 0);
}

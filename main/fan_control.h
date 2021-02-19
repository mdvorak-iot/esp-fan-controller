#pragma once
#include <esp_err.h>
#include <hal/gpio_types.h>
#include <hal/ledc_types.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t fan_control_config(gpio_num_t pin, ledc_timer_t timer, ledc_channel_t channel);

esp_err_t fan_control_set_duty(ledc_channel_t channel, float duty_percent);

#ifdef __cplusplus
}
#endif

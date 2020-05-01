#pragma once
#include <driver/gpio.h>
#include <driver/ledc.h>

const auto LO_THERSHOLD_TEMP_C = 27;
const auto HI_THERSHOLD_TEMP_C = 30;
const auto LO_THERSHOLD_DUTY = 30;
const auto HI_THERSHOLD_DUTY = 99;

const auto PWM_PIN = GPIO_NUM_2;
const auto PWM_FREQ = 25000u;
const auto PWM_RESOLUTION = LEDC_TIMER_10_BIT;
const auto PWM_INVERTED = true;

const auto RPM_PIN = GPIO_NUM_25;

const auto MAIN_LOOP_INTERVAL = 1000;

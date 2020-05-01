#pragma once
#include <driver/gpio.h>
#include <driver/ledc.h>

const auto LO_THERSHOLD_TEMP_C = 28;
const auto HI_THERSHOLD_TEMP_C = 35;
const auto LO_THERSHOLD_DUTY = 30u;
const auto HI_THERSHOLD_DUTY = 97u;

const auto PWM_PIN = GPIO_NUM_2;
const auto PWM_FREQ = 25000u;
const auto PWM_RESOLUTION = LEDC_TIMER_9_BIT;

const auto RPM_PIN = GPIO_NUM_25;
const auto RPM_SAMPLES = 10;
const auto RPM_AVERAGE = 10;
const auto RPM_INTERVAL = 100;

const auto MAIN_LOOP_INTERVAL = 1000;

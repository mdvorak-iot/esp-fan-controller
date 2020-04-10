#include <Arduino.h>
#include "Pwm.h"

const auto PWM_PIN = GPIO_NUM_25;
const uint32_t PWM_FREQ = 300000;
const auto PWM_RESOLUTION = LEDC_TIMER_9_BIT;

static Pwm pwm(PWM_PIN, LEDC_TIMER_0, LEDC_CHANNEL_0, PWM_FREQ, PWM_RESOLUTION);

void setup()
{
  // Enable ISR
  ESP_ERROR_CHECK(gpio_install_isr_service(0));

  // Initialize LEDC
  ESP_ERROR_CHECK(ledc_fade_func_install(0));

  // Power PWM
  ESP_ERROR_CHECK(pwm.begin());
  ESP_ERROR_CHECK(pwm.duty(pwm.maxDuty()));
}

void loop()
{
  // Read temperatures

  // Read external PWM signal
  // TODO pulseIn()

  // Control PWM
  uint32_t dutyPercent = 100;
  pwm.duty(dutyPercent * pwm.maxDuty() / 100U);
}

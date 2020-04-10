#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "Pwm.h"

// Config
const auto PWM_PIN = GPIO_NUM_25;
const uint32_t PWM_FREQ = 300000;
const auto PWM_RESOLUTION = LEDC_TIMER_9_BIT;

// Devices
static Pwm pwm(PWM_PIN, LEDC_TIMER_0, LEDC_CHANNEL_0, PWM_FREQ, PWM_RESOLUTION);
static OneWire wire(GPIO_NUM_23);
static DallasTemperature temp(&wire);
static std::vector<uint64_t> sensors;

void setup()
{
  // Enable ISR
  ESP_ERROR_CHECK(gpio_install_isr_service(0));

  // Initialize LEDC
  ESP_ERROR_CHECK(ledc_fade_func_install(0));

  // Power PWM
  ESP_ERROR_CHECK(pwm.begin());
  ESP_ERROR_CHECK(pwm.duty(pwm.maxDuty()));

  // Init Sensors
  temp.begin();

  DeviceAddress addr = {};
  while (wire.search(addr))
  {
    if (temp.validAddress(addr) && temp.validFamily(addr))
    {
      uint64_t addr64 = 0;
      memcpy(&addr64, addr, 8);
      log_i("Found temp sensor: 0x%016X", addr64);
      sensors.push_back(addr64);
    }
  }

  if (sensors.empty())
  {
    log_e("No temperature sensors found!");
  }

  // Done
  log_i("Started");
}

void loop()
{
  // Read temperatures
  temp.requestTemperatures();

  float highestTemp = DEVICE_DISCONNECTED_C;
  for (uint64_t addr : sensors)
  {
    float c = temp.getTempC((uint8_t *)&addr);
    if (c > highestTemp)
    {
      highestTemp = c;
    }
  }

  // Read external PWM signal
  // TODO pulseIn()

  // Control PWM
  uint32_t dutyPercent = highestTemp > DEVICE_DISCONNECTED_C ? map(highestTemp, 30, 60, 50, 100) : 100;
  pwm.duty(dutyPercent * pwm.maxDuty() / 100U);

  // Wait
  delay(100);
}

#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "Pwm.h"

// Config
const auto PWM_PIN = GPIO_NUM_14;
const uint32_t PWM_FREQ = 50000;
const auto PWM_RESOLUTION = LEDC_TIMER_8_BIT;

const auto RPM_PIN = GPIO_NUM_12;

// Devices
static Pwm pwm(PWM_PIN, LEDC_TIMER_0, LEDC_CHANNEL_0, PWM_FREQ, PWM_RESOLUTION);
static OneWire wire(GPIO_NUM_23);
static DallasTemperature temp(&wire);
static std::vector<uint64_t> sensors;

// Setup
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

  // Enumerate and store all sensors
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

  // Control PWM
  uint32_t dutyPercent = 99;
  if (highestTemp > DEVICE_DISCONNECTED_C)
  {
    dutyPercent = map(highestTemp, 30, 60, 50, 99);
  }

  // TODO
  dutyPercent = ((millis() / 1000U) % (99 - 30)) + 30;
  log_e("duty=%d", dutyPercent);
  // TODO END

  pwm.duty(dutyPercent * pwm.maxDuty() / 100U);

  // Wait
  delay(100);
}

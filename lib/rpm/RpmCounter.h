#pragma once

#include <stdint.h>
#include <vector>
#include <esp32-hal-gpio.h>

namespace rpmcounter
{

const size_t RPM_MAX_COUNTERS = 7;

esp_err_t rpm_counter_add(gpio_num_t pin);

esp_err_t rpm_counter_init();

void rpm_counter_values(std::vector<uint16_t> &out);

uint16_t rpm_counter_value();

}; // namespace rpmcounter

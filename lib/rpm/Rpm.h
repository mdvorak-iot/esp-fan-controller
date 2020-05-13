#pragma once

#include <stdint.h>
#include <vector>
#include <esp32-hal-gpio.h>

namespace rpmcounter
{

const size_t RPM_MAX_COUNTERS = 7;

class Rpm
{
public:
    ~Rpm()
    {
        throw; // Unsupported
    }

    esp_err_t add(gpio_num_t pin);

    esp_err_t begin();

    void values(std::vector<uint16_t> &out);

    uint16_t value();

private:
    // Internal class
    struct Sensor;

    // Variables
    portMUX_TYPE lock_ = portMUX_INITIALIZER_UNLOCKED;
    std::vector<Sensor *> sensors_;

    // Internal methods
    static void measureTask(void *p);
    void measureLoop();
};

}; // namespace rpmcounter

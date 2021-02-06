#pragma once

#include <stdint.h>
#include <vector>
#include <driver/pcnt.h>
#include <driver/gpio.h>

namespace rpmcounter
{
    const size_t RPM_COUNTERS_MAX = static_cast<size_t>(PCNT_UNIT_MAX);

    class RpmCounter
    {
    public:
        RpmCounter();
        ~RpmCounter()
        {
            throw "not implemented";
        }

        esp_err_t add(gpio_num_t pin, pcnt_unit_t unit = PCNT_UNIT_MAX);

        esp_err_t begin();

        void values(std::vector<uint16_t> &out) const;

        uint16_t value() const;

    private:
        struct Context;
        struct Sensor;
        struct CountSnapshot;

        Context *context_;

        void measureLoop();
        static void measureTask(void *p);
    };

    // Singleton
    extern RpmCounter Rpm;

}; // namespace rpmcounter

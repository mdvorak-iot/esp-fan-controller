#pragma once

#include <stdint.h>
#include <driver/pcnt.h>
#include <freertos/task.h>

class Rpm
{
public:
    static const size_t MAX_COUNTERS = 14;
    static const size_t SAMPLES = 10;
    static const size_t AVERAGE = 10;
    static const unsigned long INTERVAL = 100;

    Rpm(gpio_num_t pin, pcnt_unit_t unit = PCNT_UNIT_0, pcnt_channel_t channel = PCNT_CHANNEL_0);
    ~Rpm();

    esp_err_t begin();

    uint16_t value() const;

    template <typename... Args>
    static TaskHandle_t start(Rpm &inst, Args... args)
    {
        auto array = new Rpm *[MAX_COUNTERS + 1] { 0 }; // max counters + terminating null
        collectInstances(array, 0, inst, args...);

        // NOTE array is never deleted, since task is infinite
        TaskHandle_t handle = nullptr;
        xTaskCreate(measureTask, "rpm", 4096, array, tskIDLE_PRIORITY + 1, &handle);
        return handle;
    }

private:
    // Internal class
    struct Sensor;

    // Variables
    gpio_num_t const _pin;
    pcnt_unit_t const _unit;
    pcnt_channel_t const _channel;
    Sensor *const _sensor;

    // Internal methods
    uint16_t measure();
    static void measureTask(void *p);

    template <typename... Args>
    static inline void collectInstances(Rpm **list, size_t index, Rpm &inst, Args... args)
    {
        collectInstances(list, index, inst);
        collectInstances(list, index + 1, args...);
    }

    static inline void collectInstances(Rpm **list, size_t index, Rpm &inst)
    {
        assert(index < MAX_COUNTERS);
        list[index] = &inst;
    }
};

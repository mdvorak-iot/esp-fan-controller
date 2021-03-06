#include <driver/pcnt.h>
#include <esp32-hal-log.h>
#include <freertos/task.h>
#include <vector>
#include <memory>
#include <esp_task_wdt.h>
#include "rpm_counter.h"
#include "rpm_counter_circular_buffer.h"

namespace rpmcounter
{
    // Singleton
    RpmCounter Rpm;

    // Constants
    static const size_t SAMPLES = 10;
    static const unsigned long INTERVAL = 100;
    static const int16_t COUNTER_MAX_VALUE = 32767;

    // Helper structures
    struct RpmCounter::CountSnapshot
    {
        unsigned long time;
        int16_t count;
        uint16_t value;
    };

    struct RpmCounter::Sensor
    {
        gpio_num_t pin;
        pcnt_unit_t unit;
        uint32_t valueTotal;
        uint16_t rpm;
        RpmCounterCircularBuffer<CountSnapshot, SAMPLES> counts;
    };

    struct RpmCounter::Context
    {
        // Variables
        portMUX_TYPE lock;
        std::vector<Sensor *> sensors;
    };

    RpmCounter::RpmCounter()
        : context_(new Context{.lock = portMUX_INITIALIZER_UNLOCKED})
    {
    }

    // Functions
    esp_err_t RpmCounter::add(gpio_num_t pin, pcnt_unit_t unit)
    {
        // Too many
        if (context_->sensors.size() >= RPM_COUNTERS_MAX)
        {
            log_e("too many RPM counters");
            return ESP_FAIL;
        }

        // If default
        if (unit == PCNT_UNIT_MAX)
        {
            // Use next available unit
            unit = static_cast<pcnt_unit_t>(context_->sensors.size());
        }

        log_i("configuring rpm counter %d on pin %d", unit, pin);

        // Config
        pcnt_config_t pcnt_config = {
            .pulse_gpio_num = pin,
            .ctrl_gpio_num = PCNT_PIN_NOT_USED,
            .lctrl_mode = PCNT_MODE_KEEP,
            .hctrl_mode = PCNT_MODE_KEEP,
            .pos_mode = PCNT_COUNT_INC,
            .neg_mode = PCNT_COUNT_INC,
            .counter_h_lim = COUNTER_MAX_VALUE,
            .counter_l_lim = 0,
            .unit = unit,
            .channel = PCNT_CHANNEL_0,
        };

        // Init
        esp_err_t err;
        err = pcnt_unit_config(&pcnt_config);
        if (err != ESP_OK)
        {
            log_e("pcnt_unit_config failed: %d %s", err, esp_err_to_name(err));
            return err;
        }

        // Filtering
        // NOTE since we are measuring very low frequencies, we can filter out all short pulses (noise)
        // NOTE counter is 10-bit in APB_CLK cycles (80Mhz), so 1000 = 80kHz, 1023 is max allowed value
        err = pcnt_set_filter_value(pcnt_config.unit, 1023);
        if (err != ESP_OK)
        {
            log_e("pcnt_set_filter_value failed: %d %s", err, esp_err_to_name(err));
            return err;
        }

        err = pcnt_filter_enable(pcnt_config.unit);
        if (err != ESP_OK)
        {
            log_e("pcnt_filter_enable failed: %d %s", err, esp_err_to_name(err));
            return err;
        }

        // Start the counter
        err = pcnt_counter_clear(pcnt_config.unit);
        if (err != ESP_OK)
        {
            log_e("pcnt_counter_clear failed: %d %s", err, esp_err_to_name(err));
            return err;
        }

        err = pcnt_counter_resume(pcnt_config.unit);
        if (err != ESP_OK)
        {
            log_e("pcnt_counter_resume failed: %d %s", err, esp_err_to_name(err));
            return err;
        }

        // Add to collection
        portENTER_CRITICAL(&context_->lock);
        context_->sensors.push_back(new Sensor{
            .pin = pin,
            .unit = pcnt_config.unit,
            .valueTotal = 0,
            .rpm = 0,
        });
        portEXIT_CRITICAL(&context_->lock);

        // OK
        return ESP_OK;
    }

    void RpmCounter::values(std::vector<uint16_t> &out) const
    {
        portENTER_CRITICAL(&context_->lock);

        // Match the size
        out.resize(context_->sensors.size());

        // Copy values
        for (size_t i = 0, s = context_->sensors.size(); i < s; i++)
        {
            out[i] = context_->sensors[i]->rpm;
        }

        portEXIT_CRITICAL(&context_->lock);
    }

    uint16_t RpmCounter::value() const
    {
        portENTER_CRITICAL(&context_->lock);

        uint16_t value = 0;
        if (!context_->sensors.empty())
        {
            assert(context_->sensors[0] != nullptr);
            value = context_->sensors[0]->rpm;
        }

        portEXIT_CRITICAL(&context_->lock);

        return value;
    }

    void RpmCounter::measureTask(void *p)
    {
        static_cast<RpmCounter *>(p)->measureLoop();
    }

    void RpmCounter::measureLoop()
    {
        // Enable Watchdog
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_task_wdt_add(nullptr));

        TickType_t previousWakeTime = xTaskGetTickCount();

        while (true)
        {
            // Watchdog
            ESP_ERROR_CHECK_WITHOUT_ABORT(esp_task_wdt_reset());

            // Lock
            portENTER_CRITICAL(&context_->lock);

            auto now = (unsigned long)esp_timer_get_time();

            // For each configured sensor
            for (auto sensor : context_->sensors)
            {
                // Get count (ignore error, just use zero)
                int16_t count = 0;
                pcnt_get_counter_value(sensor->unit, &count);

                // Calculate
                auto oldest = sensor->counts.first();
                auto revs = ((int32_t)COUNTER_MAX_VALUE + count - oldest.count) % COUNTER_MAX_VALUE;
                auto elapsed = now - oldest.time;

                // 15000000 = 60000000 / pulses per rev / rising and falling edge = 60000000 / 2 / 2
                uint16_t value = 15000000 * revs / elapsed;

                // Average
                sensor->valueTotal -= oldest.value;
                sensor->valueTotal += value;

                // Add new readout
                sensor->counts.push({
                    .time = now,
                    .count = count,
                    .value = value,
                });

                // Expose final value
                sensor->rpm = sensor->valueTotal / sensor->counts.size();
            }

            portEXIT_CRITICAL(&context_->lock);

            // Wait
            vTaskDelayUntil(&previousWakeTime, INTERVAL / portTICK_PERIOD_MS);
        }
    }

    esp_err_t RpmCounter::begin()
    {
        xTaskCreate(measureTask, "rpm", 2048, nullptr, tskIDLE_PRIORITY + 1, nullptr);
        return ESP_OK;
    }

}; // namespace rpmcounter

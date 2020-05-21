#include <driver/pcnt.h>
#include <esp32-hal-log.h>
#include <freertos/task.h>
#include <vector>
#include <esp_task_wdt.h>
#include "rpm_counter.h"
#include "rpm_counter_circular_buffer.h"

namespace rpmcounter
{

// Constants
static const size_t SAMPLES = 10;
static const size_t AVERAGE = 10;
static const unsigned long INTERVAL = 100;
static const int16_t COUNTER_MAX_VALUE = 32767;

// Helper structures
struct rpm_counter_count_snapshot
{
    unsigned long time;
    int16_t count;
};

struct rpm_counter_sensor
{
    gpio_num_t pin;
    pcnt_unit_t unit;
    uint32_t valueTotal;
    uint16_t rpm;
    rpm_counter_circular_buffer<rpm_counter_count_snapshot, SAMPLES> counts;
    rpm_counter_circular_buffer<uint16_t, AVERAGE> values;
};

// Variables
static portMUX_TYPE lock_ = portMUX_INITIALIZER_UNLOCKED;
static std::vector<rpm_counter_sensor *> sensors_;

// Functions
esp_err_t rpm_counter_add(gpio_num_t pin, pcnt_unit_t unit)
{
    // If default
    if (unit == PCNT_UNIT_MAX) {
        // Use next available unit
        unit = static_cast<pcnt_unit_t>(sensors_.size());
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
    portENTER_CRITICAL(&lock_);
    sensors_.push_back(new rpm_counter_sensor{
        .pin = pin,
        .unit = pcnt_config.unit,
        .valueTotal = 0,
        .rpm = 0,
    });
    portEXIT_CRITICAL(&lock_);

    // OK
    return ESP_OK;
}

void rpm_counter_values(std::vector<uint16_t> &out)
{
    portENTER_CRITICAL(&lock_);

    // Match the size
    out.resize(sensors_.size());

    // Copy values
    for (size_t i = 0, s = sensors_.size(); i < s; i++)
    {
        out[i] = sensors_[i]->rpm;
    }

    portEXIT_CRITICAL(&lock_);
}

uint16_t rpm_counter_value()
{
    portENTER_CRITICAL(&lock_);

    uint16_t value = 0;
    if (!sensors_.empty())
    {
        assert(sensors_[0] != nullptr);
        value = sensors_[0]->rpm;
    }

    portEXIT_CRITICAL(&lock_);

    return value;
}

void measure_loop(void *)
{
    // Enable Watchdog
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_task_wdt_add(nullptr));

    TickType_t previousWakeTime = xTaskGetTickCount();

    while (true)
    {
        // Watchdog
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_task_wdt_reset());

        // Lock
        portENTER_CRITICAL(&lock_);

        auto now = (unsigned long)esp_timer_get_time();

        // For each configured sensor
        for (auto sensor : sensors_)
        {
            // Get count (ignore error, just use zero)
            int16_t count = 0;
            pcnt_get_counter_value(sensor->unit, &count);

            // Add new readout
            sensor->counts.push({
                .time = now,
                .count = count,
            });

            // Calculate
            auto oldest = sensor->counts.first();
            auto revs = ((int32_t)COUNTER_MAX_VALUE + count - oldest.count) % COUNTER_MAX_VALUE;
            auto elapsed = now - oldest.time;

            // 15000000 = 60000000 / pulses per rev / rising and falling edge = 60000000 / 2 / 2
            auto value = 15000000 * revs / elapsed;

            // Average
            sensor->valueTotal -= sensor->values.first();
            sensor->valueTotal += value;
            sensor->values.push(value);

            // Expose final value
            sensor->rpm = sensor->valueTotal / sensor->values.size();
        }

        portEXIT_CRITICAL(&lock_);

        // Wait
        vTaskDelayUntil(&previousWakeTime, INTERVAL / portTICK_PERIOD_MS);
    }
}

esp_err_t rpm_counter_init()
{
    xTaskCreate(measure_loop, "rpm", 2048, nullptr, tskIDLE_PRIORITY + 1, nullptr);
    return ESP_OK;
}

}; // namespace rpmcounter

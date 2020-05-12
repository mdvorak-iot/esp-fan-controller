#include <driver/pcnt.h>
#include <esp32-hal-log.h>
#include <freertos/task.h>
#include <vector>
#include "Rpm.h"

const int16_t COUNTER_MAX = 32767;

// This class is not thread-safe
template <typename T, size_t C>
class MovingWindow
{
public:
    MovingWindow()
        : _index(0), _values{0}
    {
    }

    inline size_t size() const
    {
        return C;
    }

    void push(const T &value)
    {
        _index = (_index + 1) % C;
        _values[_index] = value;
    }

    T first() const
    {
        return _values[(_index + 1) % C];
    }

    const T *values() const
    {
        return _values;
    }

private:
    size_t _index;
    T _values[C];
};

struct Snapshot
{
    unsigned long time;
    int16_t count;
};

struct Rpm::Sensor
{
    MovingWindow<Snapshot, Rpm::SAMPLES> counts;
    MovingWindow<uint16_t, Rpm::AVERAGE> values;
    uint32_t valueTotal;
    volatile uint16_t rpm;
};

Rpm::Rpm(gpio_num_t pin, pcnt_unit_t unit, pcnt_channel_t channel)
    : _pin(pin),
      _unit(unit),
      _channel(channel),
      _sensor(new Rpm::Sensor{})
{
}

Rpm::~Rpm()
{
    delete _sensor;
}

uint16_t Rpm::value() const
{
    return _sensor->rpm;
}

esp_err_t Rpm::begin()
{
    pcnt_config_t pcnt_config = {
        .pulse_gpio_num = _pin,
        .ctrl_gpio_num = PCNT_PIN_NOT_USED,
        .lctrl_mode = PCNT_MODE_KEEP,
        .hctrl_mode = PCNT_MODE_KEEP,
        .pos_mode = PCNT_COUNT_INC,
        .neg_mode = PCNT_COUNT_INC,
        .counter_h_lim = COUNTER_MAX,
        .counter_l_lim = 0,
        .unit = PCNT_UNIT_0,
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
    // NOTE counter is 10-bit in APB_CLK cycles (80Mhz), so 1000 = 80kHz, 1023 is max
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

    // OK
    return ESP_OK;
}

uint16_t Rpm::measure()
{
    // Calculate
    auto now = (unsigned long)esp_timer_get_time();
    int16_t count = 0;
    pcnt_get_counter_value(_unit, &count);

    // Append to buffer
    _sensor->counts.push({
        .time = now,
        .count = count,
    });

    // Calculate
    auto oldest = _sensor->counts.first();
    auto revs = ((int32_t)COUNTER_MAX + count - oldest.count) % COUNTER_MAX;
    auto elapsed = now - oldest.time;

    // 15000000 = 60000000 / pulses per rev / rising and falling edge = 60000000 / 2 / 2
    return 15000000 * revs / elapsed;
}

void Rpm::measureTask(void *p)
{
    std::vector<Rpm> array = *static_cast<std::vector<std::unique_ptr<Rpm>> *>(p);

    while (true)
    {
        for (Rpm **ppRpm = array; *ppRpm; ppRpm++)
        {
            auto pRpm = *ppRpm;
            auto pSensor = pRpm->_sensor;

            auto value = pRpm->measure();

            pSensor->valueTotal -= pSensor->values.first();
            pSensor->valueTotal += value;
            pSensor->values.push(value);
            pSensor->rpm = pSensor->valueTotal / pSensor->values.size();
        }

        vTaskDelay(Rpm::INTERVAL / portTICK_PERIOD_MS);
    }
}

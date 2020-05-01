#include <driver/pcnt.h>
#include <esp32-hal-log.h>
#include <freertos/task.h>
#include <vector>
#include "Rpm.h"

const int16_t COUNTER_MAX = 32767;

// This class is not thread-safe
template <typename T>
class Rpm::MovingWindow
{
public:
    MovingWindow(size_t size)
        : _index(0), _size(size), _values(new T[size])
    {
    }

    ~MovingWindow()
    {
        delete[] _values;
    }

    void push(const T &value)
    {
        _index = (_index + 1) % _size;
        _values[_index] = value;
    }

    T first() const
    {
        return _values[(_index + 1) % _size];
    }

private:
    size_t _index;
    size_t _size;
    T *_values;
};

struct Rpm::Snapshot
{
    unsigned long time;
    int16_t count;
};

Rpm::Rpm(gpio_num_t pin, pcnt_unit_t unit, pcnt_channel_t channel)
    : _pin(pin),
      _unit(unit),
      _channel(channel),
      _value(0),
      _values(new MovingWindow<Snapshot>(SAMPLES))
{
}

Rpm::~Rpm()
{
    delete _values;
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

void Rpm::measureTask(void *p)
{
    auto array = static_cast<Rpm **>(p);

    while (true)
    {
        for (Rpm **ppRpm = array; *ppRpm; ppRpm++)
        {
            Rpm *pRpm = *ppRpm;
            pRpm->_value = pRpm->measure();
        }

        vTaskDelay(Rpm::INTERVAL / portTICK_PERIOD_MS);
    }
}

uint16_t Rpm::measure()
{
    // Calculate
    auto now = (unsigned long)esp_timer_get_time();
    int16_t count = 0;
    pcnt_get_counter_value(_unit, &count);

    // Append to buffer
    _values->push({
        .time = now,
        .count = count,
    });

    // Calculate
    auto oldest = _values->first();
    auto revs = ((int32_t)COUNTER_MAX + count - oldest.count) % COUNTER_MAX;
    auto elapsed = now - oldest.time;

    // 15000000 = 60000000 / pulses per rev / rising and falling edge = 60000000 / 2 / 2
    return 15000000 * revs / elapsed;
}

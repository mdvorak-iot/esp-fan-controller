#include "Rpm.h"
#include "driver/pcnt.h"

const int16_t COUNTER_MAX = 32767;

void Rpm::worker(void *p)
{
    auto self = static_cast<Rpm *>(p);

    // Avoid unreal values during startup
    for (size_t i = 0; i < SAMPLES; i++)
    {
        self->measure();
    }

    // Infinite loop
    while (true)
    {
        self->measure();
        delay(INTERVAL);
    }
}

void Rpm::measure()
{
    int16_t count = 0;
    auto now = micros();
    pcnt_get_counter_value(_unit, &count);

    portENTER_CRITICAL(&_mutex);
    _index = (_index + 1) % SAMPLES;
    _values[_index] = {
        .time = now,
        .count = count,
    };
    portEXIT_CRITICAL(&_mutex);
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
        .unit = _unit,
        .channel = _channel,
    };

    // TODO check
    pcnt_unit_config(&pcnt_config);
    pcnt_counter_clear(pcnt_config.unit);
    pcnt_counter_resume(pcnt_config.unit);
    xTaskCreate(worker, "rpm", 10000, this, tskIDLE_PRIORITY, nullptr);
    return ESP_OK;
}

uint16_t Rpm::rpm()
{
    portENTER_CRITICAL(&_mutex);
    auto current = _values[_index];
    auto oldest = _values[(_index + 1) % SAMPLES];
    portEXIT_CRITICAL(&_mutex);

    auto revs = ((int32_t)COUNTER_MAX + current.count - oldest.count) % COUNTER_MAX;
    auto elapsed = current.time - oldest.time;
    printf("i=%i t=%d c=%i ::: t=%d c=%i ::: r=%d e=%d\n", _index, current.time, current.count, oldest.time, oldest.count, revs, elapsed);

    if (elapsed <= 0)
    {
        return 0;
    }

    return 15000000 * revs / elapsed;
}

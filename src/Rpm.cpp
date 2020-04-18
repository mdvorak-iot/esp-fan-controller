#include "Rpm.h"
#include "driver/pcnt.h"

const int16_t COUNTER_MAX = 32767;

esp_err_t Rpm::begin()
{
    pcnt_config_t pcnt_config = {
        .pulse_gpio_num = _pin,
        .ctrl_gpio_num = PCNT_PIN_NOT_USED,
        .lctrl_mode = PCNT_MODE_KEEP,
        .hctrl_mode = PCNT_MODE_KEEP,
        .pos_mode = PCNT_COUNT_INC,
        .neg_mode = PCNT_COUNT_DIS,
        .counter_h_lim = COUNTER_MAX,
        .counter_l_lim = 0,
        .unit = PCNT_UNIT_0,
        .channel = PCNT_CHANNEL_0,
    };

    // TODO check
    pcnt_unit_config(&pcnt_config);
    pcnt_set_filter_value(pcnt_config.unit, 1000);
    pcnt_filter_enable(pcnt_config.unit);
    pcnt_counter_clear(pcnt_config.unit);
    pcnt_counter_resume(pcnt_config.unit);
    return ESP_OK;
}

uint16_t Rpm::rpm()
{
    auto now = micros();
    auto lastReadout = _lastReadout;
    auto lastCount = _lastCount;

    // TODO check
    _lastReadout = now;
    pcnt_get_counter_value(PCNT_UNIT_0, &_lastCount);

    auto revs = ((int32_t)COUNTER_MAX + _lastCount - lastCount) % COUNTER_MAX;
    log_e("%d in %d = %d rpm", revs, now - lastReadout, 30000000 * revs / (now - lastReadout));

    // TODO
    return 0;
}

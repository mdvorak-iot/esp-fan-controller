#include "Rpm.h"


void IRAM_ATTR Rpm::onRpmInterruptHandler(void *self)
{
    static_cast<Rpm *>(self)->onRpmInterrupt();
}

esp_err_t Rpm::begin()
{
    // Configure GPIO for RPM
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = 1ULL << _pin;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;

    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK)
    {
        return err;
    }

    // Attach handler
    return gpio_isr_handler_add(_pin, &onRpmInterruptHandler, this);
}

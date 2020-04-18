#include "Rpm.h"
#include "driver/rmt.h"
#include "driver/periph_ctrl.h"
#include "soc/rmt_reg.h"

esp_err_t Rpm::begin()
{
    rmt_config_t rmt_cfg = {
        .rmt_mode = RMT_MODE_RX,
        .channel = RMT_CHANNEL_0, // TODO param
        .clk_div = 20,
        .gpio_num = _pin,
        .mem_block_num = 1,
        {.rx_config = {
             .filter_en = false,
             .filter_ticks_thresh = 0,
             .idle_threshold = 50000,
         }},
    };

    auto err = rmt_config(&rmt_cfg);
    if (err != ESP_OK)
    {
        return err;
    }

    rmt_driver_install(rmt_cfg.channel, 2000, 0);
    return rmt_rx_start(rmt_cfg.channel, true);
}

uint16_t Rpm::rpm()
{
    // TODO persist?
    RingbufHandle_t rb = nullptr;
    if (rmt_get_ringbuf_handle(RMT_CHANNEL_0, &rb) != ESP_OK) // TODO param
    {
        return 0;
    }

    size_t rx_size = 0;
    auto items = (rmt_item32_t *)xRingbufferReceive(rb, &rx_size, pdMS_TO_TICKS(10));
    if (items != nullptr)
    {
        if (rx_size > 0)
        {
            printf("size=%d ", rx_size);
            for (size_t i = 0; i < rx_size / sizeof(rmt_item32_t); i++)
            {
                printf("[0]%i:%i, ", items[i].level0, items[i].duration0);
                printf("[1]%i:%i", items[i].level1, items[i].duration1);
            }

            printf("\n");
        }

        vRingbufferReturnItem(rb, (void *)items);
    }
    else
    {
        log_e("no data");
    }

    // TODO
    return 0;
}

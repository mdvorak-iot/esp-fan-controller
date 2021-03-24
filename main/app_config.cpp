#include "app_config.h"
#include <esp_log.h>
#include <nvs_handle.hpp>

inline static bool is_valid_gpio_num(int pin)
{
    return pin == GPIO_NUM_NC || (pin >= 0 && GPIO_IS_VALID_GPIO(pin));
}

char *app_config_print_address(char *buf, size_t buf_len, uint64_t value)
{
    int c = snprintf(buf, buf_len, "%08x%08x", (uint32_t)(value >> 32), (uint32_t)value);
    assert(c >= 0 && c < buf_len);
    return buf;
}

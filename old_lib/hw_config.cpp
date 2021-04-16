#include "hw_config.h"
#include <esp_log.h>
#include <nvs_handle.hpp>

const std::unique_ptr<const config_state_set<hw_config>> hw_config::STATE = hw_config::state();

char *app_config_print_address(char *buf, size_t buf_len, uint64_t value)
{
    int c = snprintf(buf, buf_len, "%08x%08x", (uint32_t)(value >> 32), (uint32_t)value);
    assert(c >= 0 && c < buf_len);
    return buf;
}

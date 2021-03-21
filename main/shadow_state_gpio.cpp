#include "shadow_state_gpio.h"

template<>
bool shadow_state_ref<gpio_num_t>::get(const rapidjson::Value &root)
{
    int num = value;
    bool changed = shadow_state_value<int>::get_value(root, ptr, num);
    // TODO validate pin?
    value = static_cast<gpio_num_t>(num);
    return changed;
}

template<>
void shadow_state_ref<gpio_num_t>::load(nvs::NVSHandle &handle, const char *prefix)
{
    int num = value;
    if (handle.get_item(nvs_key(prefix && prefix[0] != '\0' ? prefix + key : key), num) == ESP_OK)
    {
        value = static_cast<gpio_num_t>(num);
    }
}

template<>
void shadow_state_ref<gpio_num_t>::store(nvs::NVSHandle &handle, const char *prefix)
{
    handle.set_item(nvs_key(prefix && prefix[0] != '\0' ? prefix + key : key), static_cast<int>(value));
}

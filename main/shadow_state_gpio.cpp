#include "shadow_state_gpio.h"

template<>
bool shadow_state_ref<gpio_num_t>::Get(const rapidjson::Value &root)
{
    int num = value;
    bool changed = shadow_state_value<int>::GetValue(root, ptr, num);
    // TODO validate pin?
    value = static_cast<gpio_num_t>(num);
    return changed;
}

template<>
void shadow_state_ref<gpio_num_t>::Load(nvs::NVSHandle &handle)
{
    int num = value;
    if (handle.get_item(key.c_str(), num) == ESP_OK)
    {
        value = static_cast<gpio_num_t>(num);
    }
}

template<>
void shadow_state_ref<gpio_num_t>::Store(nvs::NVSHandle &handle)
{
    handle.set_item(key.c_str(), static_cast<int>(value));
}

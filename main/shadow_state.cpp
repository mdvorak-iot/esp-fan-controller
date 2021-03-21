#include "shadow_state.h"
#include <string>

template<>
bool shadow_state_ref<std::string>::get(const rapidjson::Value &root)
{
    // Find object
    const rapidjson::Value *obj = ptr.Get(root);
    // Check its type
    if (obj && obj->IsString())
    {
        // Get new value
        std::string newValue(obj->GetString(), obj->GetStringLength());
        if (newValue != value)
        {
            // If it is different, update
            value = newValue;
            return true;
        }
    }
    return false;
}

template<>
void shadow_state_ref<std::string>::set(rapidjson::Value &root, rapidjson::Value::AllocatorType &allocator)
{
    ptr.Create(root, allocator, nullptr).SetString(value, allocator);
}

template<>
void shadow_state_ref<std::string>::load(nvs::NVSHandle &handle, const char *prefix)
{
    std::string full_key = prefix && prefix[0] != '\0' ? prefix + key : key;

    // First we need to know stored string length
    size_t len = 0;
    esp_err_t err = handle.get_item_size(nvs::ItemType::SZ, nvs_key(full_key), len);
    if (err == ESP_OK)
    {
        // Fast path
        if (len == 0)
        {
            value.resize(0);
            return;
        }

        // Read and store string
        std::string tmp(len, '\0');
        handle.get_string(nvs_key(full_key), &tmp[0], tmp.length());
        value.swap(tmp); // NOTE this is faster than assign, since it does not copy the bytes
    }
}

template<>
void shadow_state_ref<std::string>::store(nvs::NVSHandle &handle, const char *prefix)
{
    // NOTE this will strip string if it contains \0 character
    handle.set_string(nvs_key(prefix && prefix[0] != '\0' ? prefix + key : key), value.c_str());
}

template<>
void shadow_state_ref<double>::load(nvs::NVSHandle &handle, const char *prefix)
{
    // NVS does not support floating point, so store it under u64, bit-wise
    uint64_t value_bits = 0;
    if (handle.get_item<uint64_t>(nvs_key(prefix && prefix[0] != '\0' ? prefix + key : key), value_bits) == ESP_OK)
    {
        value = *reinterpret_cast<double *>(&value_bits);
    }
}

template<>
void shadow_state_ref<double>::store(nvs::NVSHandle &handle, const char *prefix)
{
    // NVS does not support floating point, so store it under u64, bit-wise
    uint64_t value_bits = *reinterpret_cast<uint64_t *>(&value);
    handle.set_item(nvs_key(prefix && prefix[0] != '\0' ? prefix + key : key), value_bits);
}

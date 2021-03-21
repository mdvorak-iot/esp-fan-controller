#include "shadow_state.h"
#include <string>

template<>
bool ShadowState<std::string>::Get(const rapidjson::Value &root)
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
void ShadowState<std::string>::Set(rapidjson::Value &root, rapidjson::Value::AllocatorType &allocator)
{
    ptr.Create(root, allocator, nullptr).SetString(value, allocator);
}

template<>
void ShadowState<std::string>::Load(nvs::NVSHandle &handle)
{
    // First we need to know stored string length
    size_t len = 0;
    esp_err_t err = handle.get_item_size(nvs::ItemType::SZ, key.c_str(), len);
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
        handle.get_string(key.c_str(), &tmp[0], tmp.length());
        value.swap(tmp); // NOTE this is faster than assign, since it does not copy the bytes
    }
}

template<>
void ShadowState<std::string>::Store(nvs::NVSHandle &handle)
{
    // NOTE this will strip string if it contains \0 character
    handle.set_string(key.c_str(), value.c_str());
}

template<>
void ShadowState<double>::Load(nvs::NVSHandle &handle)
{
    // NVS does not support floating point, so store it under u64, bit-wise
    uint64_t value_bits = 0;
    if (handle.get_item<uint64_t>(key.c_str(), value_bits) == ESP_OK)
    {
        value = *reinterpret_cast<double *>(&value_bits);
    }
}

template<>
void ShadowState<double>::Store(nvs::NVSHandle &handle)
{
    // NVS does not support floating point, so store it under u64, bit-wise
    uint64_t value_bits = *reinterpret_cast<uint64_t *>(&value);
    handle.set_item(key.c_str(), value_bits);
}

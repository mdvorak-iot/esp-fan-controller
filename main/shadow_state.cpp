#include "shadow_state.h"
#include <string>

template<>
bool shadow_state_helper<std::string>::get(const rapidjson::Pointer &ptr, const rapidjson::Value &root, std::string &value)
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
void shadow_state_helper<std::string>::set(const rapidjson::Pointer &ptr, rapidjson::Value &root, rapidjson::Value::AllocatorType &allocator, const std::string &value)
{
    ptr.Create(root, allocator, nullptr).SetString(value, allocator);
}

template<>
esp_err_t shadow_state_helper<std::string>::load(const std::string &key, nvs::NVSHandle &handle, const char *prefix, std::string &value)
{
    const std::string full_key = nvs_key(prefix && prefix[0] != '\0' ? prefix + key : key);

    // First we need to know stored string length
    size_t len = 0;
    esp_err_t err = handle.get_item_size(nvs::ItemType::SZ, full_key.c_str(), len);
    if (err == ESP_OK)
    {
        // Fast path
        if (len == 0)
        {
            value.resize(0);
            return ESP_OK;
        }

        // Read and store string
        std::string tmp(len, '\0');
        err = handle.get_string(full_key.c_str(), &tmp[0], tmp.length());
        if (err == ESP_OK)
        {
            value.swap(tmp); // NOTE this is faster than assign, since it does not copy the bytes
        }
    }

    // For both branches
    if (err != ESP_OK)
    {
        ESP_LOGW("shadow_state", "failed to get_string %s: %d %s", full_key.c_str(), err, esp_err_to_name(err));
    }
    return err;
}

template<>
esp_err_t shadow_state_helper<std::string>::store(const std::string &key, nvs::NVSHandle &handle, const char *prefix, const std::string &value)
{
    // NOTE this will strip string if it contains \0 character
    const std::string full_key = nvs_key(prefix && prefix[0] != '\0' ? prefix + key : key);
    esp_err_t err = handle.set_string(full_key.c_str(), value.c_str());

    if (err != ESP_OK)
    {
        ESP_LOGW("shadow_state", "failed to set_string %s: %d %s", full_key.c_str(), err, esp_err_to_name(err));
    }
    return err;
}

template<>
esp_err_t shadow_state_helper<float>::load(const std::string &key, nvs::NVSHandle &handle, const char *prefix, float &value)
{
    const std::string full_key = nvs_key(prefix && prefix[0] != '\0' ? prefix + key : key);

    // NVS does not support floating point, so store it under u32, bit-wise
    uint32_t value_bits = 0;
    esp_err_t err = handle.get_item<uint32_t>(full_key.c_str(), value_bits);
    if (err == ESP_OK)
    {
        value = *reinterpret_cast<float *>(&value_bits);
    }
    else
    {
        ESP_LOGW("shadow_state", "failed to get_item %s: %d %s", full_key.c_str(), err, esp_err_to_name(err));
    }
    return err;
}

template<>
esp_err_t shadow_state_helper<float>::store(const std::string &key, nvs::NVSHandle &handle, const char *prefix, const float &value)
{
    static_assert(sizeof(uint32_t) >= sizeof(float));

    const std::string full_key = nvs_key(prefix && prefix[0] != '\0' ? prefix + key : key);

    // NVS does not support floating point, so store it under u32, bit-wise
    uint32_t value_bits = *reinterpret_cast<const uint32_t *>(&value);
    esp_err_t err = handle.set_item(full_key.c_str(), value_bits);

    if (err != ESP_OK)
    {
        ESP_LOGW("shadow_state", "failed to set_item %s: %d %s", full_key.c_str(), err, esp_err_to_name(err));
    }
    return err;
}

template<>
esp_err_t shadow_state_helper<double>::load(const std::string &key, nvs::NVSHandle &handle, const char *prefix, double &value)
{
    const std::string full_key = nvs_key(prefix && prefix[0] != '\0' ? prefix + key : key);

    // NVS does not support floating point, so store it under u64, bit-wise
    uint64_t value_bits = 0;
    esp_err_t err = handle.get_item<uint64_t>(full_key.c_str(), value_bits);
    if (err == ESP_OK)
    {
        value = *reinterpret_cast<double *>(&value_bits);
    }
    else
    {
        ESP_LOGW("shadow_state", "failed to get_item %s: %d %s", full_key.c_str(), err, esp_err_to_name(err));
    }
    return err;
}

template<>
esp_err_t shadow_state_helper<double>::store(const std::string &key, nvs::NVSHandle &handle, const char *prefix, const double &value)
{
    static_assert(sizeof(uint64_t) >= sizeof(double));

    const std::string full_key = nvs_key(prefix && prefix[0] != '\0' ? prefix + key : key);

    // NVS does not support floating point, so store it under u64, bit-wise
    uint64_t value_bits = *reinterpret_cast<const uint64_t *>(&value);
    esp_err_t err = handle.set_item(full_key.c_str(), value_bits);

    if (err != ESP_OK)
    {
        ESP_LOGW("shadow_state", "failed to set_item %s: %d %s", full_key.c_str(), err, esp_err_to_name(err));
    }
    return err;
}

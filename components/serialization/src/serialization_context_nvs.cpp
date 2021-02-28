#include "serialization_context_nvs.hpp"
#include <nvs.h>

esp_err_t serialization_context_nvs::set_i8(const char *key, int8_t value) const
{
    return nvs_set_i8(handle, key, value);
}

esp_err_t serialization_context_nvs::set_u8(const char *key, uint8_t value) const
{
    return nvs_set_u8(handle, key, value);
}

esp_err_t serialization_context_nvs::set_str(const char *key, const char *value) const
{
    return nvs_set_str(handle, key, value);
}

esp_err_t serialization_context_nvs::get_i8(const char *key, int8_t *value) const
{
    return nvs_get_i8(handle, key, value);
}

esp_err_t serialization_context_nvs::get_u8(const char *key, uint8_t *value) const
{
    return nvs_get_u8(handle, key, value);
}

esp_err_t serialization_context_nvs::get_str(const char *key, char *value, size_t *value_len) const
{
    return nvs_get_str(handle, key, value, value_len);
}

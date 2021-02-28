#include "serialization_context_cjson.hpp"
#include <cstring>

struct serialization_context_cjson *serialization_context_cjson_create(cJSON *obj)
{
    return new serialization_context_cjson(obj);
}

esp_err_t serialization_context_cjson::set_i8(const char *key, int8_t value) const
{
    return cJSON_AddNumberToObject(obj, key, value) ? ESP_OK : ESP_FAIL;
}

esp_err_t serialization_context_cjson::set_u8(const char *key, uint8_t value) const
{
    return cJSON_AddNumberToObject(obj, key, value) ? ESP_OK : ESP_FAIL;
}

esp_err_t serialization_context_cjson::set_str(const char *key, const char *value) const
{
    return cJSON_AddStringToObject(obj, key, value) ? ESP_OK : ESP_FAIL;
}

esp_err_t serialization_context_cjson::get_i8(const char *key, int8_t *value) const
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsNumber(item))
    {
        if (item->valueint < INT8_MIN || item->valueint > INT8_MAX)
        {
            return ESP_ERR_NOT_SUPPORTED;
        }

        *value = item->valueint;
        return ESP_OK;
    }
    return ESP_ERR_NOT_FOUND;
}

esp_err_t serialization_context_cjson::get_u8(const char *key, uint8_t *value) const
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsNumber(item))
    {
        if (item->valueint < 0 || item->valueint > UINT8_MAX)
        {
            return ESP_ERR_NOT_SUPPORTED;
        }

        *value = (uint8_t)item->valueint;
        return ESP_OK;
    }
    return ESP_ERR_NOT_FOUND;
}

esp_err_t serialization_context_cjson::get_str(const char *key, char *value, size_t *value_len) const
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsString(item))
    {
        size_t len = strlen(item->valuestring) + 1; // include null terminator

        if (value)
        {
            if (len > *value_len)
            {
                // Value buffer is too small
                return ESP_ERR_INVALID_SIZE;
            }

            memcpy(value, item->valuestring, len);
        }

        *value_len = len;
        return ESP_OK;
    }
    return ESP_ERR_NOT_FOUND;
}

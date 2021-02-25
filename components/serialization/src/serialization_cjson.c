#include "serialization_cjson.h"
#include "serialization_functions.h"
#include <string.h>

struct serialization_context_cjson
{
    struct serialization_context base;
    cJSON *obj;
};

struct serialization_context *serialization_context_create_cjson(cJSON *obj)
{
    struct serialization_context_cjson *ctx = (struct serialization_context_cjson *)malloc(sizeof(*ctx));
    ctx->base.functions = &serialization_functions_cjson;

    return &ctx->base;
}

inline static cJSON *get_cjson(const struct serialization_context *ctx)
{
    return ((const struct serialization_context_cjson *)ctx)->obj;
}

esp_err_t serialization_cjson_set_i8(const struct serialization_context *ctx, const char *key, int8_t value)
{
    return cJSON_AddNumberToObject(get_cjson(ctx), key, value) ? ESP_OK : ESP_FAIL;
}

esp_err_t serialization_cjson_set_u8(const struct serialization_context *ctx, const char *key, uint8_t value)
{
    return cJSON_AddNumberToObject(get_cjson(ctx), key, value) ? ESP_OK : ESP_FAIL;
}

esp_err_t serialization_cjson_set_gpio_num(const struct serialization_context *ctx, const char *key, gpio_num_t value)
{
    return cJSON_AddNumberToObject(get_cjson(ctx), key, value) ? ESP_OK : ESP_FAIL;
}

esp_err_t serialization_cjson_set_str(const struct serialization_context *ctx, const char *key, const char *value)
{
    return cJSON_AddStringToObject(get_cjson(ctx), key, value) ? ESP_OK : ESP_FAIL;
}

esp_err_t serialization_cjson_get_i8(const struct serialization_context *ctx, const char *key, int8_t *value)
{
    cJSON *obj = cJSON_GetObjectItemCaseSensitive(get_cjson(ctx), key);
    if (cJSON_IsNumber(obj))
    {
        if (obj->valueint < INT8_MIN || obj->valueint > INT8_MAX)
        {
            return ESP_ERR_NOT_SUPPORTED;
        }

        *value = obj->valueint;
        return ESP_OK;
    }
    return ESP_ERR_NOT_FOUND;
}

esp_err_t serialization_cjson_get_u8(const struct serialization_context *ctx, const char *key, uint8_t *value)
{
    cJSON *obj = cJSON_GetObjectItemCaseSensitive(get_cjson(ctx), key);
    if (cJSON_IsNumber(obj))
    {
        if (obj->valueint < 0 || obj->valueint > UINT8_MAX)
        {
            return ESP_ERR_NOT_SUPPORTED;
        }

        *value = (uint8_t)obj->valueint;
        return ESP_OK;
    }
    return ESP_ERR_NOT_FOUND;
}

esp_err_t serialization_cjson_get_gpio_num(const struct serialization_context *ctx, const char *key, gpio_num_t *value)
{
    int8_t value_int;
    esp_err_t err = serialization_cjson_get_i8(ctx, key, &value_int);
    if (err == ESP_OK)
    {
        *value = (gpio_num_t)value_int;
    }
    return err;
}

esp_err_t serialization_cjson_get_str(const struct serialization_context *ctx, const char *key, char *value, size_t *value_len)
{
    cJSON *obj = cJSON_GetObjectItemCaseSensitive(get_cjson(ctx), key);
    if (cJSON_IsString(obj))
    {
        size_t len = strlen(obj->valuestring) + 1; // include null terminator

        if (value)
        {
            if (len > *value_len)
            {
                // Value buffer is too small
                return ESP_ERR_INVALID_SIZE;
            }

            memcpy(value, obj->valuestring, len);
        }

        *value_len = len;
        return ESP_OK;
    }
    return ESP_ERR_NOT_FOUND;
}

const struct serialization_functions serialization_functions_cjson = {
    .free_context = free,
    .set_i8 = serialization_cjson_set_i8,
    .set_u8 = serialization_cjson_set_u8,
    .set_gpio_num = serialization_cjson_set_gpio_num,
    .set_str = serialization_cjson_set_str,
    .get_i8 = serialization_cjson_get_i8,
    .get_u8 = serialization_cjson_get_u8,
    .get_gpio_num = serialization_cjson_get_gpio_num,
    .get_str = serialization_cjson_get_str,
};

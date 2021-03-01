#include "serialization_nvs.h"
#include "serialization_functions.h"
#include <nvs.h>

struct serialization_context_nvs
{
    struct serialization_context base;
    nvs_handle_t handle;
    char prefix[15]; // max length is 15 + null, but it does not make sense to have a prefix of max length
};

struct serialization_context *serialization_context_create_nvs(nvs_handle_t handle)
{
    struct serialization_context_nvs *ctx = (struct serialization_context_nvs *)malloc(sizeof(*ctx));
    ctx->base.functions = &serialization_functions_nvs;
    ctx->handle = handle;
    ctx->prefix[0] = '\0';

    return &ctx->base;
}

inline static nvs_handle_t get_handle(const struct serialization_context *ctx)
{
    return ((const struct serialization_context_nvs *)ctx)->handle;
}

esp_err_t serialization_nvs_set_i8(const struct serialization_context *ctx, const char *key, int8_t value)
{
    return nvs_set_i8(get_handle(ctx), key, value);
}

esp_err_t serialization_nvs_set_u8(const struct serialization_context *ctx, const char *key, uint8_t value)
{
    return nvs_set_u8(get_handle(ctx), key, value);
}

esp_err_t serialization_nvs_set_str(const struct serialization_context *ctx, const char *key, const char *value)
{
    return nvs_set_str(get_handle(ctx), key, value);
}

esp_err_t serialization_nvs_get_i8(const struct serialization_context *ctx, const char *key, int8_t *value)
{
    return nvs_get_i8(get_handle(ctx), key, value);
}

esp_err_t serialization_nvs_get_u8(const struct serialization_context *ctx, const char *key, uint8_t *value)
{
    return nvs_get_u8(get_handle(ctx), key, value);
}

esp_err_t serialization_nvs_get_str(const struct serialization_context *ctx, const char *key, char *value, size_t *value_len)
{
    return nvs_get_str(get_handle(ctx), key, value, value_len);
}

const struct serialization_functions serialization_functions_nvs = {
    .free_context = free,
    .set_i8 = serialization_nvs_set_i8,
    .set_u8 = serialization_nvs_set_u8,
    .set_str = serialization_nvs_set_str,
    .get_i8 = serialization_nvs_get_i8,
    .get_u8 = serialization_nvs_get_u8,
    .get_str = serialization_nvs_get_str,
};

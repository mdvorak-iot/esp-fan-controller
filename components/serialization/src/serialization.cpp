#include "serialization.h"
#include "serialization_context.hpp"

void serialization_free_context(struct serialization_context *ctx)
{
    delete ctx;
}

//struct serialization_context *serialization_nested_context(const struct serialization_context *ctx, const char *key)
//{
//    return ctx->nested_context(key);
//}

esp_err_t serialization_get_i8(const struct serialization_context *ctx, const char *key, int8_t *out_value)
{
    return ctx->get_i8(key, out_value);
}

esp_err_t serialization_get_u8(const struct serialization_context *ctx, const char *key, uint8_t *out_value)
{
    return ctx->get_u8(key, out_value);
}

esp_err_t serialization_get_i16(const struct serialization_context *ctx, const char *key, int16_t *out_value)
{
    return ctx->get_i16(key, out_value);
}

esp_err_t serialization_get_u16(const struct serialization_context *ctx, const char *key, uint16_t *out_value)
{
    return ctx->get_u16(key, out_value);
}

esp_err_t serialization_get_i32(const struct serialization_context *ctx, const char *key, int32_t *out_value)
{
    return ctx->get_i32(key, out_value);
}

esp_err_t serialization_get_u32(const struct serialization_context *ctx, const char *key, uint32_t *out_value)
{
    return ctx->get_u32(key, out_value);
}

esp_err_t serialization_get_i64(const struct serialization_context *ctx, const char *key, int64_t *out_value)
{
    return ctx->get_i64(key, out_value);
}

esp_err_t serialization_get_u64(const struct serialization_context *ctx, const char *key, uint64_t *out_value)
{
    return ctx->get_u64(key, out_value);
}

esp_err_t serialization_get_bool(const struct serialization_context *ctx, const char *key, bool *value)
{
    return ctx->get_bool(key, value);
}

esp_err_t serialization_get_float(const struct serialization_context *ctx, const char *key, float *out_value)
{
    return ctx->get_float(key, out_value);
}

esp_err_t serialization_get_double(const struct serialization_context *ctx, const char *key, double *out_value)
{
    return ctx->get_double(key, out_value);
}

esp_err_t serialization_get_str(const struct serialization_context *ctx, const char *key, char *out_value, size_t *out_value_len)
{
    return ctx->get_str(key, out_value, out_value_len);
}

esp_err_t serialization_get_blob(const struct serialization_context *ctx, const char *key, uint8_t *out_value, size_t *out_value_len)
{
    return ctx->get_blob(key, out_value, out_value_len);
}

esp_err_t serialization_set_i8(const struct serialization_context *ctx, const char *key, int8_t value)
{
    return ctx->set_i8(key, value);
}

esp_err_t serialization_set_u8(const struct serialization_context *ctx, const char *key, uint8_t value)
{
    return ctx->set_u8(key, value);
}

esp_err_t serialization_set_i16(const struct serialization_context *ctx, const char *key, int16_t value)
{
    return ctx->set_i16(key, value);
}

esp_err_t serialization_set_u16(const struct serialization_context *ctx, const char *key, uint16_t value)
{
    return ctx->set_u16(key, value);
}

esp_err_t serialization_set_i32(const struct serialization_context *ctx, const char *key, int32_t value)
{
    return ctx->set_i32(key, value);
}

esp_err_t serialization_set_u32(const struct serialization_context *ctx, const char *key, uint32_t value)
{
    return ctx->set_u32(key, value);
}

esp_err_t serialization_set_i64(const struct serialization_context *ctx, const char *key, int64_t value)
{
    return ctx->set_i64(key, value);
}

esp_err_t serialization_set_u64(const struct serialization_context *ctx, const char *key, uint64_t value)
{
    return ctx->set_u64(key, value);
}

esp_err_t serialization_set_bool(const struct serialization_context *ctx, const char *key, bool *value)
{
    return ctx->set_bool(key, value);
}

esp_err_t serialization_set_float(const struct serialization_context *ctx, const char *key, float value)
{
    return ctx->set_float(key, value);
}

esp_err_t serialization_set_double(const struct serialization_context *ctx, const char *key, double value)
{
    return ctx->set_double(key, value);
}

esp_err_t serialization_set_str(const struct serialization_context *ctx, const char *key, const char *value)
{
    return ctx->set_str(key, value);
}

esp_err_t serialization_set_blob(const struct serialization_context *ctx, const char *key, const uint8_t *value, size_t value_len)
{
    return ctx->set_blob(key, value, value_len);
}

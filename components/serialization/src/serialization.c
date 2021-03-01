#include "serialization.h"
#include "serialization_functions.h"

void serialization_free_context(struct serialization_context *ctx)
{
    ctx->functions->free_context(ctx);
}

struct serialization_context *serialization_nested_context(const struct serialization_context *ctx, const char *key)
{
    return ctx->functions->nested_context(ctx, key);
}

esp_err_t serialization_get_i8(const struct serialization_context *ctx, const char *key, int8_t *out_value)
{
    return ctx->functions->get_i8(ctx, key, out_value);
}

esp_err_t serialization_get_u8(const struct serialization_context *ctx, const char *key, uint8_t *out_value)
{
    return ctx->functions->get_u8(ctx, key, out_value);
}

esp_err_t serialization_get_i16(const struct serialization_context *ctx, const char *key, int16_t *out_value)
{
    return ctx->functions->get_i16(ctx, key, out_value);
}

esp_err_t serialization_get_u16(const struct serialization_context *ctx, const char *key, uint16_t *out_value)
{
    return ctx->functions->get_u16(ctx, key, out_value);
}

esp_err_t serialization_get_i32(const struct serialization_context *ctx, const char *key, int32_t *out_value)
{
    return ctx->functions->get_i32(ctx, key, out_value);
}

esp_err_t serialization_get_u32(const struct serialization_context *ctx, const char *key, uint32_t *out_value)
{
    return ctx->functions->get_u32(ctx, key, out_value);
}

esp_err_t serialization_get_i64(const struct serialization_context *ctx, const char *key, int64_t *out_value)
{
    return ctx->functions->get_i64(ctx, key, out_value);
}

esp_err_t serialization_get_u64(const struct serialization_context *ctx, const char *key, uint64_t *out_value)
{
    return ctx->functions->get_u64(ctx, key, out_value);
}

esp_err_t serialization_get_bool(const struct serialization_context *ctx, const char *key, bool *value)
{
    return ctx->functions->get_bool(ctx, key, value);
}

esp_err_t serialization_get_float(const struct serialization_context *ctx, const char *key, float *out_value)
{
    return ctx->functions->get_float(ctx, key, out_value);
}

esp_err_t serialization_get_double(const struct serialization_context *ctx, const char *key, double *out_value)
{
    return ctx->functions->get_double(ctx, key, out_value);
}

esp_err_t serialization_get_str(const struct serialization_context *ctx, const char *key, char *out_value, size_t *out_value_len)
{
    return ctx->functions->get_str(ctx, key, out_value, out_value_len);
}

esp_err_t serialization_get_blob(const struct serialization_context *ctx, const char *key, uint8_t *out_value, size_t *out_value_len)
{
    return ctx->functions->get_blob(ctx, key, out_value, out_value_len);
}

esp_err_t serialization_set_i8(const struct serialization_context *ctx, const char *key, int8_t value)
{
    return ctx->functions->set_i8(ctx, key, value);
}

esp_err_t serialization_set_u8(const struct serialization_context *ctx, const char *key, uint8_t value)
{
    return ctx->functions->set_u8(ctx, key, value);
}

esp_err_t serialization_set_i16(const struct serialization_context *ctx, const char *key, int16_t value)
{
    return ctx->functions->set_i16(ctx, key, value);
}

esp_err_t serialization_set_u16(const struct serialization_context *ctx, const char *key, uint16_t value)
{
    return ctx->functions->set_u16(ctx, key, value);
}

esp_err_t serialization_set_i32(const struct serialization_context *ctx, const char *key, int32_t value)
{
    return ctx->functions->set_i32(ctx, key, value);
}

esp_err_t serialization_set_u32(const struct serialization_context *ctx, const char *key, uint32_t value)
{
    return ctx->functions->set_u32(ctx, key, value);
}

esp_err_t serialization_set_i64(const struct serialization_context *ctx, const char *key, int64_t value)
{
    return ctx->functions->set_i64(ctx, key, value);
}

esp_err_t serialization_set_u64(const struct serialization_context *ctx, const char *key, uint64_t value)
{
    return ctx->functions->set_u64(ctx, key, value);
}

esp_err_t serialization_set_bool(const struct serialization_context *ctx, const char *key, bool *value)
{
    return ctx->functions->set_bool(ctx, key, value);
}

esp_err_t serialization_set_float(const struct serialization_context *ctx, const char *key, float value)
{
    return ctx->functions->set_float(ctx, key, value);
}

esp_err_t serialization_set_double(const struct serialization_context *ctx, const char *key, double value)
{
    return ctx->functions->set_double(ctx, key, value);
}

esp_err_t serialization_set_str(const struct serialization_context *ctx, const char *key, const char *value)
{
    return ctx->functions->set_str(ctx, key, value);
}

esp_err_t serialization_set_blob(const struct serialization_context *ctx, const char *key, const uint8_t *value, size_t value_len)
{
    return ctx->functions->set_blob(ctx, key, value, value_len);
}

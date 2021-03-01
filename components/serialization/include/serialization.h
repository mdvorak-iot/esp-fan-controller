#pragma once

#include <esp_err.h>
#include <hal/gpio_types.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct serialization_functions;

struct serialization_context
{
    const struct serialization_functions *functions;
};

void serialization_free_context(struct serialization_context *ctx);
struct serialization_context *serialization_nested_context(const struct serialization_context *ctx, const char *key);

void serialization_enter_nested_context(struct serialization_context *ctx, const char *key);
void serialization_enter_list_context(struct serialization_context *ctx, const char *key);
void serialization_enter_list_item(struct serialization_context *ctx);
void serialization_enter_leave_context(struct serialization_context *ctx);

esp_err_t serialization_get_i8(const struct serialization_context *ctx, const char *key, int8_t *out_value);
esp_err_t serialization_get_u8(const struct serialization_context *ctx, const char *key, uint8_t *value);
esp_err_t serialization_get_i16(const struct serialization_context *ctx, const char *key, int16_t *out_value);
esp_err_t serialization_get_u16(const struct serialization_context *ctx, const char *key, uint16_t *out_value);
esp_err_t serialization_get_i32(const struct serialization_context *ctx, const char *key, int32_t *out_value);
esp_err_t serialization_get_u32(const struct serialization_context *ctx, const char *key, uint32_t *out_value);
esp_err_t serialization_get_i64(const struct serialization_context *ctx, const char *key, int64_t *out_value);
esp_err_t serialization_get_u64(const struct serialization_context *ctx, const char *key, uint64_t *out_value);
esp_err_t serialization_get_bool(const struct serialization_context *ctx, const char *key, bool *value);
esp_err_t serialization_get_float(const struct serialization_context *ctx, const char *key, float *out_value);
esp_err_t serialization_get_double(const struct serialization_context *ctx, const char *key, double *out_value);
esp_err_t serialization_get_str(const struct serialization_context *ctx, const char *key, char *out_value, size_t *out_value_len);
esp_err_t serialization_get_blob(const struct serialization_context *ctx, const char *key, uint8_t *out_value, size_t *out_value_len);

esp_err_t serialization_set_i8(const struct serialization_context *ctx, const char *key, int8_t value);
esp_err_t serialization_set_u8(const struct serialization_context *ctx, const char *key, uint8_t value);
esp_err_t serialization_set_i16(const struct serialization_context *ctx, const char *key, int16_t value);
esp_err_t serialization_set_u16(const struct serialization_context *ctx, const char *key, uint16_t value);
esp_err_t serialization_set_i32(const struct serialization_context *ctx, const char *key, int32_t value);
esp_err_t serialization_set_u32(const struct serialization_context *ctx, const char *key, uint32_t value);
esp_err_t serialization_set_i64(const struct serialization_context *ctx, const char *key, int64_t value);
esp_err_t serialization_set_u64(const struct serialization_context *ctx, const char *key, uint64_t value);
esp_err_t serialization_set_bool(const struct serialization_context *ctx, const char *key, bool value);
esp_err_t serialization_set_float(const struct serialization_context *ctx, const char *key, float value);
esp_err_t serialization_set_double(const struct serialization_context *ctx, const char *key, double value);
esp_err_t serialization_set_str(const struct serialization_context *ctx, const char *key, const char *value);
esp_err_t serialization_set_blob(const struct serialization_context *ctx, const char *key, const uint8_t *value, size_t value_len);

#ifdef __cplusplus
}
#endif

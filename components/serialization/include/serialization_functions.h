#pragma once

#include <esp_err.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct serialization_context;

typedef struct serialization_context *(*serialization_func_nested_context)(const struct serialization_context *ctx, const char *key);
typedef void (*serialization_func_free_context)(void *ctx);

typedef esp_err_t (*serialization_func_get_i8)(const struct serialization_context *ctx, const char *key, int8_t *out_value);
typedef esp_err_t (*serialization_func_get_u8)(const struct serialization_context *ctx, const char *key, uint8_t *out_value);
typedef esp_err_t (*serialization_func_get_i16)(const struct serialization_context *ctx, const char *key, int16_t *out_value);
typedef esp_err_t (*serialization_func_get_u16)(const struct serialization_context *ctx, const char *key, uint16_t *out_value);
typedef esp_err_t (*serialization_func_get_i32)(const struct serialization_context *ctx, const char *key, int32_t *out_value);
typedef esp_err_t (*serialization_func_get_u32)(const struct serialization_context *ctx, const char *key, uint32_t *out_value);
typedef esp_err_t (*serialization_func_get_i64)(const struct serialization_context *ctx, const char *key, int64_t *out_value);
typedef esp_err_t (*serialization_func_get_u64)(const struct serialization_context *ctx, const char *key, uint64_t *out_value);
typedef esp_err_t (*serialization_func_get_bool)(const struct serialization_context *ctx, const char *key, bool *out_value);
typedef esp_err_t (*serialization_func_get_float)(const struct serialization_context *ctx, const char *key, float *out_value);
typedef esp_err_t (*serialization_func_get_double)(const struct serialization_context *ctx, const char *key, double *out_value);
typedef esp_err_t (*serialization_func_get_str)(const struct serialization_context *ctx, const char *key, char *out_value, size_t *out_value_len);
typedef esp_err_t (*serialization_func_get_blob)(const struct serialization_context *ctx, const char *key, uint8_t *out_value, size_t *out_value_len);

typedef esp_err_t (*serialization_func_set_i8)(const struct serialization_context *ctx, const char *key, int8_t value);
typedef esp_err_t (*serialization_func_set_u8)(const struct serialization_context *ctx, const char *key, uint8_t value);
typedef esp_err_t (*serialization_func_set_i16)(const struct serialization_context *ctx, const char *key, int16_t value);
typedef esp_err_t (*serialization_func_set_u16)(const struct serialization_context *ctx, const char *key, uint16_t value);
typedef esp_err_t (*serialization_func_set_i32)(const struct serialization_context *ctx, const char *key, int32_t value);
typedef esp_err_t (*serialization_func_set_u32)(const struct serialization_context *ctx, const char *key, uint32_t value);
typedef esp_err_t (*serialization_func_set_i64)(const struct serialization_context *ctx, const char *key, int64_t value);
typedef esp_err_t (*serialization_func_set_u64)(const struct serialization_context *ctx, const char *key, uint64_t value);
typedef esp_err_t (*serialization_func_set_bool)(const struct serialization_context *ctx, const char *key, bool value);
typedef esp_err_t (*serialization_func_set_float)(const struct serialization_context *ctx, const char *key, float value);
typedef esp_err_t (*serialization_func_set_double)(const struct serialization_context *ctx, const char *key, double value);
typedef esp_err_t (*serialization_func_set_str)(const struct serialization_context *ctx, const char *key, const char *value);
typedef esp_err_t (*serialization_func_set_blob)(const struct serialization_context *ctx, const char *key, const uint8_t *value, size_t value_len);

struct serialization_functions
{
    serialization_func_nested_context nested_context;
    serialization_func_free_context free_context;

    serialization_func_get_i8 get_i8;
    serialization_func_get_u8 get_u8;
    serialization_func_get_i16 get_i16;
    serialization_func_get_u16 get_u16;
    serialization_func_get_i32 get_i32;
    serialization_func_get_u32 get_u32;
    serialization_func_get_i64 get_i64;
    serialization_func_get_u64 get_u64;
    serialization_func_get_bool get_bool;
    serialization_func_get_float get_float;
    serialization_func_get_double get_double;
    serialization_func_get_str get_str;
    serialization_func_get_blob get_blob;

    serialization_func_set_i8 set_i8;
    serialization_func_set_u8 set_u8;
    serialization_func_set_i16 set_i16;
    serialization_func_set_u16 set_u16;
    serialization_func_set_i32 set_i32;
    serialization_func_set_u32 set_u32;
    serialization_func_set_i64 set_i64;
    serialization_func_set_u64 set_u64;
    serialization_func_set_bool set_bool;
    serialization_func_set_float set_float;
    serialization_func_set_double set_double;
    serialization_func_set_str set_str;
    serialization_func_set_blob set_blob;
};

#ifdef __cplusplus
}
#endif

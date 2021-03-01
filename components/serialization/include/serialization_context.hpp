#pragma once

#include <cstdint>
#include <esp_err.h>

struct serialization_context
{
    virtual ~serialization_context() = default;

    virtual serialization_context &name(const char *key) = 0;
    virtual serialization_context &nested(const char *name) = 0;
    virtual serialization_context &add() = 0;
    virtual serialization_context &leave_context() = 0;

    virtual esp_err_t get_i8(int8_t *out_value) const = 0;
    virtual esp_err_t get_u8(uint8_t *value) const = 0;
    virtual esp_err_t get_i16(int16_t *out_value) const = 0;
    virtual esp_err_t get_u16(uint16_t *out_value) const = 0;
    virtual esp_err_t get_i32(int32_t *out_value) const = 0;
    virtual esp_err_t get_u32(uint32_t *out_value) const = 0;
    virtual esp_err_t get_i64(int64_t *out_value) const = 0;
    virtual esp_err_t get_u64(uint64_t *out_value) const = 0;
    virtual esp_err_t get_bool(bool *value) const = 0;
    virtual esp_err_t get_float(float *out_value) const = 0;
    virtual esp_err_t get_double(double *out_value) const = 0;
    virtual esp_err_t get_str(char *out_value, size_t *out_value_len) const = 0;
    virtual esp_err_t get_blob(uint8_t *out_value, size_t *out_value_len) const = 0;

    virtual esp_err_t set_i8(int8_t value) const = 0;
    virtual esp_err_t set_u8(uint8_t value) const = 0;
    virtual esp_err_t set_i16(int16_t value) const = 0;
    virtual esp_err_t set_u16(uint16_t value) const = 0;
    virtual esp_err_t set_i32(int32_t value) const = 0;
    virtual esp_err_t set_u32(uint32_t value) const = 0;
    virtual esp_err_t set_i64(int64_t value) const = 0;
    virtual esp_err_t set_u64(uint64_t value) const = 0;
    virtual esp_err_t set_bool(bool value) const = 0;
    virtual esp_err_t set_float(float value) const = 0;
    virtual esp_err_t set_double(double value) const = 0;
    virtual esp_err_t set_str(const char *value) const = 0;
    virtual esp_err_t set_blob(const uint8_t *value, size_t value_len) const = 0;
};

#pragma once

#include "serialization_context.hpp"
#include "serialization_context_cjson.h"
#include <cJSON.h>
#include <cassert>

struct serialization_context_cjson : public serialization_context
{
 public:
    serialization_context_cjson(cJSON *obj)
        : obj(obj)
    {
        assert(obj);
    }

    serialization_context &name(const char *key) override
    {
        key_ = key;
        return *this;
    }

    serialization_context &nested_context(const char *name) override
    {
        return *this;
    }

    serialization_context &add() override
    {
        return *this;
    }

    serialization_context &leave_context() override
    {
        return *this;
    }

    esp_err_t get_i8(int8_t *out_value) const override;
    esp_err_t get_u8(uint8_t *value) const override;
    esp_err_t get_i16(int16_t *out_value) const override;
    esp_err_t get_u16(uint16_t *out_value) const override;
    esp_err_t get_i32(int32_t *out_value) const override;
    esp_err_t get_u32(uint32_t *out_value) const override;
    esp_err_t get_i64(int64_t *out_value) const override;
    esp_err_t get_u64(uint64_t *out_value) const override;
    esp_err_t get_bool(bool *value) const override;
    esp_err_t get_float(float *out_value) const override;
    esp_err_t get_double(double *out_value) const override;
    esp_err_t get_str(char *out_value, size_t *out_value_len) const override;
    esp_err_t get_blob(uint8_t *out_value, size_t *out_value_len) const override;

    esp_err_t set_i8(int8_t value) const override;
    esp_err_t set_u8(uint8_t value) const override;
    esp_err_t set_i16(int16_t value) const override;
    esp_err_t set_u16(uint16_t value) const override;
    esp_err_t set_i32(int32_t value) const override;
    esp_err_t set_u32(uint32_t value) const override;
    esp_err_t set_i64(int64_t value) const override;
    esp_err_t set_u64(uint64_t value) const override;
    esp_err_t set_bool(bool value) const override;
    esp_err_t set_float(float value) const override;
    esp_err_t set_double(double value) const override;
    esp_err_t set_str(const char *value) const override;
    esp_err_t set_blob(const uint8_t *value, size_t value_len) const override;

 protected:
    cJSON *const obj;
    const char *key_;
};

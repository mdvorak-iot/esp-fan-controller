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

    esp_err_t get_i8(const char *key, int8_t *out_value) const override;
    esp_err_t get_u8(const char *key, uint8_t *value) const override;
    esp_err_t get_i16(const char *key, int16_t *out_value) const override;
    esp_err_t get_u16(const char *key, uint16_t *out_value) const override;
    esp_err_t get_i32(const char *key, int32_t *out_value) const override;
    esp_err_t get_u32(const char *key, uint32_t *out_value) const override;
    esp_err_t get_i64(const char *key, int64_t *out_value) const override;
    esp_err_t get_u64(const char *key, uint64_t *out_value) const override;
    esp_err_t get_bool(const char *key, bool *value) const override;
    esp_err_t get_float(const char *key, float *out_value) const override;
    esp_err_t get_double(const char *key, double *out_value) const override;
    esp_err_t get_str(const char *key, char *out_value, size_t *out_value_len) const override;
    esp_err_t get_blob(const char *key, uint8_t *out_value, size_t *out_value_len) const override;

    esp_err_t set_i8(const char *key, int8_t value) const override;
    esp_err_t set_u8(const char *key, uint8_t value) const override;
    esp_err_t set_i16(const char *key, int16_t value) const override;
    esp_err_t set_u16(const char *key, uint16_t value) const override;
    esp_err_t set_i32(const char *key, int32_t value) const override;
    esp_err_t set_u32(const char *key, uint32_t value) const override;
    esp_err_t set_i64(const char *key, int64_t value) const override;
    esp_err_t set_u64(const char *key, uint64_t value) const override;
    esp_err_t set_bool(const char *key, bool value) const override;
    esp_err_t set_float(const char *key, float value) const override;
    esp_err_t set_double(const char *key, double value) const override;
    esp_err_t set_str(const char *key, const char *value) const override;
    esp_err_t set_blob(const char *key, const uint8_t *value, size_t value_len) const override;

 protected:
    cJSON *const obj;
};

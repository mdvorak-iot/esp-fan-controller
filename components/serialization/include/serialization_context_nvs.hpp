#pragma once

#include "serialization_context.hpp"
#include <nvs.h>
#include <string>

struct serialization_context_nvs : public serialization_context
{
 public:
    explicit serialization_context_nvs(nvs_handle_t handle, const char *prefix = "")
        : handle_(handle), prefix_(prefix), index_(SIZE_MAX)
    {
        assert(handle);
    }

    serialization_context &name(const char *key) override
    {
        key_ = prefix_ + key;
        return *this;
    }

    serialization_context &nested(const char *name) override
    {
        prefix_ = name;
        return *this;
    }

    serialization_context &add() override
    {
        if (index_ == SIZE_MAX)
        {
            index_ = 0;
        }
        else
        {
            index_++;
        }
        return *this;
    }

    serialization_context &leave_context() override
    {
        prefix_ = std::string();
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
    nvs_handle_t const handle_;
    std::string prefix_;
    std::string key_;
    size_t index_;
};

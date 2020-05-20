#include "app_temps.h"
#include <OneWire.h>
#include <DallasTemperature.h>

namespace apptemps
{

    static OneWire *wire_ = nullptr;
    static DallasTemperature *temp_ = nullptr;
    static std::vector<uint64_t> sensors_;
    static std::vector<float> values_;
    static std::vector<std::string> names_;
    static portMUX_TYPE lock_ = portMUX_INITIALIZER_UNLOCKED;

    std::string temperature_default_name(uint64_t address)
    {
        char s[17]{0};
        snprintf(s, 16, "%08X%08X", (uint32_t)(address >> 32), (uint32_t)address);
        return s;
    }

    void temperature_sensors_search()
    {
        sensors_.clear();

        DeviceAddress addr = {};
        while (wire_->search(addr))
        {
            if (temp_->validAddress(addr) && temp_->validFamily(addr))
            {
                uint64_t addr64 = 0;
                memcpy(&addr64, addr, 8);

                // WARNING esp32 does not support printing of 64-bit integer, and trying to do so corrupts heap!
                log_i("found temp sensor: 0x%08X%08X", (uint32_t)(addr64 >> 32), (uint32_t)addr64);

                portENTER_CRITICAL(&lock_);
                sensors_.push_back(addr64);
                values_.push_back(DEVICE_DISCONNECTED_C);
                names_.push_back(temperature_default_name(addr64));
                portEXIT_CRITICAL(&lock_);
            }
        }
    }

    esp_err_t temperature_sensors_init(gpio_num_t pin)
    {
        assert(wire_ == nullptr);
        assert(temp_ == nullptr);

        wire_ = new OneWire(pin);
        temp_ = new DallasTemperature(wire_);

        // Init
        temp_->begin();
        temp_->setResolution(12);

        // Enumerate and store all sensors
        temperature_sensors_search();

        // Request temperature, so sensors won't have 85C stored as initial value
        temp_->requestTemperatures();

        // Done
        return ESP_OK;
    }

    void temperature_request()
    {
        if (!temp_)
        {
            // Not initialized
            return;
        }

        temp_->requestTemperatures();

        std::vector<float> values;
        values.reserve(sensors_.size());

        for (uint64_t addr : sensors_)
        {
            float c = temp_->getTempC(reinterpret_cast<uint8_t *>(&addr));
            // WARNING esp32 does not support printing of 64-bit integer, and trying to do so corrupts heap!
            log_d("temp [%d] 0x%08X%08X: %f", i, (uint32_t)(addr >> 32), (uint32_t)addr, c);

            values.push_back(c);
        }

        portENTER_CRITICAL(&lock_);
        values_ = values;
        portEXIT_CRITICAL(&lock_);
    }

    void temperature_assign_name(uint64_t address, std::string name)
    {
        portENTER_CRITICAL(&lock_);
        // Find
        for (size_t i = 0; i < sensors_.size(); i++)
        {
            if (sensors_[i] == address)
            {
                // Store
                names_[i] = !name.empty() ? name : temperature_default_name(address);
                break; // NOTE no return here!
            }
        }
        portEXIT_CRITICAL(&lock_);
    }

    void temperature_values(std::vector<temperature_sensor> &out)
    {
        portENTER_CRITICAL(&lock_);

        // Match the size
        out.resize(sensors_.size());

        // Copy values
        for (size_t i = 0, s = sensors_.size(); i < s; i++)
        {
            out[i].address = sensors_[i];
            out[i].value = values_[i];
            out[i].name = names_[i];
        }

        portEXIT_CRITICAL(&lock_);
    }

    float temperature_value(uint64_t address)
    {
        float value = DEVICE_DISCONNECTED_C;

        portENTER_CRITICAL(&lock_);

        // Find
        for (size_t i = 0; i < sensors_.size(); i++)
        {
            if (sensors_[i] == address)
            {
                // Store
                value = values_[i];
                break; // NOTE no return here!
            }
        }

        portEXIT_CRITICAL(&lock_);

        // Return
        return value;
    }

}; // namespace apptemps

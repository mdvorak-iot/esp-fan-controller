#pragma once

#include <ArduinoJson.h>
#include "app_config.h"

namespace appconfig
{

    const size_t APP_CONFIG_JSON_SIZE = 2048;

    void app_config_to_json(const app_config &config, JsonDocument &doc)
    {
        doc["control_pin"] = config.data.control_pin;
        {
            auto rpm_pins = doc.createNestedArray("rpm_pins");
            for (size_t i = 0; i < APP_CONFIG_MAX_RPM; i++)
            {
                rpm_pins.add(config.data.rpm_pins[i]);
            }
        }
        doc["sensors_pin"] = config.data.sensors_pin;
        doc["primary_sensor_address"] = config.data.primary_sensor_address;
        {
            auto sensors = doc.createNestedArray("sensors");
            for (size_t i = 0; i < APP_CONFIG_MAX_SENSORS; i++)
            {
                auto &s = config.data.sensors[i];
                auto sensorObj = sensors.createNestedObject();
                char addrHex[17];
                snprintf(addrHex, 17, "%08X%08X", (uint32_t)(s.address >> 32), (uint32_t)s.address);
                sensorObj["address"] = std::string(addrHex);
                sensorObj["name"] = s.name;
            }
        }
        doc["low_threshold_celsius"] = config.data.low_threshold_celsius;
        doc["high_threshold_celsius"] = config.data.high_threshold_celsius;
        doc["cpu_threshold_celsius"] = config.data.cpu_threshold_celsius;
        doc["cpu_poll_interval_seconds"] = config.data.cpu_poll_interval_seconds;
        doc["low_threshold_duty_percent"] = config.data.low_threshold_duty_percent;
        doc["high_threshold_duty_percent"] = config.data.high_threshold_duty_percent;
        doc["hardware_name"] = config.data.hardware_name;
        doc["cpu_query_url"] = config.cpu_query_url;
    }

    app_config app_config_from_json(const JsonDocument &doc, app_config &config)
    {
        config.data.control_pin = doc.containsKey("control_pin") ? static_cast<gpio_num_t>(doc["control_pin"].as<int>()) : APP_CONFIG_PIN_DISABLED;
        auto rpmPins = doc["rpm_pins"].as<JsonArrayConst>();
        for (size_t i = 0; i < APP_CONFIG_MAX_RPM; i++)
        {
            auto rpmPin = i < rpmPins.size() ? static_cast<gpio_num_t>(rpmPins[i].as<int>()) : APP_CONFIG_PIN_DISABLED;
            config.data.rpm_pins[i] = rpmPin;
        }
        config.data.sensors_pin = doc.containsKey("sensors_pin") ? static_cast<gpio_num_t>(doc["sensors_pin"].as<int>()) : APP_CONFIG_PIN_DISABLED;
        config.data.primary_sensor_address = doc["primary_sensor_address"].as<uint64_t>();
        auto sensors = doc["sensors"].as<JsonArrayConst>();
        for (size_t i = 0; i < APP_CONFIG_MAX_SENSORS; i++)
        {
            if (i < sensors.size())
            {
                auto &s = config.data.sensors[i];
                sensors[i]["name"].as<std::string>().copy(s.name, APP_CONFIG_MAX_NAME_LENGHT - 1);
                s.address = strtoull(sensors[i]["address"].as<const char *>(), nullptr, 16);
            }
            else
            {
                config.data.sensors[i] = {};
            }
        }
        config.data.low_threshold_celsius = doc["low_threshold_celsius"].as<uint8_t>();
        config.data.high_threshold_celsius = doc["high_threshold_celsius"].as<uint8_t>();
        config.data.cpu_threshold_celsius = doc["cpu_threshold_celsius"].as<uint8_t>();
        config.data.cpu_poll_interval_seconds = doc["cpu_poll_interval_seconds"].as<uint16_t>();
        config.data.low_threshold_duty_percent = doc["low_threshold_duty_percent"].as<uint8_t>();
        config.data.high_threshold_duty_percent = doc["high_threshold_duty_percent"].as<uint8_t>();
        doc["hardware_name"].as<std::string>().copy(config.data.hardware_name, APP_CONFIG_MAX_NAME_LENGHT - 1);
        config.cpu_query_url = doc["cpu_query_url"].as<std::string>();

        return config;
    }

}; // namespace appconfig

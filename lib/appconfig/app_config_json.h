#pragma once

#include <ArduinoJson.h>
#include "app_config.h"

namespace appconfig
{
    
const size_t APP_CONFIG_JSON_SIZE = JSON_OBJECT_SIZE(12 + 3 * APP_CONFIG_MAX_SENSORS + APP_CONFIG_MAX_RPM) + 26;

void app_config_to_json(const app_config &config, StaticJsonDocument<APP_CONFIG_JSON_SIZE> &doc)
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
            sensorObj["address"] = s.address;
            sensorObj["name"] = s.name;
        }
    }
    doc["low_threshold_celsius"] = config.data.low_threshold_celsius;
    doc["high_threshold_celsius"] = config.data.high_threshold_celsius;
    doc["cpu_threshold_celsius"] = config.data.cpu_threshold_celsius;
    doc["cpu_poll_interval_seconds"] = config.data.cpu_poll_interval_seconds;
    doc["hardware_name"] = config.data.hardware_name;
    doc["cpu_query_url"] = config.cpu_query_url;
}

app_config app_config_from_json(const StaticJsonDocument<APP_CONFIG_JSON_SIZE> &doc)
{
    app_config config = APP_CONFIG_DEFAULT();

    config.data.control_pin = static_cast<gpio_num_t>(doc["control_pin"].as<uint8_t>());
    {
        size_t i = 0;
        for (auto rpmPin : doc["rpm_pins"].as<JsonArrayConst>())
        {
            config.data.rpm_pins[i++] = static_cast<gpio_num_t>(rpmPin.as<uint8_t>());
        }
    }
    config.data.sensors_pin = static_cast<gpio_num_t>(doc["sensors_pin"].as<uint8_t>());
    config.data.primary_sensor_address = doc["primary_sensor_address"].as<uint64_t>();
    {
        size_t i = 0;
        for (auto sensorObj : doc["sensors"].as<JsonArrayConst>())
        {
            auto &s = config.data.sensors[i];
            s.address = sensorObj["address"].as<uint64_t>();
            sensorObj["name"].as<std::string>().copy(s.name, APP_CONFIG_MAX_NAME_LENGHT - 1);
            i++;
        }
    }
    config.data.low_threshold_celsius = doc["low_threshold_celsius"].as<uint8_t>();
    config.data.high_threshold_celsius = doc["high_threshold_celsius"].as<uint8_t>();
    config.data.cpu_threshold_celsius = doc["cpu_threshold_celsius"].as<uint8_t>();
    config.data.cpu_poll_interval_seconds = doc["cpu_poll_interval_seconds"].as<uint16_t>();
    doc["hardware_name"].as<std::string>().copy(config.data.hardware_name, APP_CONFIG_MAX_NAME_LENGHT - 1);
    config.cpu_query_url = doc["cpu_query_url"].as<std::string>();

    return config;
}

}; // namespace appconfig

#pragma once

#include <config_state_gpio.h>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>

struct hw_config_sensor
{
    std::string address;
    std::string name;
    float offset_c = 0.0f;

    static std::unique_ptr<config_state<hw_config_sensor>> state()
    {
        auto *state = new config_state_set<hw_config_sensor>();
        state->add_field(&hw_config_sensor::address, "/addr", "/a");
        state->add_field(&hw_config_sensor::name, "/name", "/n");
        state->add_field(&hw_config_sensor::offset_c, "/offsetC", "/of");

        return std::unique_ptr<config_state<hw_config_sensor>>(state);
    }
};

struct hw_config
{
    gpio_num_t status_led_pin = GPIO_NUM_NC;
    bool status_led_on_state = true;
    gpio_num_t pwm_pin = GPIO_NUM_NC;
    bool pwm_inverted_duty = false;
    gpio_num_t sensors_pin = GPIO_NUM_NC;
    std::string primary_sensor_address;
    std::vector<gpio_num_t> rpm_pins;
    std::vector<hw_config_sensor> sensors;

    static std::unique_ptr<config_state_set<hw_config>> state()
    {
        auto *state = new config_state_set<hw_config>();
        state->add_field(&hw_config::status_led_pin, "/statusLed/pin", "/led/pin");
        state->add_field(&hw_config::status_led_on_state, "/statusLed/onState", "/led/onstate");
        state->add_field(&hw_config::pwm_pin, "/pwm/pin");
        state->add_field(&hw_config::pwm_inverted_duty, "/pwm/invert");
        state->add_value_list(&hw_config::rpm_pins, "/rpm/pins");
        state->add_field(&hw_config::sensors_pin, "/sensors/pin", "/sens/pin");
        state->add_field(&hw_config::primary_sensor_address, "/sensors/primaryAddr", "/sens/primary");
        state->add_list(&hw_config::sensors, "/sensors/list", "/sensLst", hw_config_sensor::state());

        return std::unique_ptr<config_state_set<hw_config>>(state);
    }

    static const std::unique_ptr<const config_state_set<hw_config>> STATE;
};

char *app_config_print_address(char *buf, size_t buf_len, uint64_t value);

#define APP_CONFIG_NVS_NAME "app_config"

idf_component_register(
        SRC_DIRS .
        INCLUDE_DIRS .
        REQUIRES
        freertos
        nvs_flash
        log
        esp32
        esp_wifi
        app_update
        esp_http_server
        json
        double_reset
        status_led
        wps_config
        wifi_reconnect
        ds18b20_group
        aws_iot_mqtt_config
        aws_iot_shadow
        config_state
)

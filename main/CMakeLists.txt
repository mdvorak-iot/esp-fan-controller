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
        esp32-owb
        esp32-ds18b20
)

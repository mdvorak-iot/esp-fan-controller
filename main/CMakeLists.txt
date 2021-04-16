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
        wifi_provisioning
        esp_rainmaker
        rmaker_common
        esp_http_server
        double_reset
        status_led
        wifi_reconnect
        app_wifi
        rainmaker_init
        ds18b20_group
)

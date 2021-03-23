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
        aws_iot_mqtt_config
        aws_iot_shadow
)

target_link_libraries(${COMPONENT_LIB} PRIVATE rapidjson)

# TODO make this global
target_compile_definitions(${COMPONENT_LIB} PUBLIC -DRAPIDJSON_HAS_STDSTRING=1 -DRAPIDJSON_ALLOCATOR_DEFAULT_CHUNK_CAPACITY=1024 -DRAPIDJSON_VALUE_DEFAULT_ARRAY_CAPACITY=4 -DRAPIDJSON_VALUE_DEFAULT_OBJECT_CAPACITY=4)

cmake_minimum_required(VERSION 3.16.0)
include(ExternalComponent.cmake)

# Components
ExternalComponent_Get(double_reset https://github.com/mdvorak-iot/esp-double-reset.git v1.0.0)
ExternalComponent_Get(status_led https://github.com/mdvorak-iot/esp-status-led.git 1fe88e4)
ExternalComponent_Get(wps_config https://github.com/mdvorak-iot/esp-wps-config.git b0576066)
ExternalComponent_Get(wifi_reconnect https://github.com/mdvorak-iot/esp-wifi-reconnect.git 1544945)
ExternalComponent_Get(esp32-owb https://github.com/DavidAntliff/esp32-owb.git b2032f7)
ExternalComponent_Get(esp32-ds18b20 https://github.com/DavidAntliff/esp32-ds18b20.git d677f09)
ExternalComponent_Get(aws_iot_mqtt_config https://github.com/mdvorak-iot/esp-aws-iot-mqtt-config.git da059444)
ExternalComponent_Get(aws_iot_shadow https://github.com/mdvorak-iot/esp-aws-iot-shadow.git f37c6c4)
ExternalComponent_Get(config_state https://github.com/mdvorak-iot/esp-config-state.git f08953f8)
ExternalLibrary_Get(rapidjson https://github.com/Tencent/rapidjson 1c2c8e0)

# Project
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp-fan-controller)

# rapidjson
add_library(rapidjson INTERFACE)
target_include_directories(rapidjson INTERFACE ${rapidjson_SOURCE_DIR}/include)
target_compile_definitions(rapidjson INTERFACE RAPIDJSON_HAS_STDSTRING=1 RAPIDJSON_ALLOCATOR_DEFAULT_CHUNK_CAPACITY=1024 RAPIDJSON_VALUE_DEFAULT_ARRAY_CAPACITY=10 RAPIDJSON_VALUE_DEFAULT_OBJECT_CAPACITY=10)

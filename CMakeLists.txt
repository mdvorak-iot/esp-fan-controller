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
ExternalLibrary_Get(modp_json https://github.com/mdvorak-iot/modp-json.git b386e13)
ExternalLibrary_Get(coreJSON https://github.com/FreeRTOS/coreJSON v3.0.1)

# Project
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp-fan-controller)

# modp_json
add_subdirectory(${modp_json_SOURCE_DIR})

# coreJSON
include(${corejson_SOURCE_DIR}/jsonFilePaths.cmake)
add_library(coreJSON STATIC ${JSON_SOURCES})
target_include_directories(coreJSON PUBLIC ${JSON_INCLUDE_PUBLIC_DIRS})

cmake_minimum_required(VERSION 3.16.0)
include(ExternalComponent.cmake)

# Components
ExternalComponent_Get(double_reset https://github.com/mdvorak-iot/esp-double-reset.git aef441a)
ExternalComponent_Get(status_led https://github.com/mdvorak-iot/esp-status-led.git 1fe88e47)
ExternalComponent_Get(wps_config https://github.com/mdvorak-iot/esp-wps-config.git b0576066)
ExternalComponent_Get(wifi_reconnect https://github.com/mdvorak-iot/esp-wifi-reconnect.git 1544945)
ExternalComponent_Get(esp32-owb https://github.com/DavidAntliff/esp32-owb.git b2032f7)
ExternalComponent_Get(esp32-ds18b20 https://github.com/DavidAntliff/esp32-ds18b20.git d677f09)
ExternalComponent_Get(aws_iot_mqtt_config https://github.com/mdvorak-iot/esp-aws-iot-mqtt-config.git fbdf8777)
ExternalComponent_Get(aws_iot_shadow https://github.com/mdvorak-iot/esp-aws-iot-shadow.git 86b344f6)

# Project
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp-fan-controller)

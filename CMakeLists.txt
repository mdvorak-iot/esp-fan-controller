cmake_minimum_required(VERSION 3.16.0)
include(ExternalComponent.cmake)

# Components
ExternalComponent_Get(double_reset https://github.com/mdvorak-iot/esp-double-reset.git 717c54d)
ExternalComponent_Get(status_led https://github.com/mdvorak-iot/esp-status-led.git 2eaf7eb)
ExternalComponent_Get(wps_config https://github.com/mdvorak-iot/esp-wps-config.git 6d8fe29)
ExternalComponent_Get(wifi_reconnect https://github.com/mdvorak-iot/esp-wifi-reconnect.git 98d2752)
ExternalComponent_Get(esp32-owb https://github.com/DavidAntliff/esp32-owb.git b2032f7)
ExternalComponent_Get(esp32-ds18b20 https://github.com/DavidAntliff/esp32-ds18b20.git d677f09)

# Project
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp-fan-controller)

idf_component_register(
        SRCS src/pc_fan_control.c
        INCLUDE_DIRS include
        PRIV_REQUIRES driver log
        REQUIRES esp_common soc
)

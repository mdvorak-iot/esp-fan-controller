idf_component_register(
        SRCS src/ds18b20_group.c
        INCLUDE_DIRS include
        REQUIRES
        esp32-owb
        esp32-ds18b20
)

idf_component_register(
    SRCS "flame_mode.c"
    INCLUDE_DIRS "include"
    REQUIRES led_driver lamp_nvs
)

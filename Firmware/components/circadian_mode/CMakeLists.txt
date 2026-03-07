idf_component_register(
    SRCS "circadian_mode.c"
    INCLUDE_DIRS "include"
    REQUIRES lamp_nvs led_driver
)

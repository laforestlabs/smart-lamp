idf_component_register(
    SRCS "auto_mode.c"
    INCLUDE_DIRS "include"
    REQUIRES led_driver lamp_nvs sensor esp_timer
)

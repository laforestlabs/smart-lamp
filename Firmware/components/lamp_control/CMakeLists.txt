idf_component_register(
    SRCS "lamp_control.c"
    INCLUDE_DIRS "include"
    REQUIRES led_driver sensor lamp_nvs auto_mode flame_mode esp_now_sync
)

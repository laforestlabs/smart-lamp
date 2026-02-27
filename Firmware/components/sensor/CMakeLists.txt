idf_component_register(
    SRCS "sensor_init.c" "sensor_pir.c" "sensor_touch.c" "sensor_light.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "."
    REQUIRES driver esp_adc esp_timer esp_driver_dac
)

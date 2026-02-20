idf_component_register(
    SRCS "led_driver.c" "led_encoder.c" "led_gamma.c" "led_layout.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "."
    REQUIRES driver
)

idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES lamp_control lamp_nvs lamp_ota led_driver sensor ble_service esp_now_sync
)

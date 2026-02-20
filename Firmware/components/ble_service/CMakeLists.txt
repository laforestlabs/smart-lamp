idf_component_register(
    SRCS "ble_service.c" "ble_gatt.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "."
    REQUIRES bt led_driver lamp_nvs lamp_ota lamp_control sensor flame_mode
)

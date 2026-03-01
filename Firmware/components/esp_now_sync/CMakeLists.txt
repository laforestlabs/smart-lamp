idf_component_register(
    SRCS "esp_now_sync.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_wifi lamp_nvs sensor esp_event esp_netif
)

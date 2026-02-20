idf_component_register(
    SRCS "lamp_ota.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_app_format app_update
)

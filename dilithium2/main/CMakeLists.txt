idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_timer ml-dsa-44 esp_wifi nvs_flash
)

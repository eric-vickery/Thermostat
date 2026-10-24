idf_component_register(SRCS "bme280.cpp" "main.cpp"
                       PRIV_REQUIRES spi_flash
                       REQUIRES esp_driver_ledc driver esp_lcd nvs_flash esp_timer esp_lcd_gc9a01 ui eez-framework
                       INCLUDE_DIRS "."
)

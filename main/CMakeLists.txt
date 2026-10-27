idf_component_register(SRCS "connectivity.cpp" "display.cpp" "idle_monitor.cpp"
                            "input_controller.cpp" "main.cpp" "power.cpp" "scheduler.cpp"
                            "sensor_monitor.cpp" "settings.cpp" "status_leds.cpp" "thermostat.cpp" "user_input.cpp"
                       PRIV_REQUIRES spi_flash
                       REQUIRES esp_driver_ledc driver esp_lcd nvs_flash esp_timer esp_lcd_gc9a01 esp_now_master bme280 ui eez-framework
                       INCLUDE_DIRS "."
)

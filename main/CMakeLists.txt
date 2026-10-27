idf_component_register(SRCS "connectivity.cpp" "display.cpp" "idle_monitor.cpp"
                            "input_controller.cpp" "main.cpp" "power.cpp" "rest_api.cpp"
                            "scheduler.cpp" "sensor_monitor.cpp" "settings.cpp" "status_leds.cpp"
                            "thermostat.cpp" "user_input.cpp"
                       PRIV_REQUIRES spi_flash espressif__cjson
                       REQUIRES esp_driver_ledc driver esp_lcd nvs_flash esp_timer esp_lcd_gc9a01 esp_now_master bme280 ui eez-framework esp_http_server
                       INCLUDE_DIRS "."
)

idf_component_register(SRCS "connectivity.cpp" "display.cpp" "heater_relay.cpp" "idle_monitor.cpp"
                            "input_controller.cpp" "main.cpp" "power.cpp" "rest_api.cpp"
                            "scheduler.cpp" "sensor_monitor.cpp" "settings.cpp" "status_leds.cpp"
                            "thermostat.cpp" "user_input.cpp"
                       PRIV_REQUIRES spi_flash espressif__cjson spiffs
                       REQUIRES esp_driver_ledc driver esp_lcd nvs_flash esp_timer esp_lcd_gc9a01 esp_now_master bme280 ui eez-framework esp_http_server esp_netif
                       INCLUDE_DIRS "."
)

# The web app in web/ is built with npm on every build and packed into the "www" SPIFFS partition
set(WEB_DIR "${CMAKE_CURRENT_LIST_DIR}/../web")
set(WEB_DIST "${WEB_DIR}/dist")
find_program(NPM_EXECUTABLE npm)

if(NPM_EXECUTABLE)
    add_custom_target(web_app
        COMMAND ${CMAKE_COMMAND} -DNPM=${NPM_EXECUTABLE} -DWEB_DIR=${WEB_DIR}
                -P ${CMAKE_CURRENT_LIST_DIR}/build_web.cmake
        COMMENT "Building the web app"
        VERBATIM)
endif()

if(NPM_EXECUTABLE OR EXISTS "${WEB_DIST}/index.html")
    spiffs_create_partition_image(www "${WEB_DIST}" FLASH_IN_PROJECT)
    if(TARGET web_app)
        add_dependencies(spiffs_www_bin web_app)
    endif()
else()
    message(WARNING "npm not found and web/dist does not exist, the web app will not be flashed.")
endif()

idf_component_register(SRCS "bme280.c" "main.c"
                       PRIV_REQUIRES spi_flash
                       REQUIRES esp_driver_ledc driver esp_lcd nvs_flash esp_timer esp_lcd_gc9a01 ui eez-framework
                       INCLUDE_DIRS "."
)

# Force the compiler to stop treating enum-float math as an error
target_compile_options(${COMPONENT_LIB} PRIVATE 
    $<$<COMPILE_LANGUAGE:CXX>:-Wno-error=enum-float-conversion>
    $<$<COMPILE_LANGUAGE:CXX>:-Wno-enum-float-conversion>
    $<$<COMPILE_LANGUAGE:CXX>:-Wno-error=deprecated-enum-float-conversion>
    $<$<COMPILE_LANGUAGE:CXX>:-Wno-deprecated-enum-float-conversion>
)

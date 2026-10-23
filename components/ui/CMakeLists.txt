idf_component_register(SRCS "images.c" "screens.c" "styles.c" "ui.c"
                       INCLUDE_DIRS "." "${CMAKE_CURRENT_SOURCE_DIR}/../eez-framework/src"
                       REQUIRES lvgl__lvgl espressif__esp_lvgl_port
)

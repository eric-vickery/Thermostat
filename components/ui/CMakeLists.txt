# EEZ Studio (source export mode) writes each bitmap to its own ui_image_<name>.c
# and each custom font to ui_font_<name>.c, and does not register them anywhere.
# Glob them so new bitmaps are picked up without editing this file.
# CONFIGURE_DEPENDS is not allowed in script mode, which ESP-IDF uses to collect
# requirements before the real configure.
if(CMAKE_SCRIPT_MODE_FILE)
    set(UI_GLOB_MODE "")
else()
    set(UI_GLOB_MODE CONFIGURE_DEPENDS)
endif()

file(GLOB UI_IMAGES ${UI_GLOB_MODE} "${CMAKE_CURRENT_SOURCE_DIR}/ui_image_*.c")
file(GLOB UI_FONTS  ${UI_GLOB_MODE} "${CMAKE_CURRENT_SOURCE_DIR}/ui_font_*.c")

idf_component_register(SRCS "images.c" "screens.c" "styles.c" "ui.c" ${UI_IMAGES} ${UI_FONTS}
                       INCLUDE_DIRS "."
                       REQUIRES lvgl__lvgl espressif__esp_lvgl_port eez-framework
)

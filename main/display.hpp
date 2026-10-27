#pragma once

#include <stdint.h>
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch.h"
#include "lvgl.h"

// GC9A01 round LCD, CST816S touch controller and the LVGL/EEZ UI on top of them
class Display {
public:
    void init_panel();
    void init_backlight();
    void init_touch();
    // Must run after init_panel() and init_touch(). mode_button_cb is attached to the UI's mode button.
    void init_lvgl(lv_event_cb_t mode_button_cb);

    // percent is 0-100; a non-zero fade_ms fades to the new level in the background
    void set_backlight(uint32_t percent, uint32_t fade_ms);
    uint32_t backlight_brightness() const { return backlight_brightness_; }

    // Fill the screen with an RGB565 color
    void fill(uint16_t color);

    // Fade the heat-on indicator ring in or out. Caller must hold the LVGL lock.
    static void set_heat_ring(bool enable);

private:
    esp_lcd_panel_handle_t panel_handle_ = NULL;
    esp_lcd_panel_io_handle_t io_handle_ = NULL;
    esp_lcd_touch_handle_t tp_handle_ = NULL;
    uint32_t backlight_brightness_ = 0;
};

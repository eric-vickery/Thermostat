#include "idle_monitor.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_lvgl_port.h"

#include "ui.h"
#include "screens.h"

void IdleMonitor::start(int32_t timeout_ms)
{
    timeout_ms_ = timeout_ms;
    xTaskCreate(task, "idle_timeout", 4096, this, 2, NULL);
}

void IdleMonitor::task(void *arg)
{
    static_cast<IdleMonitor *>(arg)->run();
}

void IdleMonitor::run()
{
    while (true) {
        uint32_t inactive_time = lv_display_get_inactive_time(NULL);

        if (inactive_time >= (uint32_t)timeout_ms_) {
            activate();
        }
        else {
            deactivate();
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void IdleMonitor::deactivate()
{
    if (is_idle_) {
        is_idle_ = false;

        lv_display_trigger_activity(NULL);

        // Bring back the backlight smoothly (e.g., over 300ms)
        display_.set_backlight(brightness_before_idle_, 300);

        // Load the main operational screen back
        if (lvgl_port_lock(0)) {
            eez_flow_set_screen(SCREEN_ID_MAIN, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0);
            lvgl_port_unlock();
        }
    }
}

void IdleMonitor::activate()
{
    if (!is_idle_) {
        is_idle_ = true;

        // Load the empty/minimalistic low-power screen
        if (lvgl_port_lock(0)) {
            eez_flow_set_screen(SCREEN_ID_IDLE, LV_SCR_LOAD_ANIM_FADE_IN, 500, 0);
            lvgl_port_unlock();
        }

        // Fade out backlight completely over 500ms (or dim down to 5-10% duty)
        brightness_before_idle_ = display_.backlight_brightness();
        display_.set_backlight(3, 500);
    }
}

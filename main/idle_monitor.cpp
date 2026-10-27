#include "idle_monitor.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_lvgl_port.h"

#include "ui.h"
#include "screens.h"

void IdleMonitor::start(int32_t timeout_ms, int32_t active_brightness, int32_t idle_brightness)
{
    timeout_ms_ = timeout_ms;
    active_brightness_ = active_brightness;
    idle_brightness_ = idle_brightness;
    xTaskCreate(task, "idle_timeout", 4096, this, 2, NULL);
}

void IdleMonitor::set_active_brightness(int32_t percent)
{
    active_brightness_ = percent;
    if (!is_idle_) {
        display_.set_backlight(percent, 300);
    }
}

void IdleMonitor::set_idle_brightness(int32_t percent)
{
    idle_brightness_ = percent;
    if (is_idle_) {
        display_.set_backlight(percent, 300);
    }
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
        display_.set_backlight(active_brightness_, 300);

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

        // Dim the backlight to the idle level over 500ms
        display_.set_backlight(idle_brightness_, 500);
    }
}

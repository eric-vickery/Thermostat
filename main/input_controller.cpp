#include "input_controller.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"

static const char *TAG = "Thermostat";

void InputController::start()
{
    xTaskCreatePinnedToCore(task, "input_task", 4096, this, 5, NULL, 1);
}

void InputController::task(void *arg)
{
    static_cast<InputController *>(arg)->run();
}

void InputController::run()
{
    UserInput::Event ev;

    while (true) {
        if (input_.wait(ev)) {
            // First event while in idle mode just deactivates idle mode
            if (idle_.is_idle()) {
                idle_.deactivate();
                continue;
            }
            // Even though these events are not touch screen activity they are activity that should reset the idle timer
            lv_display_trigger_activity(NULL);

            switch (ev) {
            case UserInput::ENC_CW:
                // Increase the setpoint
                if (lvgl_port_lock(0)) {
                    thermostat_.set_requested_setpoint(thermostat_.requested_setpoint() + 1);
                    lvgl_port_unlock();
                }
                ESP_LOGI(TAG, "Encoder CW, setpoint %d%%", (int)thermostat_.requested_setpoint());
                break;

            case UserInput::ENC_CCW:
                // Decrease the setpoint
                if (lvgl_port_lock(0)) {
                    thermostat_.set_requested_setpoint(thermostat_.requested_setpoint() - 1);
                    lvgl_port_unlock();
                }
                ESP_LOGI(TAG, "Encoder CCW, setpoint %d%%", (int)thermostat_.requested_setpoint());
                break;

            case UserInput::BTN_PRESS:
                ESP_LOGI(TAG, "Button pressed");
                break;
            }
        }
    }
}

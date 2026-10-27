#include "esp_log.h"
#include "esp_lvgl_port.h"

#include "connectivity.hpp"
#include "display.hpp"
#include "idle_monitor.hpp"
#include "input_controller.hpp"
#include "power.hpp"
#include "scheduler.hpp"
#include "sensor_monitor.hpp"
#include "settings.hpp"
#include "status_leds.hpp"
#include "thermostat.hpp"
#include "user_input.hpp"

static const char *TAG = "Thermostat";

static Power power;
static StatusLeds status_leds;
static Display display;
static IdleMonitor idle_monitor(display);
static UserInput user_input;
static Connectivity connectivity;

static void mode_button_event_handler(lv_event_t *event)
{
    // Check if the button was clicked (short press and release)
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
        Thermostat::instance().toggle_mode();
    }
}

extern "C" void app_main(void)
{
    Thermostat &thermostat = Thermostat::instance();
    static Scheduler scheduler(thermostat);
    static SensorMonitor sensors(thermostat);
    static InputController input_controller(user_input, idle_monitor, thermostat);

    Settings &settings = Settings::instance();
    settings.init();
    thermostat.load();
    scheduler.init();

    thermostat.set_heater_changed_callback([](bool on) {
        if (lvgl_port_lock(0)) {
            Display::set_heat_ring(on);
            lvgl_port_unlock();
        }
        connectivity.notify_heater_changed();
    });

    ESP_LOGI(TAG, "Switch & Encoder Init");
    user_input.init();

    ESP_LOGI(TAG, "Power Init");
    power.init();
    status_leds.init();

    ESP_LOGI(TAG, "Backlight Init");
    display.init_backlight();
    display.set_backlight(settings.default_brightness(), 0);

    ESP_LOGI(TAG, "Display Init");
    display.init_panel();

    ESP_LOGI(TAG, "Touch Init");
    display.init_touch();

    ESP_LOGI(TAG, "BME280 Init");
    ESP_ERROR_CHECK_WITHOUT_ABORT(sensors.init());

    display.init_lvgl(mode_button_event_handler);
    lv_display_trigger_activity(NULL);

    input_controller.start();
    idle_monitor.start(settings.inactivity_timeout_ms());
    thermostat.start();
    scheduler.start();
    sensors.start();
    connectivity.start();
}

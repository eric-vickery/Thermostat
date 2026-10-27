#include "thermostat.hpp"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "settings.hpp"
#include "vars.h"

static const char *TAG = "Thermostat";

#define LOCK_IN_DELAY_MS   2500

Thermostat &Thermostat::instance()
{
    static Thermostat thermostat;
    return thermostat;
}

void Thermostat::load()
{
    Settings &settings = Settings::instance();

    state_ = STATE_OFF;
    requested_mode_ = (Mode)settings.mode();
    apply_mode(requested_mode_);
    set_mode_value(requested_mode_);
    temperature_hysteresis_ = settings.temperature_hysteresis();
    requested_setpoint_ = settings.setpoint();
    current_temp_ = 87; /* replaced by the sensor task once it starts */
}

void Thermostat::start()
{
    xTaskCreate(monitor_setpoint_change_task, "setpoint_change", 4096, this, 2, NULL);
    xTaskCreate(monitor_mode_change_task, "mode_change", 4096, this, 2, NULL);
    xTaskCreate(monitor_state_task, "monitor_state", 4096, this, 2, NULL);
}

void Thermostat::set_requested_setpoint(int32_t value)
{
    if (value >= SETPOINT_MIN && value <= SETPOINT_MAX) {
        requested_setpoint_ = value;
        setpoint_changed_time_ = esp_timer_get_time()/1000;
    }
}

void Thermostat::set_temperature_hysteresis(int32_t value)
{
    temperature_hysteresis_ = value;
    Settings::instance().set_temperature_hysteresis(value);
}

void Thermostat::set_pressure_string(const char *value)
{
    strncpy(pressure_string_, value, sizeof(pressure_string_) / sizeof(char));
    pressure_string_[sizeof(pressure_string_) / sizeof(char) - 1] = 0;
}

void Thermostat::toggle_mode()
{
    // Step from the requested mode, not the applied one, so every tap advances the UI
    // even while an earlier request is still waiting to lock in
    switch (requested_mode_)
    {
    case MODE_OFF:
        request_new_mode(MODE_MANUAL);
        break;

    case MODE_MANUAL:
        request_new_mode(MODE_SCHEDULE);
        break;

    case MODE_SCHEDULE:
        request_new_mode(MODE_OFF);
        break;

    default:
        break;
    }
}

void Thermostat::request_new_mode(Mode new_mode)
{
    mode_changed_time_ = esp_timer_get_time()/1000;

    switch(new_mode) {
        case MODE_MANUAL:
        case MODE_SCHEDULE:
        case MODE_OFF:
            requested_mode_ = new_mode;
            set_mode_value(new_mode);
            break;
    }
}

void Thermostat::apply_mode(Mode new_mode)
{
    switch(new_mode) {
        case MODE_MANUAL:
        case MODE_SCHEDULE:
        case MODE_OFF:
            current_mode_ = new_mode;
            break;
    }
    ESP_LOGI(TAG, "Mode applied: %d", (int)current_mode_);
    Settings::instance().set_mode(current_mode_);
}

void Thermostat::monitor_state_task(void *arg)
{
    static_cast<Thermostat *>(arg)->monitor_state();
}

void Thermostat::monitor_mode_change_task(void *arg)
{
    static_cast<Thermostat *>(arg)->monitor_mode_change();
}

void Thermostat::monitor_setpoint_change_task(void *arg)
{
    static_cast<Thermostat *>(arg)->monitor_setpoint_change();
}

void Thermostat::monitor_state()
{
    while(true) {
        if (state_ == STATE_ON) {
            // See if we should turn off
            if (((setpoint_ + temperature_hysteresis_) <= current_temp_) || (current_mode_ == MODE_OFF)) {
                state_ = STATE_OFF;
                ESP_LOGI(TAG, "State -> OFF (temp %d, setpoint %d, hysteresis %d, mode %d)",
                         (int)current_temp_, (int)setpoint_, (int)temperature_hysteresis_, (int)current_mode_);
                if (heater_changed_cb_) {
                    heater_changed_cb_(false);
                }
            }
        }
        else { // Must be in STATE_OFF
            // See if we should turn on
            if (((current_temp_ + temperature_hysteresis_) <= setpoint_) && (current_mode_ != MODE_OFF)) {
                state_ = STATE_ON;
                ESP_LOGI(TAG, "State -> ON (temp %d, setpoint %d, hysteresis %d, mode %d)",
                         (int)current_temp_, (int)setpoint_, (int)temperature_hysteresis_, (int)current_mode_);
                if (heater_changed_cb_) {
                    heater_changed_cb_(true);
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(400));
    }
}

void Thermostat::monitor_mode_change()
{
    while(true) {
        if (((esp_timer_get_time()/1000) - mode_changed_time_) > LOCK_IN_DELAY_MS) {
            Mode target = requested_mode_;
            if (current_mode_ != target) {
                apply_mode(target);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void Thermostat::monitor_setpoint_change()
{
    while(true) {
        if (((esp_timer_get_time()/1000) - setpoint_changed_time_) > LOCK_IN_DELAY_MS) {
            if (setpoint_ != requested_setpoint_) {
                setpoint_ = requested_setpoint_;
                Settings::instance().set_setpoint(setpoint_);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/* ------------------------------------------------------------------------
 * EEZ Studio native variables
 * ------------------------------------------------------------------------ */
int32_t get_var_setpoint() {
    return Thermostat::instance().requested_setpoint();
}

void set_var_setpoint(int32_t value) {
    Thermostat::instance().set_requested_setpoint(value);
}

int32_t get_var_current_temp() {
    return Thermostat::instance().current_temp();
}

void set_var_current_temp(int32_t value) {
    Thermostat::instance().set_current_temp(value);
}

int32_t get_var_mode() {
    return Thermostat::instance().mode_value();
}

void set_var_mode(int32_t value) {
    Thermostat::instance().set_mode_value(value);
}

int32_t get_var_humidity() {
    return Thermostat::instance().humidity();
}

void set_var_humidity(int32_t value) {
    Thermostat::instance().set_humidity(value);
}

const char *get_var_pressure_string() {
    return Thermostat::instance().pressure_string();
}

void set_var_pressure_string(const char *value) {
    Thermostat::instance().set_pressure_string(value);
}

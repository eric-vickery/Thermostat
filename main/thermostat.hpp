#pragma once

#include <stdint.h>
#include <functional>

// Thermostat control logic and the state the UI binds to (via the EEZ get_var_/set_var_ functions)
class Thermostat {
public:
    enum State {
        STATE_OFF,
        STATE_ON,
    };

    enum Mode {
        MODE_OFF,
        MODE_MANUAL,
        MODE_SCHEDULE,
    };

    static constexpr int32_t SETPOINT_MIN = 50;
    static constexpr int32_t SETPOINT_MAX = 90;

    static Thermostat &instance();

    // Loads the persisted values and applies the startup mode
    void load();
    // Starts the tasks that lock in changes and drive the heater state
    void start();

    // Called from the heater state task whenever the heater turns on (true) or off (false)
    void set_heater_changed_callback(std::function<void(bool)> cb) { heater_changed_cb_ = cb; }

    bool is_heater_on() const { return state_ == STATE_ON; }

    // The setpoint takes effect after it has stopped changing for a short delay
    int32_t requested_setpoint() const { return requested_setpoint_; }
    void set_requested_setpoint(int32_t value);

    // The mode takes effect after it has stopped changing for a short delay
    // The reason there is a request_new_mode and an apply_mode is to keep the heater from changing
    // modes while the user is toggling through the modes in the ui
    void request_new_mode(Mode new_mode);
    void toggle_mode();
    // The mode that has been applied (as opposed to the one the ui is still toggling through)
    Mode current_mode() const { return current_mode_; }
    int32_t mode_value() const { return mode_value_; }
    void set_mode_value(int32_t value) { mode_value_ = value; }

    int32_t current_temp() const { return current_temp_; }
    void set_current_temp(int32_t value) { current_temp_ = value; }
    int32_t humidity() const { return humidity_; }
    void set_humidity(int32_t value) { humidity_ = value; }
    const char *pressure_string() const { return pressure_string_; }
    void set_pressure_string(const char *value);

private:
    Thermostat() = default;

    void apply_mode(Mode new_mode);

    static void monitor_state_task(void *arg);
    static void monitor_mode_change_task(void *arg);
    static void monitor_setpoint_change_task(void *arg);
    void monitor_state();
    void monitor_mode_change();
    void monitor_setpoint_change();

    std::function<void(bool)> heater_changed_cb_;

    State state_ = STATE_OFF;
    Mode requested_mode_ = MODE_MANUAL;
    Mode current_mode_ = MODE_MANUAL;
    int32_t temperature_hysteresis_ = 0;
    uint32_t setpoint_changed_time_ = 0;
    uint32_t mode_changed_time_ = 0;
    // This is the setpoint that the thermostat will control the heater from
    // It will be set by a monitor task when the requested_setpoint has stopped changing for LOCK_IN_DELAY_MS
    int32_t setpoint_ = 0;
    int32_t requested_setpoint_ = 0;
    int32_t current_temp_ = 0;
    int32_t mode_value_ = 0;
    int32_t humidity_ = 0;
    char pressure_string_[16] = { 0 };
};

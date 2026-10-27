#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "nvs.h"

// Persistent thermostat settings stored in the "storage" NVS namespace.
// Getters return the stored value, or the default if nothing has been stored yet.
// Setters write through to NVS immediately.
class Settings {
public:
    static Settings &instance();

    // Initializes the NVS flash partition (erasing it if needed) and opens the namespace
    void init();

    // Thermostat mode (Thermostat::Mode)
    int32_t mode();
    void set_mode(int32_t value);

    // Setpoint in degrees Fahrenheit
    int32_t setpoint();
    void set_setpoint(int32_t value);

    // Degrees Fahrenheit of slack before the heater turns on or off
    int32_t temperature_hysteresis();
    void set_temperature_hysteresis(int32_t value);

    // Time without activity before the idle screen takes over
    int32_t inactivity_timeout_ms();
    void set_inactivity_timeout_ms(int32_t value);

    // Backlight brightness in percent while the screen is in use
    int32_t active_brightness();
    void set_active_brightness(int32_t value);

    // Backlight brightness in percent while the idle screen is showing
    int32_t idle_brightness();
    void set_idle_brightness(int32_t value);

private:
    Settings() = default;

    void store_int32(const char *key, int32_t value);
    int32_t read_int32_with_default(const char *key, int32_t default_value);

    nvs_handle_t handle_ = 0;
};

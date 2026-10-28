#pragma once

// Optional local heater control through a relay on a GPIO. Does nothing unless
// THERMOSTAT_RELAY is defined in thermostat_config.h.
class HeaterRelay {
public:
    // Configures the pin with the relay off
    void init();
    // Called whenever the thermostat turns the heater on (true) or off (false)
    void set(bool on);
};

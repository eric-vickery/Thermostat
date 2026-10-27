#pragma once

// WiFi provisioning and the ESP-NOW heater state link. Does nothing unless
// THERMOSTAT_WIFI (and THERMOSTAT_ESPNOW) are enabled in thermostat_config.h.
class Connectivity {
public:
    void start();
    // Tell the remote the heater state changed
    void notify_heater_changed();
};

#pragma once

// Erase the saved WiFi credentials at every boot so provisioning starts over
// (thermostat settings are left alone). Flash once, then comment out again.
// #define THERMOSTAT_WIFI_RESET 1

// Switch a heater relay from a GPIO. The default pin is RXD0 (GPIO44) on the UART port: it is an
// input during boot, so the line idles high and an active-low relay stays off until the app
// starts. The port's level shifter makes the pin an open-drain style output pulled up to 5V,
// which suits active-low opto-isolated relay boards. Active-high boards need a transistor stage.
// This takes over the UART0 RX pin, so the serial console no longer accepts input.
// #define THERMOSTAT_RELAY 1
#define THERMOSTAT_RELAY_GPIO 44
// 1 if the relay turns on when the pin is pulled low (most opto-isolated relay boards)
#define THERMOSTAT_RELAY_ACTIVE_LOW 1

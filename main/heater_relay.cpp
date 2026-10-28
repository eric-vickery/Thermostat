#include "heater_relay.hpp"

#include "thermostat_config.h"

#if THERMOSTAT_RELAY
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "HeaterRelay";

#define RELAY_GPIO  ((gpio_num_t)THERMOSTAT_RELAY_GPIO)

#if THERMOSTAT_RELAY_ACTIVE_LOW
#define RELAY_ON_LEVEL   0
#define RELAY_OFF_LEVEL  1
#else
#define RELAY_ON_LEVEL   1
#define RELAY_OFF_LEVEL  0
#endif

void HeaterRelay::init()
{
    // Load the off level before switching the pin to an output so it never glitches on.
    // No internal pull-up: the board's level shifter already pulls the line up to 5V.
    gpio_reset_pin(RELAY_GPIO);
    gpio_set_level(RELAY_GPIO, RELAY_OFF_LEVEL);
    ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_set_direction(RELAY_GPIO, GPIO_MODE_OUTPUT));
    ESP_LOGI(TAG, "Relay on GPIO %d, active %s, off", (int)THERMOSTAT_RELAY_GPIO,
             THERMOSTAT_RELAY_ACTIVE_LOW ? "low" : "high");
}

void HeaterRelay::set(bool on)
{
    gpio_set_level(RELAY_GPIO, on ? RELAY_ON_LEVEL : RELAY_OFF_LEVEL);
    ESP_LOGI(TAG, "Relay %s", on ? "ON" : "OFF");
}
#else
void HeaterRelay::init() {}
void HeaterRelay::set(bool on) { (void)on; }
#endif

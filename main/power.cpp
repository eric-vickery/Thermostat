#include "power.hpp"

#include "driver/gpio.h"
#include "esp_err.h"

#define PIN_POWER_LED   GPIO_NUM_40

void Power::init()
{
    // Turn on both rails
    gpio_config_t io_pin_1 = {};
    io_pin_1.pin_bit_mask = 1ULL << GPIO_NUM_1;
    io_pin_1.mode = GPIO_MODE_OUTPUT;
    ESP_ERROR_CHECK(gpio_config(&io_pin_1));
    gpio_set_level(GPIO_NUM_1, 1);
    gpio_config_t io_pin_2 = {};
    io_pin_2.pin_bit_mask = 1ULL << GPIO_NUM_2;
    io_pin_2.mode = GPIO_MODE_OUTPUT;
    ESP_ERROR_CHECK(gpio_config(&io_pin_2));
    gpio_set_level(GPIO_NUM_2, 1);

    gpio_config_t io_led = {};
    io_led.pin_bit_mask = 1ULL << PIN_POWER_LED;
    io_led.mode = GPIO_MODE_OUTPUT;
    ESP_ERROR_CHECK(gpio_config(&io_led));
    gpio_set_level(PIN_POWER_LED, 0);
}

#include "status_leds.hpp"

#include "esp_log.h"
#include "driver/gpio.h"

#define PIXEL_COUNT     5
#define NEOPIXEL_PIN    GPIO_NUM_48

static const char *TAG = "StatusLeds";

bool StatusLeds::init()
{
    neopixel_ = neopixel_Initialize(PIXEL_COUNT, NEOPIXEL_PIN, NEOPIXEL_MODE_WS2812B);
    if (NULL == neopixel_)
    {
        ESP_LOGE(TAG, "[%s] Initialization failed\n", __func__);
        return false;
    }
    // Clear the pixels
    tNeopixel pixel[] = {{0, 0x000000},
                        {1, 0x000000},
                        {2, 0x000000},
                        {3, 0x000000},
                        {4, 0x000000}};
    neopixel_SetPixel(neopixel_, pixel, 5);

    return true;
}

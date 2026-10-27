#pragma once

#include "neopixel.h"

// NeoPixel strip used for status indication
class StatusLeds {
public:
    bool init();

private:
    tNeopixelContext neopixel_ = nullptr;
};

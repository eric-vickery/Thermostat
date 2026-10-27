#pragma once

#include "freertos/FreeRTOS.h"

// Rotary encoder and push switch. The ISRs post events onto a queue.
class UserInput {
public:
    enum Event {
        ENC_CW,
        ENC_CCW,
        BTN_PRESS,
    };

    // Creates the event queue and configures the encoder and switch GPIOs
    void init();
    // Waits for the next event; returns false on timeout
    bool wait(Event &ev, TickType_t ticks = portMAX_DELAY);
};

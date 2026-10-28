#pragma once

#include <stdint.h>
#include "display.hpp"

// Swaps to the idle screen and dims the backlight after a period without user activity
class IdleMonitor {
public:
    explicit IdleMonitor(Display &display) : display_(display) {}

    // Brightness values are backlight percents. A timeout of 0 disables the idle screen.
    void start(int32_t timeout_ms, int32_t active_brightness, int32_t idle_brightness);

    // Takes effect immediately. 0 disables the idle screen.
    void set_timeout_ms(int32_t timeout_ms) { timeout_ms_ = timeout_ms; }
    // Backlight level while the screen is in use. Applied now unless the idle screen is showing.
    void set_active_brightness(int32_t percent);
    // Backlight level while the idle screen is showing. Applied now if it is showing.
    void set_idle_brightness(int32_t percent);

    bool is_idle() const { return is_idle_; }
    void activate();
    void deactivate();

private:
    static void task(void *arg);
    void run();

    Display &display_;
    int32_t timeout_ms_ = 0;
    bool is_idle_ = false;
    uint32_t active_brightness_ = 100;
    uint32_t idle_brightness_ = 3;
};

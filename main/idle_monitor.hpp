#pragma once

#include <stdint.h>
#include "display.hpp"

// Swaps to the idle screen and dims the backlight after a period without user activity
class IdleMonitor {
public:
    explicit IdleMonitor(Display &display) : display_(display) {}

    void start(int32_t timeout_ms);

    bool is_idle() const { return is_idle_; }
    void activate();
    void deactivate();

private:
    static void task(void *arg);
    void run();

    Display &display_;
    int32_t timeout_ms_ = 0;
    bool is_idle_ = false;
    uint32_t brightness_before_idle_ = 0;
};

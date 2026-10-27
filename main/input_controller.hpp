#pragma once

#include "idle_monitor.hpp"
#include "thermostat.hpp"
#include "user_input.hpp"

// Turns encoder and button events into thermostat actions
class InputController {
public:
    InputController(UserInput &input, IdleMonitor &idle, Thermostat &thermostat)
        : input_(input), idle_(idle), thermostat_(thermostat) {}

    void start();

private:
    static void task(void *arg);
    void run();

    UserInput &input_;
    IdleMonitor &idle_;
    Thermostat &thermostat_;
};

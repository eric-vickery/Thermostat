#pragma once

#include "bme280.hpp"
#include "thermostat.hpp"

// Periodically reads the BME280 and publishes temperature, humidity and pressure to the thermostat
class SensorMonitor {
public:
    explicit SensorMonitor(Thermostat &thermostat);

    esp_err_t init();
    void start();

private:
    static void task(void *arg);
    void run();

    Thermostat &thermostat_;
    BME280 bme280_;
};

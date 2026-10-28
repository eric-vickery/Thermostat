#pragma once

#include "bme280.hpp"
#include "thermostat.hpp"

// Periodically reads the BME280 and publishes temperature, humidity and pressure to the thermostat
class SensorMonitor {
public:
    explicit SensorMonitor(Thermostat &thermostat);

    esp_err_t init();
    void start();

    // Elevation of the thermostat. The pressure published to the thermostat is corrected to sea level
    // with it, so it matches weather reports. Takes effect on the next reading.
    void set_altitude_ft(int32_t altitude_ft) { altitude_ft_ = altitude_ft; }

private:
    static void task(void *arg);
    void run();

    Thermostat &thermostat_;
    BME280 bme280_;
    volatile int32_t altitude_ft_ = 0;
};

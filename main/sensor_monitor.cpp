#include "sensor_monitor.hpp"

#include <math.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "Sensors";

#define PIN_BME_SDA     GPIO_NUM_38
#define PIN_BME_SCL     GPIO_NUM_39

SensorMonitor::SensorMonitor(Thermostat &thermostat)
    : thermostat_(thermostat), bme280_(I2C_NUM_1, PIN_BME_SDA, PIN_BME_SCL)
{
}

esp_err_t SensorMonitor::init()
{
    return bme280_.init();
}

void SensorMonitor::start()
{
    xTaskCreate(task, "sensors", 4096, this, 2, NULL);
}

void SensorMonitor::task(void *arg)
{
    static_cast<SensorMonitor *>(arg)->run();
}

// International barometric formula: p0 = p / (1 - h / 145366.45 ft) ^ 5.255
static float sea_level_inhg(float station_inhg, int32_t altitude_ft)
{
    return station_inhg / powf(1.0f - (float)altitude_ft / 145366.45f, 5.255f);
}

void SensorMonitor::run()
{
    BME280::Data data;
    while (true) {
        if (bme280_.read(data) == ESP_OK) {
            float pressure_inhg = sea_level_inhg(data.pressure_inhg, altitude_ft_);
            thermostat_.set_current_temp(data.temperature_f);
            thermostat_.set_humidity((int32_t)data.humidity_pct);
            char pressure_text[16];
            snprintf(pressure_text, sizeof(pressure_text), "%.2f", pressure_inhg);
            thermostat_.set_pressure_string(pressure_text);
            ESP_LOGI(TAG, "BME280: %d°F  %lu%%RH  %.2f inHg (sea level, %.2f at the sensor)",
                     (int)data.temperature_f,
                     (unsigned long)data.humidity_pct,
                     pressure_inhg,
                     data.pressure_inhg);
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

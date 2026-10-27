#include "sensor_monitor.hpp"

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

void SensorMonitor::run()
{
    BME280::Data data;
    while (true) {
        if (bme280_.read(data) == ESP_OK) {
            thermostat_.set_current_temp(data.temperature_f);
            thermostat_.set_humidity((int32_t)data.humidity_pct);
            char pressure_text[16];
            snprintf(pressure_text, sizeof(pressure_text), "%.2f", data.pressure_inhg);
            thermostat_.set_pressure_string(pressure_text);
            ESP_LOGI(TAG, "BME280: %d°F  %lu%%RH  %.2f inHg",
                     (int)data.temperature_f,
                     (unsigned long)data.humidity_pct,
                     data.pressure_inhg);
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

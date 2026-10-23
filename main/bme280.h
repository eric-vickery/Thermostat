#pragma once

#include <stdint.h>
#include "esp_err.h"

typedef struct {
    int32_t  temperature_f;  // whole degrees Fahrenheit
    uint32_t humidity_pct;   // relative humidity, 0-100 %
    uint32_t pressure_pa;    // absolute pressure in Pascals
} bme280_data_t;

esp_err_t bme280_init(void);
esp_err_t bme280_read(bme280_data_t *out);

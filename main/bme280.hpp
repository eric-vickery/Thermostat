#pragma once

#include <stddef.h>
#include <stdint.h>
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_err.h"

class BME280 {
public:
    struct Data {
        int32_t  temperature_f;  // whole degrees Fahrenheit
        uint32_t humidity_pct;   // relative humidity, 0-100 %
        float    pressure_inhg;  // absolute pressure in inches of mercury
    };

    // addr is 0x76, or 0x77 if SDO is pulled high
    BME280(i2c_port_num_t port, gpio_num_t sda, gpio_num_t scl,
           uint8_t addr = 0x76, uint32_t speed_hz = 100000);
    ~BME280();

    BME280(const BME280 &) = delete;
    BME280 &operator=(const BME280 &) = delete;

    esp_err_t init();
    esp_err_t read(Data &out);

private:
    struct Calib {
        uint16_t T1;
        int16_t  T2, T3;
        uint16_t P1;
        int16_t  P2, P3, P4, P5, P6, P7, P8, P9;
        uint8_t  H1, H3;
        int16_t  H2, H4, H5;
        int8_t   H6;
    };

    esp_err_t write_reg(uint8_t reg, uint8_t val);
    esp_err_t read_regs(uint8_t reg, uint8_t *out, size_t len);
    esp_err_t load_calibration();

    int32_t  compensate_temperature(int32_t adc_T, int32_t &t_fine) const;
    uint32_t compensate_pressure(int32_t adc_P, int32_t t_fine) const;
    uint32_t compensate_humidity(int32_t adc_H, int32_t t_fine) const;

    i2c_port_num_t port_;
    gpio_num_t     sda_;
    gpio_num_t     scl_;
    uint8_t        addr_;
    uint32_t       speed_hz_;

    i2c_master_bus_handle_t bus_ = nullptr;
    i2c_master_dev_handle_t dev_ = nullptr;
    Calib                   calib_ = {};
};

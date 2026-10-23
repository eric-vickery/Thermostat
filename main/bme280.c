#include "bme280.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "esp_log.h"

static const char *TAG = "BME280";

/* ----------------------------------------------------------------
 * Hardware configuration — adjust to match your board
 * ---------------------------------------------------------------- */
#define BME280_I2C_NUM         I2C_NUM_1
#define BME280_PIN_SDA         GPIO_NUM_38
#define BME280_PIN_SCL         GPIO_NUM_39
#define BME280_I2C_ADDR        0x76    /* use 0x77 if SDO is pulled high */
#define BME280_I2C_SPEED_HZ    100000

/* Register map */
#define REG_ID          0xD0
#define REG_RESET       0xE0
#define REG_CTRL_HUM    0xF2
#define REG_CTRL_MEAS   0xF4
#define REG_CONFIG      0xF5
#define REG_DATA        0xF7   /* 0xF7..0xFE: press(3), temp(3), hum(2) */
#define REG_CALIB_TP    0x88   /* 0x88..0x9F: temp + pressure calib (24 bytes) */
#define REG_CALIB_H1    0xA1   /* 1 byte */
#define REG_CALIB_H2    0xE1   /* 0xE1..0xE7: humidity calib (7 bytes) */

/* Calibration coefficients */
typedef struct {
    uint16_t T1;
    int16_t  T2, T3;
    uint16_t P1;
    int16_t  P2, P3, P4, P5, P6, P7, P8, P9;
    uint8_t  H1, H3;
    int16_t  H2, H4, H5;
    int8_t   H6;
} bme280_calib_t;

static i2c_master_bus_handle_t s_bus;
static i2c_master_dev_handle_t s_dev;
static bme280_calib_t          s_calib;

/* ----------------------------------------------------------------
 * Low-level I2C helpers
 * ---------------------------------------------------------------- */
static esp_err_t write_reg(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { reg, val };
    return i2c_master_transmit(s_dev, buf, 2, 100);
}

static esp_err_t read_regs(uint8_t reg, uint8_t *out, size_t len)
{
    return i2c_master_transmit_receive(s_dev, &reg, 1, out, len, 100);
}

/* ----------------------------------------------------------------
 * Calibration loading
 * ---------------------------------------------------------------- */
static esp_err_t load_calibration(void)
{
    uint8_t tp[24];
    esp_err_t err = read_regs(REG_CALIB_TP, tp, sizeof(tp));
    if (err != ESP_OK) return err;

    s_calib.T1 = (uint16_t)(tp[1]  << 8 | tp[0]);
    s_calib.T2 =  (int16_t)(tp[3]  << 8 | tp[2]);
    s_calib.T3 =  (int16_t)(tp[5]  << 8 | tp[4]);
    s_calib.P1 = (uint16_t)(tp[7]  << 8 | tp[6]);
    s_calib.P2 =  (int16_t)(tp[9]  << 8 | tp[8]);
    s_calib.P3 =  (int16_t)(tp[11] << 8 | tp[10]);
    s_calib.P4 =  (int16_t)(tp[13] << 8 | tp[12]);
    s_calib.P5 =  (int16_t)(tp[15] << 8 | tp[14]);
    s_calib.P6 =  (int16_t)(tp[17] << 8 | tp[16]);
    s_calib.P7 =  (int16_t)(tp[19] << 8 | tp[18]);
    s_calib.P8 =  (int16_t)(tp[21] << 8 | tp[20]);
    s_calib.P9 =  (int16_t)(tp[23] << 8 | tp[22]);

    err = read_regs(REG_CALIB_H1, &s_calib.H1, 1);
    if (err != ESP_OK) return err;

    uint8_t h[7];
    err = read_regs(REG_CALIB_H2, h, sizeof(h));
    if (err != ESP_OK) return err;

    s_calib.H2 =  (int16_t)(h[1] << 8 | h[0]);
    s_calib.H3 = h[2];
    s_calib.H4 =  (int16_t)(((int8_t)h[3] << 4) | (h[4] & 0x0F));
    s_calib.H5 =  (int16_t)(((int8_t)h[5] << 4) | (h[4] >> 4));
    s_calib.H6 =  (int8_t)h[6];

    return ESP_OK;
}

/* ----------------------------------------------------------------
 * Bosch compensation formulas (datasheet §4.2.3)
 * ---------------------------------------------------------------- */

/* Returns temperature in 0.01 °C units and sets t_fine for pressure/humidity */
static int32_t compensate_temperature(int32_t adc_T, int32_t *t_fine)
{
    int32_t var1 = ((adc_T >> 3) - ((int32_t)s_calib.T1 << 1));
    var1 = (var1 * (int32_t)s_calib.T2) >> 11;
    int32_t var2 = (adc_T >> 4) - (int32_t)s_calib.T1;
    var2 = (((var2 * var2) >> 12) * (int32_t)s_calib.T3) >> 14;
    *t_fine = var1 + var2;
    return (*t_fine * 5 + 128) >> 8;
}

/* Returns pressure in Pascals */
static uint32_t compensate_pressure(int32_t adc_P, int32_t t_fine)
{
    int64_t var1 = (int64_t)t_fine - 128000;
    int64_t var2 = var1 * var1 * (int64_t)s_calib.P6;
    var2 += (var1 * (int64_t)s_calib.P5) << 17;
    var2 += (int64_t)s_calib.P4 << 35;
    var1 = ((var1 * var1 * (int64_t)s_calib.P3) >> 8) + ((var1 * (int64_t)s_calib.P2) << 12);
    var1 = ((((int64_t)1 << 47) + var1) * (int64_t)s_calib.P1) >> 33;
    if (var1 == 0) return 0;
    int64_t p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = ((int64_t)s_calib.P9 * (p >> 13) * (p >> 13)) >> 25;
    var2 = ((int64_t)s_calib.P8 * p) >> 19;
    p = ((p + var1 + var2) >> 8) + ((int64_t)s_calib.P7 << 4);
    return (uint32_t)(p >> 8);
}

/* Returns relative humidity in 1/1024 % units; divide by 1024 for whole % */
static uint32_t compensate_humidity(int32_t adc_H, int32_t t_fine)
{
    int32_t v = t_fine - 76800;
    v = (((adc_H << 14) - ((int32_t)s_calib.H4 << 20) - ((int32_t)s_calib.H5 * v)) + 16384) >> 15;
    v = v * (((((((v * (int32_t)s_calib.H6) >> 10) *
                 (((v * (int32_t)s_calib.H3) >> 11) + 32768)) >> 10) + 2097152) *
               (int32_t)s_calib.H2 + 8192) >> 14);
    v -= (((((v >> 15) * (v >> 15)) >> 7) * (int32_t)s_calib.H1) >> 4);
    if (v < 0) v = 0;
    if (v > 419430400) v = 419430400;
    return (uint32_t)(v >> 12);  /* Q22.10: divide by 1024 for % */
}

/* ----------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------- */
esp_err_t bme280_init(void)
{
    i2c_master_bus_config_t bus_cfg = {
        .clk_source                   = I2C_CLK_SRC_DEFAULT,
        .i2c_port                     = BME280_I2C_NUM,
        .scl_io_num                   = BME280_PIN_SCL,
        .sda_io_num                   = BME280_PIN_SDA,
        .glitch_ignore_cnt            = 7,
        .flags.enable_internal_pullup = true,
    };
    esp_err_t err = i2c_new_master_bus(&bus_cfg, &s_bus);
    if (err != ESP_OK) return err;

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = BME280_I2C_ADDR,
        .scl_speed_hz    = BME280_I2C_SPEED_HZ,
    };
    err = i2c_master_bus_add_device(s_bus, &dev_cfg, &s_dev);
    if (err != ESP_OK) return err;

    uint8_t chip_id = 0;
    read_regs(REG_ID, &chip_id, 1);
    if (chip_id != 0x60) {
        ESP_LOGE(TAG, "chip_id=0x%02X (expected 0x60) — check wiring/address", chip_id);
        return ESP_ERR_NOT_FOUND;
    }

    /* Soft reset and wait for sensor to come back */
    write_reg(REG_RESET, 0xB6);
    vTaskDelay(pdMS_TO_TICKS(10));

    err = load_calibration();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read calibration: %s", esp_err_to_name(err));
        return err;
    }

    /* ctrl_hum must be written before ctrl_meas for humidity config to take effect */
    write_reg(REG_CTRL_HUM,  0x01);  /* humidity oversampling x1 */
    write_reg(REG_CTRL_MEAS, 0x27);  /* temp x1, pressure x1, normal mode */
    write_reg(REG_CONFIG,    0xA0);  /* 1000 ms standby, filter off */

    ESP_LOGI(TAG, "initialized — T1=%u T2=%d T3=%d P1=%u H1=%u H2=%d",
             s_calib.T1, s_calib.T2, s_calib.T3,
             s_calib.P1, s_calib.H1, s_calib.H2);
    return ESP_OK;
}

esp_err_t bme280_read(bme280_data_t *out)
{
    uint8_t data[8];
    esp_err_t err = read_regs(REG_DATA, data, sizeof(data));
    if (err != ESP_OK) return err;

    int32_t adc_P = ((int32_t)data[0] << 12) | ((int32_t)data[1] << 4) | (data[2] >> 4);
    int32_t adc_T = ((int32_t)data[3] << 12) | ((int32_t)data[4] << 4) | (data[5] >> 4);
    int32_t adc_H = ((int32_t)data[6] <<  8) |  (int32_t)data[7];

    int32_t t_fine;
    int32_t temp_c_hundredths = compensate_temperature(adc_T, &t_fine);
    uint32_t pressure_pa      = compensate_pressure(adc_P, t_fine);
    uint32_t hum_q22_10       = compensate_humidity(adc_H, t_fine);

    out->temperature_f = (temp_c_hundredths * 9 / 5 + 3200) / 100;
    out->pressure_pa   = pressure_pa;
    out->humidity_pct  = hum_q22_10 / 1024;

    return ESP_OK;
}

#include "settings.hpp"

#include "esp_log.h"
#include "nvs_flash.h"

static const char *TAG = "Settings";

Settings &Settings::instance()
{
    static Settings settings;
    return settings;
}

void Settings::init()
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // Open NVS handle
    ESP_LOGI(TAG, "\nOpening Non-Volatile Storage (NVS) handle...");
    err = nvs_open("storage", NVS_READWRITE, &handle_);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
        return;
    }
}

int32_t Settings::mode()
{
    return read_int32_with_default("mode", 1);  // Thermostat::MODE_MANUAL
}

void Settings::set_mode(int32_t value)
{
    store_int32("mode", value);
}

int32_t Settings::setpoint()
{
    return read_int32_with_default("setpoint", 68);
}

void Settings::set_setpoint(int32_t value)
{
    store_int32("setpoint", value);
}

int32_t Settings::temperature_hysteresis()
{
    return read_int32_with_default("temp_hyst", 2);
}

void Settings::set_temperature_hysteresis(int32_t value)
{
    store_int32("temp_hyst", value);
}

int32_t Settings::inactivity_timeout_ms()
{
    return read_int32_with_default("timeout_ms", 30000);
}

void Settings::set_inactivity_timeout_ms(int32_t value)
{
    store_int32("timeout_ms", value);
}

int32_t Settings::default_brightness()
{
    return read_int32_with_default("def_bright", 100);
}

void Settings::set_default_brightness(int32_t value)
{
    store_int32("def_bright", value);
}

void Settings::store_int32(const char *key, int32_t value)
{
    esp_err_t err = nvs_set_i32(handle_, key, value);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write to storage!");
    }
    err = nvs_commit(handle_);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NVS changes!");
    }
}

int32_t Settings::read_int32_with_default(const char *key, int32_t default_value)
{
    int32_t value;
    ESP_LOGI(TAG, "\nReading counter from NVS...");
    esp_err_t err = nvs_get_i32(handle_, key, &value);
    if (err != ESP_OK) {
        value = default_value;
    }
    return value;
}

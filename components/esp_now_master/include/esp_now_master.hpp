#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "esp_now.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "espnow_protocol.h"

// ESP-NOW master. Sends the heater ON/OFF state to one slave every
// ESPNOW_SEND_INTERVAL_MS. The slave announces itself with a broadcast hello;
// the master pairs with it, remembers its MAC in NVS and from then on sends
// unicast. The unicast peer uses channel 0, so it follows the channel the WiFi
// STA is on (the router's channel).
class EspNowMaster {
public:
    static constexpr uint32_t ESPNOW_SEND_INTERVAL_MS = 3000;
    // The slave counts as offline after this many intervals without a delivered message
    static constexpr uint32_t ESPNOW_OFFLINE_INTERVALS = 3;

    // Returns true when the heater is on
    using StateGetter = bool (*)();

    // WiFi must already be started (STA or APSTA). Call once.
    esp_err_t begin(StateGetter getter);

    // Send the current state right away instead of waiting for the next heartbeat.
    // Safe to call from any task; does nothing before begin().
    void send_now();

    bool has_slave() const { return _has_slave; }
    bool slave_online() const;

private:
    static void task_entry_(void *arg);
    static void recv_cb_(const esp_now_recv_info_t *info, const uint8_t *data, int len);
    static void send_cb_(const esp_now_send_info_t *info, esp_now_send_status_t status);

    void task_();
    void handle_hello_(const uint8_t *mac);
    esp_err_t set_slave_(const uint8_t *mac);
    void send_state_();
    void load_slave_();
    void save_slave_();

    StateGetter _getter = nullptr;
    QueueHandle_t _hello_queue = nullptr;
    uint8_t _slave_mac[6] = {};
    volatile bool _has_slave = false;
    volatile int64_t _last_delivered_ms = 0;
    uint8_t _seq = 0;

    static EspNowMaster *s_instance;
};

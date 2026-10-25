#include "esp_now_master.hpp"

#include <string.h>
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_now.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "nvs.h"

static const char *TAG = "EspNowMaster";

#define NVS_NAMESPACE "espnow_m"
#define NVS_KEY_SLAVE "slave"

EspNowMaster *EspNowMaster::s_instance = nullptr;

static int64_t now_ms() { return esp_timer_get_time() / 1000; }

esp_err_t EspNowMaster::begin(StateGetter getter)
{
    if (s_instance) {
        return ESP_ERR_INVALID_STATE;
    }
    s_instance = this;
    _getter = getter;

    // Modem sleep makes the radio miss broadcast frames, and the slave's hello is one
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_set_ps(WIFI_PS_NONE));

    esp_err_t err = esp_now_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_now_init failed: %s", esp_err_to_name(err));
        s_instance = nullptr;
        return err;
    }
    esp_now_register_recv_cb(recv_cb_);
    esp_now_register_send_cb(send_cb_);

    _hello_queue = xQueueCreate(4, 6);

    load_slave_();
    if (_has_slave) {
        uint8_t mac[6];
        memcpy(mac, _slave_mac, 6);
        if (set_slave_(mac) != ESP_OK) {
            _has_slave = false;
        }
    }

    if (xTaskCreate(task_entry_, "espnow_master", 4096, this, 3, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create task");
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "Started, send interval %lu ms", (unsigned long)ESPNOW_SEND_INTERVAL_MS);
    return ESP_OK;
}

void EspNowMaster::send_now()
{
    if (!_hello_queue) {
        return;
    }
    // An all-zero MAC in the queue is the wake-up marker; the task then sends without pairing
    const uint8_t wake[6] = {};
    xQueueSend(_hello_queue, wake, 0);
}

bool EspNowMaster::slave_online() const
{
    return _has_slave &&
           (now_ms() - _last_delivered_ms) < (int64_t)(ESPNOW_SEND_INTERVAL_MS * ESPNOW_OFFLINE_INTERVALS);
}

// Runs in the WiFi task, so only validate and hand off
void EspNowMaster::recv_cb_(const esp_now_recv_info_t *info, const uint8_t *data, int len)
{
    if (!s_instance || !info || !info->src_addr || len != (int)sizeof(espnow_hello_msg_t)) {
        return;
    }
    const espnow_header_t *h = (const espnow_header_t *)data;
    if (h->magic != ESPNOW_PROTO_MAGIC || h->version != ESPNOW_PROTO_VERSION || h->type != ESPNOW_MSG_HELLO) {
        return;
    }
    xQueueSend(s_instance->_hello_queue, info->src_addr, 0);
}

void EspNowMaster::send_cb_(const esp_now_send_info_t *info, esp_now_send_status_t status)
{
    (void)info;
    if (s_instance && status == ESP_NOW_SEND_SUCCESS) {
        s_instance->_last_delivered_ms = now_ms();
    }
}

void EspNowMaster::task_entry_(void *arg)
{
    static_cast<EspNowMaster *>(arg)->task_();
}

void EspNowMaster::task_()
{
    uint8_t mac[6];
    while (true) {
        // A hello or send_now() wakes the loop early; otherwise means it is time for the next heartbeat
        if (xQueueReceive(_hello_queue, mac, pdMS_TO_TICKS(ESPNOW_SEND_INTERVAL_MS)) == pdTRUE) {
            static const uint8_t wake[6] = {};
            if (memcmp(mac, wake, 6) != 0) {
                handle_hello_(mac);
            }
        }
        send_state_();
    }
}

void EspNowMaster::handle_hello_(const uint8_t *mac)
{
    if (_has_slave && memcmp(mac, _slave_mac, 6) == 0) {
        // Known slave (re)started or lost us; the send that follows brings it up to date
        ESP_LOGI(TAG, "Hello from slave " MACSTR, MAC2STR(mac));
        return;
    }
    if (_has_slave && slave_online()) {
        ESP_LOGW(TAG, "Ignoring hello from " MACSTR ", slave " MACSTR " is online",
                 MAC2STR(mac), MAC2STR(_slave_mac));
        return;
    }
    if (_has_slave) {
        ESP_LOGW(TAG, "Slave " MACSTR " is offline, replacing it", MAC2STR(_slave_mac));
        esp_now_del_peer(_slave_mac);
        _has_slave = false;
    }
    if (set_slave_(mac) == ESP_OK) {
        save_slave_();
        ESP_LOGI(TAG, "Paired with new slave " MACSTR, MAC2STR(mac));
    }
}

esp_err_t EspNowMaster::set_slave_(const uint8_t *mac)
{
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, mac, 6);
    peer.channel = 0;               // 0 = whatever channel the radio is on
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;

    esp_err_t err = esp_now_add_peer(&peer);
    if (err != ESP_OK && err != ESP_ERR_ESPNOW_EXIST) {
        ESP_LOGE(TAG, "esp_now_add_peer failed: %s", esp_err_to_name(err));
        return err;
    }
    memcpy(_slave_mac, mac, 6);
    _last_delivered_ms = 0;
    _has_slave = true;
    return ESP_OK;
}

void EspNowMaster::send_state_()
{
    if (!_has_slave || !_getter) {
        return;
    }
    espnow_state_msg_t msg = {};
    msg.header.magic = ESPNOW_PROTO_MAGIC;
    msg.header.version = ESPNOW_PROTO_VERSION;
    msg.header.type = ESPNOW_MSG_STATE;
    msg.on = _getter() ? 1 : 0;
    msg.seq = _seq++;

    esp_err_t err = esp_now_send(_slave_mac, (const uint8_t *)&msg, sizeof(msg));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "esp_now_send failed: %s", esp_err_to_name(err));
    }
}

void EspNowMaster::load_slave_()
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK) {
        return;
    }
    size_t len = sizeof(_slave_mac);
    if (nvs_get_blob(h, NVS_KEY_SLAVE, _slave_mac, &len) == ESP_OK && len == sizeof(_slave_mac)) {
        _has_slave = true;
        ESP_LOGI(TAG, "Loaded slave " MACSTR " from NVS", MAC2STR(_slave_mac));
    }
    nvs_close(h);
}

void EspNowMaster::save_slave_()
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS to save slave");
        return;
    }
    nvs_set_blob(h, NVS_KEY_SLAVE, _slave_mac, sizeof(_slave_mac));
    nvs_commit(h);
    nvs_close(h);
}

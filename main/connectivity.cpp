#include "connectivity.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <stdio.h>
#include "esp_netif.h"
#include "esp_timer.h"
#include "nvs.h"
#include "wifi_provisioner.hpp"
#include "esp_now_master.hpp"
#include "rest_api.hpp"
#include "thermostat.hpp"
#include "thermostat_config.h"
#include "vars.h"

static const char *TAG = "Connectivity";

#define WIFI_AP_SSID     "Thermostat-Setup"
#define WIFI_HOSTNAME    "thermostat"

static EspNowMaster espnow_master;

// UI variables: the station IP, only once WiFi is connected in STA mode (not while the setup AP is up)
static char sta_ip[16];
static bool sta_connected;
static int64_t sta_polled_ms = -1000;

static void poll_sta_ip(void)
{
    int64_t now_ms = esp_timer_get_time() / 1000;
    if (now_ms - sta_polled_ms < 500) {
        return;
    }
    sta_polled_ms = now_ms;

    esp_netif_ip_info_t info = {};
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    sta_connected = netif && esp_netif_get_ip_info(netif, &info) == ESP_OK && info.ip.addr != 0;
    if (sta_connected) {
        snprintf(sta_ip, sizeof(sta_ip), IPSTR, IP2STR(&info.ip));
    } else {
        sta_ip[0] = '\0';
    }
}

int32_t get_var_wifi_connected() {
    poll_sta_ip();
    return sta_connected;
}

void set_var_wifi_connected(int32_t value) {
}

const char *get_var_ip_address() {
    poll_sta_ip();
    return sta_ip;
}

void set_var_ip_address(const char *value) {
}

static bool heater_is_on(void)
{
    return Thermostat::instance().is_heater_on();
}

// start_provisioning() blocks until the user submits credentials through the
// captive portal, so this runs in its own task and the thermostat keeps working.
static void wifi_task(void *arg)
{
    static WifiProvisioner provisioner;

    // This section is only used for testing if you want to get rid of the provisioned WiFi
#if THERMOSTAT_WIFI_RESET
    {
        // Same namespace the provisioner stores its credentials in
        nvs_handle_t h;
        if (nvs_open("wifi_prov", NVS_READWRITE, &h) == ESP_OK) {
            nvs_erase_all(h);
            nvs_commit(h);
            nvs_close(h);
            ESP_LOGW(TAG, "THERMOSTAT_WIFI_RESET: saved WiFi credentials erased");
        }
    }
#endif

    if (provisioner.is_provisioned()) {
        provisioner.get_credentials();
    } else {
        ESP_LOGI(TAG, "No WiFi credentials, starting captive portal \"%s\"", WIFI_AP_SSID);
        provisioner.start_provisioning(WIFI_AP_SSID, true);
    }
    provisioner.connect_sta();

    ESP_ERROR_CHECK_WITHOUT_ABORT(static_cast<RestApi *>(arg)->start());

    // connect_sta() starts the radio; ESP-NOW follows the router's channel from here
    ESP_ERROR_CHECK_WITHOUT_ABORT(espnow_master.begin(heater_is_on));

    vTaskDelete(NULL);
}

void Connectivity::start(RestApi &rest_api)
{
    xTaskCreate(wifi_task, "wifi", 8192, &rest_api, 3, NULL);
}

void Connectivity::notify_heater_changed()
{
    espnow_master.send_now();
}

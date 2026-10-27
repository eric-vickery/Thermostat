#pragma once

// WiFi provisioning, the REST API and the ESP-NOW heater state link
class RestApi;

class Connectivity {
public:
    // The REST API is started once WiFi is connected
    void start(RestApi &rest_api);
    // Tell the remote the heater state changed
    void notify_heater_changed();
};

#pragma once

// WiFi provisioning, the REST API and the ESP-NOW heater state link
class RestApi;

// True when a remote has paired but has stopped acknowledging messages
bool remote_missing();

class Connectivity {
public:
    // The REST API is started once WiFi is connected
    void start(RestApi &rest_api);
    // Tell the remote the heater state changed
    void notify_heater_changed();
};

#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

class IdleMonitor;
class Scheduler;
class Thermostat;

// JSON REST API over HTTP for the thermostat, its settings and the schedule.
// No authentication, so it is only meant for a trusted LAN. Request bodies are small JSON
// objects; PUT bodies are partial (only the fields present are changed) and every
// successful call returns the resulting resource.
//
//   GET  /api/thermostat   mode ("off"|"manual"|"schedule"), setpoint, current_temp,
//                          humidity, pressure, heater_on
//   PUT  /api/thermostat   mode, setpoint (SETPOINT_MIN..SETPOINT_MAX)
//
//   GET  /api/settings     temperature_hysteresis (0..4), inactivity_timeout_s (1..3600),
//                          active_brightness (1..100), idle_brightness (0..100)
//   PUT  /api/settings     any of the above
//
//   GET    /api/schedule                      all seven days
//   GET    /api/schedule/{day}                one day, {day} is sunday..saturday or 0..6 (0 = sunday)
//   POST   /api/schedule/{day}/events         {"time":"HH:MM","setpoint":70}
//   DELETE /api/schedule/{day}/events/{index} remove one event (index as shown by GET)
//   DELETE /api/schedule/{day}                remove all events of the day
//   POST   /api/schedule/{day}/clone          {"to":"monday"} copies the day onto another day
//   POST   /api/schedule/save                 writes the schedule to NVS (edits are in memory until then)
//
// Errors are {"error":"message"} with 400, 404, 405, 409 (day full) or 500.
class RestApi {
public:
    RestApi(Thermostat &thermostat, Scheduler &scheduler, IdleMonitor &idle_monitor)
        : thermostat_(thermostat), scheduler_(scheduler), idle_monitor_(idle_monitor) {}

    // Starts the HTTP server. Call once the network is up.
    esp_err_t start();

private:
    static RestApi &self(httpd_req_t *req) { return *static_cast<RestApi *>(req->user_ctx); }

    static esp_err_t thermostat_get(httpd_req_t *req);
    static esp_err_t thermostat_put(httpd_req_t *req);
    static esp_err_t settings_get(httpd_req_t *req);
    static esp_err_t settings_put(httpd_req_t *req);
    static esp_err_t schedule_get(httpd_req_t *req);
    static esp_err_t schedule_day(httpd_req_t *req);

    esp_err_t handle_schedule_day(httpd_req_t *req);

    Thermostat &thermostat_;
    Scheduler &scheduler_;
    IdleMonitor &idle_monitor_;
    httpd_handle_t server_ = nullptr;
};

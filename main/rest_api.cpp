#include "rest_api.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "esp_log.h"
#include "esp_spiffs.h"
#include "cJSON.h"

#include "idle_monitor.hpp"
#include "scheduler.hpp"
#include "sensor_monitor.hpp"
#include "settings.hpp"
#include "thermostat.hpp"

static const char *TAG = "RestApi";

#define MAX_BODY_SIZE   512

#define WEB_BASE_PATH   "/www"
#define WEB_PARTITION   "www"
#define WEB_CHUNK_SIZE  1024

#define HYSTERESIS_MIN  0
#define HYSTERESIS_MAX  4
#define TIMEOUT_MIN_S   1
#define TIMEOUT_MAX_S   3600
#define ACTIVE_BRIGHTNESS_MIN   1
#define IDLE_BRIGHTNESS_MIN     0
#define BRIGHTNESS_MAX          100
#define ALTITUDE_MIN_FT         -1500
#define ALTITUDE_MAX_FT         20000

#define STATUS_OK           "200 OK"
#define STATUS_CREATED      "201 Created"
#define STATUS_BAD_REQUEST  "400 Bad Request"
#define STATUS_NOT_FOUND    "404 Not Found"
#define STATUS_NOT_ALLOWED  "405 Method Not Allowed"
#define STATUS_CONFLICT     "409 Conflict"
#define STATUS_TOO_LARGE    "413 Payload Too Large"
#define STATUS_SERVER_ERROR "500 Internal Server Error"

static const char *DAY_NAMES[Scheduler::DAYS] = {
    "sunday", "monday", "tuesday", "wednesday", "thursday", "friday", "saturday"
};

static const char *MODE_NAMES[] = { "off", "manual", "schedule" };

/* ------------------------------------------------------------------------
 * Helpers
 * ------------------------------------------------------------------------ */

// Sends the JSON and takes ownership of (frees) it
static esp_err_t send_json(httpd_req_t *req, cJSON *root, const char *status = STATUS_OK)
{
    char *text = root ? cJSON_PrintUnformatted(root) : nullptr;
    cJSON_Delete(root);
    if (text == nullptr) {
        httpd_resp_set_status(req, STATUS_SERVER_ERROR);
        return httpd_resp_sendstr(req, "{\"error\":\"out of memory\"}");
    }
    httpd_resp_set_status(req, status);
    httpd_resp_set_type(req, "application/json");
    esp_err_t err = httpd_resp_sendstr(req, text);
    free(text);
    return err;
}

static esp_err_t send_error(httpd_req_t *req, const char *status, const char *message)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "error", message);
    return send_json(req, root, status);
}

// Reads and parses the JSON object body. On failure the error response has already been sent.
static cJSON *read_json(httpd_req_t *req)
{
    size_t total = req->content_len;
    if (total == 0) {
        send_error(req, STATUS_BAD_REQUEST, "JSON body required");
        return nullptr;
    }
    if (total > MAX_BODY_SIZE) {
        send_error(req, STATUS_TOO_LARGE, "body too large");
        return nullptr;
    }

    char buf[MAX_BODY_SIZE + 1];
    size_t received = 0;
    while (received < total) {
        int ret = httpd_req_recv(req, buf + received, total - received);
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            continue;
        }
        if (ret <= 0) {
            httpd_resp_send_err(req, HTTPD_408_REQ_TIMEOUT, nullptr);
            return nullptr;
        }
        received += ret;
    }
    buf[received] = 0;

    cJSON *root = cJSON_Parse(buf);
    if (root == nullptr || !cJSON_IsObject(root)) {
        cJSON_Delete(root);
        send_error(req, STATUS_BAD_REQUEST, "body must be a JSON object");
        return nullptr;
    }
    return root;
}

enum class Field { ABSENT, OK, BAD };

static Field get_int(const cJSON *obj, const char *key, int &out)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (item == nullptr) {
        return Field::ABSENT;
    }
    if (!cJSON_IsNumber(item) || item->valuedouble != (double)item->valueint) {
        return Field::BAD;
    }
    out = item->valueint;
    return Field::OK;
}

// Reads an integer field that must be within [min, max] if present.
// Returns false (after sending a 400) if it is present but invalid.
static bool get_ranged_int(httpd_req_t *req, const cJSON *obj, const char *key, int min, int max,
                           bool &present, int &value)
{
    Field f = get_int(obj, key, value);
    present = (f == Field::OK);
    if (f == Field::BAD || (f == Field::OK && (value < min || value > max))) {
        char msg[80];
        snprintf(msg, sizeof(msg), "%s must be an integer from %d to %d", key, min, max);
        send_error(req, STATUS_BAD_REQUEST, msg);
        return false;
    }
    return true;
}

static int parse_day(const char *text)
{
    if (text[0] >= '0' && text[0] <= '6' && text[1] == 0) {
        return text[0] - '0';
    }
    for (int i = 0; i < Scheduler::DAYS; i++) {
        if (strcasecmp(text, DAY_NAMES[i]) == 0) {
            return i;
        }
    }
    return -1;
}

static int parse_mode(const char *text)
{
    for (int i = 0; i < (int)(sizeof(MODE_NAMES) / sizeof(MODE_NAMES[0])); i++) {
        if (strcasecmp(text, MODE_NAMES[i]) == 0) {
            return i;
        }
    }
    return -1;
}

static cJSON *day_json(Scheduler &scheduler, int day)
{
    cJSON *obj = cJSON_CreateObject();
    cJSON_AddStringToObject(obj, "day", DAY_NAMES[day]);
    cJSON *events = cJSON_AddArrayToObject(obj, "events");
    Scheduler::Event event;
    for (int i = 0; scheduler.get_event((Scheduler::Day)day, i, event); i++) {
        cJSON *e = cJSON_CreateObject();
        char time[8];
        snprintf(time, sizeof(time), "%02d:%02d", event.minutes / 60, event.minutes % 60);
        cJSON_AddNumberToObject(e, "index", i);
        cJSON_AddStringToObject(e, "time", time);
        cJSON_AddNumberToObject(e, "setpoint", event.setpoint);
        cJSON_AddItemToArray(events, e);
    }
    return obj;
}

static cJSON *thermostat_json(Thermostat &thermostat)
{
    cJSON *obj = cJSON_CreateObject();
    int32_t mode = thermostat.mode_value();
    cJSON_AddStringToObject(obj, "mode", MODE_NAMES[(mode >= 0 && mode <= 2) ? mode : 1]);
    cJSON_AddNumberToObject(obj, "setpoint", thermostat.requested_setpoint());
    cJSON_AddNumberToObject(obj, "current_temp", thermostat.current_temp());
    cJSON_AddNumberToObject(obj, "humidity", thermostat.humidity());
    cJSON_AddStringToObject(obj, "pressure", thermostat.pressure_string());
    cJSON_AddBoolToObject(obj, "heater_on", thermostat.is_heater_on());
    return obj;
}

static cJSON *settings_json()
{
    Settings &settings = Settings::instance();
    cJSON *obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(obj, "temperature_hysteresis", settings.temperature_hysteresis());
    cJSON_AddNumberToObject(obj, "inactivity_timeout_s", settings.inactivity_timeout_ms() / 1000);
    cJSON_AddNumberToObject(obj, "active_brightness", settings.active_brightness());
    cJSON_AddNumberToObject(obj, "idle_brightness", settings.idle_brightness());
    cJSON_AddNumberToObject(obj, "altitude_ft", settings.altitude_ft());
    return obj;
}

/* ------------------------------------------------------------------------
 * Server
 * ------------------------------------------------------------------------ */

esp_err_t RestApi::start()
{
    if (server_ != nullptr) {
        return ESP_OK;
    }

    mount_web_files();

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 12;
    config.stack_size = 8192;
    config.uri_match_fn = httpd_uri_match_wildcard;

    esp_err_t err = httpd_start(&server_, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server (%s)", esp_err_to_name(err));
        server_ = nullptr;
        return err;
    }

    const httpd_uri_t uris[] = {
        { "/api/thermostat", HTTP_GET, thermostat_get, this },
        { "/api/thermostat", HTTP_PUT, thermostat_put, this },
        { "/api/settings", HTTP_GET, settings_get, this },
        { "/api/settings", HTTP_PUT, settings_put, this },
        { "/api/schedule", HTTP_GET, schedule_get, this },
        { "/api/schedule/*", HTTP_GET, schedule_day, this },
        { "/api/schedule/*", HTTP_POST, schedule_day, this },
        { "/api/schedule/*", HTTP_DELETE, schedule_day, this },
        // Must stay last: handlers are matched in registration order
        { "/*", HTTP_GET, static_file, this },
    };
    for (const httpd_uri_t &uri : uris) {
        err = httpd_register_uri_handler(server_, &uri);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register %s (%s)", uri.uri, esp_err_to_name(err));
            httpd_stop(server_);
            server_ = nullptr;
            return err;
        }
    }

    ESP_LOGI(TAG, "REST API started");
    return ESP_OK;
}

/* ------------------------------------------------------------------------
 * Web app files
 * ------------------------------------------------------------------------ */

void RestApi::mount_web_files()
{
    esp_vfs_spiffs_conf_t conf = {};
    conf.base_path = WEB_BASE_PATH;
    conf.partition_label = WEB_PARTITION;
    conf.max_files = 4;
    conf.format_if_mount_failed = false;

    esp_err_t err = esp_vfs_spiffs_register(&conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount the \"%s\" partition (%s), the web app won't be served",
                 WEB_PARTITION, esp_err_to_name(err));
    }
}

static const char *content_type_for(const char *path)
{
    const char *ext = strrchr(path, '.');
    if (ext == nullptr) {
        return "application/octet-stream";
    }
    static const struct { const char *ext; const char *type; } types[] = {
        { ".html", "text/html" },
        { ".js", "text/javascript" },
        { ".css", "text/css" },
        { ".json", "application/json" },
        { ".svg", "image/svg+xml" },
        { ".png", "image/png" },
        { ".ico", "image/x-icon" },
        { ".woff2", "font/woff2" },
        { ".woff", "font/woff" },
    };
    for (const auto &t : types) {
        if (strcasecmp(ext, t.ext) == 0) {
            return t.type;
        }
    }
    return "application/octet-stream";
}

esp_err_t RestApi::static_file(httpd_req_t *req)
{
    // Unknown API paths are errors, not the web app
    if (strncmp(req->uri, "/api/", 5) == 0) {
        return send_error(req, STATUS_NOT_FOUND, "not found");
    }

    char path[64] = WEB_BASE_PATH;
    size_t base_len = strlen(WEB_BASE_PATH);
    size_t uri_len = strcspn(req->uri, "?");
    if (uri_len == 1 && req->uri[0] == '/') {
        strcpy(path + base_len, "/index.html");
    }
    else if (base_len + uri_len >= sizeof(path) || strstr(req->uri, "..") != nullptr) {
        return send_error(req, STATUS_NOT_FOUND, "not found");
    }
    else {
        memcpy(path + base_len, req->uri, uri_len);
        path[base_len + uri_len] = 0;
    }

    FILE *file = fopen(path, "r");
    if (file == nullptr) {
        // A path without an extension is a page route of the web app
        if (strrchr(req->uri, '.') != nullptr) {
            return send_error(req, STATUS_NOT_FOUND, "not found");
        }
        strcpy(path, WEB_BASE_PATH "/index.html");
        file = fopen(path, "r");
        if (file == nullptr) {
            return send_error(req, STATUS_NOT_FOUND, "web app not installed");
        }
    }

    bool is_html = strcmp(content_type_for(path), "text/html") == 0;
    httpd_resp_set_type(req, content_type_for(path));
    // Everything but index.html has a content hash in its name
    httpd_resp_set_hdr(req, "Cache-Control", is_html ? "no-cache" : "public, max-age=31536000, immutable");

    char chunk[WEB_CHUNK_SIZE];
    esp_err_t err = ESP_OK;
    size_t read;
    while (err == ESP_OK && (read = fread(chunk, 1, sizeof(chunk), file)) > 0) {
        err = httpd_resp_send_chunk(req, chunk, read);
    }
    fclose(file);
    if (err != ESP_OK) {
        // The client went away mid-transfer; abort the chunked response
        httpd_resp_send_chunk(req, nullptr, 0);
        return ESP_FAIL;
    }
    return httpd_resp_send_chunk(req, nullptr, 0);
}

/* ------------------------------------------------------------------------
 * /api/thermostat
 * ------------------------------------------------------------------------ */

esp_err_t RestApi::thermostat_get(httpd_req_t *req)
{
    return send_json(req, thermostat_json(self(req).thermostat_));
}

esp_err_t RestApi::thermostat_put(httpd_req_t *req)
{
    Thermostat &thermostat = self(req).thermostat_;
    cJSON *body = read_json(req);
    if (body == nullptr) {
        return ESP_OK;
    }

    // Validate everything before changing anything
    bool has_setpoint, has_mode = false;
    int setpoint = 0, mode = 0;
    if (!get_ranged_int(req, body, "setpoint", Thermostat::SETPOINT_MIN, Thermostat::SETPOINT_MAX,
                        has_setpoint, setpoint)) {
        cJSON_Delete(body);
        return ESP_OK;
    }
    const cJSON *mode_item = cJSON_GetObjectItemCaseSensitive(body, "mode");
    if (mode_item != nullptr) {
        mode = cJSON_IsString(mode_item) ? parse_mode(mode_item->valuestring) : -1;
        if (mode < 0) {
            cJSON_Delete(body);
            return send_error(req, STATUS_BAD_REQUEST, "mode must be \"off\", \"manual\" or \"schedule\"");
        }
        has_mode = true;
    }
    cJSON_Delete(body);

    if (has_mode) {
        thermostat.request_new_mode((Thermostat::Mode)mode);
    }
    if (has_setpoint) {
        thermostat.set_requested_setpoint(setpoint);
    }
    return send_json(req, thermostat_json(thermostat));
}

/* ------------------------------------------------------------------------
 * /api/settings
 * ------------------------------------------------------------------------ */

esp_err_t RestApi::settings_get(httpd_req_t *req)
{
    return send_json(req, settings_json());
}

esp_err_t RestApi::settings_put(httpd_req_t *req)
{
    RestApi &api = self(req);
    cJSON *body = read_json(req);
    if (body == nullptr) {
        return ESP_OK;
    }

    bool has_hysteresis, has_timeout, has_active, has_idle, has_altitude;
    int hysteresis = 0, timeout = 0, active = 0, idle = 0, altitude = 0;
    bool valid = get_ranged_int(req, body, "temperature_hysteresis", HYSTERESIS_MIN, HYSTERESIS_MAX,
                                has_hysteresis, hysteresis) &&
                 get_ranged_int(req, body, "inactivity_timeout_s", TIMEOUT_MIN_S, TIMEOUT_MAX_S,
                                has_timeout, timeout) &&
                 get_ranged_int(req, body, "active_brightness", ACTIVE_BRIGHTNESS_MIN, BRIGHTNESS_MAX,
                                has_active, active) &&
                 get_ranged_int(req, body, "idle_brightness", IDLE_BRIGHTNESS_MIN, BRIGHTNESS_MAX,
                                has_idle, idle) &&
                 get_ranged_int(req, body, "altitude_ft", ALTITUDE_MIN_FT, ALTITUDE_MAX_FT,
                                has_altitude, altitude);
    cJSON_Delete(body);
    if (!valid) {
        return ESP_OK;
    }

    // The thermostat and idle monitor keep running copies, so go through them
    if (has_hysteresis) {
        api.thermostat_.set_temperature_hysteresis(hysteresis);
    }
    if (has_timeout) {
        api.idle_monitor_.set_timeout_ms(timeout * 1000);
        Settings::instance().set_inactivity_timeout_ms(timeout * 1000);
    }
    if (has_active) {
        api.idle_monitor_.set_active_brightness(active);
        Settings::instance().set_active_brightness(active);
    }
    if (has_idle) {
        api.idle_monitor_.set_idle_brightness(idle);
        Settings::instance().set_idle_brightness(idle);
    }
    if (has_altitude) {
        api.sensors_.set_altitude_ft(altitude);
        Settings::instance().set_altitude_ft(altitude);
    }
    return send_json(req, settings_json());
}

/* ------------------------------------------------------------------------
 * /api/schedule
 * ------------------------------------------------------------------------ */

esp_err_t RestApi::schedule_get(httpd_req_t *req)
{
    Scheduler &scheduler = self(req).scheduler_;
    cJSON *root = cJSON_CreateObject();
    cJSON *days = cJSON_AddArrayToObject(root, "days");
    for (int day = 0; day < Scheduler::DAYS; day++) {
        cJSON_AddItemToArray(days, day_json(scheduler, day));
    }
    return send_json(req, root);
}

esp_err_t RestApi::schedule_day(httpd_req_t *req)
{
    return self(req).handle_schedule_day(req);
}

esp_err_t RestApi::handle_schedule_day(httpd_req_t *req)
{
    // Split what follows "/api/schedule/" (minus any query string) into at most 3 segments
    char path[64];
    const char *rest = req->uri + strlen("/api/schedule/");
    size_t len = strcspn(rest, "?");
    if (len >= sizeof(path)) {
        return send_error(req, STATUS_NOT_FOUND, "not found");
    }
    memcpy(path, rest, len);
    path[len] = 0;

    char *segs[4];
    int count = 0;
    char *save_ptr;
    for (char *tok = strtok_r(path, "/", &save_ptr); tok != nullptr; tok = strtok_r(nullptr, "/", &save_ptr)) {
        if (count == 4) {
            return send_error(req, STATUS_NOT_FOUND, "not found");
        }
        segs[count++] = tok;
    }
    if (count == 0) {
        return send_error(req, STATUS_NOT_FOUND, "not found");
    }

    int method = req->method;

    // POST /api/schedule/save
    if (count == 1 && strcmp(segs[0], "save") == 0) {
        if (method != HTTP_POST) {
            return send_error(req, STATUS_NOT_ALLOWED, "use POST");
        }
        if (scheduler_.save() != ESP_OK) {
            return send_error(req, STATUS_SERVER_ERROR, "failed to save schedule");
        }
        cJSON *root = cJSON_CreateObject();
        cJSON_AddBoolToObject(root, "saved", true);
        return send_json(req, root);
    }

    int day = parse_day(segs[0]);
    if (day < 0) {
        return send_error(req, STATUS_NOT_FOUND, "unknown day");
    }

    // GET / DELETE /api/schedule/{day}
    if (count == 1) {
        if (method == HTTP_GET) {
            return send_json(req, day_json(scheduler_, day));
        }
        if (method == HTTP_DELETE) {
            scheduler_.clear_day((Scheduler::Day)day);
            return send_json(req, day_json(scheduler_, day));
        }
        return send_error(req, STATUS_NOT_ALLOWED, "use GET or DELETE");
    }

    // POST /api/schedule/{day}/events
    if (count == 2 && strcmp(segs[1], "events") == 0) {
        if (method != HTTP_POST) {
            return send_error(req, STATUS_NOT_ALLOWED, "use POST");
        }
        cJSON *body = read_json(req);
        if (body == nullptr) {
            return ESP_OK;
        }
        const cJSON *time_item = cJSON_GetObjectItemCaseSensitive(body, "time");
        int hour = 0, minute = 0, setpoint = 0;
        char extra;
        bool valid = cJSON_IsString(time_item) &&
                     sscanf(time_item->valuestring, "%d:%d%c", &hour, &minute, &extra) == 2 &&
                     hour >= 0 && hour < 24 && minute >= 0 && minute < 60 &&
                     get_int(body, "setpoint", setpoint) == Field::OK;
        cJSON_Delete(body);
        if (!valid) {
            return send_error(req, STATUS_BAD_REQUEST, "body needs time (\"HH:MM\") and integer setpoint");
        }

        Scheduler::Event event = { (uint16_t)(hour * 60 + minute), (int16_t)setpoint };
        esp_err_t err = scheduler_.add_event((Scheduler::Day)day, event);
        if (err == ESP_ERR_NO_MEM) {
            return send_error(req, STATUS_CONFLICT, "day already has the maximum number of events");
        }
        if (err != ESP_OK) {
            return send_error(req, STATUS_BAD_REQUEST, "invalid setpoint or an event already exists at that time");
        }
        return send_json(req, day_json(scheduler_, day), STATUS_CREATED);
    }

    // POST /api/schedule/{day}/clone
    if (count == 2 && strcmp(segs[1], "clone") == 0) {
        if (method != HTTP_POST) {
            return send_error(req, STATUS_NOT_ALLOWED, "use POST");
        }
        cJSON *body = read_json(req);
        if (body == nullptr) {
            return ESP_OK;
        }
        const cJSON *to_item = cJSON_GetObjectItemCaseSensitive(body, "to");
        int to = -1;
        if (cJSON_IsString(to_item)) {
            to = parse_day(to_item->valuestring);
        }
        else if (cJSON_IsNumber(to_item) && to_item->valuedouble >= 0 && to_item->valuedouble <= 6 &&
                 to_item->valuedouble == (double)to_item->valueint) {
            to = to_item->valueint;
        }
        cJSON_Delete(body);
        if (to < 0) {
            return send_error(req, STATUS_BAD_REQUEST, "body needs \"to\" set to a day");
        }
        scheduler_.clone_day((Scheduler::Day)day, (Scheduler::Day)to);
        return send_json(req, day_json(scheduler_, to));
    }

    // DELETE /api/schedule/{day}/events/{index}
    if (count == 3 && strcmp(segs[1], "events") == 0) {
        if (method != HTTP_DELETE) {
            return send_error(req, STATUS_NOT_ALLOWED, "use DELETE");
        }
        char *end;
        long index = strtol(segs[2], &end, 10);
        if (*end != 0 || scheduler_.remove_event((Scheduler::Day)day, (int)index) != ESP_OK) {
            return send_error(req, STATUS_NOT_FOUND, "no such event");
        }
        return send_json(req, day_json(scheduler_, day));
    }

    return send_error(req, STATUS_NOT_FOUND, "not found");
}

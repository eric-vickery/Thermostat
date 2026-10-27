#include "scheduler.hpp"

#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "thermostat.hpp"

static const char *TAG = "Scheduler";

static const char *NVS_NAMESPACE = "schedule";
static const char *NVS_KEY = "days";

// Anything before this year means the clock has not been set yet
#define MIN_VALID_YEAR   2024

bool Scheduler::valid_event(const Event &event)
{
    return event.minutes < MINUTES_PER_DAY &&
           event.setpoint >= Thermostat::SETPOINT_MIN &&
           event.setpoint <= Thermostat::SETPOINT_MAX;
}

void Scheduler::init()
{
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle_);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (!load()) {
        schedule_ = { SCHEDULE_VERSION, {} };
    }
}

void Scheduler::start()
{
    xTaskCreate(task, "scheduler", 4096, this, 2, NULL);
}

bool Scheduler::load()
{
    Schedule stored;
    size_t size = sizeof(stored);
    esp_err_t err = nvs_get_blob(handle_, NVS_KEY, &stored, &size);
    if (err != ESP_OK) {
        if (err != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to read schedule (%s)", esp_err_to_name(err));
        }
        return false;
    }
    if (size != sizeof(stored) || stored.version != SCHEDULE_VERSION) {
        ESP_LOGW(TAG, "Ignoring stored schedule with unexpected size or version");
        return false;
    }

    // Don't trust the blob: every day must be sorted, unique and in range
    for (const DaySchedule &day : stored.days) {
        if (day.count > MAX_EVENTS_PER_DAY) {
            return false;
        }
        for (int i = 0; i < day.count; i++) {
            if (!valid_event(day.events[i]) || (i > 0 && day.events[i].minutes <= day.events[i - 1].minutes)) {
                return false;
            }
        }
    }
    schedule_ = stored;
    return true;
}

esp_err_t Scheduler::save()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (handle_ == 0) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t err = nvs_set_blob(handle_, NVS_KEY, &schedule_, sizeof(schedule_));
    if (err == ESP_OK) {
        err = nvs_commit(handle_);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to store schedule (%s)", esp_err_to_name(err));
    }
    return err;
}

int Scheduler::event_count(Day day)
{
    if (!valid_day(day)) {
        return 0;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    return schedule_.days[day].count;
}

bool Scheduler::get_event(Day day, int index, Event &event)
{
    if (!valid_day(day)) {
        return false;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    const DaySchedule &d = schedule_.days[day];
    if (index < 0 || index >= d.count) {
        return false;
    }
    event = d.events[index];
    return true;
}

esp_err_t Scheduler::add_event(Day day, const Event &event)
{
    if (!valid_day(day) || !valid_event(event)) {
        return ESP_ERR_INVALID_ARG;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    DaySchedule &d = schedule_.days[day];
    if (d.count >= MAX_EVENTS_PER_DAY) {
        return ESP_ERR_NO_MEM;
    }

    // Find the sorted position and reject a duplicate time
    int pos = 0;
    while (pos < d.count && d.events[pos].minutes < event.minutes) {
        pos++;
    }
    if (pos < d.count && d.events[pos].minutes == event.minutes) {
        return ESP_ERR_INVALID_ARG;
    }

    for (int i = d.count; i > pos; i--) {
        d.events[i] = d.events[i - 1];
    }
    d.events[pos] = event;
    d.count++;
    applied_valid_ = false;     // The active event may have changed
    return ESP_OK;
}

esp_err_t Scheduler::remove_event(Day day, int index)
{
    if (!valid_day(day)) {
        return ESP_ERR_INVALID_ARG;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    DaySchedule &d = schedule_.days[day];
    if (index < 0 || index >= d.count) {
        return ESP_ERR_INVALID_ARG;
    }

    for (int i = index; i < d.count - 1; i++) {
        d.events[i] = d.events[i + 1];
    }
    d.count--;
    applied_valid_ = false;
    return ESP_OK;
}

esp_err_t Scheduler::clear_day(Day day)
{
    if (!valid_day(day)) {
        return ESP_ERR_INVALID_ARG;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    schedule_.days[day].count = 0;
    applied_valid_ = false;
    return ESP_OK;
}

esp_err_t Scheduler::clone_day(Day from, Day to)
{
    if (!valid_day(from) || !valid_day(to)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (from == to) {
        return ESP_OK;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    schedule_.days[to] = schedule_.days[from];
    applied_valid_ = false;
    return ESP_OK;
}

bool Scheduler::find_active(int day, int minutes, Event &event, int &event_index)
{
    const DaySchedule &ds = schedule_.days[day];
    for (int i = ds.count - 1; i >= 0; i--) {
        if (ds.events[i].minutes <= minutes) {
            event = ds.events[i];
            event_index = i;
            return true;
        }
    }
    return false;
}

void Scheduler::task(void *arg)
{
    static_cast<Scheduler *>(arg)->run();
}

void Scheduler::run()
{
    while (true) {
        check();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void Scheduler::check()
{
    if (thermostat_.current_mode() != Thermostat::MODE_SCHEDULE) {
        // Re-apply the active event when schedule mode is entered again
        std::lock_guard<std::mutex> lock(mutex_);
        applied_valid_ = false;
        return;
    }

    time_t now = time(nullptr);
    struct tm local;
    localtime_r(&now, &local);
    if (local.tm_year + 1900 < MIN_VALID_YEAR) {
        return;
    }

    Event event;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        int event_index;
        if (!find_active(local.tm_wday, local.tm_hour * 60 + local.tm_min, event, event_index)) {
            return;
        }
        if (applied_valid_ && applied_day_ == local.tm_wday && applied_index_ == event_index) {
            return;
        }
        applied_valid_ = true;
        applied_day_ = local.tm_wday;
        applied_index_ = event_index;
    }

    ESP_LOGI(TAG, "Applying event: setpoint %d", (int)event.setpoint);
    thermostat_.set_requested_setpoint(event.setpoint);
}

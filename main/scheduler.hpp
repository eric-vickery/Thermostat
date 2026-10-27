#pragma once

#include <stdint.h>
#include <mutex>
#include "esp_err.h"
#include "nvs.h"

class Thermostat;

// Seven day schedule with up to MAX_EVENTS_PER_DAY events per day.
// Each day holds only the events the caller configured, kept sorted by time.
// The schedule is stored in NVS so it survives reboots.
// While the thermostat is in MODE_SCHEDULE the scheduler task sets the thermostat's setpoint
// to that of the most recent event today when schedule mode is entered, and again whenever
// a new event occurs. If nothing has occurred yet today the setpoint is left alone. The clock comes from the C library (time/localtime_r), so
// nothing happens until the system time has been set.
class Scheduler {
public:
    static constexpr int DAYS = 7;
    static constexpr int MAX_EVENTS_PER_DAY = 6;
    static constexpr int MINUTES_PER_DAY = 24 * 60;

    // Days are numbered like struct tm::tm_wday
    enum Day {
        SUNDAY,
        MONDAY,
        TUESDAY,
        WEDNESDAY,
        THURSDAY,
        FRIDAY,
        SATURDAY,
    };

    struct Event {
        uint16_t minutes;   // Minutes since midnight, 0..MINUTES_PER_DAY - 1
        int16_t setpoint;   // Degrees Fahrenheit, Thermostat::SETPOINT_MIN..SETPOINT_MAX
    };

    explicit Scheduler(Thermostat &thermostat) : thermostat_(thermostat) {}

    // Opens the NVS namespace and loads the stored schedule (empty if none/invalid).
    // Settings::init() must have run first so NVS flash is initialized.
    void init();
    // Starts the task that applies events to the thermostat
    void start();

    // Number of events configured for the day (0 if the day is invalid)
    int event_count(Day day);
    // Copies out event index (sorted by time) of the day. Returns false if out of range.
    bool get_event(Day day, int index, Event &event);

    // The editing methods below only change the schedule in memory. Call save() once the user
    // is done to write it to NVS.

    // Adds an event to the day, keeping the day sorted by time. Returns ESP_ERR_INVALID_ARG for
    // an invalid day, time or setpoint, or if the day already has an event at that time, and
    // ESP_ERR_NO_MEM if the day already has MAX_EVENTS_PER_DAY events.
    esp_err_t add_event(Day day, const Event &event);
    // Removes event index (sorted by time) from the day
    esp_err_t remove_event(Day day, int index);
    // Removes all events from the day
    esp_err_t clear_day(Day day);
    // Copies every event of the from day over the to day
    esp_err_t clone_day(Day from, Day to);

    // Writes the schedule to NVS
    esp_err_t save();

private:
    struct DaySchedule {
        uint8_t count;
        Event events[MAX_EVENTS_PER_DAY];
    };

    struct Schedule {
        uint8_t version;
        DaySchedule days[DAYS];
    };

    static constexpr uint8_t SCHEDULE_VERSION = 1;

    static bool valid_day(int day) { return day >= 0 && day < DAYS; }
    static bool valid_event(const Event &event);

    bool load();
    // Finds the latest event of the day at or before the given time
    bool find_active(int day, int minutes, Event &event, int &event_index);

    static void task(void *arg);
    void run();
    void check();

    Thermostat &thermostat_;
    std::mutex mutex_;
    nvs_handle_t handle_ = 0;
    Schedule schedule_ = { SCHEDULE_VERSION, {} };

    // The event last applied to the thermostat, so a setpoint changed by hand during an
    // event is left alone until the next event occurs. Index is into applied_day_'s events.
    bool applied_valid_ = false;
    int applied_day_ = 0;
    int applied_index_ = 0;
};

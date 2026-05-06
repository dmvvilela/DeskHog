#pragma once

#include <lvgl.h>
#include <Preferences.h>
#include "ui/InputHandler.h"

/**
 * @class WorkTimerCard
 * @brief Open-ended stopwatch for tracking deep-work sessions.
 *
 * Press the center button to start; press again to stop and persist the session.
 * Shows current elapsed time, today's session list, and today's running total.
 * "Today" rolls over at 3am local time (GMT-3, no DST).
 *
 * Sessions persist in NVS (last 20). Survives reboot, but a session that is
 * still running when the device reboots is lost.
 */
class WorkTimerCard : public InputHandler {
public:
    WorkTimerCard(lv_obj_t* parent);
    ~WorkTimerCard();

    lv_obj_t* getCard() { return _card; }
    bool handleButtonPress(uint8_t button_index) override;
    void prepareForRemoval() override;

private:
    static constexpr size_t MAX_SESSIONS = 20;
    static constexpr int TZ_OFFSET_SECONDS = -3 * 3600;
    static constexpr int DAY_ROLLOVER_HOUR = 3;
    static constexpr uint32_t CLOCK_SYNCED_THRESHOLD = 1700000000; // ~Nov 2023

    struct Session {
        uint32_t start_epoch;       // UTC; 0 means clock was unsynced at start
        uint32_t duration_seconds;
    };

    void startTimer();
    void stopTimer();
    void redraw();
    void appendSession(const Session& s);
    void loadSessions();
    void saveSessions();
    uint32_t currentElapsedSeconds() const;
    uint32_t todayTotalSeconds() const;
    bool isClockSynced() const;
    uint32_t dayIdFor(uint32_t epoch) const;
    uint32_t currentDayId() const;
    bool isValidObject(lv_obj_t* obj) const;

    static String formatElapsed(uint32_t total_seconds);    // HH:MM:SS or MM:SS
    static String formatCompact(uint32_t total_seconds);    // 47m / 1h12m

    static void tick_cb(lv_timer_t* timer);

    // UI
    lv_obj_t* _card;
    lv_obj_t* _background;
    lv_obj_t* _status_label;
    lv_obj_t* _time_label;
    lv_obj_t* _today_label;
    lv_obj_t* _sessions_label;
    lv_timer_t* _timer;

    // State
    bool _is_running;
    uint32_t _start_millis;
    uint32_t _start_epoch;
    Session _sessions[MAX_SESSIONS];
    uint8_t _session_count;

    Preferences _prefs;
    static constexpr const char* PREF_NAMESPACE = "worktimer";
    static constexpr const char* PREF_KEY_DATA = "data";
    static constexpr const char* PREF_KEY_COUNT = "count";

    // Colors
    static const lv_color_t IDLE_BG_COLOR;
    static const lv_color_t RUNNING_BG_COLOR;
};

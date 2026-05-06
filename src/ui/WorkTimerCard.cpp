#include "ui/WorkTimerCard.h"
#include "Style.h"
#include <Arduino.h>
#include <time.h>
#include <cstring>

const lv_color_t WorkTimerCard::IDLE_BG_COLOR = lv_color_hex(0x2C3E50);
const lv_color_t WorkTimerCard::RUNNING_BG_COLOR = lv_color_hex(0x27AE60);

WorkTimerCard::WorkTimerCard(lv_obj_t* parent)
    : _card(nullptr)
    , _background(nullptr)
    , _status_label(nullptr)
    , _time_label(nullptr)
    , _today_label(nullptr)
    , _sessions_label(nullptr)
    , _timer(nullptr)
    , _is_running(false)
    , _start_millis(0)
    , _start_epoch(0)
    , _session_count(0)
{
    memset(_sessions, 0, sizeof(_sessions));
    loadSessions();

    // Idempotent — kicks SNTP background sync once WiFi is up.
    configTime(TZ_OFFSET_SECONDS, 0, "pool.ntp.org", "time.nist.gov");

    _card = lv_obj_create(parent);
    if (!_card) return;
    lv_obj_set_width(_card, lv_pct(100));
    lv_obj_set_height(_card, lv_pct(100));
    lv_obj_set_style_bg_color(_card, lv_color_black(), 0);
    lv_obj_set_style_border_width(_card, 0, 0);
    lv_obj_set_style_pad_all(_card, 5, 0);
    lv_obj_set_style_margin_all(_card, 0, 0);

    _background = lv_obj_create(_card);
    if (!_background) return;
    lv_obj_set_style_radius(_background, 8, LV_PART_MAIN);
    lv_obj_set_style_bg_color(_background, IDLE_BG_COLOR, 0);
    lv_obj_set_style_border_width(_background, 0, 0);
    lv_obj_set_style_pad_all(_background, 6, 0);
    lv_obj_set_width(_background, lv_pct(100));
    lv_obj_set_height(_background, lv_pct(100));
    lv_obj_clear_flag(_background, LV_OBJ_FLAG_SCROLLABLE);

    _status_label = lv_label_create(_background);
    if (_status_label) {
        lv_obj_set_style_text_font(_status_label, Style::labelFont(), 0);
        lv_obj_set_style_text_color(_status_label, lv_color_hex(0xCCCCCC), 0);
        lv_obj_align(_status_label, LV_ALIGN_TOP_MID, 0, 0);
    }

    _time_label = lv_label_create(_background);
    if (_time_label) {
        lv_obj_set_style_text_font(_time_label, Style::loudNoisesFont(), 0);
        lv_obj_set_style_text_color(_time_label, lv_color_white(), 0);
        lv_obj_align(_time_label, LV_ALIGN_CENTER, 0, -8);
    }

    _today_label = lv_label_create(_background);
    if (_today_label) {
        lv_obj_set_style_text_font(_today_label, Style::labelFont(), 0);
        lv_obj_set_style_text_color(_today_label, lv_color_white(), 0);
        lv_obj_align(_today_label, LV_ALIGN_BOTTOM_MID, 0, -14);
    }

    _sessions_label = lv_label_create(_background);
    if (_sessions_label) {
        lv_obj_set_style_text_font(_sessions_label, Style::labelFont(), 0);
        lv_obj_set_style_text_color(_sessions_label, lv_color_hex(0xCCCCCC), 0);
        lv_obj_align(_sessions_label, LV_ALIGN_BOTTOM_MID, 0, 0);
    }

    redraw();
}

WorkTimerCard::~WorkTimerCard() {
    if (_timer) {
        lv_timer_del(_timer);
        _timer = nullptr;
    }
    if (isValidObject(_card)) {
        lv_obj_add_flag(_card, LV_OBJ_FLAG_HIDDEN);
        lv_obj_del_async(_card);
        _card = nullptr;
        _background = nullptr;
        _status_label = nullptr;
        _time_label = nullptr;
        _today_label = nullptr;
        _sessions_label = nullptr;
    }
}

void WorkTimerCard::prepareForRemoval() {
    if (_timer) {
        lv_timer_del(_timer);
        _timer = nullptr;
    }
}

bool WorkTimerCard::handleButtonPress(uint8_t button_index) {
    if (button_index == 1) { // BUTTON_CENTER
        if (_is_running) {
            stopTimer();
        } else {
            startTimer();
        }
        return true;
    }
    return false;
}

void WorkTimerCard::startTimer() {
    if (_is_running) return;
    _is_running = true;
    _start_millis = millis();
    _start_epoch = isClockSynced() ? (uint32_t)time(nullptr) : 0;

    if (_background) {
        lv_obj_set_style_bg_color(_background, RUNNING_BG_COLOR, 0);
    }

    if (!_timer) {
        _timer = lv_timer_create(tick_cb, 1000, this);
    }
    redraw();
}

void WorkTimerCard::stopTimer() {
    if (!_is_running) return;
    uint32_t elapsed = currentElapsedSeconds();
    _is_running = false;

    if (_timer) {
        lv_timer_del(_timer);
        _timer = nullptr;
    }

    // Backfill epoch if clock synced after we started.
    if (_start_epoch == 0 && isClockSynced()) {
        _start_epoch = (uint32_t)time(nullptr) - elapsed;
    }

    if (elapsed > 0) {
        Session s{ _start_epoch, elapsed };
        appendSession(s);
        saveSessions();
    }

    if (_background) {
        lv_obj_set_style_bg_color(_background, IDLE_BG_COLOR, 0);
    }
    redraw();
}

uint32_t WorkTimerCard::currentElapsedSeconds() const {
    if (!_is_running) return 0;
    if (_start_epoch != 0 && isClockSynced()) {
        uint32_t now = (uint32_t)time(nullptr);
        return now > _start_epoch ? now - _start_epoch : 0;
    }
    return (millis() - _start_millis) / 1000;
}

void WorkTimerCard::redraw() {
    if (!_status_label || !_time_label || !_today_label || !_sessions_label) return;

    lv_label_set_text(_status_label, _is_running ? "running" : "ready");

    uint32_t live = currentElapsedSeconds();
    lv_label_set_text(_time_label, formatElapsed(live).c_str());
    lv_obj_align(_time_label, LV_ALIGN_CENTER, 0, -8);

    uint32_t today_total = todayTotalSeconds();
    if (_is_running) today_total += live;

    char today_buf[40];
    if (!isClockSynced()) {
        snprintf(today_buf, sizeof(today_buf), "no clock yet");
    } else if (today_total == 0) {
        snprintf(today_buf, sizeof(today_buf), "today: --");
    } else {
        snprintf(today_buf, sizeof(today_buf), "today: %s", formatCompact(today_total).c_str());
    }
    lv_label_set_text(_today_label, today_buf);

    // Build session list (most recent first, today only, max 4).
    String list = "";
    if (isClockSynced()) {
        uint32_t today = currentDayId();
        int shown = 0;
        for (int i = (int)_session_count - 1; i >= 0 && shown < 4; --i) {
            const Session& s = _sessions[i];
            if (s.start_epoch == 0) continue;
            if (dayIdFor(s.start_epoch) != today) continue;
            if (!list.isEmpty()) list += " \xC2\xB7 "; // " · "
            list += formatCompact(s.duration_seconds);
            shown++;
        }
    }
    lv_label_set_text(_sessions_label, list.c_str());
}

uint32_t WorkTimerCard::todayTotalSeconds() const {
    if (!isClockSynced()) return 0;
    uint32_t today = currentDayId();
    uint32_t total = 0;
    for (size_t i = 0; i < _session_count; ++i) {
        const Session& s = _sessions[i];
        if (s.start_epoch == 0) continue;
        if (dayIdFor(s.start_epoch) == today) total += s.duration_seconds;
    }
    return total;
}

bool WorkTimerCard::isClockSynced() const {
    return (uint32_t)time(nullptr) > CLOCK_SYNCED_THRESHOLD;
}

uint32_t WorkTimerCard::currentDayId() const {
    return dayIdFor((uint32_t)time(nullptr));
}

uint32_t WorkTimerCard::dayIdFor(uint32_t epoch) const {
    // Local time, then push back so 3am = day boundary.
    int64_t shifted = (int64_t)epoch + TZ_OFFSET_SECONDS - (int64_t)DAY_ROLLOVER_HOUR * 3600;
    if (shifted < 0) shifted = 0;
    return (uint32_t)(shifted / 86400);
}

void WorkTimerCard::appendSession(const Session& s) {
    if (_session_count >= MAX_SESSIONS) {
        for (size_t i = 0; i + 1 < MAX_SESSIONS; ++i) {
            _sessions[i] = _sessions[i + 1];
        }
        _sessions[MAX_SESSIONS - 1] = s;
    } else {
        _sessions[_session_count++] = s;
    }
}

void WorkTimerCard::loadSessions() {
    if (!_prefs.begin(PREF_NAMESPACE, true)) {
        _session_count = 0;
        return;
    }
    _session_count = _prefs.getUChar(PREF_KEY_COUNT, 0);
    if (_session_count > MAX_SESSIONS) _session_count = MAX_SESSIONS;
    if (_session_count > 0) {
        size_t expected = sizeof(Session) * MAX_SESSIONS;
        size_t got = _prefs.getBytes(PREF_KEY_DATA, _sessions, expected);
        if (got != expected) {
            _session_count = 0;
            memset(_sessions, 0, sizeof(_sessions));
        }
    }
    _prefs.end();
}

void WorkTimerCard::saveSessions() {
    if (!_prefs.begin(PREF_NAMESPACE, false)) return;
    _prefs.putUChar(PREF_KEY_COUNT, _session_count);
    _prefs.putBytes(PREF_KEY_DATA, _sessions, sizeof(_sessions));
    _prefs.end();
}

String WorkTimerCard::formatElapsed(uint32_t total_seconds) {
    uint32_t h = total_seconds / 3600;
    uint32_t m = (total_seconds % 3600) / 60;
    uint32_t s = total_seconds % 60;
    char buf[12];
    if (h > 0) {
        snprintf(buf, sizeof(buf), "%lu:%02lu:%02lu",
                 (unsigned long)h, (unsigned long)m, (unsigned long)s);
    } else {
        snprintf(buf, sizeof(buf), "%02lu:%02lu",
                 (unsigned long)m, (unsigned long)s);
    }
    return String(buf);
}

String WorkTimerCard::formatCompact(uint32_t total_seconds) {
    uint32_t h = total_seconds / 3600;
    uint32_t m = (total_seconds % 3600) / 60;
    char buf[12];
    if (h > 0) {
        snprintf(buf, sizeof(buf), "%luh%02lum", (unsigned long)h, (unsigned long)m);
    } else if (m > 0) {
        snprintf(buf, sizeof(buf), "%lum", (unsigned long)m);
    } else {
        snprintf(buf, sizeof(buf), "%lus", (unsigned long)total_seconds);
    }
    return String(buf);
}

void WorkTimerCard::tick_cb(lv_timer_t* timer) {
    auto* self = static_cast<WorkTimerCard*>(lv_timer_get_user_data(timer));
    if (self) self->redraw();
}

bool WorkTimerCard::isValidObject(lv_obj_t* obj) const {
    return obj != nullptr && lv_obj_is_valid(obj);
}

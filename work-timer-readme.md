# Work Timer Card

An open-ended stopwatch for tracking deep-work sessions. Press once to start, press again to stop. The card persists session history and shows today's totals at a glance.

## Why

Pomodoro is a fixed countdown — useful, but not what you want when a "session" is whatever length you happen to focus for. Work Timer is the inverse: you decide when to start and stop, the card keeps the receipts.

## Behavior

- **Center button (●)**: start / stop the running session
- **Up / Down (▲ / ▼)**: pass through to card-stack navigation (the timer keeps running while you scroll to other cards)
- **Background color**: dark slate when idle, green when running

The display always shows:
- Status (`ready` / `running`)
- Live elapsed time of the current session (or `00:00` when idle)
- `today: <total>` — sum of all sessions today, including the live one
- A list of today's most recent session durations (up to 4)

## Day rollover

A "day" runs from **3 AM to 3 AM local time**, in **GMT-3 (Brazil, no DST)**. So a session you start at 1 AM still counts toward the previous calendar day, which matches how late-night work actually feels.

To change the timezone, edit `TZ_OFFSET_SECONDS` and (if needed) the rollover hour in [`src/ui/WorkTimerCard.h`](src/ui/WorkTimerCard.h).

## Storage

Sessions live in NVS under the `worktimer` namespace:

| Key     | Type                | Description                                    |
| ------- | ------------------- | ---------------------------------------------- |
| `count` | `uint8_t`           | Number of valid sessions (0–20)                |
| `data`  | `Session[20]` blob  | Fixed-size array, oldest first                 |

```c
struct Session {
    uint32_t start_epoch;       // UTC seconds; 0 = clock was unsynced at start
    uint32_t duration_seconds;
};
```

When the array is full (20 sessions), the oldest is dropped on the next save. Total storage: 160 bytes + a handful for the count.

## Time sync

The card calls `configTime(...)` in its constructor — idempotent, sets up SNTP. Once WiFi connects, the system clock syncs in the background (typically within seconds). `OtaManager` does the same call independently, so syncing is best-effort and shared across the firmware.

If a session is started before the clock is synced:
- Elapsed time is computed from `millis()` (still accurate)
- `start_epoch` is stored as `0`
- If the clock syncs **before** the session ends, `start_epoch` is backfilled (`now - elapsed`)
- If still unsynced at session end, the session is saved with `start_epoch = 0` and won't appear in any "today" view (no way to know what day it belonged to)

The `isClockSynced()` check uses a threshold of `1700000000` (~Nov 2023). Anything before that is treated as un-set.

## Sleep / reboot caveats

- A session **in progress** during a reboot or deep sleep is **lost** — there is no on-flash "session running" flag. v1 keeps it simple; if this matters, add a `running` key written at start and cleared at stop, then on boot save a partial session ending at boot time.
- `millis()` resets across deep sleep. The card prefers wall-clock math (`time(nullptr) - start_epoch`) when the clock is synced, so sessions started post-sync survive brief sleeps. Pre-sync sessions will get their elapsed time wrong if the device sleeps.

## Files

| File                                  | Purpose                                                |
| ------------------------------------- | ------------------------------------------------------ |
| `src/ui/WorkTimerCard.h`              | Class declaration, constants, NVS schema               |
| `src/ui/WorkTimerCard.cpp`            | Implementation                                         |
| `src/ui/CardController.cpp`           | Registration in `initializeCardTypes()`                |
| `src/config/CardConfig.h`             | `CardType::WORK_TIMER` enum + string conversion        |

## Threading

The card lives on Core 1 (the LVGL/UI core), like every other card. The 1-second tick is an `lv_timer_t`, which runs in the LVGL task. NVS read/write happens synchronously inside `start`/`stop` button handlers — fast enough not to matter (~ms).

## Possible v2 ideas

- Multi-day history view (▲/▼ scroll past days when no session is running)
- Persist in-progress sessions across reboot
- NeoPixel state to mirror running status (like Pomodoro does)
- Move TZ offset into the captive portal config so it's editable without a re-flash
- Export session history over the web portal as JSON / CSV

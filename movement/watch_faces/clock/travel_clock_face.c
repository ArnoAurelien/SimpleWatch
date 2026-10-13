#include <stdlib.h>
#include "travel_clock_face.h"
#include "watch_utility.h"

#define UNLOCK_HZ 64
#define HOLD_TICKS (3 * UNLOCK_HZ)
#define WINDOW_TICKS (15 * UNLOCK_HZ)
#define SETTINGS_FACE 3

typedef struct {
    uint8_t index;
    uint8_t step;
    uint8_t pressed;
    bool active;
    bool failed;
    uint32_t started_tick;
    uint32_t down_tick;
} travel_clock_state_t;

static void reset_unlock(travel_clock_state_t *state) {
    state->active = false;
    state->step = 0;
    state->failed = state->pressed != 0;
    movement_request_tick_frequency(1);
}

// Both countries change at 01:00 UTC on the last Sunday of March/October.
static bool summer_time(uint32_t timestamp) {
    watch_date_time utc = watch_utility_date_time_from_unix_time(timestamp, 0);
    uint16_t year = utc.unit.year + WATCH_RTC_REFERENCE_YEAR;
    uint8_t march = 31 - watch_utility_get_iso8601_weekday_number(year, 3, 31) % 7;
    uint8_t october = 31 - watch_utility_get_iso8601_weekday_number(year, 10, 31) % 7;
    uint32_t start = watch_utility_convert_to_unix_time(year, 3, march, 1, 0, 0, 0);
    uint32_t end = watch_utility_convert_to_unix_time(year, 10, october, 1, 0, 0, 0);
    return timestamp >= start && timestamp < end;
}

static void display(travel_clock_state_t *state, movement_settings_t *settings, bool low_energy) {
    watch_date_time rtc = watch_rtc_get_date_time();
    uint32_t utc = watch_utility_date_time_to_unix_time(rtc, movement_timezone_offsets[settings->bit.time_zone] * 60);
    int offset = (state->index == 1 ? 2 : 1) + summer_time(utc);
    watch_date_time local = watch_utility_date_time_from_unix_time(utc, offset * 3600);
    char buf[16];
    if (state->index == 2) {
        watch_clear_colon();
        sprintf(buf, "%s%2u%02u%04u", watch_utility_get_weekday(local),
                local.unit.day, local.unit.month, local.unit.year + WATCH_RTC_REFERENCE_YEAR);
    } else {
        watch_set_colon();
        sprintf(buf, "%s  %02u%02u%02u", state->index == 0 ? "FR" : "FI",
                local.unit.hour, local.unit.minute, local.unit.second);
        // The sleeping RTC updates once a minute; don't show stale seconds.
        if (low_energy) buf[8] = buf[9] = ' ';
    }
    watch_display_string(buf, 0);
}

void travel_clock_face_setup(movement_settings_t *settings, uint8_t index, void **context) {
    (void)settings;
    if (*context == NULL) *context = calloc(1, sizeof(travel_clock_state_t));
    ((travel_clock_state_t *)*context)->index = index;
}

void travel_clock_face_activate(movement_settings_t *settings, void *context) {
    (void)settings;
    travel_clock_state_t *state = context;
    state->pressed = 0;
    reset_unlock(state);
    watch_clear_display();
    if (watch_tick_animation_is_running()) watch_stop_tick_animation();
}

bool travel_clock_face_loop(movement_event_t event, movement_settings_t *settings, void *context) {
    travel_clock_state_t *state = context;
    uint32_t now = movement_get_tick_count();
    if (state->active && now - state->started_tick >= WINDOW_TICKS) reset_unlock(state);

    uint8_t button = 0;
    bool down = false, up = false;
    switch (event.event_type) {
        case EVENT_MODE_BUTTON_DOWN: button = 1; down = true; break;
        case EVENT_LIGHT_BUTTON_DOWN: button = 2; down = true; movement_illuminate_led(); break;
        case EVENT_ALARM_BUTTON_DOWN: button = 4; down = true; break;
        case EVENT_MODE_BUTTON_UP:
        case EVENT_MODE_LONG_UP: button = 1; up = true; break;
        case EVENT_LIGHT_BUTTON_UP:
        case EVENT_LIGHT_LONG_UP: button = 2; up = true; break;
        case EVENT_ALARM_BUTTON_UP:
        case EVENT_ALARM_LONG_UP: button = 4; up = true; break;
        default: break;
    }

    if (down) {
        uint8_t pins = (watch_get_pin_level(BTN_MODE) ? 1 : 0) |
                       (watch_get_pin_level(BTN_LIGHT) ? 2 : 0) |
                       (watch_get_pin_level(BTN_ALARM) ? 4 : 0);
        bool overlap = state->pressed != 0 || __builtin_popcount(pins) > 1;
        state->pressed |= button;
        if (overlap || state->failed) reset_unlock(state);
        else if (!state->active && button == 1) {
            state->active = true;
            state->step = 0;
            state->started_tick = state->down_tick = now;
            movement_request_tick_frequency(UNLOCK_HZ);
        } else if (state->active) {
            if (button != (1u << state->step)) reset_unlock(state);
            else state->down_tick = now;
        }
    }
    if (up && button == 2) {
        movement_event_t light_up = { .event_type = EVENT_LIGHT_BUTTON_UP };
        movement_default_loop_handler(light_up, settings);
    }
    if (up) {
        bool was_pressed = (state->pressed & button) != 0;
        state->pressed &= ~button;
        if (state->active && was_pressed) {
            if (button == (1u << state->step) && now - state->down_tick >= HOLD_TICKS) {
                if (++state->step == 3) {
                    reset_unlock(state);
                    movement_move_to_face(SETTINGS_FACE);
                    return true;
                }
            } else {
                bool normal_mode = state->step == 0 && button == 1;
                reset_unlock(state);
                if (normal_mode) {
                    movement_move_to_face((state->index + 1) % 3);
                    return true;
                }
            }
        }
        if (!state->pressed) state->failed = false;
    }
    // Simultaneous releases can coalesce into one event; physical levels let
    // a failed chord recover as soon as every button is released.
    if (state->failed && !watch_get_pin_level(BTN_MODE) &&
        !watch_get_pin_level(BTN_LIGHT) && !watch_get_pin_level(BTN_ALARM)) {
        state->pressed = 0;
        state->failed = false;
    }
    if (event.event_type == EVENT_ACTIVATE || event.event_type == EVENT_LOW_ENERGY_UPDATE ||
        (event.event_type == EVENT_TICK && (!state->active || event.subsecond == 0)))
        display(state, settings, event.event_type == EVENT_LOW_ENERGY_UPDATE);
    return true;
}

void travel_clock_face_resign(movement_settings_t *settings, void *context) {
    (void)settings;
    reset_unlock(context);
}

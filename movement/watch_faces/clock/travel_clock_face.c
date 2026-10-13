#include <stdlib.h>
#include "travel_clock_face.h"
#include "watch_utility.h"

typedef struct {
    uint8_t index;
} travel_clock_state_t;

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
    (void)context;
    movement_request_tick_frequency(1);
    watch_clear_display();
    if (watch_tick_animation_is_running()) watch_stop_tick_animation();
}

bool travel_clock_face_loop(movement_event_t event, movement_settings_t *settings, void *context) {
    travel_clock_state_t *state = context;
    switch (event.event_type) {
        case EVENT_MODE_BUTTON_UP:
        case EVENT_MODE_LONG_UP:
            movement_move_to_face((state->index + 1) % 3);
            break;
        case EVENT_LIGHT_BUTTON_DOWN:
            movement_illuminate_led();
            break;
        case EVENT_LIGHT_BUTTON_UP:
        case EVENT_LIGHT_LONG_UP:
            event.event_type = EVENT_LIGHT_BUTTON_UP;
            movement_default_loop_handler(event, settings);
            break;
        case EVENT_ACTIVATE:
        case EVENT_TICK:
        case EVENT_LOW_ENERGY_UPDATE:
            display(state, settings, event.event_type == EVENT_LOW_ENERGY_UPDATE);
            break;
        default:
            break;
    }
    return true;
}

void travel_clock_face_resign(movement_settings_t *settings, void *context) {
    (void)settings;
    (void)context;
    movement_request_tick_frequency(1);
}

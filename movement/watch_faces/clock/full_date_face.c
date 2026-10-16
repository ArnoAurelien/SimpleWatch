#include "full_date_face.h"
#include "watch_utility.h"

static uint32_t deadline;

static uint32_t now(void) {
    return watch_utility_date_time_to_unix_time(watch_rtc_get_date_time(), 0);
}

void full_date_face_setup(movement_settings_t *settings, uint8_t index, void **context) {
    (void)settings;
    (void)index;
    travel_clock_setup(context, travel_clock_return_face);
}

void full_date_face_activate(movement_settings_t *settings, void *context) {
    (void)settings;
    travel_clock_activate(context);
    ((travel_clock_state_t *)context)->mode_target = travel_clock_return_face;
    deadline = now() + TRAVEL_CLOCK_DATE_SECONDS;
}

bool full_date_face_loop(movement_event_t event, movement_settings_t *settings, void *context) {
    bool refresh = travel_clock_handle_event(event, settings, context);
    travel_clock_state_t *state = context;
    if (state->alarm_tapped) {
        state->alarm_tapped = false;
        deadline = now() + TRAVEL_CLOCK_DATE_SECONDS;
    }
    // An in-progress unlock sequence outlasts the countdown.
    if ((event.event_type == EVENT_TICK || event.event_type == EVENT_LOW_ENERGY_UPDATE) &&
        !state->active && now() >= deadline) {
        movement_move_to_face(travel_clock_return_face);
        return true;
    }
    if (refresh) {
        watch_date_time local = travel_clock_local_time(settings, 1);
        char buf[16];
        watch_clear_colon();
        sprintf(buf, "%s%2u%02u%04u", watch_utility_get_weekday(local),
                local.unit.day, local.unit.month, local.unit.year + WATCH_RTC_REFERENCE_YEAR);
        watch_display_string(buf, 0);
    }
    return true;
}

void full_date_face_resign(movement_settings_t *settings, void *context) {
    (void)settings;
    travel_clock_resign(context);
}

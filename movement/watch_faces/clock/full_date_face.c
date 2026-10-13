#include "full_date_face.h"
#include "watch_utility.h"

void full_date_face_setup(movement_settings_t *settings, uint8_t index, void **context) {
    (void)settings;
    (void)index;
    travel_clock_setup(context);
}

void full_date_face_activate(movement_settings_t *settings, void *context) {
    (void)settings;
    travel_clock_activate(context);
}

bool full_date_face_loop(movement_event_t event, movement_settings_t *settings, void *context) {
    if (travel_clock_handle_event(event, settings, context)) {
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

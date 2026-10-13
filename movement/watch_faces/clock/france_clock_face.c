#include "france_clock_face.h"

void france_clock_face_setup(movement_settings_t *settings, uint8_t index, void **context) {
    (void)settings;
    (void)index;
    travel_clock_setup(context);
}

void france_clock_face_activate(movement_settings_t *settings, void *context) {
    (void)settings;
    travel_clock_activate(context);
}

bool france_clock_face_loop(movement_event_t event, movement_settings_t *settings, void *context) {
    if (travel_clock_handle_event(event, settings, context)) {
        watch_date_time local = travel_clock_local_time(settings, 1);
        travel_clock_display_time("FR", local, event.event_type == EVENT_LOW_ENERGY_UPDATE);
    }
    return true;
}

void france_clock_face_resign(movement_settings_t *settings, void *context) {
    (void)settings;
    travel_clock_resign(context);
}

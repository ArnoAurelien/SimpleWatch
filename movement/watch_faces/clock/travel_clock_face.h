#ifndef TRAVEL_CLOCK_FACE_H_
#define TRAVEL_CLOCK_FACE_H_

#include "movement.h"

void travel_clock_face_setup(movement_settings_t *settings, uint8_t index, void **context);
void travel_clock_face_activate(movement_settings_t *settings, void *context);
bool travel_clock_face_loop(movement_event_t event, movement_settings_t *settings, void *context);
void travel_clock_face_resign(movement_settings_t *settings, void *context);

#define travel_clock_face ((const watch_face_t){ \
    travel_clock_face_setup, travel_clock_face_activate, \
    travel_clock_face_loop, travel_clock_face_resign, NULL })

#endif

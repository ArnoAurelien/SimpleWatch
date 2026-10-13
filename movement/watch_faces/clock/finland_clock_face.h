#ifndef FINLAND_CLOCK_FACE_H_
#define FINLAND_CLOCK_FACE_H_

#include "travel_clock.h"

void finland_clock_face_setup(movement_settings_t *settings, uint8_t index, void **context);
void finland_clock_face_activate(movement_settings_t *settings, void *context);
bool finland_clock_face_loop(movement_event_t event, movement_settings_t *settings, void *context);
void finland_clock_face_resign(movement_settings_t *settings, void *context);

#define finland_clock_face ((const watch_face_t){ \
    finland_clock_face_setup, finland_clock_face_activate, \
    finland_clock_face_loop, finland_clock_face_resign, NULL })

#endif

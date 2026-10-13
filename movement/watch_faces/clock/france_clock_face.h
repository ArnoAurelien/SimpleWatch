#ifndef FRANCE_CLOCK_FACE_H_
#define FRANCE_CLOCK_FACE_H_

#include "travel_clock.h"

void france_clock_face_setup(movement_settings_t *settings, uint8_t index, void **context);
void france_clock_face_activate(movement_settings_t *settings, void *context);
bool france_clock_face_loop(movement_event_t event, movement_settings_t *settings, void *context);
void france_clock_face_resign(movement_settings_t *settings, void *context);

#define france_clock_face ((const watch_face_t){ \
    france_clock_face_setup, france_clock_face_activate, \
    france_clock_face_loop, france_clock_face_resign, NULL })

#endif

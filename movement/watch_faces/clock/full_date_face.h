#ifndef FULL_DATE_FACE_H_
#define FULL_DATE_FACE_H_

#include "travel_clock.h"

void full_date_face_setup(movement_settings_t *settings, uint8_t index, void **context);
void full_date_face_activate(movement_settings_t *settings, void *context);
bool full_date_face_loop(movement_event_t event, movement_settings_t *settings, void *context);
void full_date_face_resign(movement_settings_t *settings, void *context);

#define full_date_face ((const watch_face_t){ \
    full_date_face_setup, full_date_face_activate, \
    full_date_face_loop, full_date_face_resign, NULL })

#endif

#ifndef TRAVEL_CLOCK_H_
#define TRAVEL_CLOCK_H_

#include "movement.h"

// The configuration uses the same index to exclude the editor from rotation.
#define TRAVEL_CLOCK_SETTINGS_FACE_INDEX 3

typedef struct {
    uint8_t step;
    uint8_t pressed;
    bool active;
    bool failed;
    uint32_t started_tick;
    uint32_t down_tick;
} travel_clock_state_t;

void travel_clock_setup(void **context);
void travel_clock_activate(void *context);
// Handles shared controls; returns true when the face should refresh its display.
bool travel_clock_handle_event(movement_event_t event, movement_settings_t *settings, void *context);
void travel_clock_resign(void *context);
watch_date_time travel_clock_local_time(movement_settings_t *settings, uint8_t standard_offset);
void travel_clock_display_time(const char *label, watch_date_time local, bool low_energy);

#endif

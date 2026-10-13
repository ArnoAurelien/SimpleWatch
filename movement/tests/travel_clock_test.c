#include <stdio.h>
#include <string.h>
#include "france_clock_face.h"
#include "finland_clock_face.h"
#include "full_date_face.h"
#include "watch_utility.h"
#include <stdlib.h>
#include <assert.h>

#define HOLD_TICKS (3 * 64)
#define WINDOW_TICKS (15 * 64)

static const watch_face_t faces[] = {france_clock_face, finland_clock_face, full_date_face};
static const watch_face_t *active_face;
static travel_clock_state_t state;
static uint8_t active_face_position;

static int next_face = -1;
static int led_calls;
static int frequency;
static uint32_t tick_count;
static uint8_t pins;
bool watch_get_pin_level(uint8_t pin) {
    return (pins & (pin == BTN_MODE ? 1 : pin == BTN_LIGHT ? 2 : 4)) != 0;
}
uint32_t movement_get_tick_count(void) { return tick_count; }
static bool colon;
static char screen[11];
static watch_date_time rtc;
const int16_t movement_timezone_offsets[] = {0, 60, 120, 180};

void movement_request_tick_frequency(uint8_t hz) { frequency = hz; }
void movement_move_to_face(uint8_t index) { next_face = index; }
void movement_move_to_next_face(void) { next_face = (active_face_position + 1) % 3; }
void movement_illuminate_led(void) { led_calls++; }
bool movement_default_loop_handler(movement_event_t e, movement_settings_t *s) { (void)e; (void)s; return true; }
watch_date_time watch_rtc_get_date_time(void) { return rtc; }
void watch_clear_display(void) { memset(screen, ' ', 10); screen[10] = 0; }
void watch_set_colon(void) { colon = true; }
void watch_clear_colon(void) { colon = false; }
bool watch_tick_animation_is_running(void) { return false; }
void watch_stop_tick_animation(void) {}
void watch_display_string(char *s, uint8_t pos) { for (; *s && pos < 10; pos++) screen[pos] = *s++; }

static movement_settings_t settings;
static void send(uint8_t type) {
    if (type == EVENT_MODE_BUTTON_DOWN) pins |= 1;
    if (type == EVENT_LIGHT_BUTTON_DOWN) pins |= 2;
    if (type == EVENT_ALARM_BUTTON_DOWN) pins |= 4;
    if (type == EVENT_MODE_BUTTON_UP || type == EVENT_MODE_LONG_UP) pins &= ~1;
    if (type == EVENT_LIGHT_BUTTON_UP || type == EVENT_LIGHT_LONG_UP) pins &= ~2;
    if (type == EVENT_ALARM_BUTTON_UP || type == EVENT_ALARM_LONG_UP) pins &= ~4;
    movement_event_t e = {.event_type = type};
    active_face->loop(e, &settings, &state);
}
static void ticks(int count) { while (count--) { tick_count++; send(EVENT_TICK); } }
static void fresh(uint8_t index) {
    memset(&state, 0, sizeof(state)); active_face_position = index;
    active_face = &faces[index];
    next_face = -1; led_calls = 0; pins = 0;
    active_face->activate(&settings, &state);
}
static void hold(uint8_t down, uint8_t up, int duration) {
    send(down); ticks(duration); send(up);
}
static void unlock(void) {
    hold(EVENT_MODE_BUTTON_DOWN, EVENT_MODE_LONG_UP, HOLD_TICKS);
    hold(EVENT_LIGHT_BUTTON_DOWN, EVENT_LIGHT_LONG_UP, HOLD_TICKS);
    hold(EVENT_ALARM_BUTTON_DOWN, EVENT_ALARM_LONG_UP, HOLD_TICKS);
}
static void expect_display(uint8_t index, uint32_t utc, const char *expected) {
    rtc = watch_utility_date_time_from_unix_time(utc, 0);
    fresh(index); send(EVENT_ACTIVATE);
    assert(strcmp(screen, expected) == 0);
    assert(colon == (index != 2));
}
int main(void) {
    rtc = watch_utility_date_time_from_unix_time(1767225600, 0);
    for (int i = 0; i < 3; i++) {
        fresh(i); hold(EVENT_MODE_BUTTON_DOWN, EVENT_MODE_BUTTON_UP, 10);
        assert(next_face == (i + 1) % 3);
        fresh(i); unlock(); assert(next_face == 3); assert(frequency == 1);
        fresh(i); hold(EVENT_ALARM_BUTTON_DOWN, EVENT_ALARM_LONG_UP, 300);
        assert(next_face == -1 && led_calls == 0);
        hold(EVENT_LIGHT_BUTTON_DOWN, EVENT_LIGHT_LONG_UP, 300);
        assert(next_face == -1 && led_calls == 1);
    }
    // MODE held for just less than three seconds changes view, never opens settings.
    fresh(0); hold(EVENT_MODE_BUTTON_DOWN, EVENT_MODE_LONG_UP, HOLD_TICKS - 1);
    assert(next_face == 1);
    // A too-short second or third hold resets, and a fresh sequence succeeds.
    for (int short_step = 1; short_step <= 2; short_step++) {
        fresh(0); hold(EVENT_MODE_BUTTON_DOWN, EVENT_MODE_LONG_UP, HOLD_TICKS);
        hold(EVENT_LIGHT_BUTTON_DOWN, EVENT_LIGHT_LONG_UP, short_step == 1 ? HOLD_TICKS - 1 : HOLD_TICKS);
        hold(EVENT_ALARM_BUTTON_DOWN, EVENT_ALARM_LONG_UP, HOLD_TICKS - 1);
        assert(next_face == -1 && !state.active); unlock(); assert(next_face == 3);
    }
    // Wrong order, including an extra MODE after the first step.
    for (int wrong = 0; wrong < 2; wrong++) {
        fresh(0); hold(EVENT_MODE_BUTTON_DOWN, EVENT_MODE_LONG_UP, HOLD_TICKS);
        hold(wrong ? EVENT_MODE_BUTTON_DOWN : EVENT_ALARM_BUTTON_DOWN,
             wrong ? EVENT_MODE_LONG_UP : EVENT_ALARM_LONG_UP, HOLD_TICKS);
        assert(!state.active && next_face == -1); unlock(); assert(next_face == 3);
    }
    // Chords invalidate the attempt until all buttons are released.
    fresh(0); send(EVENT_MODE_BUTTON_DOWN); ticks(HOLD_TICKS);
    send(EVENT_LIGHT_BUTTON_DOWN); ticks(HOLD_TICKS);
    send(EVENT_MODE_LONG_UP); send(EVENT_LIGHT_LONG_UP);
    hold(EVENT_ALARM_BUTTON_DOWN, EVENT_ALARM_LONG_UP, HOLD_TICKS);
    assert(next_face == -1); unlock(); assert(next_face == 3);
    // Recover when the two releases of a failed chord coalesce.
    fresh(0); send(EVENT_MODE_BUTTON_DOWN); send(EVENT_LIGHT_BUTTON_DOWN);
    pins = 0; send(EVENT_LIGHT_LONG_UP);
    assert(!state.failed && state.pressed == 0); unlock(); assert(next_face == 3);
    // Final release just inside the window succeeds; at the deadline it fails.
    for (int boundary = -1; boundary <= 0; boundary++) {
        fresh(0); hold(EVENT_MODE_BUTTON_DOWN, EVENT_MODE_LONG_UP, HOLD_TICKS);
        ticks(WINDOW_TICKS - 3 * HOLD_TICKS + boundary);
        hold(EVENT_LIGHT_BUTTON_DOWN, EVENT_LIGHT_LONG_UP, HOLD_TICKS);
        hold(EVENT_ALARM_BUTTON_DOWN, EVENT_ALARM_LONG_UP, HOLD_TICKS);
        assert(next_face == (boundary == -1 ? 3 : -1));
    }
    fresh(0); send(EVENT_MODE_BUTTON_DOWN); ticks(WINDOW_TICKS + 10);
    send(EVENT_MODE_LONG_UP); assert(next_face == -1); unlock(); assert(next_face == 3);
    // Missed foreground tick events still count toward hold/window durations.
    fresh(0); send(EVENT_MODE_BUTTON_DOWN); tick_count += HOLD_TICKS; send(EVENT_MODE_LONG_UP);
    assert(state.step == 1); tick_count += WINDOW_TICKS; send(EVENT_LIGHT_BUTTON_DOWN);
    assert(!state.active && next_face == -1); send(EVENT_LIGHT_BUTTON_UP); unlock(); assert(next_face == 3);
    // Face setup/activation must preserve the configured deep-sleep interval.
    for (uint8_t interval = 1; interval <= 7; interval++) {
        settings.bit.le_interval = interval;
        for (uint8_t index = 0; index < 3; index++) {
            void *context = NULL;
            faces[index].setup(&settings, index, &context);
            faces[index].activate(&settings, context);
            assert(settings.bit.le_interval == interval);
            free(context);
        }
    }
    // Named faces retain their identity when assigned different carousel positions.
    for (uint8_t identity = 0; identity < 3; identity++) {
        void *context = NULL;
        uint8_t position = (identity + 1) % 3;
        faces[identity].setup(&settings, position, &context);
        faces[identity].activate(&settings, context);
        movement_event_t e = {.event_type = EVENT_ACTIVATE};
        faces[identity].loop(e, &settings, context);
        assert(strcmp(screen, identity == 0 ? "FR  010000" :
                              identity == 1 ? "FI  020000" : "TH 1012026") == 0);
        free(context);
    }
    // Sleeping views update without lighting LEDs or enabling faster ticks.
    for (uint8_t index = 0; index < 3; index++) {
        fresh(index); send(EVENT_LOW_ENERGY_UPDATE);
        assert(strcmp(screen, index == 0 ? "FR  0100  " :
                              index == 1 ? "FI  0200  " : "TH 1012026") == 0);
        assert(led_calls == 0 && frequency == 1 && next_face == -1);
    }
    // Winter, summer, midnight/year rollover and leap day layouts.
    expect_display(0, 1767225600, "FR  010000");
    expect_display(1, 1767225600, "FI  020000");
    expect_display(2, 1767225600, "TH 1012026");
    uint32_t summer = watch_utility_convert_to_unix_time(2026, 7, 1, 12, 34, 56, 0);
    expect_display(0, summer, "FR  143456"); expect_display(1, summer, "FI  153456");
    expect_display(2, watch_utility_convert_to_unix_time(2024, 2, 29, 12, 0, 0, 0), "TH29022024");
    expect_display(2, watch_utility_convert_to_unix_time(2025, 12, 31, 23, 30, 0, 0), "TH 1012026");
    // DST changes at 01:00 UTC, including both sides of the autumn repeated hour.
    uint32_t spring = watch_utility_convert_to_unix_time(2026, 3, 29, 1, 0, 0, 0);
    expect_display(0, spring - 1, "FR  015959"); expect_display(0, spring, "FR  030000");
    expect_display(1, spring - 1, "FI  025959"); expect_display(1, spring, "FI  040000");
    uint32_t autumn = watch_utility_convert_to_unix_time(2026, 10, 25, 1, 0, 0, 0);
    expect_display(0, autumn - 1, "FR  025959"); expect_display(0, autumn, "FR  020000");
    expect_display(1, autumn - 1, "FI  035959"); expect_display(1, autumn, "FI  030000");
    // The unchanged editor's ZO setting describes the RTC's fixed UTC offset.
    settings.bit.time_zone = 2;
    rtc = watch_utility_date_time_from_unix_time(summer, 7200);
    fresh(1); send(EVENT_ACTIVATE); assert(strcmp(screen, "FI  153456") == 0);
    puts("PASS: views, buttons, unlock/reset/timing boundaries, dates, DST, RTC timezone, sleep preferences and sleeping display");
    return 0;
}

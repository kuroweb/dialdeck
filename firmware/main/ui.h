#pragma once

#include <stdbool.h>

#include "lvgl.h"

typedef enum {
    UI_SCREEN_TOUCHPAD,
    UI_SCREEN_SETTINGS,
    UI_SCREEN_COUNT,
} ui_screen_t;

// Build all screens and show the touchpad. Call with the LVGL lock held.
void ui_create(void);

ui_screen_t ui_active(void);

// True if a touch starting at p is a screen-switch swipe rather than input for the screen.
bool ui_is_edge(lv_point_t p);

// Feed knob detents to the active screen (other than the touchpad, whose knob scrolls the host).
// Call with the LVGL lock held.
void ui_knob(int detents);

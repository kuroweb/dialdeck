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

// True if a touch starting at p may pull down the settings panel rather than being input for the
// touchpad.
bool ui_is_pull_down_start(lv_point_t p);

// Draw a small grab bar marking the edge a swipe starts from.
void ui_add_handle(lv_obj_t *scr, lv_align_t align, int y);

// Feed knob detents to the active screen (other than the touchpad, whose knob scrolls the host).
// Call with the LVGL lock held.
void ui_knob(int detents);

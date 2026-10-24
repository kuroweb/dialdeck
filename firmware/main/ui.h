#pragma once

#include <stdbool.h>

#include "lvgl.h"

// Screens in menu order. Screens without a menu name (see screen.h) come after the menu's.
typedef enum {
    UI_SCREEN_TOUCHPAD,
    UI_SCREEN_DECK,
    UI_SCREEN_AGENTS,
    UI_SCREEN_SETTINGS,
    UI_SCREEN_AGENT,    // one agent, opened from the agents screen
    UI_SCREEN_MENU,     // pulled down from the top of any other screen
    UI_SCREEN_COUNT,
} ui_screen_t;

// Build all screens and show the agents screen. Call with the LVGL lock held.
void ui_create(void);

ui_screen_t ui_active(void);

// Switch to another screen. Call with the LVGL lock held.
void ui_show(ui_screen_t screen);

// True if a touch starting at p may pull down the menu rather than being input for the screen.
bool ui_is_pull_down_start(lv_point_t p);

// True if a touch starting at p may switch to the previous or next function.
bool ui_is_side_swipe_start(lv_point_t p);

// Feed knob detents to the active screen. Call with the LVGL lock held.
void ui_knob(int detents);

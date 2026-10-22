// What every screen provides to the UI shell (ui.c). The shell builds the menu from these, routes
// the knob to the screen on show, and refreshes it while it is shown.
#pragma once

#include "lvgl.h"

typedef struct {
    const char *name;   // menu label; NULL keeps the screen out of the menu
    const char *icon;   // LV_SYMBOL_* shown before the name in the menu
    void (*create)(lv_obj_t *scr);
    void (*knob)(int detents);   // NULL if the knob does nothing on this screen
    void (*tick)(void);          // called every 200 ms while the screen is shown; may be NULL
} screen_def_t;

extern const screen_def_t touchpad_screen;
extern const screen_def_t deck_screen;
extern const screen_def_t agents_screen;
extern const screen_def_t agent_screen;   // one agent, opened from agents_screen
extern const screen_def_t hosts_screen;
extern const screen_def_t settings_screen;

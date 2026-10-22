#pragma once

#include "lvgl.h"

// Tap a row to select it, turn the knob to change its value.
void settings_screen_create(lv_obj_t *scr);

void settings_screen_knob(int detents);

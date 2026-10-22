#pragma once

#include "lvgl.h"

// Shortcut buttons around a center button. The knob changes the volume.
void deck_screen_create(lv_obj_t *scr);

void deck_screen_knob(int detents);

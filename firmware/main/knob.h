#pragma once

#include "lvgl.h"

// Register the knob as an LVGL encoder input device.
lv_indev_t *knob_init(void);

// Detents turned since boot (right = +1, left = -1).
int knob_position(void);

// Detents turned since the last call, for sending as scroll.
int knob_take_scroll(void);

#pragma once

#include "lvgl.h"

// Register the knob as an LVGL encoder input device.
lv_indev_t *knob_init(void);

// Detents turned since boot (right = +1, left = -1).
int knob_position(void);

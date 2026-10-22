#pragma once

#include "lvgl.h"

// Phone-style status bar at the top of a screen: Bluetooth (with the active host), battery, and
// the most urgent herdr agent status. Every screen gets one; all are refreshed together.
void status_bar_create(lv_obj_t *scr);

// Call with the LVGL lock held.
void status_bar_refresh(void);

#pragma once

#include "lvgl.h"

// List of the hosts the device can switch between. Tap a host to switch to it (an empty slot
// starts pairing), hold one to forget it and pair a new host in its place.
void hosts_screen_create(lv_obj_t *scr);

// Redraw the rows from the current link state. Call with the LVGL lock held.
void hosts_screen_refresh(void);

#pragma once

#include "lvgl.h"

// Turn an LVGL object into a touchpad: drag moves the pointer, a short tap clicks.
void touchpad_attach(lv_obj_t *obj);

// Pointer movement and left clicks accumulated since the last call.
void touchpad_take(int *dx, int *dy, int *clicks);

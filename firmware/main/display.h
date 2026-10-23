#pragma once

#include "lvgl.h"

// Bring up the ST77916 panel, CST816 touch and LVGL, and turn the backlight on.
lv_display_t *display_init(void);

// Backlight level, 1-100. Duty follows the percentage, so backlight current does too.
void display_set_brightness(int percent);

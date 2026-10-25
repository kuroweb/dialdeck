#pragma once

#include <stdbool.h>

#include "lvgl.h"

// Bring up the ST77916 panel, CST816 touch and LVGL, and turn the backlight on.
lv_display_t *display_init(void);

// Backlight level, 1-100. Duty follows the percentage, so backlight current does too.
void display_set_brightness(int percent);

// Blank the backlight after inactivity and restore it when input resumes.
void display_set_idle_timeout(int seconds);
void display_note_activity(void);
void display_idle_check(void);
bool display_is_sleeping(void);

#pragma once

#include <stdbool.h>

#include "lvgl.h"

// Register the knob as an LVGL encoder input device.
lv_indev_t *knob_init(void);

// Detents turned since the last call, for sending as scroll.
int knob_take_scroll(void);

// Switch between edge interrupts while active and low-level wakeup while sleeping.
void knob_set_sleep_wakeup(bool enabled);

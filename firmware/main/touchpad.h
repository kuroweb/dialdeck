#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

// One mouse report: pointer movement with the buttons held during it.
typedef struct {
    int dx;
    int dy;
    uint8_t buttons;
} touchpad_report_t;

// Turn an LVGL object into a touchpad.
//   tap                      left click (sent after a short wait for a second touch)
//   tap, tap                 double click
//   tap, touch again, drag   drag with the left button held
//   hold still               right click
void touchpad_attach(lv_obj_t *obj);

// Pop the next report, in the order the gestures produced them. Returns false when empty.
bool touchpad_next(touchpad_report_t *out);

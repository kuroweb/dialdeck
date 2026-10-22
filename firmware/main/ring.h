// Layout of round screens: eight items on a ring around a center item.
//
// The ring sits a little below the middle of the screen. The status bar takes the top edge, so
// shifting down lets the ring grow into the otherwise empty bottom and sides.
#pragma once

#include <math.h>

#include "lvgl.h"

#define RING_SLOTS     8
#define RING_RADIUS    124   // ring center to the center of a ring item
#define RING_OFFSET_Y  10    // ring center below the screen center
#define RING_ITEM      80    // ring item diameter
#define RING_CENTER    160   // center item diameter

// Place `obj` at ring position `slot` (0-7), clockwise from the top. Offset by half a step so no
// item sits right under the pull-down edge or the status bar at the top.
static inline void ring_place(lv_obj_t *obj, int slot)
{
    const float a = 2 * (float)M_PI / RING_SLOTS * (slot + 0.5f);
    lv_obj_align(obj, LV_ALIGN_CENTER, (int)lroundf(RING_RADIUS * sinf(a)),
                 RING_OFFSET_Y + (int)lroundf(-RING_RADIUS * cosf(a)));
}

static inline void ring_place_center(lv_obj_t *obj)
{
    lv_obj_align(obj, LV_ALIGN_CENTER, 0, RING_OFFSET_Y);
}

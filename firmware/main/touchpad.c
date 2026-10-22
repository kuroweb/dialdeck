#include "touchpad.h"

#include <stdatomic.h>
#include <stdlib.h>

// Pointer pixels per touch pixel: GAIN for slow moves, rising by ACCEL per pixel of a single
// step (one touch sample, ~16 ms) up to GAIN_MAX, so a quick flick crosses the screen.
#define TOUCHPAD_GAIN        2.5f
#define TOUCHPAD_ACCEL       0.1f
#define TOUCHPAD_GAIN_MAX    5.0f
// Movement below this from the touch-down point is ignored, so a tap does not nudge the pointer.
#define TOUCHPAD_DEADZONE    6
// A touch shorter than this that never left the dead zone is a click.
#define TOUCHPAD_TAP_MS      250

static atomic_int s_dx;
static atomic_int s_dy;
static atomic_int s_clicks;

static lv_point_t s_start;
static lv_point_t s_last;
static uint32_t s_start_ms;
static bool s_moving;
static float s_rem_x;   // sub-pixel remainder carried to the next step
static float s_rem_y;

static int scale(int d, float gain, float *rem)
{
    const float v = d * gain + *rem;
    const int out = (int)v;
    *rem = v - out;
    return out;
}

static void on_event(lv_event_t *e)
{
    lv_point_t p;
    lv_indev_get_point(lv_indev_active(), &p);

    switch (lv_event_get_code(e)) {
    case LV_EVENT_PRESSED:
        s_start = s_last = p;
        s_start_ms = lv_tick_get();
        s_moving = false;
        s_rem_x = s_rem_y = 0;
        break;
    case LV_EVENT_PRESSING:
        if (!s_moving) {
            if (abs(p.x - s_start.x) < TOUCHPAD_DEADZONE && abs(p.y - s_start.y) < TOUCHPAD_DEADZONE) {
                break;
            }
            s_moving = true;
            s_last = s_start;
        }
    {
        const int dx = p.x - s_last.x;
        const int dy = p.y - s_last.y;
        float gain = TOUCHPAD_GAIN + TOUCHPAD_ACCEL * (abs(dx) + abs(dy));
        if (gain > TOUCHPAD_GAIN_MAX) {
            gain = TOUCHPAD_GAIN_MAX;
        }
        atomic_fetch_add(&s_dx, scale(dx, gain, &s_rem_x));
        atomic_fetch_add(&s_dy, scale(dy, gain, &s_rem_y));
        s_last = p;
        break;
    }
    case LV_EVENT_RELEASED:
        if (!s_moving && lv_tick_elaps(s_start_ms) < TOUCHPAD_TAP_MS) {
            atomic_fetch_add(&s_clicks, 1);
        }
        break;
    default:
        break;
    }
}

void touchpad_attach(lv_obj_t *obj)
{
    lv_obj_set_clickable(obj, true);
    lv_obj_add_event_cb(obj, on_event, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(obj, on_event, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(obj, on_event, LV_EVENT_RELEASED, NULL);
}

void touchpad_take(int *dx, int *dy, int *clicks)
{
    *dx = atomic_exchange(&s_dx, 0);
    *dy = atomic_exchange(&s_dy, 0);
    *clicks = atomic_exchange(&s_clicks, 0);
}

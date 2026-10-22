#include "touchpad.h"

#include <stdatomic.h>
#include <stdlib.h>

#include "settings.h"
#include "ui.h"

// Pointer speed comes from settings: the gain for slow moves rises by the accel value per pixel
// moved per 16 ms, up to the max, so a quick flick crosses the screen. Touch samples arrive
// 5-15 ms apart, so speed is measured per time rather than per sample.
#define TOUCHPAD_SPEED_MS    16

// Movement below this from the touch-down point is ignored, so a tap does not nudge the pointer.
// The pointer starts moving from where the finger leaves this zone, not from the touch-down point,
// otherwise the whole zone would be sent at once and the pointer would jump.
#define TOUCHPAD_DEADZONE    6
// A touch shorter than this that never left the dead zone is a click.
#define TOUCHPAD_TAP_MS      250

static atomic_int s_dx;
static atomic_int s_dy;
static atomic_int s_clicks;

static lv_point_t s_start;
static lv_point_t s_last;
static uint32_t s_start_ms;
static uint32_t s_last_ms;
static bool s_moving;
static bool s_ignored;  // touch started on the edge ring: it belongs to screen navigation
static float s_rem_x;   // sub-pixel remainder carried to the next step
static float s_rem_y;

static int scale(int d, float gain, float *rem)
{
    const float v = d * gain + *rem;
    const int out = (int)v;
    *rem = v - out;
    return out;
}

static float gain_for(int dx, int dy, uint32_t dt_ms)
{
    const settings_t *cfg = settings();
    const float speed = (abs(dx) + abs(dy)) * (float)TOUCHPAD_SPEED_MS / (dt_ms > 0 ? dt_ms : 1);
    const float gain = (cfg->pointer_gain + cfg->pointer_accel * speed) / 100.0f;
    const float max = cfg->pointer_gain_max / 100.0f;
    return gain > max ? max : gain;
}

static void on_event(lv_event_t *e)
{
    lv_point_t p;
    lv_indev_get_point(lv_indev_active(), &p);

    const lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESSED) {
        s_ignored = ui_is_edge(p);
    }
    if (s_ignored) {
        return;
    }

    switch (code) {
    case LV_EVENT_PRESSED:
        s_start = s_last = p;
        s_start_ms = s_last_ms = lv_tick_get();
        s_moving = false;
        s_rem_x = s_rem_y = 0;
        break;
    case LV_EVENT_PRESSING: {
        const uint32_t now = lv_tick_get();
        if (!s_moving) {
            if (abs(p.x - s_start.x) >= TOUCHPAD_DEADZONE || abs(p.y - s_start.y) >= TOUCHPAD_DEADZONE) {
                s_moving = true;
                s_last = p;
                s_last_ms = now;
            }
            break;
        }
        const int dx = p.x - s_last.x;
        const int dy = p.y - s_last.y;
        if (dx == 0 && dy == 0) {
            break;
        }
        const float gain = gain_for(dx, dy, now - s_last_ms);
        atomic_fetch_add(&s_dx, scale(dx, gain, &s_rem_x));
        atomic_fetch_add(&s_dy, scale(dy, gain, &s_rem_y));
        s_last = p;
        s_last_ms = now;
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

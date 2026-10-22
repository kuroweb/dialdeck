#include "touchpad.h"

#include <stdlib.h>

#include "ble_hid.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
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
// A touch shorter than this that never left the dead zone is a tap.
#define TOUCHPAD_TAP_MS      250
// After a tap, a new touch within this window turns into a drag or a double click, so the click
// of the first tap is held back until the window closes.
#define TOUCHPAD_SECOND_TOUCH_MS  200
// Holding still this long is a right click.
#define TOUCHPAD_LONG_PRESS_MS    500

#define REPORT_QUEUE_LEN     64

typedef enum {
    STATE_IDLE,
    STATE_TOUCH,        // first touch: may become a tap, a move or a long press
    STATE_TAP_PENDING,  // tapped; waiting to see whether a second touch follows
    STATE_DRAG,         // second touch: left button held until release
    STATE_DONE,         // long press already fired: ignore the rest of this touch
    STATE_IGNORED,      // touch started at the top edge: it may pull down the menu
} state_t;

static QueueHandle_t s_reports;
static lv_timer_t *s_timer;   // second-touch window or long press, depending on the state
static state_t s_state;
static uint8_t s_held;

static lv_point_t s_start;
static lv_point_t s_last;
static uint32_t s_start_ms;
static uint32_t s_last_ms;
static bool s_moving;
static float s_rem_x;   // sub-pixel remainder carried to the next step
static float s_rem_y;

static void emit(int dx, int dy, uint8_t buttons)
{
    const touchpad_report_t r = { .dx = dx, .dy = dy, .buttons = buttons };
    xQueueSend(s_reports, &r, 0);
}

static void click(uint8_t button)
{
    emit(0, 0, s_held | button);
    emit(0, 0, s_held);
}

static void set_held(uint8_t buttons)
{
    s_held = buttons;
    emit(0, 0, s_held);
}

static void start_timer(uint32_t ms)
{
    lv_timer_set_period(s_timer, ms);
    lv_timer_reset(s_timer);
    lv_timer_resume(s_timer);
}

static void on_timer(lv_timer_t *t)
{
    lv_timer_pause(s_timer);
    if (s_state == STATE_TAP_PENDING) {
        click(BLE_HID_BUTTON_LEFT);
        s_state = STATE_IDLE;
    } else if (s_state == STATE_TOUCH && !s_moving) {
        click(BLE_HID_BUTTON_RIGHT);
        s_state = STATE_DONE;
    }
}

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

static void begin_touch(lv_point_t p)
{
    s_start = s_last = p;
    s_start_ms = s_last_ms = lv_tick_get();
    s_moving = false;
    s_rem_x = s_rem_y = 0;
}

static bool is_tap(void)
{
    return !s_moving && lv_tick_elaps(s_start_ms) < TOUCHPAD_TAP_MS;
}

static void move(lv_point_t p)
{
    const uint32_t now = lv_tick_get();
    if (!s_moving) {
        if (abs(p.x - s_start.x) >= TOUCHPAD_DEADZONE || abs(p.y - s_start.y) >= TOUCHPAD_DEADZONE) {
            s_moving = true;
            s_last = p;
            s_last_ms = now;
        }
        return;
    }
    const int dx = p.x - s_last.x;
    const int dy = p.y - s_last.y;
    if (dx == 0 && dy == 0) {
        return;
    }
    const float gain = gain_for(dx, dy, now - s_last_ms);
    emit(scale(dx, gain, &s_rem_x), scale(dy, gain, &s_rem_y), s_held);
    s_last = p;
    s_last_ms = now;
}

static void on_pressed(lv_point_t p)
{
    if (ui_is_pull_down_start(p)) {
        if (s_state == STATE_TAP_PENDING) {
            click(BLE_HID_BUTTON_LEFT);
        }
        lv_timer_pause(s_timer);
        s_state = STATE_IGNORED;
        return;
    }
    begin_touch(p);
    if (s_state == STATE_TAP_PENDING) {
        // Second touch: hold the button now. Moving drags; lifting quickly completes a double
        // click (see on_released).
        lv_timer_pause(s_timer);
        set_held(BLE_HID_BUTTON_LEFT);
        s_state = STATE_DRAG;
    } else {
        s_state = STATE_TOUCH;
        start_timer(TOUCHPAD_LONG_PRESS_MS);
    }
}

static void on_released(void)
{
    switch (s_state) {
    case STATE_TOUCH:
        lv_timer_pause(s_timer);
        if (is_tap()) {
            s_state = STATE_TAP_PENDING;
            start_timer(TOUCHPAD_SECOND_TOUCH_MS);
            return;
        }
        break;
    case STATE_DRAG: {
        const bool tapped = is_tap();
        set_held(0);
        if (tapped) {
            // The second touch was a tap: that press and release were the first click, add the
            // second one to make a double click.
            click(BLE_HID_BUTTON_LEFT);
        }
        break;
    }
    default:
        break;
    }
    s_state = STATE_IDLE;
}

static void on_event(lv_event_t *e)
{
    lv_point_t p;
    lv_indev_get_point(lv_indev_active(), &p);

    switch (lv_event_get_code(e)) {
    case LV_EVENT_PRESSED:
        on_pressed(p);
        break;
    case LV_EVENT_PRESSING:
        if (s_state == STATE_TOUCH || s_state == STATE_DRAG) {
            move(p);
        }
        break;
    case LV_EVENT_RELEASED:
        on_released();
        break;
    default:
        break;
    }
}

void touchpad_attach(lv_obj_t *obj)
{
    s_reports = xQueueCreate(REPORT_QUEUE_LEN, sizeof(touchpad_report_t));
    s_timer = lv_timer_create(on_timer, TOUCHPAD_LONG_PRESS_MS, NULL);
    lv_timer_pause(s_timer);

    lv_obj_set_clickable(obj, true);
    lv_obj_add_event_cb(obj, on_event, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(obj, on_event, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(obj, on_event, LV_EVENT_RELEASED, NULL);
}

bool touchpad_next(touchpad_report_t *out)
{
    return xQueueReceive(s_reports, out, 0) == pdTRUE;
}

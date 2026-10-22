#include "ui.h"

#include <stdlib.h>

#include "ble_hid.h"
#include "board.h"
#include "knob.h"
#include "settings_screen.h"
#include "touchpad.h"

// A touch starting farther than this from the center is on the edge ring.
#define EDGE_RADIUS      (BOARD_LCD_H_RES / 2 - 36)
// Horizontal travel needed for an edge touch to switch screens.
#define SWIPE_MIN        50

static lv_obj_t *s_screens[UI_SCREEN_COUNT];
static ui_screen_t s_active;
static lv_obj_t *s_knob_label;
static lv_obj_t *s_ble_label;

static lv_point_t s_nav_start;
static bool s_nav_edge;

bool ui_is_edge(lv_point_t p)
{
    const int dx = p.x - BOARD_LCD_H_RES / 2;
    const int dy = p.y - BOARD_LCD_V_RES / 2;
    return dx * dx + dy * dy > EDGE_RADIUS * EDGE_RADIUS;
}

ui_screen_t ui_active(void)
{
    return s_active;
}

static void show(ui_screen_t screen, lv_screen_load_anim_t anim)
{
    s_active = screen;
    lv_screen_load_anim(s_screens[screen], anim, 200, 0, false);
}

static void on_nav(lv_event_t *e)
{
    lv_point_t p;
    lv_indev_get_point(lv_indev_active(), &p);

    if (lv_event_get_code(e) == LV_EVENT_PRESSED) {
        s_nav_start = p;
        s_nav_edge = ui_is_edge(p);
        return;
    }
    // LV_EVENT_RELEASED
    const int dx = p.x - s_nav_start.x;
    const int dy = p.y - s_nav_start.y;
    if (!s_nav_edge || abs(dx) < SWIPE_MIN || abs(dx) < abs(dy)) {
        return;
    }
    if (dx < 0) {
        show((s_active + 1) % UI_SCREEN_COUNT, LV_SCREEN_LOAD_ANIM_MOVE_LEFT);
    } else {
        show((s_active + UI_SCREEN_COUNT - 1) % UI_SCREEN_COUNT, LV_SCREEN_LOAD_ANIM_MOVE_RIGHT);
    }
}

static lv_obj_t *screen_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_clickable(scr, true);
    lv_obj_set_scrollable(scr, false);
    lv_obj_add_event_cb(scr, on_nav, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(scr, on_nav, LV_EVENT_RELEASED, NULL);
    return scr;
}

static void refresh_status(lv_timer_t *t)
{
    static const char *const names[] = {
        [BLE_HID_STATE_STARTING] = "BLE starting",
        [BLE_HID_STATE_ADVERTISING] = "BLE waiting",
        [BLE_HID_STATE_CONNECTED] = "BLE connected",
    };
    lv_label_set_text_fmt(s_knob_label, "knob %d", knob_position());
    lv_label_set_text(s_ble_label, names[ble_hid_state()]);
}

static void touchpad_screen_create(lv_obj_t *scr)
{
    touchpad_attach(scr);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "DialDeck");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -50);

    s_knob_label = lv_label_create(scr);
    lv_obj_set_style_text_color(s_knob_label, lv_palette_main(LV_PALETTE_AMBER), 0);
    lv_obj_align(s_knob_label, LV_ALIGN_CENTER, 0, 0);

    s_ble_label = lv_label_create(scr);
    lv_obj_set_style_text_color(s_ble_label, lv_palette_main(LV_PALETTE_LIGHT_BLUE), 0);
    lv_obj_align(s_ble_label, LV_ALIGN_CENTER, 0, 40);

    lv_timer_create(refresh_status, 50, NULL);
    refresh_status(NULL);
}

void ui_create(void)
{
    for (int i = 0; i < UI_SCREEN_COUNT; i++) {
        s_screens[i] = screen_create();
    }
    touchpad_screen_create(s_screens[UI_SCREEN_TOUCHPAD]);
    settings_screen_create(s_screens[UI_SCREEN_SETTINGS]);
    show(UI_SCREEN_TOUCHPAD, LV_SCREEN_LOAD_ANIM_NONE);
}

void ui_knob(int detents)
{
    if (s_active == UI_SCREEN_SETTINGS) {
        settings_screen_knob(detents);
    }
}

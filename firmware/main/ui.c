#include "ui.h"

#include <stdio.h>
#include <stdlib.h>

#include "battery.h"
#include "ble_hid.h"
#include "board.h"
#include "deck_screen.h"
#include "hosts_screen.h"
#include "settings_screen.h"
#include "theme.h"
#include "touchpad.h"

// Like a phone's notification shade: the menu is pulled down from the top edge of any screen and
// pushed back up from its bottom edge. Only those edges are reserved, so a drag on the touchpad
// that wanders to a side never switches screens.
#define EDGE_BAND        52
// Vertical travel needed to open or close the menu.
#define SWIPE_MIN        50
#define ANIM_MS          200

static lv_obj_t *s_screens[UI_SCREEN_COUNT];
static ui_screen_t s_active;
static ui_screen_t s_before_menu;   // where pushing the menu up returns to
// Phone-style status bar at the top of every screen: Bluetooth on the left, battery on the right.
typedef struct {
    lv_obj_t *ble;
    lv_obj_t *battery;
} status_bar_t;
static status_bar_t s_status[UI_SCREEN_COUNT];
static lv_obj_t *s_menu_items[UI_SCREEN_COUNT];

static lv_point_t s_nav_start;

bool ui_is_pull_down_start(lv_point_t p)
{
    return p.y < EDGE_BAND;
}

static bool is_push_up_start(lv_point_t p)
{
    return p.y >= BOARD_LCD_V_RES - EDGE_BAND;
}

ui_screen_t ui_active(void)
{
    return s_active;
}

static void show(ui_screen_t screen, lv_screen_load_anim_t anim)
{
    s_active = screen;
    lv_screen_load_anim(s_screens[screen], anim, ANIM_MS, 0, false);
}

static void mark_menu_item(ui_screen_t current)
{
    for (int i = 0; i < UI_SCREEN_COUNT; i++) {
        if (s_menu_items[i]) {
            lv_obj_set_style_bg_color(s_menu_items[i], i == current ? theme_accent() : theme_button(), 0);
        }
    }
}

static void open_menu(void)
{
    s_before_menu = s_active;
    mark_menu_item(s_active);
    show(UI_SCREEN_MENU, LV_SCREEN_LOAD_ANIM_OVER_BOTTOM);
}

static void close_menu(ui_screen_t target)
{
    show(target, LV_SCREEN_LOAD_ANIM_OUT_TOP);
}

static void on_nav(lv_event_t *e)
{
    lv_point_t p;
    lv_indev_get_point(lv_indev_active(), &p);

    if (lv_event_get_code(e) == LV_EVENT_PRESSED) {
        s_nav_start = p;
        return;
    }
    // LV_EVENT_RELEASED
    const int dx = p.x - s_nav_start.x;
    const int dy = p.y - s_nav_start.y;
    if (abs(dy) < SWIPE_MIN || abs(dy) < abs(dx)) {
        return;
    }
    if (s_active != UI_SCREEN_MENU && dy > 0 && ui_is_pull_down_start(s_nav_start)) {
        open_menu();
    } else if (s_active == UI_SCREEN_MENU && dy < 0 && is_push_up_start(s_nav_start)) {
        close_menu(s_before_menu);
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

static void add_handle(lv_obj_t *scr, lv_align_t align, int y)
{
    lv_obj_t *bar = lv_obj_create(scr);
    lv_obj_remove_style_all(bar);
    lv_obj_set_size(bar, 48, 5);
    lv_obj_set_style_radius(bar, 3, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(bar, theme_button(), 0);
    // Not clickable, so a swipe that starts on the bar reaches the screen and opens the menu.
    lv_obj_set_clickable(bar, false);
    lv_obj_align(bar, align, 0, y);
}

static void battery_text(char *buf, size_t len)
{
    const int percent = battery_percent();
    if (battery_charging()) {
        snprintf(buf, len, LV_SYMBOL_CHARGE);
    } else if (percent < 0) {
        snprintf(buf, len, LV_SYMBOL_BATTERY_EMPTY);
    } else {
        const char *icon = percent > 80 ? LV_SYMBOL_BATTERY_FULL
                           : percent > 55 ? LV_SYMBOL_BATTERY_3
                           : percent > 30 ? LV_SYMBOL_BATTERY_2
                           : percent > 10 ? LV_SYMBOL_BATTERY_1
                           : LV_SYMBOL_BATTERY_EMPTY;
        snprintf(buf, len, "%d%% %s", percent, icon);
    }
}

static void refresh_status(lv_timer_t *t)
{
    const ble_hid_state_t state = ble_hid_state();
    const lv_color_t ble_color = state == BLE_HID_STATE_CONNECTED ? lv_palette_main(LV_PALETTE_TEAL)
                                 : state == BLE_HID_STATE_PAIRING ? lv_palette_main(LV_PALETTE_AMBER)
                                 : state == BLE_HID_STATE_ADVERTISING ? theme_text_muted()
                                 : theme_button_dim();
    char ble[16];
    snprintf(ble, sizeof(ble), LV_SYMBOL_BLUETOOTH "%d", ble_hid_host() + 1);
    char battery[16];
    battery_text(battery, sizeof(battery));

    for (int i = 0; i < UI_SCREEN_COUNT; i++) {
        lv_obj_set_style_text_color(s_status[i].ble, ble_color, 0);
        lv_label_set_text(s_status[i].ble, ble);
        lv_label_set_text(s_status[i].battery, battery);
    }
    if (s_active == UI_SCREEN_HOSTS) {
        hosts_screen_refresh();
    }
}

static void status_bar_create(ui_screen_t screen, int y)
{
    lv_obj_t *bar = lv_obj_create(s_screens[screen]);
    lv_obj_remove_style_all(bar);
    lv_obj_set_size(bar, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(bar, 14, 0);
    lv_obj_set_clickable(bar, false);
    lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, y);

    s_status[screen].ble = lv_label_create(bar);
    lv_label_set_text(s_status[screen].ble, LV_SYMBOL_BLUETOOTH);
    s_status[screen].battery = lv_label_create(bar);
    lv_obj_set_style_text_color(s_status[screen].battery, theme_text_muted(), 0);
}

static void touchpad_screen_create(lv_obj_t *scr)
{
    touchpad_attach(scr);

    // Outline of the touch area, drawn like the deck's buttons. Touches pass through to the screen.
    lv_obj_t *ring = lv_obj_create(scr);
    lv_obj_remove_style_all(ring);
    lv_obj_set_size(ring, 320, 320);
    lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(ring, 2, 0);
    lv_obj_set_style_border_color(ring, theme_button_dim(), 0);
    lv_obj_set_clickable(ring, false);
    lv_obj_center(ring);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Touchpad");
    lv_obj_set_style_text_color(title, theme_text_muted(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_center(title);
}

static void on_menu_item(lv_event_t *e)
{
    close_menu((ui_screen_t)(intptr_t)lv_event_get_user_data(e));
}

static void menu_screen_create(lv_obj_t *scr)
{
    static const struct {
        const char *label;
        ui_screen_t screen;
    } items[] = {
        { LV_SYMBOL_GPS "  Touchpad", UI_SCREEN_TOUCHPAD },
        { LV_SYMBOL_KEYBOARD "  Deck", UI_SCREEN_DECK },
        { LV_SYMBOL_BLUETOOTH "  Connect", UI_SCREEN_HOSTS },
        { LV_SYMBOL_SETTINGS "  Settings", UI_SCREEN_SETTINGS },
    };

    lv_obj_t *list = lv_obj_create(scr);
    lv_obj_remove_style_all(list);
    lv_obj_set_size(list, 220, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(list, 10, 0);
    lv_obj_set_event_bubble(list, true);
    lv_obj_center(list);

    for (int i = 0; i < sizeof(items) / sizeof(items[0]); i++) {
        lv_obj_t *btn = lv_button_create(list);
        lv_obj_set_size(btn, LV_PCT(100), 50);
        lv_obj_set_style_radius(btn, 25, 0);
        lv_obj_set_style_bg_color(btn, theme_button(), 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        s_menu_items[items[i].screen] = btn;
        lv_obj_set_event_bubble(btn, true);
        lv_obj_add_event_cb(btn, on_menu_item, LV_EVENT_CLICKED, (void *)(intptr_t)items[i].screen);

        lv_obj_t *label = lv_label_create(btn);
        lv_label_set_text(label, items[i].label);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
        lv_obj_center(label);
    }

    add_handle(scr, LV_ALIGN_BOTTOM_MID, -14);
}

void ui_create(void)
{
    for (int i = 0; i < UI_SCREEN_COUNT; i++) {
        s_screens[i] = screen_create();
        if (i != UI_SCREEN_MENU) {
            add_handle(s_screens[i], LV_ALIGN_TOP_MID, 12);
            status_bar_create(i, 24);
        } else {
            status_bar_create(i, 20);
        }
    }
    touchpad_screen_create(s_screens[UI_SCREEN_TOUCHPAD]);
    deck_screen_create(s_screens[UI_SCREEN_DECK]);
    settings_screen_create(s_screens[UI_SCREEN_SETTINGS]);
    hosts_screen_create(s_screens[UI_SCREEN_HOSTS]);
    menu_screen_create(s_screens[UI_SCREEN_MENU]);

    lv_timer_create(refresh_status, 200, NULL);
    refresh_status(NULL);
    show(UI_SCREEN_TOUCHPAD, LV_SCREEN_LOAD_ANIM_NONE);
}

void ui_knob(int detents)
{
    switch (s_active) {
    case UI_SCREEN_DECK:
        deck_screen_knob(detents);
        break;
    case UI_SCREEN_SETTINGS:
        settings_screen_knob(detents);
        break;
    default:
        break;
    }
}

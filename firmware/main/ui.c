// UI shell: owns the screens, switches between them, and draws the menu. Each screen's content
// lives in its own module behind screen_def_t (screen.h).
#include "ui.h"

#include <stdlib.h>

#include "board.h"
#include "screen.h"
#include "status_bar.h"
#include "theme.h"
#include "widgets.h"

// Like a phone's notification shade: the menu is pulled down from the top edge of any screen and
// pushed back up from its bottom edge. Only those edges are reserved, so a drag on the touchpad
// that wanders to a side never switches screens.
#define EDGE_BAND        52
// Vertical travel needed to open or close the menu.
#define SWIPE_MIN        50
#define ANIM_MS          200
#define TICK_MS          200

static const screen_def_t menu_screen;

static const screen_def_t *const s_defs[UI_SCREEN_COUNT] = {
    [UI_SCREEN_TOUCHPAD] = &touchpad_screen,
    [UI_SCREEN_DECK] = &deck_screen,
    [UI_SCREEN_AGENTS] = &agents_screen,
    [UI_SCREEN_HOSTS] = &hosts_screen,
    [UI_SCREEN_SETTINGS] = &settings_screen,
    [UI_SCREEN_AGENT] = &agent_screen,
    [UI_SCREEN_MENU] = &menu_screen,
};

static lv_obj_t *s_screens[UI_SCREEN_COUNT];
static ui_screen_t s_active;
static ui_screen_t s_before_menu;   // where pushing the menu up returns to
static lv_obj_t *s_menu_rows[UI_SCREEN_COUNT];
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
    if (s_defs[screen]->tick) {
        s_defs[screen]->tick();
    }
    lv_screen_load_anim(s_screens[screen], anim, ANIM_MS, 0, false);
}

void ui_show(ui_screen_t screen)
{
    show(screen, LV_SCREEN_LOAD_ANIM_FADE_IN);
}

static void open_menu(void)
{
    s_before_menu = s_active;
    for (int i = 0; i < UI_SCREEN_COUNT; i++) {
        if (s_menu_rows[i]) {
            widget_row_set_selected(s_menu_rows[i], i == s_active);
        }
    }
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

static void on_menu_row(lv_event_t *e)
{
    close_menu((ui_screen_t)(intptr_t)lv_event_get_user_data(e));
}

static void menu_create(lv_obj_t *scr)
{
    lv_obj_t *list = widget_list(scr, 6);
    for (int i = 0; i < UI_SCREEN_COUNT; i++) {
        if (s_defs[i]->name == NULL) {
            continue;
        }
        char label[32];
        lv_snprintf(label, sizeof(label), "%s  %s", s_defs[i]->icon, s_defs[i]->name);
        s_menu_rows[i] = widget_row(list, label, NULL, on_menu_row, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }
    widget_handle(scr, LV_ALIGN_BOTTOM_MID, -14);
}

static const screen_def_t menu_screen = {
    .create = menu_create,
};

static lv_obj_t *screen_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_text_color(scr, theme_text(), 0);
    lv_obj_set_style_text_font(scr, THEME_FONT_BODY, 0);
    lv_obj_set_clickable(scr, true);
    lv_obj_set_scrollable(scr, false);
    lv_obj_add_event_cb(scr, on_nav, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(scr, on_nav, LV_EVENT_RELEASED, NULL);
    return scr;
}

static void tick(lv_timer_t *t)
{
    status_bar_refresh();
    if (s_defs[s_active]->tick) {
        s_defs[s_active]->tick();
    }
}

void ui_create(void)
{
    for (int i = 0; i < UI_SCREEN_COUNT; i++) {
        s_screens[i] = screen_create();
        s_defs[i]->create(s_screens[i]);
        if (i != UI_SCREEN_MENU) {
            widget_handle(s_screens[i], LV_ALIGN_TOP_MID, THEME_HANDLE_Y);
        }
        status_bar_create(s_screens[i]);
    }

    lv_timer_create(tick, TICK_MS, NULL);
    status_bar_refresh();
    show(UI_SCREEN_AGENTS, LV_SCREEN_LOAD_ANIM_NONE);
}

void ui_knob(int detents)
{
    if (s_defs[s_active]->knob) {
        s_defs[s_active]->knob(detents);
    }
}

// UI shell: owns the screens, switches between them, and draws the menu. Each screen's content
// lives in its own module behind screen_def_t (screen.h).
#include "ui.h"

#include <math.h>
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

// Icon launcher, in the shape of the round knob's home screen. The ring holds at most
// MENU_VISIBLE icons. The focused one is drawn large in the center, and an accent sector marks
// its place on the ring. With more entries than that, the knob rotates the rest in and the
// focus stays in slot 0.
#define MENU_VISIBLE     6
#define MENU_RADIUS      116
#define MENU_ITEM        72
#define MENU_CENTER      120
#define WEDGE_SIZE       340   // arc diameter; the stroke's outer edge is half of this
#define WEDGE_WIDTH      90    // inward from that edge, so the sector sits behind the icons

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
static ui_screen_t s_menu_items[UI_SCREEN_COUNT];
static int s_menu_count;
static int s_menu_focus;
static lv_obj_t *s_wedge;
static lv_obj_t *s_slots[MENU_VISIBLE];
static lv_obj_t *s_center;
static lv_point_t s_nav_start;
static bool s_nav_swiped;   // this press already opened or closed the menu

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

static int menu_shown(void)
{
    return s_menu_count < MENU_VISIBLE ? s_menu_count : MENU_VISIBLE;
}

// Menu index drawn at `slot`, or -1 when the slot is unused.
static int menu_index_at(int slot)
{
    const int shown = menu_shown();
    if (slot < 0 || slot >= shown) {
        return -1;
    }
    if (s_menu_count <= MENU_VISIBLE) {
        return slot;
    }
    return (s_menu_focus + slot) % s_menu_count;
}

// Slot the accent sector sits on. Fixed at the first slot once the ring has to scroll.
static int menu_focus_slot(void)
{
    return s_menu_count <= MENU_VISIBLE ? s_menu_focus : 0;
}

static void place_slot(lv_obj_t *obj, int slot, int shown)
{
    const float a = 2.f * (float)M_PI * ((float)slot + 0.5f) / (float)shown;
    lv_obj_align(obj, LV_ALIGN_CENTER, (int)lroundf(MENU_RADIUS * sinf(a)),
                 (int)lroundf(-MENU_RADIUS * cosf(a)));
}

// LVGL measures arcs clockwise from 3 o'clock. Slot angles start at 12 o'clock.
static void place_wedge(int slot, int shown)
{
    const int mid = (270 + (int)lroundf(360.f * ((float)slot + 0.5f) / (float)shown)) % 360;
    int span = (360 * 2) / (3 * shown);
    if (span < 36) {
        span = 36;
    }
    if (span > 64) {
        span = 64;
    }
    int start = mid - span / 2;
    if (start < 0) {
        start += 360;
    }
    lv_arc_set_rotation(s_wedge, start);
    lv_arc_set_angles(s_wedge, 0, span);
}

static void menu_refresh(void)
{
    if (s_menu_count == 0) {
        return;
    }
    const int shown = menu_shown();
    const int focus_slot = menu_focus_slot();

    for (int slot = 0; slot < MENU_VISIBLE; slot++) {
        lv_obj_t *btn = s_slots[slot];
        const int index = menu_index_at(slot);
        if (index < 0) {
            lv_obj_set_hidden(btn, true);
            continue;
        }
        lv_obj_set_hidden(btn, false);
        place_slot(btn, slot, shown);
        lv_label_set_text(lv_obj_get_child(btn, 0), s_defs[s_menu_items[index]]->icon);
        const bool selected = slot == focus_slot;
        // The focused icon sits on the sector, so its disc stays out of the way.
        lv_obj_set_style_bg_opa(btn, selected ? LV_OPA_TRANSP : LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(btn, selected ? LV_OPA_TRANSP : LV_OPA_COVER, LV_PART_MAIN | LV_STATE_PRESSED);
    }

    place_wedge(focus_slot, shown);
    lv_label_set_text(lv_obj_get_child(s_center, 0), s_defs[s_menu_items[s_menu_focus]]->icon);
}

static void menu_focus_active(void)
{
    for (int i = 0; i < s_menu_count; i++) {
        if (s_menu_items[i] == s_active) {
            s_menu_focus = i;
            return;
        }
    }
}

static void open_menu(void)
{
    s_before_menu = s_active;
    menu_focus_active();
    menu_refresh();
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
        s_nav_swiped = false;
        return;
    }
    // LV_EVENT_RELEASED
    const int dx = p.x - s_nav_start.x;
    const int dy = p.y - s_nav_start.y;
    if (abs(dy) < SWIPE_MIN || abs(dy) < abs(dx)) {
        return;
    }
    if (s_active != UI_SCREEN_MENU && dy > 0 && ui_is_pull_down_start(s_nav_start)) {
        s_nav_swiped = true;
        open_menu();
    } else if (s_active == UI_SCREEN_MENU && dy < 0 && is_push_up_start(s_nav_start)) {
        s_nav_swiped = true;
        close_menu(s_before_menu);
    }
}

static void on_menu_slot(lv_event_t *e)
{
    if (s_nav_swiped) {
        return;
    }
    const int index = menu_index_at((int)(intptr_t)lv_event_get_user_data(e));
    if (index >= 0) {
        close_menu(s_menu_items[index]);
    }
}

static void on_menu_center(lv_event_t *e)
{
    LV_UNUSED(e);
    if (!s_nav_swiped && s_menu_count > 0) {
        close_menu(s_menu_items[s_menu_focus]);
    }
}

static void menu_knob(int detents)
{
    if (s_menu_count == 0 || detents == 0) {
        return;
    }
    s_menu_focus = (s_menu_focus + detents) % s_menu_count;
    if (s_menu_focus < 0) {
        s_menu_focus += s_menu_count;
    }
    menu_refresh();
}

static lv_obj_t *wedge_create(lv_obj_t *scr)
{
    lv_obj_t *arc = lv_arc_create(scr);
    lv_obj_set_size(arc, WEDGE_SIZE, WEDGE_SIZE);
    lv_obj_center(arc);
    lv_obj_set_clickable(arc, false);
    lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(arc, 0, 0);
    lv_obj_set_style_arc_opa(arc, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 0, LV_PART_MAIN);
    lv_arc_set_bg_angles(arc, 0, 0);
    lv_obj_set_style_arc_width(arc, WEDGE_WIDTH, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, theme_accent(), LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc, false, LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(arc, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_opa(arc, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_pad_all(arc, 0, LV_PART_KNOB);
    return arc;
}

static void menu_create(lv_obj_t *scr)
{
    s_menu_count = 0;
    for (int i = 0; i < UI_SCREEN_COUNT; i++) {
        if (s_defs[i]->name == NULL) {
            continue;
        }
        s_menu_items[s_menu_count++] = (ui_screen_t)i;
    }

    s_wedge = wedge_create(scr);
    for (int slot = 0; slot < MENU_VISIBLE; slot++) {
        s_slots[slot] = widget_circle(scr, MENU_ITEM, theme_button(), on_menu_slot, (void *)(intptr_t)slot);
        lv_obj_set_style_text_font(lv_obj_get_child(s_slots[slot], 0), THEME_FONT_TITLE, 0);
    }
    s_center = widget_circle(scr, MENU_CENTER, theme_button(), on_menu_center, NULL);
    lv_obj_set_style_text_font(lv_obj_get_child(s_center, 0), &lv_font_montserrat_28, 0);
    lv_obj_center(s_center);

    widget_handle(scr, LV_ALIGN_BOTTOM_MID, -14);
    menu_refresh();
}

static const screen_def_t menu_screen = {
    .create = menu_create,
    .knob = menu_knob,
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

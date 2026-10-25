// Device settings grouped by function. Tap a value row to select it, then turn the knob to change it.
#include <stddef.h>

#include "battery.h"
#include "ble_hid.h"
#include "display.h"
#include "esp_lvgl_port.h"
#include "host_slots.h"
#include "screen.h"
#include "config_service.h"
#include "settings.h"
#include "theme.h"
#include "usage_sources.h"
#include "widgets.h"

typedef enum {
    GROUP_USAGE,
    GROUP_BATTERY,
    GROUP_BLUETOOTH,
    GROUP_DISPLAY,
    GROUP_TRACKPAD,
    GROUP_COUNT,
} setting_group_t;

static const char *const s_group_names[GROUP_COUNT] = {
    "Agents", "Battery", "Bluetooth", "Display", "TrackPad",
};

typedef enum {
    FMT_X100,
    FMT_INT,
    FMT_PERCENT,
    FMT_DIRECTION,
    FMT_SLEEP_TIMEOUT,
} item_format_t;

typedef struct {
    const char *name;
    setting_group_t group;
    size_t offset;   // field in settings_t; its range comes from settings_range()
    int step;
    item_format_t format;
} item_t;

static const item_t s_items[] = {
    { "Pointer speed",  GROUP_TRACKPAD, offsetof(settings_t, pointer_gain),     25, FMT_X100 },
    { "Acceleration",   GROUP_TRACKPAD, offsetof(settings_t, pointer_accel),    5,  FMT_X100 },
    { "Max speed",      GROUP_TRACKPAD, offsetof(settings_t, pointer_gain_max), 50, FMT_X100 },
    { "Scroll step",    GROUP_TRACKPAD, offsetof(settings_t, scroll_step),      1,  FMT_INT },
    { "Scroll dir",     GROUP_TRACKPAD, offsetof(settings_t, scroll_reverse),   1,  FMT_DIRECTION },
    { "Brightness",     GROUP_DISPLAY, offsetof(settings_t, brightness),       1,  FMT_PERCENT },
    { "Sleep after",    GROUP_DISPLAY, 0,                                      1,  FMT_SLEEP_TIMEOUT },
};
#define ITEM_COUNT  (sizeof(s_items) / sizeof(s_items[0]))

static const int s_sleep_timeout_values[] = { 0, 15, 30, 60, 120, 300 };

static lv_obj_t *s_rows[ITEM_COUNT];
static lv_obj_t *s_values[ITEM_COUNT];
static lv_obj_t *s_bars[ITEM_COUNT];
static lv_obj_t *s_group_rows[GROUP_COUNT];
static lv_obj_t *s_group_title;
static lv_obj_t *s_list;
static lv_obj_t *s_host_rows[HOST_SLOT_COUNT];
static lv_obj_t *s_host_states[HOST_SLOT_COUNT];
static lv_obj_t *s_host_dots[HOST_SLOT_COUNT];
static lv_obj_t *s_battery_level;
static lv_obj_t *s_battery_voltage;
static lv_obj_t *s_battery_power;
static lv_obj_t *s_usage_rows[USAGE_SOURCE_COUNT];
static lv_obj_t *s_usage_switches[USAGE_SOURCE_COUNT];
static const char *const s_usage_names[USAGE_SOURCE_COUNT] = { "Claude", "Codex", "Cursor" };
static int s_selected;
static setting_group_t s_group;
static lv_obj_t *s_sidebar;

// Left column: groups at 30 px + 6 px gap; scroll when they do not fit the round screen.
#define SIDEBAR_ROW_HEIGHT  30
#define SIDEBAR_ROW_GAP     6
#define SIDEBAR_VIEW_HEIGHT 186
#define DETAIL_VIEW_HEIGHT  192

static void refresh_usage_sources(void);
static void on_usage_switch(lv_event_t *e);
static void scroll_group_into_view(setting_group_t group);
static bool group_uses_value_knob(void);

static void style_detail_row(lv_obj_t *row, bool clickable)
{
    lv_obj_set_style_radius(row, 0, 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, theme_button_dim(), 0);
    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_pad_hor(row, 10, 0);
    lv_obj_set_clickable(row, clickable);
}

static void set_detail_selected(lv_obj_t *row, bool selected)
{
    lv_obj_set_style_bg_color(row, theme_accent(), 0);
    lv_obj_set_style_bg_opa(row, selected ? LV_OPA_20 : LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(row, selected ? theme_accent_text() : theme_button_dim(), 0);
    lv_obj_set_style_border_width(row, selected ? 3 : 1, 0);
    lv_obj_set_style_border_side(row, selected ? LV_BORDER_SIDE_LEFT : LV_BORDER_SIDE_BOTTOM, 0);
}

static int32_t *item_value(const item_t *item)
{
    return (int32_t *)((char *)settings() + item->offset);
}

static int sleep_timeout_index(void)
{
    const int timeout = settings_sleep_timeout();
    const int count = sizeof(s_sleep_timeout_values) / sizeof(s_sleep_timeout_values[0]);
    for (int i = 0; i < count; i++) {
        if (s_sleep_timeout_values[i] == timeout) {
            return i;
        }
    }
    return 2;
}

static void render_value(int i)
{
    if (s_values[i] == NULL) {
        return;
    }
    const item_t *item = &s_items[i];
    const int v = item->format == FMT_SLEEP_TIMEOUT ? settings_sleep_timeout() : (int)*item_value(item);
    char text[16];
    switch (item->format) {
    case FMT_X100:
        lv_snprintf(text, sizeof(text), "%d.%02d", v / 100, v % 100);
        break;
    case FMT_INT:
        lv_snprintf(text, sizeof(text), "%d", v);
        break;
    case FMT_PERCENT:
        lv_snprintf(text, sizeof(text), "%d%%", v);
        break;
    case FMT_DIRECTION:
        lv_snprintf(text, sizeof(text), "%s", v ? "Reverse" : "Normal");
        break;
    case FMT_SLEEP_TIMEOUT:
        if (v == 0) {
            lv_snprintf(text, sizeof(text), "Never");
        } else if (v < 60) {
            lv_snprintf(text, sizeof(text), "%ds", v);
        } else {
            lv_snprintf(text, sizeof(text), "%dm", v / 60);
        }
        break;
    }
    lv_label_set_text(s_values[i], text);
    if (s_bars[i] != NULL) {
        lv_bar_set_value(s_bars[i], v, LV_ANIM_OFF);
    }
}

static void select_row(int i)
{
    s_selected = i;
    for (int row = 0; row < ITEM_COUNT; row++) {
        if (s_rows[row] != NULL) {
            set_detail_selected(s_rows[row], row == i);
        }
    }
    if (i >= 0 && i < ITEM_COUNT && s_rows[i] != NULL) {
        lv_obj_scroll_to_view(s_rows[i], LV_ANIM_OFF);
    }
}

static void on_row(lv_event_t *e)
{
    const int index = (int)(intptr_t)lv_event_get_user_data(e);
    select_row(index);
}

static void refresh_hosts(void);
static void refresh_battery(void);
static void on_host_tap(lv_event_t *e);
static void on_host_hold(lv_event_t *e);

static void show_group(setting_group_t group)
{
    const bool new_group = group != s_group;
    s_group = group;
    s_battery_level = NULL;
    s_battery_voltage = NULL;
    s_battery_power = NULL;
    for (int i = 0; i < ITEM_COUNT; i++) {
        s_rows[i] = NULL;
        s_values[i] = NULL;
        s_bars[i] = NULL;
    }
    s_selected = -1;
    for (int i = 0; i < HOST_SLOT_COUNT; i++) {
        s_host_rows[i] = NULL;
        s_host_states[i] = NULL;
        s_host_dots[i] = NULL;
    }
    for (int i = 0; i < USAGE_SOURCE_COUNT; i++) {
        s_usage_rows[i] = NULL;
        s_usage_switches[i] = NULL;
    }

    for (int i = 0; i < GROUP_COUNT; i++) {
        lv_obj_set_style_border_width(s_group_rows[i], i == group ? 3 : 0, 0);
        lv_obj_set_style_text_color(lv_obj_get_child(s_group_rows[i], 0),
                                    i == group ? theme_text() : theme_text_muted(), 0);
    }
    lv_label_set_text(s_group_title, s_group_names[group]);
    scroll_group_into_view(group);

    lv_obj_clean(s_list);
    if (new_group && s_list != NULL) {
        lv_obj_scroll_to_y(s_list, 0, LV_ANIM_OFF);
    }
    if (group == GROUP_BATTERY) {
        lv_obj_t *level = widget_row(s_list, "Level", &s_battery_level, NULL, 0, NULL);
        lv_obj_t *voltage = widget_row(s_list, "Voltage", &s_battery_voltage, NULL, 0, NULL);
        lv_obj_t *power = widget_row(s_list, "Power", &s_battery_power, NULL, 0, NULL);
        style_detail_row(level, false);
        style_detail_row(voltage, false);
        style_detail_row(power, false);
        lv_obj_set_height(level, 42);
        lv_obj_set_height(voltage, 42);
        lv_obj_set_style_text_font(s_battery_level, THEME_FONT_TITLE, 0);
        lv_obj_set_style_text_font(s_battery_voltage, THEME_FONT_TITLE, 0);
        refresh_battery();
        return;
    }
    if (group == GROUP_BLUETOOTH) {
        for (int i = 0; i < HOST_SLOT_COUNT; i++) {
            char label[24];
            lv_snprintf(label, sizeof(label), LV_SYMBOL_BLUETOOTH "  Host %d", i + 1);
            s_host_rows[i] = widget_row(s_list, label, &s_host_states[i], on_host_tap,
                                        LV_EVENT_SHORT_CLICKED, (void *)(intptr_t)i);
            style_detail_row(s_host_rows[i], true);
            s_host_dots[i] = lv_obj_create(s_host_rows[i]);
            lv_obj_remove_style_all(s_host_dots[i]);
            lv_obj_set_size(s_host_dots[i], 6, 6);
            lv_obj_set_style_radius(s_host_dots[i], LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_bg_opa(s_host_dots[i], LV_OPA_COVER, 0);
            lv_obj_set_style_bg_color(s_host_dots[i], theme_accent_text(), 0);
            lv_obj_set_clickable(s_host_dots[i], false);
            lv_obj_align(s_host_dots[i], LV_ALIGN_LEFT_MID, -7, 0);
            lv_obj_add_event_cb(s_host_rows[i], on_host_hold, LV_EVENT_LONG_PRESSED, (void *)(intptr_t)i);
        }
        refresh_hosts();
        return;
    }
    if (group == GROUP_USAGE) {
        for (int i = 0; i < USAGE_SOURCE_COUNT; i++) {
            s_usage_rows[i] = widget_toggle_row(s_list, s_usage_names[i], &s_usage_switches[i],
                                                on_usage_switch, (void *)(intptr_t)i);
            style_detail_row(s_usage_rows[i], false);
        }
        refresh_usage_sources();
        return;
    }

    for (int i = 0; i < ITEM_COUNT; i++) {
        if (s_items[i].group != group) {
            continue;
        }
        s_rows[i] = widget_row(s_list, s_items[i].name, &s_values[i], on_row, LV_EVENT_SHORT_CLICKED,
                               (void *)(intptr_t)i);
        style_detail_row(s_rows[i], true);
        lv_obj_set_scroll_dir(s_rows[i], LV_DIR_NONE);
        const bool numeric = s_items[i].format == FMT_X100 || s_items[i].format == FMT_INT ||
                             s_items[i].format == FMT_PERCENT;
        if (numeric) {
            lv_obj_set_height(s_rows[i], 48);
            lv_obj_align(lv_obj_get_child(s_rows[i], 0), LV_ALIGN_TOP_LEFT, 0, 5);
            lv_obj_align(s_values[i], LV_ALIGN_TOP_RIGHT, 0, 5);
            s_bars[i] = lv_bar_create(s_rows[i]);
            lv_obj_set_size(s_bars[i], 166, 3);
            lv_obj_align(s_bars[i], LV_ALIGN_BOTTOM_MID, 0, -7);
            lv_obj_set_style_radius(s_bars[i], 0, LV_PART_MAIN);
            lv_obj_set_style_radius(s_bars[i], 0, LV_PART_INDICATOR);
            lv_obj_set_style_bg_color(s_bars[i], theme_button(), LV_PART_MAIN);
            lv_obj_set_style_bg_color(s_bars[i], theme_accent_text(), LV_PART_INDICATOR);
            lv_obj_set_clickable(s_bars[i], false);
            int min, max;
            settings_range(s_items[i].offset, &min, &max);
            lv_bar_set_range(s_bars[i], min, max);
        }
        set_detail_selected(s_rows[i], i == s_selected);
        render_value(i);
    }
}

static void scroll_group_into_view(setting_group_t group)
{
    if (s_sidebar == NULL || s_group_rows[group] == NULL) {
        return;
    }
    lv_obj_scroll_to_view(s_group_rows[group], LV_ANIM_OFF);
}

static bool group_uses_value_knob(void)
{
    return s_group == GROUP_TRACKPAD || s_group == GROUP_DISPLAY;
}

static void on_group(lv_event_t *e)
{
    const setting_group_t group = (setting_group_t)(intptr_t)lv_event_get_user_data(e);
    if (group == s_group) {
        select_row(-1);
        return;
    }
    show_group(group);
}

static void refresh_hosts(void)
{
    const int active = ble_hid_host();
    const ble_hid_state_t state = ble_hid_state();
    for (int i = 0; i < HOST_SLOT_COUNT; i++) {
        if (s_host_rows[i] == NULL) {
            continue;
        }
        const char *text = "Empty";
        if (i == active) {
            text = state == BLE_HID_STATE_CONNECTED ? "Connected"
                   : state == BLE_HID_STATE_PAIRING ? "Pairing..."
                   : "Waiting";
        } else if (ble_hid_host_paired(i)) {
            text = "Paired";
        }
        lv_label_set_text(s_host_states[i], text);
        lv_obj_set_hidden(s_host_dots[i], i != active);
    }
}

static void refresh_battery(void)
{
    const bool external = battery_charging();
    const int percent = battery_percent();
    const int mv = battery_voltage_mv();
    if (external || percent < 0) {
        lv_label_set_text(s_battery_level, "--");
    } else {
        lv_label_set_text_fmt(s_battery_level, "%d%%", percent);
    }
    if (mv < 0) {
        lv_label_set_text(s_battery_voltage, "--");
    } else {
        lv_label_set_text_fmt(s_battery_voltage, "%d.%03dV", mv / 1000, mv % 1000);
    }
    lv_label_set_text(s_battery_power, mv < 0 ? "--" : external ? "External" : "Battery");
}

static void refresh_usage_sources(void)
{
    for (int i = 0; i < USAGE_SOURCE_COUNT; i++) {
        if (s_usage_switches[i] == NULL) {
            continue;
        }
        widget_toggle_set(s_usage_switches[i], usage_sources_enabled(i));
    }
}

static void on_usage_switch(lv_event_t *e)
{
    const int index = (int)(intptr_t)lv_event_get_user_data(e);
    const bool on = widget_toggle_get(lv_event_get_target(e));
    usage_sources_set_enabled(index, on);
    config_service_notify_usage_sources();
}

static void on_host_tap(lv_event_t *e)
{
    const int slot = (int)(intptr_t)lv_event_get_user_data(e);
    if (slot != ble_hid_host() || ble_hid_state() != BLE_HID_STATE_CONNECTED) {
        ble_hid_select_host(slot);
    }
}

static void on_host_hold(lv_event_t *e)
{
    ble_hid_pair_host((int)(intptr_t)lv_event_get_user_data(e));
}

static void knob(int detents)
{
    if (detents == 0) {
        return;
    }
    if (!group_uses_value_knob() || s_selected < 0) {
        int group = (int)s_group + detents;
        while (group < 0) {
            group += GROUP_COUNT;
        }
        show_group((setting_group_t)(group % GROUP_COUNT));
        return;
    }
    const item_t *item = &s_items[s_selected];
    if (item->format == FMT_SLEEP_TIMEOUT) {
        const int count = sizeof(s_sleep_timeout_values) / sizeof(s_sleep_timeout_values[0]);
        int index = (sleep_timeout_index() + detents) % count;
        if (index < 0) {
            index += count;
        }
        const int seconds = s_sleep_timeout_values[index];
        if (seconds != settings_sleep_timeout()) {
            settings_set_sleep_timeout(seconds);
            display_set_sleep_timeout(seconds);
            render_value(s_selected);
        }
        return;
    }
    int32_t *value = item_value(item);
    int min, max;
    settings_range(item->offset, &min, &max);
    int v = *value + detents * item->step;
    v = v < min ? min : (v > max ? max : v);
    if (v == *value) {
        return;
    }
    *value = v;
    if (item->offset == offsetof(settings_t, brightness)) {
        display_set_brightness(v);
    }
    render_value(s_selected);
    settings_changed();
}

// Runs on the NimBLE host task when the Mac app replaces the settings.
static void on_usage_sources_change(void)
{
    lvgl_port_lock(0);
    const lv_coord_t scroll_y = s_list != NULL ? lv_obj_get_scroll_y(s_list) : 0;
    refresh_usage_sources();
    if (s_list != NULL) {
        lv_obj_scroll_to_y(s_list, scroll_y, LV_ANIM_OFF);
    }
    lvgl_port_unlock();
}

static void on_settings_change(void)
{
    lvgl_port_lock(0);
    const lv_coord_t scroll_y = s_list != NULL ? lv_obj_get_scroll_y(s_list) : 0;
    for (int i = 0; i < ITEM_COUNT; i++) {
        render_value(i);
    }
    refresh_hosts();
    if (s_list != NULL) {
        lv_obj_scroll_to_y(s_list, scroll_y, LV_ANIM_OFF);
    }
    lvgl_port_unlock();
    display_set_brightness(settings()->brightness);
    display_set_sleep_timeout(settings_sleep_timeout());
}

static void tick(void)
{
    if (s_group == GROUP_BLUETOOTH) {
        refresh_hosts();
    } else if (s_group == GROUP_BATTERY) {
        refresh_battery();
    }
}

static void create(lv_obj_t *scr)
{
    widget_title(scr, "Settings");
    s_sidebar = widget_list(scr, 0);
    lv_obj_set_width(s_sidebar, 100);
    lv_obj_set_height(s_sidebar, SIDEBAR_VIEW_HEIGHT);
    lv_obj_set_style_pad_all(s_sidebar, 0, 0);
    lv_obj_set_style_pad_row(s_sidebar, SIDEBAR_ROW_GAP, 0);
    lv_obj_set_flex_align(s_sidebar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_scroll_dir(s_sidebar, LV_DIR_VER);
    lv_obj_clear_flag(s_sidebar, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_set_scrollbar_mode(s_sidebar, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scrollable(s_sidebar, true);
    lv_obj_align(s_sidebar, LV_ALIGN_TOP_LEFT, 34, 100);
    for (int i = 0; i < GROUP_COUNT; i++) {
        s_group_rows[i] = widget_row(s_sidebar, s_group_names[i], NULL, on_group,
                                     LV_EVENT_SHORT_CLICKED, (void *)(intptr_t)i);
        lv_obj_set_height(s_group_rows[i], SIDEBAR_ROW_HEIGHT);
        lv_obj_set_style_radius(s_group_rows[i], 0, 0);
        lv_obj_set_style_bg_opa(s_group_rows[i], LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_color(s_group_rows[i], theme_accent_text(), 0);
        lv_obj_set_style_border_side(s_group_rows[i], LV_BORDER_SIDE_LEFT, 0);
        lv_obj_set_style_pad_hor(s_group_rows[i], 10, 0);
        lv_obj_t *name = lv_obj_get_child(s_group_rows[i], 0);
        lv_obj_set_width(name, lv_pct(100));
        lv_label_set_long_mode(name, LV_LABEL_LONG_MODE_DOTS);
    }

    s_list = widget_list(scr, 0);
    lv_obj_set_width(s_list, 192);
    lv_obj_set_height(s_list, DETAIL_VIEW_HEIGHT);
    lv_obj_set_style_pad_row(s_list, 0, 0);
    lv_obj_set_scroll_dir(s_list, LV_DIR_VER);
    lv_obj_clear_flag(s_list, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_add_flag(s_list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(s_list, LV_SCROLLBAR_MODE_OFF);
    lv_obj_align(s_list, LV_ALIGN_TOP_LEFT, 138, 121);
    s_group_title = lv_label_create(scr);
    lv_obj_set_style_text_font(s_group_title, THEME_FONT_BODY, 0);
    lv_obj_set_style_text_color(s_group_title, theme_accent_text(), 0);
    lv_obj_align(s_group_title, LV_ALIGN_TOP_LEFT, 150, 94);
    show_group(GROUP_TRACKPAD);

    settings_on_change(on_settings_change);
    usage_sources_on_change(on_usage_sources_change);
}

const screen_def_t settings_screen = {
    .name = "Settings",
    .icon = LV_SYMBOL_SETTINGS,
    .create = create,
    .knob = knob,
    .tick = tick,
};

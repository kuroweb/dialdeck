// Touchpad, knob and display settings. Tap a row to select it, turn the knob to change it; the
// selected row shows arrows around its value.
#include <stddef.h>

#include "display.h"
#include "esp_lvgl_port.h"
#include "screen.h"
#include "settings.h"
#include "theme.h"
#include "widgets.h"

typedef enum {
    GROUP_POINTER,
    GROUP_SCROLL,
    GROUP_DISPLAY,
    GROUP_COUNT,
} setting_group_t;

static const char *const s_group_names[GROUP_COUNT] = { "Pointer", "Scroll", "Display" };

typedef enum {
    FMT_X100,
    FMT_INT,
    FMT_PERCENT,
    FMT_DIRECTION,
    FMT_TIMEOUT,
} item_format_t;

typedef struct {
    const char *name;
    setting_group_t group;
    size_t offset;   // field in settings_t; its range comes from settings_range()
    int step;
    item_format_t format;
} item_t;

static const item_t s_items[] = {
    { "Pointer speed",  GROUP_POINTER, offsetof(settings_t, pointer_gain),     25, FMT_X100 },
    { "Acceleration",   GROUP_POINTER, offsetof(settings_t, pointer_accel),    5,  FMT_X100 },
    { "Max speed",      GROUP_POINTER, offsetof(settings_t, pointer_gain_max), 50, FMT_X100 },
    { "Scroll step",    GROUP_SCROLL,  offsetof(settings_t, scroll_step),      1,  FMT_INT },
    { "Scroll dir",     GROUP_SCROLL,  offsetof(settings_t, scroll_reverse),   1,  FMT_DIRECTION },
    { "Brightness",     GROUP_DISPLAY, offsetof(settings_t, brightness),       1,  FMT_PERCENT },
    { "Screen timeout", GROUP_DISPLAY, 0,                                      1,  FMT_TIMEOUT },
};
#define ITEM_COUNT  (sizeof(s_items) / sizeof(s_items[0]))

static const int s_timeout_values[] = { 0, 15, 30, 60, 120, 300 };

static lv_obj_t *s_rows[ITEM_COUNT];
static lv_obj_t *s_values[ITEM_COUNT];
static lv_obj_t *s_group_buttons[GROUP_COUNT];
static lv_obj_t *s_list;
static int s_selected;

static int32_t *item_value(const item_t *item)
{
    return (int32_t *)((char *)settings() + item->offset);
}

static int timeout_index(void)
{
    const int timeout = settings_display_timeout();
    const int count = sizeof(s_timeout_values) / sizeof(s_timeout_values[0]);
    for (int i = 0; i < count; i++) {
        if (s_timeout_values[i] == timeout) {
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
    const int v = item->format == FMT_TIMEOUT ? settings_display_timeout() : (int)*item_value(item);
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
    case FMT_TIMEOUT:
        if (v == 0) {
            lv_snprintf(text, sizeof(text), "Never");
        } else if (v < 60) {
            lv_snprintf(text, sizeof(text), "%ds", v);
        } else {
            lv_snprintf(text, sizeof(text), "%dm", v / 60);
        }
        break;
    }
    // Arrows on the selected row show that the knob changes it.
    if (i == s_selected) {
        lv_label_set_text_fmt(s_values[i], LV_SYMBOL_LEFT " %s " LV_SYMBOL_RIGHT, text);
    } else {
        lv_label_set_text(s_values[i], text);
    }
}

static void select_row(int i)
{
    const int previous = s_selected;
    s_selected = i;
    widget_row_set_selected(s_rows[previous], false);
    widget_row_set_selected(s_rows[i], true);
    render_value(previous);
    render_value(i);
}

static void on_row(lv_event_t *e)
{
    select_row((int)(intptr_t)lv_event_get_user_data(e));
}

static void show_group(setting_group_t group)
{
    int first = -1;
    for (int i = 0; i < ITEM_COUNT; i++) {
        s_rows[i] = NULL;
        s_values[i] = NULL;
        if (first < 0 && s_items[i].group == group) {
            first = i;
        }
    }
    s_selected = first;

    for (int i = 0; i < GROUP_COUNT; i++) {
        lv_obj_set_style_bg_color(s_group_buttons[i], i == group ? theme_accent() : theme_button(), 0);
    }

    lv_obj_clean(s_list);
    for (int i = 0; i < ITEM_COUNT; i++) {
        if (s_items[i].group != group) {
            continue;
        }
        s_rows[i] = widget_row(s_list, s_items[i].name, &s_values[i], on_row, LV_EVENT_CLICKED,
                               (void *)(intptr_t)i);
        widget_row_set_selected(s_rows[i], i == s_selected);
        render_value(i);
    }
}

static void on_group(lv_event_t *e)
{
    show_group((setting_group_t)(intptr_t)lv_event_get_user_data(e));
}

static void knob(int detents)
{
    const item_t *item = &s_items[s_selected];
    if (item->format == FMT_TIMEOUT) {
        const int count = sizeof(s_timeout_values) / sizeof(s_timeout_values[0]);
        int index = (timeout_index() + detents) % count;
        if (index < 0) {
            index += count;
        }
        const int seconds = s_timeout_values[index];
        if (seconds != settings_display_timeout()) {
            settings_set_display_timeout(seconds);
            display_set_idle_timeout(seconds);
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
static void on_settings_change(void)
{
    lvgl_port_lock(0);
    for (int i = 0; i < ITEM_COUNT; i++) {
        render_value(i);
    }
    lvgl_port_unlock();
    display_set_brightness(settings()->brightness);
    display_set_idle_timeout(settings_display_timeout());
}

static void create(lv_obj_t *scr)
{
    widget_title(scr, "Settings");
    lv_obj_t *group_row = widget_button_row(scr, -82);
    for (int i = 0; i < GROUP_COUNT; i++) {
        s_group_buttons[i] = widget_button(group_row, s_group_names[i], false, on_group, (void *)(intptr_t)i);
    }

    s_list = widget_list(scr, 24);
    lv_obj_set_width(s_list, 250);
    lv_obj_set_scrollable(s_list, false);
    widget_hint(scr, "Tap a row, turn knob to change");
    show_group(GROUP_POINTER);

    settings_on_change(on_settings_change);
}

const screen_def_t settings_screen = {
    .name = "Settings",
    .icon = LV_SYMBOL_SETTINGS,
    .create = create,
    .knob = knob,
};

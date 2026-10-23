// Touchpad, knob and display settings. Tap a row to select it, turn the knob to change it; the
// selected row shows arrows around its value.
#include <stddef.h>

#include "display.h"
#include "esp_lvgl_port.h"
#include "screen.h"
#include "settings.h"
#include "widgets.h"

typedef enum {
    FMT_X100,
    FMT_INT,
    FMT_PERCENT,
    FMT_DIRECTION,
} item_format_t;

typedef struct {
    const char *name;
    size_t offset;   // field in settings_t; its range comes from settings_range()
    int step;
    item_format_t format;
} item_t;

static const item_t s_items[] = {
    { "Pointer speed", offsetof(settings_t, pointer_gain),     25, FMT_X100 },
    { "Acceleration",  offsetof(settings_t, pointer_accel),    5,  FMT_X100 },
    { "Max speed",     offsetof(settings_t, pointer_gain_max), 50, FMT_X100 },
    { "Scroll step",   offsetof(settings_t, scroll_step),      1,  FMT_INT },
    { "Scroll dir",    offsetof(settings_t, scroll_reverse),   1,  FMT_DIRECTION },
    { "Brightness",    offsetof(settings_t, brightness),       1,  FMT_PERCENT },
};
#define ITEM_COUNT  (sizeof(s_items) / sizeof(s_items[0]))

static lv_obj_t *s_rows[ITEM_COUNT];
static lv_obj_t *s_values[ITEM_COUNT];
static int s_selected;

static int32_t *item_value(const item_t *item)
{
    return (int32_t *)((char *)settings() + item->offset);
}

static void render_value(int i)
{
    const item_t *item = &s_items[i];
    const int v = (int)*item_value(item);
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

static void knob(int detents)
{
    const item_t *item = &s_items[s_selected];
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
}

static void create(lv_obj_t *scr)
{
    widget_title(scr, "Settings");
    lv_obj_t *list = widget_list(scr, 16);
    for (int i = 0; i < ITEM_COUNT; i++) {
        s_rows[i] = widget_row(list, s_items[i].name, &s_values[i], on_row, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        render_value(i);
    }
    select_row(0);

    settings_on_change(on_settings_change);
}

const screen_def_t settings_screen = {
    .name = "Settings",
    .icon = LV_SYMBOL_SETTINGS,
    .create = create,
    .knob = knob,
};

#include "settings_screen.h"

#include <stddef.h>

#include "display.h"
#include "esp_lvgl_port.h"
#include "settings.h"
#include "theme.h"

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
    { "Brightness",    offsetof(settings_t, brightness),       10, FMT_PERCENT },
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
    switch (item->format) {
    case FMT_X100:
        lv_label_set_text_fmt(s_values[i], "%d.%02d", v / 100, v % 100);
        break;
    case FMT_INT:
        lv_label_set_text_fmt(s_values[i], "%d", v);
        break;
    case FMT_PERCENT:
        lv_label_set_text_fmt(s_values[i], "%d%%", v);
        break;
    case FMT_DIRECTION:
        lv_label_set_text(s_values[i], v ? "Reverse" : "Normal");
        break;
    }
}

static void select_row(int i)
{
    lv_obj_set_style_bg_color(s_rows[s_selected], theme_button_dim(), 0);
    s_selected = i;
    lv_obj_set_style_bg_color(s_rows[s_selected], theme_accent(), 0);
}

static void on_row(lv_event_t *e)
{
    select_row((int)(intptr_t)lv_event_get_user_data(e));
}

void settings_screen_knob(int detents)
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

void settings_screen_create(lv_obj_t *scr)
{
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Settings");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 52);

    lv_obj_t *list = lv_obj_create(scr);
    lv_obj_remove_style_all(list);
    lv_obj_set_size(list, 260, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(list, 4, 0);
    lv_obj_set_event_bubble(list, true);
    lv_obj_align(list, LV_ALIGN_CENTER, 0, 10);

    for (int i = 0; i < ITEM_COUNT; i++) {
        lv_obj_t *row = lv_obj_create(list);
        lv_obj_remove_style_all(row);
        lv_obj_set_size(row, LV_PCT(100), 32);
        lv_obj_set_style_radius(row, 16, 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(row, theme_button_dim(), 0);
        lv_obj_set_style_pad_hor(row, 14, 0);
        lv_obj_set_clickable(row, true);
        lv_obj_set_event_bubble(row, true);
        lv_obj_add_event_cb(row, on_row, LV_EVENT_CLICKED, (void *)(intptr_t)i);

        lv_obj_t *name = lv_label_create(row);
        lv_label_set_text(name, s_items[i].name);
        lv_obj_set_style_text_color(name, lv_color_white(), 0);
        lv_obj_align(name, LV_ALIGN_LEFT_MID, 0, 0);

        s_values[i] = lv_label_create(row);
        lv_obj_set_style_text_color(s_values[i], lv_color_white(), 0);
        lv_obj_align(s_values[i], LV_ALIGN_RIGHT_MID, 0, 0);

        s_rows[i] = row;
        render_value(i);
    }
    select_row(0);

    lv_obj_t *hint = lv_label_create(scr);
    lv_label_set_text(hint, "Tap an item, turn the knob");
    lv_obj_set_style_text_color(hint, theme_text_muted(), 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -40);

    settings_on_change(on_settings_change);
}

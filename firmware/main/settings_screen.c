#include "settings_screen.h"

#include <stddef.h>

#include "display.h"
#include "settings.h"
#include "ui.h"

typedef enum {
    FMT_X100,
    FMT_INT,
    FMT_PERCENT,
    FMT_DIRECTION,
} item_format_t;

typedef struct {
    const char *name;
    size_t offset;   // field in settings_t
    int min;
    int max;
    int step;
    item_format_t format;
} item_t;

static const item_t s_items[] = {
    { "Pointer speed", offsetof(settings_t, pointer_gain),     100, 500,  25, FMT_X100 },
    { "Acceleration",  offsetof(settings_t, pointer_accel),    0,   50,   5,  FMT_X100 },
    { "Max speed",     offsetof(settings_t, pointer_gain_max), 100, 1000, 50, FMT_X100 },
    { "Scroll step",   offsetof(settings_t, scroll_step),      1,   5,    1,  FMT_INT },
    { "Scroll dir",    offsetof(settings_t, scroll_reverse),   0,   1,    1,  FMT_DIRECTION },
    { "Brightness",    offsetof(settings_t, brightness),       10,  100,  10, FMT_PERCENT },
};
#define ITEM_COUNT  (sizeof(s_items) / sizeof(s_items[0]))

static lv_obj_t *s_rows[ITEM_COUNT];
static lv_obj_t *s_values[ITEM_COUNT];
static int s_selected;

static int *item_value(const item_t *item)
{
    return (int *)((char *)settings() + item->offset);
}

static void render_value(int i)
{
    const item_t *item = &s_items[i];
    const int v = *item_value(item);
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
    lv_obj_set_style_bg_opa(s_rows[s_selected], LV_OPA_TRANSP, 0);
    s_selected = i;
    lv_obj_set_style_bg_opa(s_rows[s_selected], LV_OPA_COVER, 0);
}

static void on_row(lv_event_t *e)
{
    select_row((int)(intptr_t)lv_event_get_user_data(e));
}

void settings_screen_knob(int detents)
{
    const item_t *item = &s_items[s_selected];
    int *value = item_value(item);
    int v = *value + detents * item->step;
    v = v < item->min ? item->min : (v > item->max ? item->max : v);
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

void settings_screen_create(lv_obj_t *scr)
{
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Settings");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 40);

    lv_obj_t *list = lv_obj_create(scr);
    lv_obj_remove_style_all(list);
    lv_obj_set_size(list, 260, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_event_bubble(list, true);
    lv_obj_align(list, LV_ALIGN_CENTER, 0, 10);

    for (int i = 0; i < ITEM_COUNT; i++) {
        lv_obj_t *row = lv_obj_create(list);
        lv_obj_remove_style_all(row);
        lv_obj_set_size(row, LV_PCT(100), 34);
        lv_obj_set_style_radius(row, 8, 0);
        lv_obj_set_style_bg_color(row, lv_palette_darken(LV_PALETTE_BLUE, 3), 0);
        lv_obj_set_style_pad_hor(row, 10, 0);
        lv_obj_set_clickable(row, true);
        lv_obj_set_event_bubble(row, true);
        lv_obj_add_event_cb(row, on_row, LV_EVENT_CLICKED, (void *)(intptr_t)i);

        lv_obj_t *name = lv_label_create(row);
        lv_label_set_text(name, s_items[i].name);
        lv_obj_set_style_text_color(name, lv_color_white(), 0);
        lv_obj_align(name, LV_ALIGN_LEFT_MID, 0, 0);

        s_values[i] = lv_label_create(row);
        lv_obj_set_style_text_color(s_values[i], lv_palette_main(LV_PALETTE_AMBER), 0);
        lv_obj_align(s_values[i], LV_ALIGN_RIGHT_MID, 0, 0);

        s_rows[i] = row;
        render_value(i);
    }
    select_row(0);

    lv_obj_t *hint = lv_label_create(scr);
    lv_label_set_text(hint, "Tap an item, turn the knob");
    lv_obj_set_style_text_color(hint, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -46);

    ui_add_handle(scr, LV_ALIGN_BOTTOM_MID, -14);
}

#include "widgets.h"

#include "theme.h"

#define BUTTON_WIDTH   92
#define BUTTON_HEIGHT  40

lv_obj_t *widget_title(lv_obj_t *scr, const char *text)
{
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, text);
    lv_obj_set_style_text_font(title, THEME_FONT_TITLE, 0);
    lv_obj_set_style_text_color(title, theme_text(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, THEME_TITLE_Y);
    return title;
}

lv_obj_t *widget_hint(lv_obj_t *scr, const char *text)
{
    lv_obj_t *hint = lv_label_create(scr);
    lv_label_set_text(hint, text);
    lv_obj_set_style_text_font(hint, THEME_FONT_BODY, 0);
    lv_obj_set_style_text_color(hint, theme_text_muted(), 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, THEME_HINT_Y);
    return hint;
}

lv_obj_t *widget_list(lv_obj_t *scr, int y)
{
    lv_obj_t *list = lv_obj_create(scr);
    lv_obj_remove_style_all(list);
    lv_obj_set_size(list, THEME_ROW_WIDTH, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(list, THEME_ROW_GAP, 0);
    lv_obj_set_event_bubble(list, true);
    lv_obj_align(list, LV_ALIGN_CENTER, 0, y);
    return list;
}

lv_obj_t *widget_row(lv_obj_t *list, const char *label, lv_obj_t **value,
                     lv_event_cb_t cb, lv_event_code_t code, void *user_data)
{
    lv_obj_t *row = lv_obj_create(list);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_PCT(100), THEME_ROW_HEIGHT);
    lv_obj_set_style_radius(row, THEME_ROW_HEIGHT / 2, 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(row, theme_button_dim(), 0);
    lv_obj_set_style_pad_hor(row, 18, 0);
    lv_obj_set_clickable(row, true);
    // Let the screen see the touch too, so a pull-down that starts on a row still opens the menu.
    lv_obj_set_event_bubble(row, true);
    if (cb) {
        lv_obj_add_event_cb(row, cb, code, user_data);
    }

    lv_obj_t *name = lv_label_create(row);
    lv_label_set_text(name, label);
    lv_obj_set_style_text_font(name, THEME_FONT_BODY, 0);
    lv_obj_set_style_text_color(name, theme_text(), 0);
    lv_obj_align(name, LV_ALIGN_LEFT_MID, 0, 0);

    if (value) {
        *value = lv_label_create(row);
        lv_obj_set_style_text_font(*value, THEME_FONT_BODY, 0);
        lv_obj_set_style_text_color(*value, theme_text(), 0);
        lv_obj_align(*value, LV_ALIGN_RIGHT_MID, 0, 0);
    }
    return row;
}

void widget_row_set_selected(lv_obj_t *row, bool selected)
{
    lv_obj_set_style_bg_color(row, selected ? theme_accent() : theme_button_dim(), 0);
}

lv_obj_t *widget_button(lv_obj_t *parent, const char *text, bool accent, lv_event_cb_t cb, void *user_data)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, BUTTON_WIDTH, BUTTON_HEIGHT);
    lv_obj_set_style_radius(btn, BUTTON_HEIGHT / 2, 0);
    lv_obj_set_style_bg_color(btn, accent ? theme_accent() : theme_button(), 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_event_bubble(btn, true);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, THEME_FONT_BODY, 0);
    lv_obj_center(label);
    return btn;
}

lv_obj_t *widget_circle(lv_obj_t *parent, int diameter, lv_color_t color, lv_event_cb_t cb, void *user_data)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, diameter, diameter);
    lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(btn, color, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_border_color(btn, theme_text(), 0);
    // Let the screen see the touch too, so a pull-down that starts on a circle still opens the menu.
    lv_obj_set_event_bubble(btn, true);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);

    lv_obj_t *label = lv_label_create(btn);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(label);
    return btn;
}

lv_obj_t *widget_button_row(lv_obj_t *scr, int y)
{
    lv_obj_t *row = lv_obj_create(scr);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row, 10, 0);
    lv_obj_set_event_bubble(row, true);
    lv_obj_align(row, LV_ALIGN_CENTER, 0, y);
    return row;
}

void widget_handle(lv_obj_t *scr, lv_align_t align, int y)
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

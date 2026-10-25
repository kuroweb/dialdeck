// Building blocks shared by the screens, drawn with theme.h. A screen composes these rather than
// styling LVGL objects itself.
#pragma once

#include <stdbool.h>

#include "lvgl.h"

// Screen title at the top of a list screen.
lv_obj_t *widget_title(lv_obj_t *scr, const char *text);

// One line of help at the bottom of a screen.
lv_obj_t *widget_hint(lv_obj_t *scr, const char *text);

// Column of rows, centered on the screen (shifted down by `y`).
lv_obj_t *widget_list(lv_obj_t *scr, int y);

// Row of a list: a label on the left and an optional value on the right (`value` may be NULL).
// `cb` runs on `code` (e.g. LV_EVENT_CLICKED) with `user_data`.
lv_obj_t *widget_row(lv_obj_t *list, const char *label, lv_obj_t **value,
                     lv_event_cb_t cb, lv_event_code_t code, void *user_data);

// Mark a row as the selected or current one.
void widget_row_set_selected(lv_obj_t *row, bool selected);

// Settings-style row with a label and an LVGL switch on the right (`toggle` receives the switch).
lv_obj_t *widget_toggle_row(lv_obj_t *list, const char *label, lv_obj_t **toggle,
                            lv_event_cb_t cb, void *user_data);

void widget_toggle_set(lv_obj_t *toggle, bool on);
bool widget_toggle_get(lv_obj_t *toggle);

// Rounded button with a text label. `accent` draws it as the primary action.
lv_obj_t *widget_button(lv_obj_t *parent, const char *text, bool accent, lv_event_cb_t cb, void *user_data);

// Round button, used for items on a ring (see ring.h). Its label is the first child.
lv_obj_t *widget_circle(lv_obj_t *parent, int diameter, lv_color_t color, lv_event_cb_t cb, void *user_data);

// Horizontal group of buttons, centered on the screen (shifted down by `y`).
lv_obj_t *widget_button_row(lv_obj_t *scr, int y);

// Small grab bar marking the edge a swipe starts from.
void widget_handle(lv_obj_t *scr, lv_align_t align, int y);

// The whole screen is a touchpad; the knob scrolls the host (handled in main.c, since it sends
// with the touchpad's buttons).
#include "screen.h"
#include "theme.h"
#include "touchpad.h"

static void create(lv_obj_t *scr)
{
    touchpad_attach(scr);

    // Outline of the touch area. Touches pass through to the screen.
    lv_obj_t *ring = lv_obj_create(scr);
    lv_obj_remove_style_all(ring);
    lv_obj_set_size(ring, 320, 320);
    lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(ring, 2, 0);
    lv_obj_set_style_border_color(ring, theme_button_dim(), 0);
    lv_obj_set_clickable(ring, false);
    lv_obj_center(ring);

    lv_obj_t *label = lv_label_create(scr);
    lv_label_set_text(label, "Touchpad");
    lv_obj_set_style_text_font(label, THEME_FONT_TITLE, 0);
    lv_obj_set_style_text_color(label, theme_text_muted(), 0);
    lv_obj_center(label);
}

const screen_def_t touchpad_screen = {
    .name = "Touchpad",
    .icon = LV_SYMBOL_GPS,
    .create = create,
};

// Bring-up screen: shows touch coordinates and knob position to verify the drivers.
#include "display.h"
#include "esp_lvgl_port.h"
#include "knob.h"

static lv_obj_t *s_touch_label;
static lv_obj_t *s_knob_label;

static void on_touch(lv_event_t *e)
{
    lv_point_t p;
    lv_indev_get_point(lv_indev_active(), &p);
    lv_label_set_text_fmt(s_touch_label, "touch %d, %d", (int)p.x, (int)p.y);
}

static void refresh_knob(lv_timer_t *t)
{
    lv_label_set_text_fmt(s_knob_label, "knob %d", knob_position());
}

static void ui_create(void)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_clickable(scr, true);
    lv_obj_add_event_cb(scr, on_touch, LV_EVENT_PRESSING, NULL);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "DialDeck");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -50);

    s_touch_label = lv_label_create(scr);
    lv_label_set_text(s_touch_label, "touch -");
    lv_obj_set_style_text_color(s_touch_label, lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_obj_align(s_touch_label, LV_ALIGN_CENTER, 0, 0);

    s_knob_label = lv_label_create(scr);
    lv_obj_set_style_text_color(s_knob_label, lv_palette_main(LV_PALETTE_AMBER), 0);
    lv_obj_align(s_knob_label, LV_ALIGN_CENTER, 0, 40);
    lv_timer_create(refresh_knob, 50, NULL);
    refresh_knob(NULL);
}

void app_main(void)
{
    display_init();
    knob_init();

    lvgl_port_lock(0);
    ui_create();
    lvgl_port_unlock();
}

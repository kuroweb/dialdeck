// Mouse mode: the screen is a touchpad, the knob scrolls the host.
#include "ble_hid.h"
#include "display.h"
#include "esp_lvgl_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "knob.h"
#include "touchpad.h"

// Wheel units per detent; the sign sets which way the knob scrolls. Checked on macOS with
// natural scrolling as set on the test Mac. The host's natural scrolling setting also flips it.
#define SCROLL_PER_DETENT  1

static lv_obj_t *s_knob_label;
static lv_obj_t *s_ble_label;

static void refresh_status(lv_timer_t *t)
{
    static const char *const names[] = {
        [BLE_HID_STATE_STARTING] = "BLE starting",
        [BLE_HID_STATE_ADVERTISING] = "BLE waiting",
        [BLE_HID_STATE_CONNECTED] = "BLE connected",
    };
    lv_label_set_text_fmt(s_knob_label, "knob %d", knob_position());
    lv_label_set_text(s_ble_label, names[ble_hid_state()]);
}

static void ui_create(void)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    touchpad_attach(scr);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "DialDeck");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -50);

    s_knob_label = lv_label_create(scr);
    lv_obj_set_style_text_color(s_knob_label, lv_palette_main(LV_PALETTE_AMBER), 0);
    lv_obj_align(s_knob_label, LV_ALIGN_CENTER, 0, 40);

    s_ble_label = lv_label_create(scr);
    lv_obj_set_style_text_color(s_ble_label, lv_palette_main(LV_PALETTE_LIGHT_BLUE), 0);
    lv_obj_align(s_ble_label, LV_ALIGN_CENTER, 0, 80);

    lv_timer_create(refresh_status, 50, NULL);
    refresh_status(NULL);
}

static void mouse_task(void *arg)
{
    while (1) {
        int dx, dy, clicks;
        touchpad_take(&dx, &dy, &clicks);
        const int wheel = knob_take_scroll() * SCROLL_PER_DETENT;
        if (dx != 0 || dy != 0 || wheel != 0) {
            ble_hid_mouse(dx, dy, wheel, 0);
        }
        for (; clicks > 0; clicks--) {
            ble_hid_mouse(0, 0, 0, BLE_HID_BUTTON_LEFT);
            ble_hid_mouse(0, 0, 0, 0);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void app_main(void)
{
    display_init();
    knob_init();
    ble_hid_init();

    lvgl_port_lock(0);
    ui_create();
    lvgl_port_unlock();

    xTaskCreate(mouse_task, "mouse", 3072, NULL, 5, NULL);
}

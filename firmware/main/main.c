#include "ble_hid.h"
#include "deck_config.h"
#include "display.h"
#include "esp_lvgl_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "knob.h"
#include "nvs_flash.h"
#include "settings.h"
#include "touchpad.h"
#include "ui.h"

static void nvs_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

// Forward touchpad and knob input. On the touchpad screen the knob scrolls the host; on other
// screens it drives the screen itself.
static void input_task(void *arg)
{
    uint8_t buttons = 0;   // last buttons sent, so the wheel does not drop a held drag
    while (1) {
        // Send touchpad reports in order; consecutive moves with the same buttons go as one.
        touchpad_report_t r;
        bool have = touchpad_next(&r);
        while (have) {
            touchpad_report_t next;
            have = touchpad_next(&next);
            while (have && next.buttons == r.buttons) {
                r.dx += next.dx;
                r.dy += next.dy;
                have = touchpad_next(&next);
            }
            ble_hid_mouse(r.dx, r.dy, 0, r.buttons);
            buttons = r.buttons;
            r = next;
        }

        const int detents = knob_take_scroll();

        int wheel = 0;
        if (ui_active() == UI_SCREEN_TOUCHPAD) {
            // The sign was checked on macOS with natural scrolling as set on the test Mac; the
            // host's natural scrolling setting flips it too, hence the setting.
            const settings_t *cfg = settings();
            wheel = detents * cfg->scroll_step * (cfg->scroll_reverse ? -1 : 1);
        } else if (detents != 0) {
            lvgl_port_lock(0);
            ui_knob(detents);
            lvgl_port_unlock();
        }

        if (wheel != 0) {
            ble_hid_mouse(0, 0, wheel, buttons);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void app_main(void)
{
    nvs_init();
    settings_load();
    deck_config_load();

    display_init();
    display_set_brightness(settings()->brightness);
    knob_init();
    ble_hid_init();

    lvgl_port_lock(0);
    ui_create();
    lvgl_port_unlock();

    xTaskCreate(input_task, "input", 4096, NULL, 5, NULL);
}

#include "deck_screen.h"

#include <math.h>

#include "ble_hid.h"
#include "deck_config.h"
#include "esp_lvgl_port.h"
#include "theme.h"

#define RING_COUNT    (DECK_SLOT_COUNT - 1)
#define RING_RADIUS   112
#define RING_BUTTON   64
#define CENTER_BUTTON 92

static lv_obj_t *s_labels[DECK_SLOT_COUNT];

static void on_button(lv_event_t *e)
{
    const deck_slot_t *slot = deck_config_slot((int)(intptr_t)lv_event_get_user_data(e));
    if (slot->key) {
        ble_hid_key(slot->modifiers, slot->key);
    } else if (slot->consumer) {
        ble_hid_consumer(slot->consumer);
    }
}

static lv_obj_t *button_create(lv_obj_t *scr, int slot, int size, lv_color_t color)
{
    lv_obj_t *btn = lv_button_create(scr);
    lv_obj_set_size(btn, size, size);
    lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(btn, color, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    // Let the screen see the touch too, so a pull-down that starts on a button still opens the menu.
    lv_obj_set_event_bubble(btn, true);
    lv_obj_add_event_cb(btn, on_button, LV_EVENT_CLICKED, (void *)(intptr_t)slot);

    s_labels[slot] = lv_label_create(btn);
    lv_label_set_text(s_labels[slot], deck_config_slot(slot)->label);
    lv_obj_center(s_labels[slot]);
    return btn;
}

// Runs on the NimBLE host task when the Mac app changes an assignment.
static void on_config_change(int slot)
{
    lvgl_port_lock(0);
    lv_label_set_text(s_labels[slot], deck_config_slot(slot)->label);
    lvgl_port_unlock();
}

void deck_screen_create(lv_obj_t *scr)
{
    // Offset by half a step so no button sits right under the pull-down edge at the top.
    const float step = 2 * (float)M_PI / RING_COUNT;
    for (int i = 0; i < RING_COUNT; i++) {
        const float a = step * (i + 0.5f);
        lv_obj_t *btn = button_create(scr, i + 1, RING_BUTTON, theme_button());
        lv_obj_align(btn, LV_ALIGN_CENTER, (int)lroundf(RING_RADIUS * sinf(a)), (int)lroundf(-RING_RADIUS * cosf(a)));
    }
    lv_obj_t *center = button_create(scr, 0, CENTER_BUTTON, theme_accent());
    lv_obj_center(center);

    deck_config_on_change(on_config_change);
}

void deck_screen_knob(int detents)
{
    const uint16_t usage = detents > 0 ? BLE_HID_CONSUMER_VOLUME_UP : BLE_HID_CONSUMER_VOLUME_DOWN;
    for (int n = detents > 0 ? detents : -detents; n > 0; n--) {
        ble_hid_consumer(usage);
    }
}

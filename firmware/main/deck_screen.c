#include "deck_screen.h"

#include "ble_hid.h"
#include "deck_config.h"
#include "esp_lvgl_port.h"
#include "ring.h"
#include "theme.h"

_Static_assert(DECK_SLOT_COUNT - 1 == RING_SLOTS, "deck slots 1-8 fill the ring");


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
    for (int i = 0; i < RING_SLOTS; i++) {
        ring_place(button_create(scr, i + 1, RING_ITEM, theme_button()), i);
    }
    ring_place_center(button_create(scr, 0, RING_CENTER, theme_accent()));

    deck_config_on_change(on_config_change);
}

void deck_screen_knob(int detents)
{
    const uint16_t usage = detents > 0 ? BLE_HID_CONSUMER_VOLUME_UP : BLE_HID_CONSUMER_VOLUME_DOWN;
    for (int n = detents > 0 ? detents : -detents; n > 0; n--) {
        ble_hid_consumer(usage);
    }
}

// Shortcut buttons: eight on the ring and one in the center, assigned from the Mac app. The knob
// changes the volume.
#include "ble_hid.h"
#include "deck_config.h"
#include "esp_lvgl_port.h"
#include "ring.h"
#include "screen.h"
#include "theme.h"
#include "widgets.h"

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

static lv_obj_t *button_create(lv_obj_t *scr, int slot, int diameter, lv_color_t color)
{
    lv_obj_t *btn = widget_circle(scr, diameter, color, on_button, (void *)(intptr_t)slot);
    s_labels[slot] = lv_obj_get_child(btn, 0);
    lv_label_set_text(s_labels[slot], deck_config_slot(slot)->label);
    return btn;
}

// Runs on the NimBLE host task when the Mac app changes an assignment.
static void on_config_change(int slot)
{
    lvgl_port_lock(0);
    lv_label_set_text(s_labels[slot], deck_config_slot(slot)->label);
    lvgl_port_unlock();
}

static void create(lv_obj_t *scr)
{
    for (int i = 0; i < RING_SLOTS; i++) {
        ring_place(button_create(scr, i + 1, RING_ITEM, theme_button()), i);
    }
    ring_place_center(button_create(scr, 0, RING_CENTER, theme_accent()));

    deck_config_on_change(on_config_change);
}

static void knob(int detents)
{
    const uint16_t usage = detents > 0 ? BLE_HID_CONSUMER_VOLUME_UP : BLE_HID_CONSUMER_VOLUME_DOWN;
    for (int n = detents > 0 ? detents : -detents; n > 0; n--) {
        ble_hid_consumer(usage);
    }
}

const screen_def_t deck_screen = {
    .name = "Deck",
    .icon = LV_SYMBOL_KEYBOARD,
    .create = create,
    .knob = knob,
};

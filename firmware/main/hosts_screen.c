// The hosts the device can switch between. Tap a host to switch to it (an empty slot starts
// pairing); hold one to forget it and pair a new host in its place.
#include "ble_hid.h"
#include "host_slots.h"
#include "screen.h"
#include "widgets.h"

static lv_obj_t *s_rows[HOST_SLOT_COUNT];
static lv_obj_t *s_states[HOST_SLOT_COUNT];

static void on_tap(lv_event_t *e)
{
    const int slot = (int)(intptr_t)lv_event_get_user_data(e);
    if (slot != ble_hid_host() || ble_hid_state() != BLE_HID_STATE_CONNECTED) {
        ble_hid_select_host(slot);
    }
}

static void on_hold(lv_event_t *e)
{
    ble_hid_pair_host((int)(intptr_t)lv_event_get_user_data(e));
}

static void tick(void)
{
    const int active = ble_hid_host();
    const ble_hid_state_t state = ble_hid_state();
    for (int i = 0; i < HOST_SLOT_COUNT; i++) {
        const char *text = "";
        if (i == active) {
            text = state == BLE_HID_STATE_CONNECTED ? "Connected"
                   : state == BLE_HID_STATE_PAIRING ? "Pairing..."
                   : "Waiting";
        } else if (!ble_hid_host_paired(i)) {
            text = "Empty";
        }
        lv_label_set_text(s_states[i], text);
        widget_row_set_selected(s_rows[i], i == active);
    }
}

static void create(lv_obj_t *scr)
{
    widget_title(scr, "Connect");
    lv_obj_t *list = widget_list(scr, 6);
    for (int i = 0; i < HOST_SLOT_COUNT; i++) {
        char label[24];
        lv_snprintf(label, sizeof(label), LV_SYMBOL_BLUETOOTH "  Host %d", i + 1);
        // SHORT_CLICKED does not fire after a long press, so holding does not also switch.
        s_rows[i] = widget_row(list, label, &s_states[i], on_tap, LV_EVENT_SHORT_CLICKED, (void *)(intptr_t)i);
        lv_obj_add_event_cb(s_rows[i], on_hold, LV_EVENT_LONG_PRESSED, (void *)(intptr_t)i);
    }
    widget_hint(scr, "Tap to switch, hold to pair again");
    tick();
}

const screen_def_t hosts_screen = {
    .name = "Connect",
    .icon = LV_SYMBOL_BLUETOOTH,
    .create = create,
    .tick = tick,
};

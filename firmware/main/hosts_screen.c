#include "hosts_screen.h"

#include "ble_hid.h"
#include "host_slots.h"
#include "theme.h"

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

void hosts_screen_refresh(void)
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
        lv_obj_set_style_bg_color(s_rows[i], i == active ? theme_accent() : theme_button_dim(), 0);
    }
}

void hosts_screen_create(lv_obj_t *scr)
{
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Connect");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 52);

    lv_obj_t *list = lv_obj_create(scr);
    lv_obj_remove_style_all(list);
    lv_obj_set_size(list, 240, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(list, 10, 0);
    lv_obj_set_event_bubble(list, true);
    lv_obj_align(list, LV_ALIGN_CENTER, 0, 6);

    for (int i = 0; i < HOST_SLOT_COUNT; i++) {
        lv_obj_t *row = lv_obj_create(list);
        lv_obj_remove_style_all(row);
        lv_obj_set_size(row, LV_PCT(100), 48);
        lv_obj_set_style_radius(row, 24, 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_pad_hor(row, 20, 0);
        lv_obj_set_clickable(row, true);
        lv_obj_set_event_bubble(row, true);
        // SHORT_CLICKED does not fire after a long press, so holding does not also switch.
        lv_obj_add_event_cb(row, on_tap, LV_EVENT_SHORT_CLICKED, (void *)(intptr_t)i);
        lv_obj_add_event_cb(row, on_hold, LV_EVENT_LONG_PRESSED, (void *)(intptr_t)i);

        lv_obj_t *name = lv_label_create(row);
        lv_label_set_text_fmt(name, LV_SYMBOL_BLUETOOTH "  Host %d", i + 1);
        lv_obj_set_style_text_color(name, lv_color_white(), 0);
        lv_obj_set_style_text_font(name, &lv_font_montserrat_20, 0);
        lv_obj_align(name, LV_ALIGN_LEFT_MID, 0, 0);

        s_states[i] = lv_label_create(row);
        lv_obj_set_style_text_color(s_states[i], lv_color_white(), 0);
        lv_obj_align(s_states[i], LV_ALIGN_RIGHT_MID, 0, 0);

        s_rows[i] = row;
    }

    lv_obj_t *hint = lv_label_create(scr);
    lv_label_set_text(hint, "Tap to switch, hold to pair again");
    lv_obj_set_style_text_color(hint, theme_text_muted(), 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -40);

    hosts_screen_refresh();
}

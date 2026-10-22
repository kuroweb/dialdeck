#include "deck_screen.h"

#include <math.h>

#include "ble_hid.h"

// HID keyboard usages.
#define KEY_4      0x21
#define KEY_C      0x06
#define KEY_V      0x19
#define KEY_Z      0x1D
#define KEY_TAB    0x2B
#define KEY_UP     0x52

typedef struct {
    const char *label;
    uint8_t modifiers;
    uint8_t key;        // keyboard usage, or 0 for a consumer control
    uint16_t consumer;
} deck_action_t;

// Clockwise from the top.
static const deck_action_t s_ring[] = {
    { .label = "Copy",    .modifiers = BLE_HID_MOD_GUI, .key = KEY_C },
    { .label = "Paste",   .modifiers = BLE_HID_MOD_GUI, .key = KEY_V },
    { .label = "Undo",    .modifiers = BLE_HID_MOD_GUI, .key = KEY_Z },
    { .label = "App",     .modifiers = BLE_HID_MOD_GUI, .key = KEY_TAB },
    { .label = LV_SYMBOL_VOLUME_MAX, .consumer = BLE_HID_CONSUMER_VOLUME_UP },
    { .label = LV_SYMBOL_VOLUME_MID, .consumer = BLE_HID_CONSUMER_VOLUME_DOWN },
    { .label = "Shot",    .modifiers = BLE_HID_MOD_GUI | BLE_HID_MOD_SHIFT, .key = KEY_4 },
    { .label = "Mission", .modifiers = BLE_HID_MOD_CTRL, .key = KEY_UP },
};
#define RING_COUNT  (sizeof(s_ring) / sizeof(s_ring[0]))

static const deck_action_t s_center = {
    .label = LV_SYMBOL_PLAY " " LV_SYMBOL_PAUSE,
    .consumer = BLE_HID_CONSUMER_PLAY_PAUSE,
};

#define RING_RADIUS   112
#define RING_BUTTON   64
#define CENTER_BUTTON 92

static void run(const deck_action_t *action)
{
    if (action->key) {
        ble_hid_key(action->modifiers, action->key);
    } else {
        ble_hid_consumer(action->consumer);
    }
}

static void on_button(lv_event_t *e)
{
    run(lv_event_get_user_data(e));
}

static lv_obj_t *button_create(lv_obj_t *scr, const deck_action_t *action, int size, lv_color_t color)
{
    lv_obj_t *btn = lv_button_create(scr);
    lv_obj_set_size(btn, size, size);
    lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(btn, color, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    // Let the screen see the touch too, so a pull-down that starts on a button still opens the menu.
    lv_obj_set_event_bubble(btn, true);
    lv_obj_add_event_cb(btn, on_button, LV_EVENT_CLICKED, (void *)action);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, action->label);
    lv_obj_center(label);
    return btn;
}

void deck_screen_create(lv_obj_t *scr)
{
    // Offset by half a step so no button sits right under the pull-down edge at the top.
    const float step = 2 * (float)M_PI / RING_COUNT;
    for (int i = 0; i < RING_COUNT; i++) {
        const float a = step * (i + 0.5f);
        lv_obj_t *btn = button_create(scr, &s_ring[i], RING_BUTTON, lv_palette_darken(LV_PALETTE_BLUE_GREY, 2));
        lv_obj_align(btn, LV_ALIGN_CENTER, (int)lroundf(RING_RADIUS * sinf(a)), (int)lroundf(-RING_RADIUS * cosf(a)));
    }
    lv_obj_t *center = button_create(scr, &s_center, CENTER_BUTTON, lv_palette_darken(LV_PALETTE_TEAL, 2));
    lv_obj_center(center);
}

void deck_screen_knob(int detents)
{
    const uint16_t usage = detents > 0 ? BLE_HID_CONSUMER_VOLUME_UP : BLE_HID_CONSUMER_VOLUME_DOWN;
    for (int n = detents > 0 ? detents : -detents; n > 0; n--) {
        ble_hid_consumer(usage);
    }
}

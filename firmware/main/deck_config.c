#include "deck_config.h"

#include <string.h>

#include "ble_hid.h"
#include "esp_log.h"
#include "lvgl.h"
#include "nvs.h"

static const char *TAG = "deck_config";

#define NVS_NAMESPACE  "dialdeck"
#define NVS_KEY        "deck"

// HID keyboard usages.
#define KEY_4      0x21
#define KEY_C      0x06
#define KEY_V      0x19
#define KEY_Z      0x1D
#define KEY_TAB    0x2B
#define KEY_UP     0x52

static deck_slot_t s_slots[DECK_SLOT_COUNT] = {
    { .consumer = BLE_HID_CONSUMER_PLAY_PAUSE, .label = LV_SYMBOL_PLAY " " LV_SYMBOL_PAUSE },
    { .modifiers = BLE_HID_MOD_GUI, .key = KEY_C, .label = "Copy" },
    { .modifiers = BLE_HID_MOD_GUI, .key = KEY_V, .label = "Paste" },
    { .modifiers = BLE_HID_MOD_GUI, .key = KEY_Z, .label = "Undo" },
    { .modifiers = BLE_HID_MOD_GUI, .key = KEY_TAB, .label = "App" },
    { .consumer = BLE_HID_CONSUMER_VOLUME_UP, .label = LV_SYMBOL_VOLUME_MAX },
    { .consumer = BLE_HID_CONSUMER_VOLUME_DOWN, .label = LV_SYMBOL_VOLUME_MID },
    { .modifiers = BLE_HID_MOD_GUI | BLE_HID_MOD_SHIFT, .key = KEY_4, .label = "Shot" },
    { .modifiers = BLE_HID_MOD_CTRL, .key = KEY_UP, .label = "Mission" },
};

static void (*s_on_change)(int slot);

void deck_config_load(void)
{
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK) {
        return;
    }
    deck_slot_t stored[DECK_SLOT_COUNT];
    size_t len = sizeof(stored);
    // A blob of another size comes from an older layout; keep the built-in assignments then.
    if (nvs_get_blob(nvs, NVS_KEY, stored, &len) == ESP_OK && len == sizeof(stored)) {
        memcpy(s_slots, stored, sizeof(s_slots));
    }
    nvs_close(nvs);
}

const deck_slot_t *deck_config_slot(int slot)
{
    return &s_slots[slot];
}

static void save(void)
{
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs) != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open failed");
        return;
    }
    esp_err_t err = nvs_set_blob(nvs, NVS_KEY, s_slots, sizeof(s_slots));
    if (err == ESP_OK) {
        err = nvs_commit(nvs);
    }
    nvs_close(nvs);
    ESP_LOGI(TAG, "saved: %s", esp_err_to_name(err));
}

bool deck_config_set(int slot, const deck_slot_t *value)
{
    if (slot < 0 || slot >= DECK_SLOT_COUNT || memchr(value->label, '\0', DECK_LABEL_LEN) == NULL) {
        return false;
    }
    s_slots[slot] = *value;
    save();
    if (s_on_change) {
        s_on_change(slot);
    }
    return true;
}

void deck_config_on_change(void (*callback)(int slot))
{
    s_on_change = callback;
}

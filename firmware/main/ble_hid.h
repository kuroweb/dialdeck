#pragma once

#include <stdint.h>

typedef enum {
    BLE_HID_STATE_STARTING,
    BLE_HID_STATE_ADVERTISING,
    BLE_HID_STATE_CONNECTED,   // link encrypted, reports reach the host
} ble_hid_state_t;

// Start NimBLE and advertise as a BLE HID mouse, keyboard and media keys. Bonds persist in NVS,
// so call after nvs_flash_init().
void ble_hid_init(void);

ble_hid_state_t ble_hid_state(void);

#define BLE_HID_BUTTON_LEFT   0x01
#define BLE_HID_BUTTON_RIGHT  0x02

// Send pointer movement, wheel (positive = scroll up) and the held buttons.
// Large movements are split into several reports. Dropped while not connected.
void ble_hid_mouse(int dx, int dy, int wheel, uint8_t buttons);

// Keyboard modifier bits.
#define BLE_HID_MOD_CTRL   0x01
#define BLE_HID_MOD_SHIFT  0x02
#define BLE_HID_MOD_ALT    0x04   // Option on macOS
#define BLE_HID_MOD_GUI    0x08   // Command on macOS

// Press and release one key (HID keyboard usage) with the given modifiers held.
void ble_hid_key(uint8_t modifiers, uint8_t key);

// Consumer control usages.
#define BLE_HID_CONSUMER_PLAY_PAUSE  0x00CD
#define BLE_HID_CONSUMER_MUTE        0x00E2
#define BLE_HID_CONSUMER_VOLUME_UP   0x00E9
#define BLE_HID_CONSUMER_VOLUME_DOWN 0x00EA

// Press and release one consumer control (media key).
void ble_hid_consumer(uint16_t usage);

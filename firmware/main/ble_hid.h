#pragma once

#include <stdint.h>

typedef enum {
    BLE_HID_STATE_STARTING,
    BLE_HID_STATE_ADVERTISING,
    BLE_HID_STATE_CONNECTED,   // link encrypted, reports reach the host
} ble_hid_state_t;

// Start NimBLE and advertise as a BLE HID mouse. Bonds persist in NVS, so call after
// nvs_flash_init().
void ble_hid_init(void);

ble_hid_state_t ble_hid_state(void);

#define BLE_HID_BUTTON_LEFT  0x01

// Send pointer movement, wheel (positive = scroll up) and the held buttons.
// Large movements are split into several reports. Dropped while not connected.
void ble_hid_mouse(int dx, int dy, int wheel, uint8_t buttons);

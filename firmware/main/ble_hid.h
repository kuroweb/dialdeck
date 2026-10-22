#pragma once

typedef enum {
    BLE_HID_STATE_STARTING,
    BLE_HID_STATE_ADVERTISING,
    BLE_HID_STATE_CONNECTED,   // link encrypted, reports reach the host
} ble_hid_state_t;

// Start NimBLE and advertise as a BLE HID mouse. Bonds persist in NVS.
void ble_hid_init(void);

ble_hid_state_t ble_hid_state(void);

// Send a mouse wheel movement (positive = scroll up). Dropped while not connected.
void ble_hid_scroll(int wheel);

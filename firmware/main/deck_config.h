#pragma once

#include <stdbool.h>
#include <stdint.h>

// Slot 0 is the center button, slots 1-8 run clockwise around the ring from the top.
#define DECK_SLOT_COUNT  9
#define DECK_LABEL_LEN   16   // bytes including the terminating NUL

// One button assignment. The layout is also the wire format of the config GATT service.
typedef struct __attribute__((packed)) {
    uint8_t modifiers;        // BLE_HID_MOD_* bits
    uint8_t key;              // HID keyboard usage, 0 for none
    uint16_t consumer;        // consumer control usage (little endian), used when key is 0
    char label[DECK_LABEL_LEN];
} deck_slot_t;

_Static_assert(sizeof(deck_slot_t) == 20, "deck_slot_t is a wire format");

// Load from NVS, falling back to the built-in assignments. Call after nvs_flash_init().
void deck_config_load(void);

const deck_slot_t *deck_config_slot(int slot);

// Replace one assignment and persist it. Returns false for an invalid slot or label.
bool deck_config_set(int slot, const deck_slot_t *value);

// Called after an assignment changes, from the task that changed it.
void deck_config_on_change(void (*callback)(int slot));

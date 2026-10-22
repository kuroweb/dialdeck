#pragma once

#include <stdbool.h>

#include "host/ble_hs.h"

// Hosts (Macs, phones, ...) the device can switch between. Each slot holds the identity address
// of one bonded host.
#define HOST_SLOT_COUNT  3

// Load from NVS. Call after nvs_flash_init().
void host_slots_load(void);

// Identity address of the host in `slot`, or NULL if the slot is empty.
const ble_addr_t *host_slots_addr(int slot);

// Slot of the host with this identity address, or -1.
int host_slots_find(const ble_addr_t *addr);

int host_slots_active(void);

void host_slots_set_active(int slot);

// Put a host in `slot` (NULL empties it). Persisted immediately.
void host_slots_assign(int slot, const ble_addr_t *addr);

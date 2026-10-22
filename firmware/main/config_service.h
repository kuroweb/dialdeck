#pragma once

// GATT service the Mac app uses to read and change the deck button assignments.
// Register after the HID service and before the NimBLE host starts. See docs/config-protocol.md.
void config_service_register(void);

#pragma once

#include <stdbool.h>

// GATT service the Mac app uses to read and change the deck button assignments and settings,
// and to exchange herdr agents with the device.
// Register after the HID service and before the NimBLE host starts. See docs/config-protocol.md.
void config_service_register(void);

// Ask the Mac app to act on an agent (agent_action_t). Returns false when no host is connected.
bool config_service_send_action(int action, const char *agent_id);

// Tell the Mac app the usage-source mask changed on the device. Returns false when no host is connected.
bool config_service_notify_usage_sources(void);

#pragma once

#include <stdbool.h>

// Start sampling the battery voltage in the background.
void battery_init(void);

// True while USB or a charger powers the device, which also charges the battery.
bool battery_charging(void);

// Remaining charge, 0-100, estimated from the voltage while running on the battery. While
// charging the voltage is the charger's, so the last value measured on battery is kept.
// -1 until the first reading on battery.
int battery_percent(void);

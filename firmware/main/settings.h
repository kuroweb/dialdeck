#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// User-adjustable values, kept in NVS. Fractional values are stored x100.
// The layout (int32, little endian) is also the wire format of the config GATT service.
typedef struct {
    int32_t pointer_gain;       // x100, pointer pixels per touch pixel when moving slowly
    int32_t pointer_accel;      // x100, gain added per touch pixel moved per 16 ms
    int32_t pointer_gain_max;   // x100
    int32_t scroll_step;        // wheel units per knob detent
    int32_t scroll_reverse;     // 0 or 1
    int32_t brightness;         // percent
} settings_t;

_Static_assert(sizeof(settings_t) == 24, "settings_t is a wire format");

// Load from NVS, falling back to defaults. Call after nvs_flash_init().
void settings_load(void);

settings_t *settings(void);

// Allowed range of the field at `offset` in settings_t.
void settings_range(size_t offset, int *min, int *max);

// Persist the current values. Saves are coalesced so turning the knob does not wear the flash.
void settings_changed(void);

// Replace every value at once (from the Mac app). Returns false, changing nothing, if any value is
// out of range. On success the values are saved and the change callback runs.
bool settings_replace(const settings_t *value);

// Called after settings_replace(), from the task that called it.
void settings_on_change(void (*callback)(void));

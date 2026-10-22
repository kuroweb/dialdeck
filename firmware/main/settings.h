#pragma once

// User-adjustable values, kept in NVS. Fractional values are stored x100.
typedef struct {
    int pointer_gain;       // x100, pointer pixels per touch pixel when moving slowly
    int pointer_accel;      // x100, gain added per touch pixel of a single step
    int pointer_gain_max;   // x100
    int scroll_step;        // wheel units per knob detent
    int scroll_reverse;     // 0 or 1
    int brightness;         // percent
} settings_t;

// Load from NVS, falling back to defaults. Call after nvs_flash_init().
void settings_load(void);

settings_t *settings(void);

// Persist the current values. Saves are coalesced so turning the knob does not wear the flash.
void settings_changed(void);

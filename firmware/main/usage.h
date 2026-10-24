#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// How much of a coding agent's plan is used, as last sent by the Mac app (see
// docs/config-protocol.md). The Mac app picks the provider (Claude, Codex, Cursor); the device
// can ask it for the next one.

#define USAGE_NAME_LEN     16    // provider name, bytes including NUL
#define USAGE_LABEL_LEN    8     // window label such as "5h", bytes including NUL
#define USAGE_WINDOWS      2
#define USAGE_UNKNOWN      0xFF        // percent not known
#define USAGE_NO_RESET     0xFFFF      // reset time not known
#define USAGE_ACTION_NEXT  6           // action asking the Mac app to show the next provider

typedef struct {
    char label[USAGE_LABEL_LEN];
    uint8_t percent;          // 0-100, or USAGE_UNKNOWN
    uint16_t reset_minutes;   // minutes until the window resets when sent, or USAGE_NO_RESET
} usage_window_t;

typedef struct {
    char name[USAGE_NAME_LEN];   // empty when the Mac app shows no usage
    int count;
    usage_window_t windows[USAGE_WINDOWS];
} usage_t;

// Call once before the BLE stack starts.
void usage_init(void);

// Replace the usage from the wire format. Returns false, changing nothing, if malformed.
bool usage_replace(const uint8_t *data, size_t len);

// Copy the current usage. Returns false before the Mac app has sent any.
bool usage_get(usage_t *out);

// Called after the usage changes, from the task that changed it.
void usage_on_change(void (*callback)(void));

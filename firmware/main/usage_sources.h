#pragma once

#include <stdbool.h>
#include <stdint.h>

#define USAGE_SOURCE_CLAUDE  (1 << 0)
#define USAGE_SOURCE_CODEX   (1 << 1)
#define USAGE_SOURCE_CURSOR  (1 << 2)
#define USAGE_SOURCE_DEFAULT (USAGE_SOURCE_CLAUDE | USAGE_SOURCE_CODEX | USAGE_SOURCE_CURSOR)
#define USAGE_SOURCE_COUNT   3

void usage_sources_load(void);

uint8_t usage_sources_mask(void);

// Low three bits only. Saves to NVS and returns false when unchanged or invalid.
bool usage_sources_set_mask(uint8_t mask);

bool usage_sources_enabled(int index);

// Flip one source (0 Claude, 1 Codex, 2 Cursor). Saves to NVS.
void usage_sources_toggle(int index);

void usage_sources_on_change(void (*callback)(void));

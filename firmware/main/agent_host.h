#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    AGENT_HOST_HERDR = 0,
    AGENT_HOST_ORCA = 1,
} agent_host_t;

void agent_host_load(void);
agent_host_t agent_host_get(void);
bool agent_host_set(uint8_t value);
const char *agent_host_name(void);
void agent_host_on_change(void (*callback)(void));

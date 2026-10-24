#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// herdr agents as last sent by the Mac app (see docs/config-protocol.md).

#define AGENT_MAX         32
#define AGENT_PAGE        8     // agents per write, and per page on the agents screen
#define AGENT_ID_LEN      24    // herdr pane id, bytes including NUL
#define AGENT_TITLE_LEN   96    // UTF-8 bytes including NUL
#define AGENT_DETAIL_LEN  160   // what a blocked agent is asking, UTF-8 bytes including NUL

// Order is also the wire value and the display order: what needs a reply comes first.
typedef enum {
    AGENT_BLOCKED,
    AGENT_DONE,
    AGENT_WORKING,
    AGENT_IDLE,
    AGENT_UNKNOWN,
} agent_status_t;

typedef struct {
    agent_status_t status;
    char id[AGENT_ID_LEN];
    char title[AGENT_TITLE_LEN];
    char detail[AGENT_DETAIL_LEN];
} agent_t;

// What the device asks the Mac app to do with an agent.
typedef enum {
    AGENT_ACTION_FOCUS = 1,   // focus it in herdr and bring the terminal to the front
    AGENT_ACTION_ENTER,
    AGENT_ACTION_ESC,
    AGENT_ACTION_UP,
    AGENT_ACTION_DOWN,
} agent_action_t;

// Call once before the BLE stack starts.
void agents_init(void);

// Take one page of the list in the wire format. The list is replaced once its last page arrives.
// Returns false, changing nothing, if malformed or out of order.
bool agents_replace(const uint8_t *data, size_t len);

// True once the Mac app has sent a list since boot.
bool agents_known(void);

// Copy the current list (at most `max`). Returns the number copied.
int agents_copy(agent_t *out, int max);

// Number of agents that are blocked or done.
int agents_attention(void);

// The most urgent status in the list (lowest value), or -1 when there are no agents.
int agents_dominant(void);

// Called after the list changes, from the task that changed it.
void agents_on_change(void (*callback)(void));

#pragma once

#include "agents.h"
#include "lvgl.h"

// Color and icon of an agent status, shared by the agents screens and the status bar. Same scheme
// as the Raycast extension's menu bar: blocked red, done green, working orange, the rest grey.
lv_color_t agents_status_color(agent_status_t status);
const char *agents_status_icon(agent_status_t status);

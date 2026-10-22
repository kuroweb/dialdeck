#pragma once

#include "agents.h"
#include "lvgl.h"

// herdr agents: a dial with one circle per agent and the selected agent in the center, and a
// detail screen opened by tapping the center. The detail shows what a blocked agent is asking and
// sends Focus, Enter, Esc, and up/down from the knob.
void agents_screen_create(lv_obj_t *list_scr, lv_obj_t *detail_scr);

// Knob on the dial moves the selection around the ring; on the detail of a blocked agent it moves
// through the choices.
void agents_screen_list_knob(int detents);
void agents_screen_detail_knob(int detents);

// Color and icon of an agent status, shared with the status bar. Same scheme as the Raycast
// extension's menu bar: blocked red, done green, working orange, the rest grey.
lv_color_t agents_status_color(agent_status_t status);
const char *agents_status_icon(agent_status_t status);

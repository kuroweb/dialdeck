// herdr agents. The agents screen is a dial: one circle per agent on the ring, colored by status,
// and the selected agent in the center; the knob moves the selection, tapping the center opens
// it. The agent screen shows what a blocked agent is asking and sends Focus, Enter, Esc, and
// up/down from the knob.
#include "agents_screen.h"

#include <string.h>

#include "config_service.h"
#include "esp_lvgl_port.h"
#include "font_ja.h"
#include "ring.h"
#include "screen.h"
#include "theme.h"
#include "ui.h"
#include "widgets.h"

_Static_assert(AGENT_MAX <= RING_SLOTS, "every agent needs a place on the ring");

static agent_t s_agents[AGENT_MAX];      // copy drawn by the UI; agents.c owns the live list
static int s_count;

// Dial
static lv_obj_t *s_dots[RING_SLOTS];
static lv_obj_t *s_center_status;
static lv_obj_t *s_center_title;
static lv_obj_t *s_empty;
static char s_selected_id[AGENT_ID_LEN];
static int s_selected;

// Agent
static lv_obj_t *s_detail_status;
static lv_obj_t *s_detail_title;
static lv_obj_t *s_detail_text;
static lv_obj_t *s_reply_row;
static char s_detail_id[AGENT_ID_LEN];

lv_color_t agents_status_color(agent_status_t status)
{
    switch (status) {
    case AGENT_BLOCKED: return theme_danger();
    case AGENT_DONE: return theme_success();
    case AGENT_WORKING: return theme_busy();
    default: return theme_text_muted();
    }
}

const char *agents_status_icon(agent_status_t status)
{
    switch (status) {
    case AGENT_BLOCKED: return LV_SYMBOL_WARNING;
    case AGENT_DONE: return LV_SYMBOL_OK;
    case AGENT_WORKING: return LV_SYMBOL_REFRESH;
    default: return LV_SYMBOL_BULLET;
    }
}

static const char *status_name(agent_status_t status)
{
    static const char *const names[] = { "Blocked", "Done", "Working", "Idle", "Unknown" };
    return names[status];
}

// Label for text from the host: the Japanese font, clipped to `lines` lines.
static lv_obj_t *host_text(lv_obj_t *parent, int width, int lines, lv_color_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_size(label, width, lines * lv_font_get_line_height(font_ja()));
    lv_obj_set_style_text_font(label, font_ja(), 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    return label;
}

static const agent_t *find(const char *id)
{
    for (int i = 0; i < s_count; i++) {
        if (strcmp(s_agents[i].id, id) == 0) {
            return &s_agents[i];
        }
    }
    return NULL;
}

// Agent screen

static void refresh_agent(void)
{
    const agent_t *agent = find(s_detail_id);
    if (agent == NULL) {
        lv_label_set_text(s_detail_status, "Gone");
        lv_obj_set_style_text_color(s_detail_status, theme_text_muted(), 0);
        lv_obj_set_hidden(s_reply_row, true);
        return;
    }
    lv_label_set_text(s_detail_status, status_name(agent->status));
    lv_obj_set_style_text_color(s_detail_status, agents_status_color(agent->status), 0);
    lv_label_set_text(s_detail_title, agent->title);
    lv_label_set_text(s_detail_text, agent->detail);
    lv_obj_set_hidden(s_reply_row, agent->status != AGENT_BLOCKED);
}

static void send(agent_action_t action)
{
    if (s_detail_id[0]) {
        config_service_send_action(action, s_detail_id);
    }
}

static void on_action(lv_event_t *e)
{
    send((agent_action_t)(intptr_t)lv_event_get_user_data(e));
}

static void on_back(lv_event_t *e)
{
    s_detail_id[0] = '\0';
    ui_show(UI_SCREEN_AGENTS);
}

static void agent_create(lv_obj_t *scr)
{
    s_detail_status = widget_title(scr, "");

    s_detail_title = host_text(scr, 250, 2, theme_text());
    lv_obj_align(s_detail_title, LV_ALIGN_TOP_MID, 0, THEME_TITLE_Y + 28);

    s_detail_text = host_text(scr, 270, 4, theme_text_muted());
    lv_obj_align(s_detail_text, LV_ALIGN_TOP_MID, 0, THEME_TITLE_Y + 74);

    s_reply_row = widget_button_row(scr, 54);
    widget_button(s_reply_row, "Esc", false, on_action, (void *)(intptr_t)AGENT_ACTION_ESC);
    widget_button(s_reply_row, "Enter", true, on_action, (void *)(intptr_t)AGENT_ACTION_ENTER);

    lv_obj_t *nav = widget_button_row(scr, 104);
    widget_button(nav, LV_SYMBOL_LEFT " Back", false, on_back, NULL);
    widget_button(nav, "Focus", false, on_action, (void *)(intptr_t)AGENT_ACTION_FOCUS);
}

static void agent_knob(int detents)
{
    // Up/down only mean something while the agent shows choices.
    const agent_t *agent = find(s_detail_id);
    if (agent == NULL || agent->status != AGENT_BLOCKED) {
        return;
    }
    const agent_action_t action = detents > 0 ? AGENT_ACTION_DOWN : AGENT_ACTION_UP;
    for (int n = detents > 0 ? detents : -detents; n > 0; n--) {
        send(action);
    }
}

const screen_def_t agent_screen = {
    .create = agent_create,
    .knob = agent_knob,
};

// Agents screen (dial)

static void refresh_dial(void)
{
    // Keep the same agent selected while the list reorders; fall back to the most urgent one.
    s_selected = 0;
    for (int i = 0; i < s_count; i++) {
        if (strcmp(s_agents[i].id, s_selected_id) == 0) {
            s_selected = i;
        }
    }
    if (s_count > 0) {
        strlcpy(s_selected_id, s_agents[s_selected].id, sizeof(s_selected_id));
    }

    for (int i = 0; i < RING_SLOTS; i++) {
        const bool used = i < s_count;
        lv_obj_set_hidden(s_dots[i], !used);
        if (!used) {
            continue;
        }
        lv_label_set_text(lv_obj_get_child(s_dots[i], 0), s_agents[i].title);
        lv_obj_set_style_bg_color(s_dots[i], agents_status_color(s_agents[i].status), 0);
        lv_obj_set_style_border_width(s_dots[i], i == s_selected ? 3 : 0, 0);
    }

    lv_label_set_text(s_empty, agents_known() ? "No agents" : "Waiting for\nthe DialDeck app");
    lv_obj_set_hidden(s_empty, s_count > 0);
    lv_obj_set_hidden(s_center_title, s_count == 0);
    lv_obj_set_hidden(s_center_status, s_count == 0);
    if (s_count > 0) {
        const agent_t *agent = &s_agents[s_selected];
        lv_label_set_text(s_center_title, agent->title);
        lv_label_set_text(s_center_status, status_name(agent->status));
        lv_obj_set_style_text_color(s_center_status, agents_status_color(agent->status), 0);
    }
}

static void select_agent(int index)
{
    if (index >= 0 && index < s_count) {
        strlcpy(s_selected_id, s_agents[index].id, sizeof(s_selected_id));
        refresh_dial();
    }
}

static void on_dot(lv_event_t *e)
{
    select_agent((int)(intptr_t)lv_event_get_user_data(e));
}

static void on_center(lv_event_t *e)
{
    if (s_count == 0) {
        return;
    }
    strlcpy(s_detail_id, s_agents[s_selected].id, sizeof(s_detail_id));
    refresh_agent();
    ui_show(UI_SCREEN_AGENT);
}

// Runs on the NimBLE host task when the Mac app sends a new list.
static void on_agents_change(void)
{
    lvgl_port_lock(0);
    s_count = agents_copy(s_agents, AGENT_MAX);
    refresh_dial();
    if (s_detail_id[0]) {
        refresh_agent();
    }
    lvgl_port_unlock();
}

static void agents_create(lv_obj_t *scr)
{
    for (int i = 0; i < RING_SLOTS; i++) {
        s_dots[i] = widget_circle(scr, RING_ITEM, theme_button(), on_dot, (void *)(intptr_t)i);
        ring_place(s_dots[i], i);
        // The start of the title, two lines in the square inside the circle.
        lv_obj_t *title = lv_obj_get_child(s_dots[i], 0);
        lv_label_set_long_mode(title, LV_LABEL_LONG_MODE_CLIP);
        lv_obj_set_size(title, 56, 2 * lv_font_get_line_height(font_ja()));
        lv_obj_set_style_text_font(title, font_ja(), 0);
    }

    lv_obj_t *center = widget_circle(scr, RING_CENTER, theme_button_dim(), on_center, NULL);
    ring_place_center(center);

    s_center_status = lv_obj_get_child(center, 0);
    lv_obj_align(s_center_status, LV_ALIGN_TOP_MID, 0, 22);

    s_center_title = host_text(center, 124, 4, theme_text());
    lv_obj_align(s_center_title, LV_ALIGN_TOP_MID, 0, 44);

    s_empty = lv_label_create(center);
    lv_obj_set_style_text_color(s_empty, theme_text_muted(), 0);
    lv_obj_set_style_text_align(s_empty, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(s_empty);

    // The Mac app may have sent a list before the UI existed.
    s_count = agents_copy(s_agents, AGENT_MAX);
    refresh_dial();
    agents_on_change(on_agents_change);
}

static void agents_knob(int detents)
{
    if (s_count > 0) {
        select_agent(((s_selected + detents) % s_count + s_count) % s_count);
    }
}

const screen_def_t agents_screen = {
    .name = "Agents",
    .icon = LV_SYMBOL_LIST,
    .create = agents_create,
    .knob = agents_knob,
};

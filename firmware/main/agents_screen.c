#include "agents_screen.h"

#include <string.h>

#include "agents.h"
#include "config_service.h"
#include "esp_lvgl_port.h"
#include "font_ja.h"
#include "ring.h"
#include "theme.h"
#include "ui.h"

// The list screen is a dial: one circle per agent on the ring, the selected agent in the center.
_Static_assert(AGENT_MAX <= RING_SLOTS, "every agent needs a place on the ring");

static lv_obj_t *s_dots[RING_SLOTS];
static lv_obj_t *s_dot_titles[RING_SLOTS];
static lv_obj_t *s_center_status;
static lv_obj_t *s_center_title;
static lv_obj_t *s_empty;
static char s_selected_id[AGENT_ID_LEN];
static int s_selected;

static lv_obj_t *s_detail_status;
static lv_obj_t *s_detail_title;
static lv_obj_t *s_detail_text;
static lv_obj_t *s_reply_row;
static char s_detail_id[AGENT_ID_LEN];   // agent shown on the detail screen

static agent_t s_agents[AGENT_MAX];      // copy drawn by the UI; agents.c owns the live list
static int s_count;

lv_color_t agents_status_color(agent_status_t status)
{
    switch (status) {
    case AGENT_BLOCKED: return lv_palette_main(LV_PALETTE_RED);
    case AGENT_DONE: return lv_palette_main(LV_PALETTE_GREEN);
    case AGENT_WORKING: return lv_palette_main(LV_PALETTE_ORANGE);
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

static const agent_t *find(const char *id)
{
    for (int i = 0; i < s_count; i++) {
        if (strcmp(s_agents[i].id, id) == 0) {
            return &s_agents[i];
        }
    }
    return NULL;
}

static void refresh_detail(void)
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

static void open_detail(int index)
{
    strlcpy(s_detail_id, s_agents[index].id, sizeof(s_detail_id));
    lv_label_set_text(s_detail_title, "");
    lv_label_set_text(s_detail_text, "");
    refresh_detail();
    ui_show(UI_SCREEN_AGENT);
}

static void refresh_list(void)
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
        lv_label_set_text(s_dot_titles[i], s_agents[i].title);
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
        refresh_list();
    }
}

static void on_dot(lv_event_t *e)
{
    select_agent((int)(intptr_t)lv_event_get_user_data(e));
}

static void on_center(lv_event_t *e)
{
    if (s_count > 0) {
        open_detail(s_selected);
    }
}

// Runs on the NimBLE host task when the Mac app sends a new list.
static void on_agents_change(void)
{
    lvgl_port_lock(0);
    s_count = agents_copy(s_agents, AGENT_MAX);
    refresh_list();
    if (s_detail_id[0]) {
        refresh_detail();
    }
    lvgl_port_unlock();
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

static lv_obj_t *pill(lv_obj_t *parent, const char *text, lv_color_t color, lv_event_cb_t cb, void *user_data)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, 92, 40);
    lv_obj_set_style_radius(btn, 20, 0);
    lv_obj_set_style_bg_color(btn, color, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_event_bubble(btn, true);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);
    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    return btn;
}

static lv_obj_t *button_row(lv_obj_t *scr, int y)
{
    lv_obj_t *row = lv_obj_create(scr);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row, 10, 0);
    lv_obj_set_event_bubble(row, true);
    lv_obj_align(row, LV_ALIGN_CENTER, 0, y);
    return row;
}

static void list_screen_create(lv_obj_t *scr)
{
    for (int i = 0; i < RING_SLOTS; i++) {
        lv_obj_t *dot = lv_obj_create(scr);
        lv_obj_remove_style_all(dot);
        lv_obj_set_size(dot, RING_ITEM, RING_ITEM);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(dot, lv_color_white(), 0);
        lv_obj_set_clickable(dot, true);
        // Let the screen see the touch too, so a pull-down that starts on a dot still opens the menu.
        lv_obj_set_event_bubble(dot, true);
        lv_obj_add_event_cb(dot, on_dot, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        ring_place(dot, i);
        s_dots[i] = dot;

        // The start of the title, two lines in the square inside the circle.
        lv_obj_t *title = lv_label_create(dot);
        lv_label_set_long_mode(title, LV_LABEL_LONG_MODE_CLIP);
        lv_obj_set_size(title, 56, 40);
        lv_obj_set_style_text_font(title, font_ja(), 0);
        lv_obj_set_style_text_color(title, lv_color_white(), 0);
        lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(title);
        s_dot_titles[i] = title;
    }

    lv_obj_t *center = lv_obj_create(scr);
    lv_obj_remove_style_all(center);
    lv_obj_set_size(center, RING_CENTER, RING_CENTER);
    lv_obj_set_style_radius(center, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(center, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(center, theme_button_dim(), 0);
    lv_obj_set_clickable(center, true);
    lv_obj_set_event_bubble(center, true);
    lv_obj_add_event_cb(center, on_center, LV_EVENT_CLICKED, NULL);
    ring_place_center(center);

    s_center_status = lv_label_create(center);
    lv_obj_align(s_center_status, LV_ALIGN_TOP_MID, 0, 22);

    s_center_title = lv_label_create(center);
    lv_label_set_long_mode(s_center_title, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_size(s_center_title, 124, 80);   // four lines of the 16 px font
    lv_obj_set_style_text_font(s_center_title, font_ja(), 0);
    lv_obj_set_style_text_color(s_center_title, lv_color_white(), 0);
    lv_obj_set_style_text_align(s_center_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_center_title, LV_ALIGN_TOP_MID, 0, 44);

    s_empty = lv_label_create(center);
    lv_obj_set_style_text_color(s_empty, theme_text_muted(), 0);
    lv_obj_set_style_text_align(s_empty, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(s_empty);
}

static void detail_screen_create(lv_obj_t *scr)
{
    s_detail_status = lv_label_create(scr);
    lv_obj_align(s_detail_status, LV_ALIGN_TOP_MID, 0, 50);

    s_detail_title = lv_label_create(scr);
    lv_label_set_long_mode(s_detail_title, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_size(s_detail_title, 250, 40);
    lv_obj_set_style_text_font(s_detail_title, font_ja(), 0);
    lv_obj_set_style_text_color(s_detail_title, lv_color_white(), 0);
    lv_obj_set_style_text_align(s_detail_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_detail_title, LV_ALIGN_TOP_MID, 0, 74);

    s_detail_text = lv_label_create(scr);
    lv_label_set_long_mode(s_detail_text, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_size(s_detail_text, 270, 80);
    lv_obj_set_style_text_font(s_detail_text, font_ja(), 0);
    lv_obj_set_style_text_color(s_detail_text, theme_text_muted(), 0);
    lv_obj_align(s_detail_text, LV_ALIGN_TOP_MID, 0, 120);

    s_reply_row = button_row(scr, 54);
    pill(s_reply_row, "Esc", theme_button(), on_action, (void *)(intptr_t)AGENT_ACTION_ESC);
    pill(s_reply_row, "Enter", theme_accent(), on_action, (void *)(intptr_t)AGENT_ACTION_ENTER);

    lv_obj_t *nav = button_row(scr, 104);
    pill(nav, LV_SYMBOL_LEFT " Back", theme_button_dim(), on_back, NULL);
    pill(nav, "Focus", theme_button(), on_action, (void *)(intptr_t)AGENT_ACTION_FOCUS);
}

void agents_screen_create(lv_obj_t *list_scr, lv_obj_t *detail_scr)
{
    list_screen_create(list_scr);
    detail_screen_create(detail_scr);
    refresh_list();
    agents_on_change(on_agents_change);
}

void agents_screen_list_knob(int detents)
{
    if (s_count > 0) {
        select_agent(((s_selected + detents) % s_count + s_count) % s_count);
    }
}

void agents_screen_detail_knob(int detents)
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

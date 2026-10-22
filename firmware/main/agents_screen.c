#include "agents_screen.h"

#include <string.h>

#include "agents.h"
#include "config_service.h"
#include "esp_lvgl_port.h"
#include "font_ja.h"
#include "theme.h"
#include "ui.h"

#define ROW_HEIGHT  40

static lv_obj_t *s_list;
static lv_obj_t *s_empty;

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

static void on_row(lv_event_t *e)
{
    const agent_t *agent = &s_agents[(int)(intptr_t)lv_event_get_user_data(e)];
    strlcpy(s_detail_id, agent->id, sizeof(s_detail_id));
    lv_label_set_text(s_detail_title, "");
    lv_label_set_text(s_detail_text, "");
    refresh_detail();
    ui_show(UI_SCREEN_AGENT);
}

static void refresh_list(void)
{
    lv_obj_clean(s_list);
    for (int i = 0; i < s_count; i++) {
        lv_obj_t *row = lv_obj_create(s_list);
        lv_obj_remove_style_all(row);
        lv_obj_set_size(row, LV_PCT(100), ROW_HEIGHT);
        lv_obj_set_style_radius(row, ROW_HEIGHT / 2, 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(row, theme_button_dim(), 0);
        lv_obj_set_style_pad_hor(row, 14, 0);
        lv_obj_set_clickable(row, true);
        lv_obj_set_event_bubble(row, true);
        lv_obj_add_event_cb(row, on_row, LV_EVENT_CLICKED, (void *)(intptr_t)i);

        lv_obj_t *dot = lv_obj_create(row);
        lv_obj_remove_style_all(dot);
        lv_obj_set_size(dot, 10, 10);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(dot, agents_status_color(s_agents[i].status), 0);
        lv_obj_set_clickable(dot, false);
        lv_obj_align(dot, LV_ALIGN_LEFT_MID, 0, 0);

        lv_obj_t *title = lv_label_create(row);
        lv_label_set_text(title, s_agents[i].title);
        lv_label_set_long_mode(title, LV_LABEL_LONG_MODE_DOTS);
        lv_obj_set_width(title, 206);
        lv_obj_set_style_text_font(title, font_ja(), 0);
        lv_obj_set_style_text_color(title, lv_color_white(), 0);
        lv_obj_align(title, LV_ALIGN_LEFT_MID, 18, 0);
    }
    lv_label_set_text(s_empty, agents_known() ? "No agents" : "Waiting for the DialDeck app");
    lv_obj_set_hidden(s_empty, s_count > 0);
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
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Agents");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 52);

    s_list = lv_obj_create(scr);
    lv_obj_remove_style_all(s_list);
    lv_obj_set_size(s_list, 260, 4 * ROW_HEIGHT + 3 * 6);
    lv_obj_set_flex_flow(s_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_list, 6, 0);
    lv_obj_set_scroll_dir(s_list, LV_DIR_VER);
    lv_obj_set_event_bubble(s_list, true);
    lv_obj_align(s_list, LV_ALIGN_CENTER, 0, 14);

    s_empty = lv_label_create(scr);
    lv_obj_set_style_text_color(s_empty, theme_text_muted(), 0);
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
    lv_obj_scroll_by_bounded(s_list, 0, -detents * (ROW_HEIGHT + 6), LV_ANIM_ON);
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

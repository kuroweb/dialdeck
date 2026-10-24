// herdr agents. The agents screen is a ring of eight circles, one per agent, colored by status
// and showing the start of the title; tapping one opens it. More than eight agents go on further
// pages, turned by a vertical swipe or the knob. The center shows how much of the agent plan the
// Mac app reports as used; tapping it switches to the next provider. The agent screen shows what a blocked agent is asking and sends Focus, Enter, Esc, and
// up/down from the knob.
#include "agents_screen.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "config_service.h"
#include "esp_lvgl_port.h"
#include "font_ja.h"
#include "ring.h"
#include "screen.h"
#include "theme.h"
#include "ui.h"
#include "usage.h"
#include "widgets.h"

_Static_assert(AGENT_PAGE == RING_SLOTS, "a page fills the ring");

static agent_t s_agents[AGENT_MAX];      // copy drawn by the UI; agents.c owns the live list
static int s_count;

// Ring. Larger circles and a smaller center than ring.h, for more of each title.
#define DOT_SIZE      88
#define DOT_RADIUS    122
#define CENTER_SIZE   140
#define ARC_WIDTH     6

static lv_obj_t *s_dots[RING_SLOTS];
static lv_obj_t *s_empty;
static lv_obj_t *s_pager;
static int s_page;

// Center
static lv_obj_t *s_arcs[USAGE_WINDOWS];
static lv_obj_t *s_usage_name;
static lv_obj_t *s_usage_values[USAGE_WINDOWS];   // "5h 20%"; the first one large
static lv_obj_t *s_usage_resets[USAGE_WINDOWS];   // time until that window resets

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

// Agents screen (ring)

static int page_count(void)
{
    return s_count > 0 ? (s_count + AGENT_PAGE - 1) / AGENT_PAGE : 1;
}

static void refresh_ring(void)
{
    if (s_page >= page_count()) {
        s_page = page_count() - 1;
    }
    const int first = s_page * AGENT_PAGE;
    for (int i = 0; i < RING_SLOTS; i++) {
        const bool used = first + i < s_count;
        lv_obj_set_hidden(s_dots[i], !used);
        if (!used) {
            continue;
        }
        const agent_t *agent = &s_agents[first + i];
        lv_label_set_text(lv_obj_get_child(s_dots[i], 0), agent->title);
        lv_obj_set_style_bg_color(s_dots[i], agents_status_color(agent->status), 0);
    }

    const int pages = page_count();
    lv_obj_set_hidden(s_pager, pages < 2);
    for (int i = 0; i < (int)lv_obj_get_child_count(s_pager); i++) {
        lv_obj_t *dot = lv_obj_get_child(s_pager, i);
        lv_obj_set_hidden(dot, i >= pages);
        lv_obj_set_style_bg_color(dot, i == s_page ? theme_text() : theme_button(), 0);
    }

    lv_label_set_text(s_empty, agents_known() ? "No agents" : "Waiting for\nthe DialDeck app");
    lv_obj_set_hidden(s_empty, s_count > 0);
}

static lv_color_t usage_color(int percent)
{
    return percent >= 90 ? theme_danger() : percent >= 70 ? theme_warning() : theme_accent_text();
}

// "45m" and "3h 20m" within a few hours, whole hours up to a week (a weekly limit reads as hours
// left), days beyond.
static void format_reset(char *buf, size_t len, int minutes)
{
    if (minutes < 60) {
        snprintf(buf, len, LV_SYMBOL_REFRESH " %dm", minutes);
    } else if (minutes < 10 * 60) {
        snprintf(buf, len, LV_SYMBOL_REFRESH " %dh %dm", minutes / 60, minutes % 60);
    } else if (minutes <= 7 * 24 * 60) {
        snprintf(buf, len, LV_SYMBOL_REFRESH " %dh", minutes / 60);
    } else {
        snprintf(buf, len, LV_SYMBOL_REFRESH " %dd", minutes / (24 * 60));
    }
}

static void format_window(char *buf, size_t len, const usage_window_t *w)
{
    if (w->percent == USAGE_UNKNOWN) {
        snprintf(buf, len, "%s --", w->label);
    } else {
        snprintf(buf, len, "%s %d%%", w->label, w->percent);
    }
}

// The usage the Mac app sent, or the number of agents when it sends none.
static void refresh_usage(void)
{
    usage_t usage;
    const bool shown = usage_get(&usage) && usage.name[0] != '\0';
    char text[32];

    for (int i = 0; i < USAGE_WINDOWS; i++) {
        const bool used = shown && i < usage.count;
        lv_obj_set_hidden(s_arcs[i], !used);
        if (used) {
            const int percent = usage.windows[i].percent == USAGE_UNKNOWN ? 0 : usage.windows[i].percent;
            lv_arc_set_value(s_arcs[i], percent);
            lv_obj_set_style_arc_color(s_arcs[i], usage_color(percent), LV_PART_INDICATOR);
        }
    }

    if (!shown) {
        lv_label_set_text(s_usage_name, "Agents");
        snprintf(text, sizeof(text), "%d", s_count);
        lv_label_set_text(s_usage_values[0], text);
        lv_label_set_text(s_usage_resets[0], "");
        lv_label_set_text(s_usage_values[1], "");
        lv_label_set_text(s_usage_resets[1], "");
        return;
    }
    lv_label_set_text(s_usage_name, usage.name);
    for (int i = 0; i < USAGE_WINDOWS; i++) {
        const usage_window_t *w = &usage.windows[i];
        if (i < usage.count) {
            format_window(text, sizeof(text), w);
        } else {
            strlcpy(text, i == 0 ? "--" : "", sizeof(text));
        }
        lv_label_set_text(s_usage_values[i], text);
        if (i < usage.count && w->reset_minutes != USAGE_NO_RESET) {
            format_reset(text, sizeof(text), w->reset_minutes);
        } else {
            text[0] = '\0';
        }
        lv_label_set_text(s_usage_resets[i], text);
    }
}

static void on_dot(lv_event_t *e)
{
    const int index = s_page * AGENT_PAGE + (int)(intptr_t)lv_event_get_user_data(e);
    if (index >= s_count) {
        return;
    }
    strlcpy(s_detail_id, s_agents[index].id, sizeof(s_detail_id));
    refresh_agent();
    ui_show(UI_SCREEN_AGENT);
}

// Runs on the NimBLE host task when the Mac app sends a new list.
static void on_agents_change(void)
{
    lvgl_port_lock(0);
    s_count = agents_copy(s_agents, AGENT_MAX);
    refresh_ring();
    refresh_usage();
    if (s_detail_id[0]) {
        refresh_agent();
    }
    lvgl_port_unlock();
}

// Runs on the NimBLE host task when the Mac app sends new usage.
static void on_usage_change(void)
{
    lvgl_port_lock(0);
    refresh_usage();
    lvgl_port_unlock();
}

static void on_center(lv_event_t *e)
{
    config_service_send_action(USAGE_ACTION_NEXT, "");
}

static lv_obj_t *center_label(lv_obj_t *center, const lv_font_t *font, lv_color_t color, int y)
{
    lv_obj_t *label = lv_label_create(center);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, y);
    return label;
}

static void center_create(lv_obj_t *scr)
{
    lv_obj_t *center = lv_obj_create(scr);
    lv_obj_remove_style_all(center);
    lv_obj_set_size(center, CENTER_SIZE, CENTER_SIZE);
    lv_obj_set_style_radius(center, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(center, theme_button_dim(), 0);
    lv_obj_set_style_bg_color(center, theme_button(), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(center, LV_OPA_COVER, 0);
    lv_obj_set_scrollable(center, false);
    // Let the screen see the touch too, so a swipe that starts on the center still pages.
    lv_obj_set_event_bubble(center, true);
    lv_obj_add_event_cb(center, on_center, LV_EVENT_CLICKED, NULL);
    ring_place_center(center);

    // One arc per usage window, the first outermost.
    for (int i = 0; i < USAGE_WINDOWS; i++) {
        lv_obj_t *arc = lv_arc_create(center);
        const int size = CENTER_SIZE - 2 * i * (ARC_WIDTH + 2);
        lv_obj_set_size(arc, size, size);
        lv_obj_center(arc);
        lv_arc_set_rotation(arc, 270);
        lv_arc_set_bg_angles(arc, 0, 360);
        lv_arc_set_range(arc, 0, 100);
        lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
        lv_obj_set_clickable(arc, false);
        lv_obj_set_style_arc_width(arc, ARC_WIDTH, LV_PART_MAIN);
        lv_obj_set_style_arc_width(arc, ARC_WIDTH, LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(arc, theme_button(), LV_PART_MAIN);
        s_arcs[i] = arc;
    }

    // Name, then each window with its reset time under it. Kept inside the inner arc.
    s_usage_name = center_label(center, THEME_FONT_BODY, theme_text_muted(), -42);
    s_usage_values[0] = center_label(center, THEME_FONT_TITLE, theme_text(), -22);
    s_usage_resets[0] = center_label(center, THEME_FONT_BODY, theme_text_muted(), -2);
    s_usage_values[1] = center_label(center, THEME_FONT_BODY, theme_text(), 19);
    s_usage_resets[1] = center_label(center, THEME_FONT_BODY, theme_text_muted(), 37);
}

static void agents_create(lv_obj_t *scr)
{
    // The start of the title, as many lines as fit inside the circle.
    const int line = lv_font_get_line_height(font_ja());
    const int lines = 3;
    const float r = DOT_SIZE / 2.0f;
    const float half = lines * line / 2.0f;
    const int width = 2 * (int)sqrtf(r * r - half * half) - 8;
    for (int i = 0; i < RING_SLOTS; i++) {
        s_dots[i] = widget_circle(scr, DOT_SIZE, theme_button(), on_dot, (void *)(intptr_t)i);
        ring_place_at(s_dots[i], i, DOT_RADIUS);
        lv_obj_t *title = lv_obj_get_child(s_dots[i], 0);
        lv_label_set_long_mode(title, LV_LABEL_LONG_MODE_DOTS);
        lv_obj_set_size(title, width, lines * line);
        lv_obj_set_style_text_font(title, font_ja(), 0);
    }

    center_create(scr);

    // A column of dots on the right edge, one per page.
    s_pager = lv_obj_create(scr);
    lv_obj_remove_style_all(s_pager);
    lv_obj_set_size(s_pager, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(s_pager, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_pager, 6, 0);
    lv_obj_set_clickable(s_pager, false);
    lv_obj_align(s_pager, LV_ALIGN_RIGHT_MID, -6, RING_OFFSET_Y);
    for (int i = 0; i < AGENT_MAX / AGENT_PAGE; i++) {
        lv_obj_t *dot = lv_obj_create(s_pager);
        lv_obj_remove_style_all(dot);
        lv_obj_set_size(dot, 6, 6);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    }

    s_empty = lv_label_create(scr);
    lv_obj_set_style_text_color(s_empty, theme_text_muted(), 0);
    lv_obj_set_style_text_align(s_empty, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_empty, LV_ALIGN_CENTER, 0, RING_OFFSET_Y + CENTER_SIZE / 2 + 36);

    // The Mac app may have sent a list before the UI existed.
    s_count = agents_copy(s_agents, AGENT_MAX);
    refresh_ring();
    refresh_usage();
    agents_on_change(on_agents_change);
    usage_on_change(on_usage_change);
}

// Wraps around: past the last page comes the first again.
static void agents_page(int delta)
{
    const int pages = page_count();
    if (pages < 2) {
        return;
    }
    s_page = ((s_page + delta) % pages + pages) % pages;
    refresh_ring();
}

static void agents_knob(int detents)
{
    agents_page(detents > 0 ? 1 : -1);
}

const screen_def_t agents_screen = {
    .name = "Agents",
    .icon = LV_SYMBOL_LIST,
    .create = agents_create,
    .knob = agents_knob,
    .page = agents_page,
};

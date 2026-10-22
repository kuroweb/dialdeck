#include "status_bar.h"

#include <stdio.h>

#include "agents.h"
#include "agents_screen.h"
#include "battery.h"
#include "ble_hid.h"
#include "theme.h"
#include "ui.h"

typedef struct {
    lv_obj_t *ble;
    lv_obj_t *battery;
    lv_obj_t *agents;
} status_bar_t;

static status_bar_t s_bars[UI_SCREEN_COUNT];
static int s_count;

void status_bar_create(lv_obj_t *scr)
{
    lv_obj_t *bar = lv_obj_create(scr);
    lv_obj_remove_style_all(bar);
    lv_obj_set_size(bar, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(bar, 14, 0);
    lv_obj_set_style_text_font(bar, THEME_FONT_BODY, 0);
    lv_obj_set_clickable(bar, false);
    lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, THEME_STATUS_Y);

    status_bar_t *s = &s_bars[s_count++];
    s->ble = lv_label_create(bar);
    s->battery = lv_label_create(bar);
    lv_obj_set_style_text_color(s->battery, theme_text_muted(), 0);
    s->agents = lv_label_create(bar);
}

static void battery_text(char *buf, size_t len)
{
    const int percent = battery_percent();
    if (battery_charging()) {
        snprintf(buf, len, LV_SYMBOL_CHARGE);
    } else if (percent < 0) {
        snprintf(buf, len, LV_SYMBOL_BATTERY_EMPTY);
    } else {
        const char *icon = percent > 80 ? LV_SYMBOL_BATTERY_FULL
                           : percent > 55 ? LV_SYMBOL_BATTERY_3
                           : percent > 30 ? LV_SYMBOL_BATTERY_2
                           : percent > 10 ? LV_SYMBOL_BATTERY_1
                           : LV_SYMBOL_BATTERY_EMPTY;
        snprintf(buf, len, "%d%% %s", percent, icon);
    }
}

void status_bar_refresh(void)
{
    const ble_hid_state_t state = ble_hid_state();
    const lv_color_t ble_color = state == BLE_HID_STATE_CONNECTED ? theme_accent_text()
                                 : state == BLE_HID_STATE_PAIRING ? theme_warning()
                                 : state == BLE_HID_STATE_ADVERTISING ? theme_text_muted()
                                 : theme_button_dim();
    char ble[16];
    snprintf(ble, sizeof(ble), LV_SYMBOL_BLUETOOTH "%d", ble_hid_host() + 1);

    char battery[16];
    battery_text(battery, sizeof(battery));

    // Like the Raycast menu bar: the most urgent status, and a count only when something needs
    // the user. With no agents, a dim dot; dimmer still before the Mac app has sent anything.
    const int attention = agents_attention();
    const int dominant = agents_dominant();
    char agents[16];
    lv_color_t agents_color;
    if (dominant >= 0) {
        snprintf(agents, sizeof(agents), attention > 0 ? "%s%d" : "%s", agents_status_icon(dominant), attention);
        agents_color = agents_status_color(dominant);
    } else {
        snprintf(agents, sizeof(agents), "%s", agents_status_icon(AGENT_IDLE));
        agents_color = agents_known() ? theme_text_muted() : theme_button_dim();
    }

    for (int i = 0; i < s_count; i++) {
        lv_label_set_text(s_bars[i].ble, ble);
        lv_obj_set_style_text_color(s_bars[i].ble, ble_color, 0);
        lv_label_set_text(s_bars[i].battery, battery);
        lv_label_set_text(s_bars[i].agents, agents);
        lv_obj_set_style_text_color(s_bars[i].agents, agents_color, 0);
    }
}

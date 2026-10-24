// The look shared by every screen: colors, fonts and the vertical layout. Screens use these
// instead of LVGL palettes, fonts or coordinates directly, so they stay consistent.
//
// Palette taken from the deck: blue-grey for buttons and rows, teal for anything selected or
// active, white text on black, and the herdr status colors used by the Raycast extension.
#pragma once

#include "lvgl.h"

// Colors

static inline lv_color_t theme_button(void)
{
    return lv_palette_darken(LV_PALETTE_BLUE_GREY, 2);
}

// Rows, outlines and anything that sits behind buttons.
static inline lv_color_t theme_button_dim(void)
{
    return lv_palette_darken(LV_PALETTE_BLUE_GREY, 4);
}

// Selected, active, connected.
static inline lv_color_t theme_accent(void)
{
    return lv_palette_darken(LV_PALETTE_TEAL, 2);
}

// The accent as text or an icon on black, where the darker fill would be hard to read.
static inline lv_color_t theme_accent_text(void)
{
    return lv_palette_main(LV_PALETTE_TEAL);
}

static inline lv_color_t theme_text(void)
{
    return lv_color_white();
}

static inline lv_color_t theme_text_muted(void)
{
    return lv_palette_main(LV_PALETTE_GREY);
}

// Waiting for the user: pairing, an agent asking for a reply.
static inline lv_color_t theme_warning(void)
{
    return lv_palette_main(LV_PALETTE_AMBER);
}

static inline lv_color_t theme_danger(void)
{
    return lv_palette_main(LV_PALETTE_RED);
}

static inline lv_color_t theme_success(void)
{
    return lv_palette_main(LV_PALETTE_GREEN);
}

static inline lv_color_t theme_busy(void)
{
    return lv_palette_main(LV_PALETTE_ORANGE);
}

// Fonts

#define THEME_FONT_TITLE  (&lv_font_montserrat_20)   // screen titles, menu items
#define THEME_FONT_BODY   (&lv_font_montserrat_14)   // labels, values, the status bar
// Text that comes from a host (agent titles) uses font_ja() from fonts/font_ja.h.

// Layout of a screen, top to bottom. The status bar sits in the band the menu is pulled down from.

#define THEME_STATUS_Y      14
#define THEME_TITLE_Y       48    // titles of list screens
#define THEME_HINT_Y        (-40) // from the bottom

// Lists (menu, connect, settings).
#define THEME_ROW_WIDTH     240
#define THEME_ROW_HEIGHT    34
#define THEME_ROW_GAP       6

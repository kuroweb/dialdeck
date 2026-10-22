// Colors shared by every screen, taken from the deck: blue-grey round buttons, teal for the
// center button and anything selected or active, white text on black.
#pragma once

#include "lvgl.h"

static inline lv_color_t theme_button(void)
{
    return lv_palette_darken(LV_PALETTE_BLUE_GREY, 2);
}

// Background of rows and outlines that should sit behind the buttons.
static inline lv_color_t theme_button_dim(void)
{
    return lv_palette_darken(LV_PALETTE_BLUE_GREY, 4);
}

static inline lv_color_t theme_accent(void)
{
    return lv_palette_darken(LV_PALETTE_TEAL, 2);
}

static inline lv_color_t theme_text_muted(void)
{
    return lv_palette_main(LV_PALETTE_GREY);
}

#include "font_ja.h"

LV_FONT_DECLARE(font_ja_16);

const lv_font_t *font_ja(void)
{
    // The generated font is const; copy it once to chain the symbol font as fallback.
    static lv_font_t font;
    if (font.dsc == NULL) {
        font = font_ja_16;
        font.fallback = &lv_font_montserrat_14;
    }
    return &font;
}

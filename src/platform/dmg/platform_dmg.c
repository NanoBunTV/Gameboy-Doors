#include <gb/gb.h>
#include "platform.h"

const char platform_name[] = "DMG";

void platform_init(void) {
    // Shade numbers match scripts/assets.py: 0 = white, 1 = light grey, 2 = dark grey, 3 = black
    BGP_REG  = DMG_PALETTE(DMG_WHITE, DMG_LITE_GRAY, DMG_DARK_GRAY, DMG_BLACK);
    // Sprites: colour 0 is transparent. A sprite gets one of these per 8x16 column (DMG_OBP in assets.py)
    OBP0_REG = DMG_PALETTE(DMG_WHITE, DMG_WHITE, DMG_DARK_GRAY, DMG_BLACK);      // white, dark, black
    OBP1_REG = DMG_PALETTE(DMG_WHITE, DMG_LITE_GRAY, DMG_DARK_GRAY, DMG_BLACK);  // light, dark, black
}

#include <gb/gb.h>
#include "platform.h"

const char platform_name[] = "DMG";

uint8_t platform_fade;
static uint8_t dirty;

// Shade numbers match scripts/assets.py: 0 = white, 1 = light grey, 2 = dark grey, 3 = black.
// Background: colour n is shade n. Sprites: colour 0 is transparent; a sprite gets one of these per 8x16
// column (DMG_OBP in assets.py).
static const uint8_t bgp[4]  = { 0, 1, 2, 3 };
static const uint8_t obp0[4] = { 0, 0, 2, 3 };   // white, dark, black
static const uint8_t obp1[4] = { 0, 1, 2, 3 };   // light, dark, black

// A palette register with every shade darkened by the fade level
static uint8_t reg(const uint8_t *shades) {
    uint8_t v = 0;
    for (uint8_t i = 0; i < 4; i++) {
        uint8_t s = shades[i] + platform_fade;
        v |= (s > 3 ? 3 : s) << (i * 2);
    }
    return v;
}

void platform_init(void) {
    platform_fade = 0;
    dirty = 1;
    platform_vblank();
}

void platform_set_fade(uint8_t level) {
    platform_fade = level;
    dirty = 1;
}

void platform_vblank(void) {
    if (!dirty)
        return;
    dirty = 0;
    BGP_REG = reg(bgp);
    OBP0_REG = reg(obp0);
    OBP1_REG = reg(obp1);
}

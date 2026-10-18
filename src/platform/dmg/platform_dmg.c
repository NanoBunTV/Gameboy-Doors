#include <gb/gb.h>
#include "platform.h"

const char platform_name[] = "DMG";

uint8_t platform_fade;
uint8_t platform_dim;
#define dim platform_dim
static uint8_t bgp_out, obp0_out, obp1_out, dirty;

// Shade numbers match scripts/assets.py: 0 = white, 1 = light grey, 2 = dark grey, 3 = black.
// Background: colour n is shade n. Sprites: colour 0 is transparent; a sprite gets one of these per 8x16
// column (DMG_OBP in assets.py).
static const uint8_t bgp[4]  = { 0, 1, 2, 3 };
static const uint8_t obp0[4] = { 0, 0, 2, 3 };   // white, dark, black
static const uint8_t obp1[4] = { 0, 1, 2, 3 };   // light, dark, black

// A palette register with every shade darkened by `by` levels
static uint8_t reg(const uint8_t *shades, uint8_t by) {
    uint8_t v = 0;
    for (uint8_t i = 0; i < 4; i++) {
        uint8_t s = shades[i] + by;
        v |= (s > 3 ? 3 : s) << (i * 2);
    }
    return v;
}

static void update(void) {
    // The DMG has one background palette, so the HUD dims with the room
    bgp_out = reg(bgp, platform_fade + dim);
    obp0_out = reg(obp0, platform_fade);
    obp1_out = reg(obp1, platform_fade);
    dirty = 1;
}

void platform_init(void) {
    platform_fade = 0;
    update();
    platform_vblank();
}

void platform_set_fade(uint8_t level) {
    platform_fade = level;
    update();
}

void platform_set_dim(uint8_t level, uint8_t tint) {
    (void)tint;   // no colour on DMG: Ambush's sound tells him apart
    if (level == dim)
        return;
    dim = level;
    update();
}

uint8_t platform_vblank(void) {
    if (!dirty)
        return 0;   // three register writes: never in the way of the map writes
    dirty = 0;
    BGP_REG = bgp_out;
    OBP0_REG = obp0_out;
    OBP1_REG = obp1_out;
    return 0;
}

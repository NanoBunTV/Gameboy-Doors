#pragma bank 255
#include <gb/gb.h>
#include "platform.h"
#include "platform_dmg.h"

// Banked (moved out of platform_dmg.c, 2026-10-19, with the GBC's): fading, dimming, the flash

#define dim platform_dim
#define flash platform_flash

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

void dmg_update(void) BANKED {
    // The DMG has one background palette, so the HUD dims with the room
    dmg_bgp_out = flash ? 0 : reg(bgp, platform_fade + dim);   // Halt's flash: every shade white
    dmg_obp0_out = reg(obp0, platform_fade);
    dmg_obp1_out = reg(obp1, platform_fade);
    dmg_dirty = 1;
}

void platform_set_fade(uint8_t level) BANKED {
    platform_fade = level;
    dmg_update();
}

void platform_set_dim(uint8_t level, uint8_t tint) BANKED {
    (void)tint;   // no colour on DMG: Ambush's sound tells him apart
    if (level == dim)
        return;
    dim = level;
    dmg_update();
}

void platform_set_flash(uint8_t on) BANKED {
    flash = on;
    dmg_update();
}

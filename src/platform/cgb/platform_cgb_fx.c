#pragma bank 255
#include <gb/gb.h>
#include <gb/cgb.h>
#include <string.h>
#include "platform.h"
#include "platform_cgb.h"
#include "sram.h"

// Banked (moved out of platform_cgb.c, 2026-10-19: bank 0 was 95% full): fading, dimming, the tints, the flash.
// Everything here reads only WRAM and SRAM, never a caller's bank.

#define PAL_TEXT 7   // room.h: the font's palette, never dimmed
#define tint platform_tint
#define flash platform_flash
#define dim platform_dim

// The room palettes (0-6) at each dim level and tint, made the first time they're needed after the
// palettes or the fade change: the light flicker switches level every few frames, and recomputing 28
// colours each time cost the GBC whole frames.
// Not blue: Halt's hallway sets it once. In SRAM bank 1 (sram.h): only ever read after dim_valid says it was made.
palette_color_t __at(SRAM_DIM) dim_cache[2][4][28];   // per tint (none, green)
static uint8_t dim_valid[2];   // bit per level

// Scales every channel of n colours by keep/9 (keep 0-9), then tints: green (Ambush) or blue (Halt)
static void scale(palette_color_t *out, const palette_color_t *in, uint8_t n, uint8_t keep, uint8_t t) {
    uint8_t tab[32];
    for (uint8_t v = 0; v < 32; v++)
        tab[v] = (uint8_t)((v * keep) / 9);
    while (n--) {
        uint16_t c = *in++;
        uint8_t r = tab[c & 31], g = tab[(c >> 5) & 31], b = tab[(c >> 10) & 31];
        if (t == TINT_GREEN) {
            r >>= 2;
            b >>= 1;
            g = g < 24 ? g + 6 : 31;
        } else if (t == TINT_BLUE) {
            r >>= 2;
            g -= g >> 2;
            b = b < 22 ? b + 9 : 31;
        }
        *out++ = r | ((uint16_t)g << 5) | ((uint16_t)b << 10);
    }
}

// Fade and dim levels 0-3 each keep (3 - level) / 3 of the brightness; together, the product
static void update_bkg(void) {
    uint8_t g = tint;
    if (flash) {
        for (uint8_t i = 0; i < 28; i++)
            cgb_bkg_out[i] = RGB(31, 31, 31);
    } else if (g == TINT_BLUE) {
        scale(cgb_bkg_out, cgb_bkg, 28, (FADE_BLACK - platform_fade) * (3 - dim), g);
    } else {
        if (!(dim_valid[g] & (1 << dim))) {
            scale(dim_cache[g][dim], cgb_bkg, 28, (FADE_BLACK - platform_fade) * (3 - dim), g);
            dim_valid[g] |= 1 << dim;
        }
        memcpy(cgb_bkg_out, dim_cache[g][dim], sizeof(dim_cache[0][0]));
    }
    cgb_bkg_dirty = 1;
}

// The palettes or the fade changed: every cached level is stale, and the text palette needs redoing
void cgb_bkg_changed(void) BANKED {
    dim_valid[0] = dim_valid[1] = 0;
    scale(cgb_bkg_out + 28, cgb_bkg + 28, 4, (FADE_BLACK - platform_fade) * 3, TINT_NONE);
    update_bkg();
}

void cgb_update_spr(void) BANKED {
    scale(cgb_spr_out, cgb_spr, 32, (FADE_BLACK - platform_fade) * 3, TINT_NONE);
    cgb_spr_dirty = 1;
}

void platform_set_fade(uint8_t level) BANKED {
    platform_fade = level;
    cgb_bkg_changed();
    cgb_update_spr();
}

void platform_set_dim(uint8_t level, uint8_t t) BANKED {
    if (level == dim && t == tint)
        return;
    dim = level;
    tint = t;
    update_bkg();
}

void platform_set_flash(uint8_t on) BANKED {
    flash = on;
    update_bkg();
}

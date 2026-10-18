#include <gb/gb.h>
#include <gb/cgb.h>
#include <string.h>
#include "platform.h"

const char platform_name[] = "GBC";

#define PAL_TEXT 7   // room.h: the font's palette, never dimmed

uint8_t platform_fade;
uint8_t platform_dim;
uint8_t platform_tint;   // not static: scripts/scroll-test.py skips green (Ambush) frames
#define tint platform_tint
uint8_t platform_flash;
#define flash platform_flash
#define dim platform_dim
static palette_color_t bkg[32], spr[32];       // 8 palettes x 4 colours each, as set
static palette_color_t bkg_out[32], spr_out[32]; // with fade and dimming applied, for the next VBlank
static uint8_t bkg_dirty, spr_dirty;
// The room palettes (0-6) at each dim level and tint, made the first time they're needed after the
// palettes or the fade change: the light flicker switches level every few frames, and recomputing 28
// colours each time cost the GBC whole frames.
// Not blue: Halt's hallway sets it once (and WRAM bank 1 has no room for a third set).
static palette_color_t dim_cache[2][4][28];   // per tint (none, green)
static uint8_t dim_valid[2];   // bit per level

void platform_init(void) {
    // Double speed: the ROM is GBC only, so this is always available. GBDK does the STOP sequence.
    cpu_fast();
}

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
            bkg_out[i] = RGB(31, 31, 31);
    } else if (g == TINT_BLUE) {
        scale(bkg_out, bkg, 28, (FADE_BLACK - platform_fade) * (3 - dim), g);
    } else {
        if (!(dim_valid[g] & (1 << dim))) {
            scale(dim_cache[g][dim], bkg, 28, (FADE_BLACK - platform_fade) * (3 - dim), g);
            dim_valid[g] |= 1 << dim;
        }
        memcpy(bkg_out, dim_cache[g][dim], sizeof(dim_cache[0][0]));
    }
    bkg_dirty = 1;
}

// The palettes or the fade changed: every cached level is stale, and the text palette needs redoing
static void bkg_changed(void) {
    dim_valid[0] = dim_valid[1] = 0;
    scale(bkg_out + 28, bkg + 28, 4, (FADE_BLACK - platform_fade) * 3, TINT_NONE);
    update_bkg();
}

static void update_spr(void) {
    scale(spr_out, spr, 32, (FADE_BLACK - platform_fade) * 3, TINT_NONE);
    spr_dirty = 1;
}

void platform_set_bkg_palettes(uint8_t first, const palette_color_t *colours, uint8_t count) {
    memcpy(&bkg[first * 4], colours, count * 4 * sizeof(palette_color_t));
    bkg_changed();
}

void platform_set_sprite_palettes(uint8_t first, const palette_color_t *colours, uint8_t count) {
    memcpy(&spr[first * 4], colours, count * 4 * sizeof(palette_color_t));
    update_spr();
}

void platform_set_fade(uint8_t level) {
    platform_fade = level;
    bkg_changed();
    update_spr();
}

void platform_set_dim(uint8_t level, uint8_t t) {
    if (level == dim && t == tint)
        return;
    dim = level;
    tint = t;
    update_bkg();
}

void platform_set_flash(uint8_t on) {
    flash = on;
    update_bkg();
}

// Palette RAM can't be written while the LCD draws (mode 3), so this only runs from bg_flush() right
// after vsync(): 64 bytes per set at double speed, before the STAT-checked map writes.
uint8_t platform_vblank(void) {
    uint8_t busy = 0;
    if (bkg_dirty) {
        bkg_dirty = 0;
        set_bkg_palette(0, 8, bkg_out);
        busy = 1;
    }
    if (spr_dirty) {
        spr_dirty = 0;
        set_sprite_palette(0, 8, spr_out);
        busy = 1;
    }
    return busy;
}

#include <gb/gb.h>
#include <gb/cgb.h>
#include <string.h>
#include "platform.h"

const char platform_name[] = "GBC";

uint8_t platform_fade;
static palette_color_t bkg[32], spr[32];   // 8 palettes x 4 colours each, before fading
static uint8_t dirty;

void platform_init(void) {
    // Double speed: the ROM is GBC only, so this is always available. GBDK does the STOP sequence.
    cpu_fast();
}

void platform_set_bkg_palettes(uint8_t first, const palette_color_t *colours, uint8_t count) {
    memcpy(&bkg[first * 4], colours, count * 4 * sizeof(palette_color_t));
    dirty = 1;
}

void platform_set_sprite_palettes(uint8_t first, const palette_color_t *colours, uint8_t count) {
    memcpy(&spr[first * 4], colours, count * 4 * sizeof(palette_color_t));
    dirty = 1;
}

void platform_set_fade(uint8_t level) {
    platform_fade = level;
    dirty = 1;
}

// Each channel scaled by (FADE_BLACK - level) / FADE_BLACK
static void faded(palette_color_t *out, const palette_color_t *in) {
    uint8_t keep = FADE_BLACK - platform_fade;
    for (uint8_t i = 0; i < 32; i++) {
        uint16_t c = in[i];
        uint8_t r = c & 31, g = (c >> 5) & 31, b = (c >> 10) & 31;
        if (keep != FADE_BLACK) {
            r = (uint8_t)((r * keep) / FADE_BLACK);
            g = (uint8_t)((g * keep) / FADE_BLACK);
            b = (uint8_t)((b * keep) / FADE_BLACK);
        }
        out[i] = r | ((uint16_t)g << 5) | ((uint16_t)b << 10);
    }
}

// Palette RAM can't be written while the LCD draws (mode 3), so this only runs from bg_flush() right
// after vsync(). 128 bytes at double speed fit comfortably in VBlank; nothing else heavy happens in a
// frame that changes palettes (fades pause the game).
void platform_vblank(void) {
    static palette_color_t out[32];
    if (!dirty)
        return;
    dirty = 0;
    faded(out, bkg);
    set_bkg_palette(0, 8, out);
    faded(out, spr);
    set_sprite_palette(0, 8, out);
}

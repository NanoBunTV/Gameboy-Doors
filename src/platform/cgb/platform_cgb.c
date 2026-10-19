#include <gb/gb.h>
#include <gb/cgb.h>
#include <string.h>
#include "platform.h"
#include "platform_cgb.h"

// Bank 0: the palette setters (their colours are in the caller's bank, switched in) and the VBlank copy. The
// fading, dimming and tints are banked, in platform_cgb_fx.c.

const char platform_name[] = "GBC";

uint8_t platform_fade;
uint8_t platform_dim;
uint8_t platform_tint;   // not static: scripts/scroll-test.py skips green (Ambush) frames
uint8_t platform_flash;
palette_color_t cgb_bkg[32], cgb_spr[32];
palette_color_t cgb_bkg_out[32], cgb_spr_out[32];
uint8_t cgb_bkg_dirty, cgb_spr_dirty;

void platform_init(void) {
    // Double speed: the ROM is GBC only, so this is always available. GBDK does the STOP sequence.
    cpu_fast();
}

void platform_set_bkg_palettes(uint8_t first, const palette_color_t *colours, uint8_t count) {
    memcpy(&cgb_bkg[first * 4], colours, count * 4 * sizeof(palette_color_t));
    cgb_bkg_changed();
}

void platform_set_sprite_palettes(uint8_t first, const palette_color_t *colours, uint8_t count) {
    memcpy(&cgb_spr[first * 4], colours, count * 4 * sizeof(palette_color_t));
    cgb_update_spr();
}

// Palette RAM can't be written while the LCD draws (mode 3), so this only runs from bg_flush() right
// after vsync(): 64 bytes per set at double speed, before the STAT-checked map writes.
uint8_t platform_vblank(void) {
    uint8_t busy = 0;
    if (cgb_bkg_dirty) {
        cgb_bkg_dirty = 0;
        set_bkg_palette(0, 8, cgb_bkg_out);
        busy = 1;
    }
    if (cgb_spr_dirty) {
        cgb_spr_dirty = 0;
        set_sprite_palette(0, 8, cgb_spr_out);
        busy = 1;
    }
    return busy;
}

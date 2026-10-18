#include <gb/gb.h>
#include <gb/cgb.h>
#include "platform.h"

const char platform_name[] = "GBC";

void platform_init(void) {
    // Double speed: the ROM is GBC only, so this is always available. GBDK does the STOP sequence.
    cpu_fast();
}

void platform_set_bkg_palettes(const palette_color_t *colours, uint8_t count) {
    set_bkg_palette(0, count, colours);
}

void platform_set_sprite_palettes(uint8_t first, const palette_color_t *colours, uint8_t count) {
    set_sprite_palette(first, count, colours);
}

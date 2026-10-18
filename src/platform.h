// Things that differ between the GBC and DMG builds. Implemented in platform/cgb/ and platform/dmg/;
// the Makefile compiles only the target's own folder and defines CGB or DMG.
#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdint.h>
#include <gb/cgb.h>

#if defined(CGB) == defined(DMG)
#error "Define exactly one of CGB or DMG (the Makefile does this)"
#endif

// Short name of the build, shown on screen
extern const char platform_name[];

// Once at boot, with the LCD off: CPU speed, default palettes
void platform_init(void);

#ifdef CGB
// Loads background / sprite palettes starting at palette 0 / first
void platform_set_bkg_palettes(const palette_color_t *colours, uint8_t count);
void platform_set_sprite_palettes(uint8_t first, const palette_color_t *colours, uint8_t count);
#endif

#endif

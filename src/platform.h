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
// Sets background / sprite palettes. They're kept in WRAM and written to the hardware in the next
// VBlank (platform_vblank), with the current fade applied.
void platform_set_bkg_palettes(uint8_t first, const palette_color_t *colours, uint8_t count);
void platform_set_sprite_palettes(uint8_t first, const palette_color_t *colours, uint8_t count);
#endif

// Fade to black: 0 = normal ... FADE_BLACK = everything black. Takes effect in the next VBlank.
#define FADE_BLACK 3
void platform_set_fade(uint8_t level);
extern uint8_t platform_fade;

// Called by bg_flush() in VBlank: writes changed palettes
void platform_vblank(void);

#endif

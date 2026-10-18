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
// VBlank (platform_vblank), with the current fade and dimming applied.
void platform_set_bkg_palettes(uint8_t first, const palette_color_t *colours, uint8_t count);
void platform_set_sprite_palettes(uint8_t first, const palette_color_t *colours, uint8_t count);
#endif

// Fade to black: 0 = normal ... FADE_BLACK = everything black. Takes effect in the next VBlank.
#define FADE_BLACK 3
void platform_set_fade(uint8_t level);
extern uint8_t platform_fade;

// Dims the room's background only (not sprites, not the GBC text palette): 0 = normal ... 3 = black.
// Used for the light flicker and while hiding. TINT_GREEN (GBC only; ignored on DMG) gives Ambush's
// green flicker. Cheap to call every frame: nothing happens unless the values change.
#define TINT_NONE  0
#define TINT_GREEN 1
#define TINT_BLUE  2   // Halt's hallway
void platform_set_dim(uint8_t level, uint8_t tint);

// Halt's flash: the whole background white while on (both consoles)
void platform_set_flash(uint8_t on);
extern uint8_t platform_flash;   // (scripts/scroll-test.py skips flashed frames)
extern uint8_t platform_dim;   // the current level (scripts/scroll-test.py skips dimmed frames)

// Called by bg_flush() in VBlank: writes changed palettes. All the arithmetic happens in the set_*
// functions, so this is only a copy. Returns 1 if that took a large part of VBlank (GBC palette RAM), so
// bg_flush leaves the map writes for the next frame.
uint8_t platform_vblank(void);

#endif

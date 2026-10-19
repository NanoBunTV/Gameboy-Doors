// What platform_cgb.c (bank 0: the palette setters, the VBlank copy) and platform_cgb_fx.c (banked: fading,
// dimming, tints, the flash) share. Not for anything else.
#ifndef PLATFORM_CGB_H
#define PLATFORM_CGB_H
#include <gb/gb.h>
#include <gb/cgb.h>

extern palette_color_t cgb_bkg[32], cgb_spr[32];         // 8 palettes x 4 colours each, as set
extern palette_color_t cgb_bkg_out[32], cgb_spr_out[32]; // with fade and dimming applied, for the next VBlank
extern uint8_t cgb_bkg_dirty, cgb_spr_dirty;
extern uint8_t platform_tint;   // TINT_*

// The palettes changed: remake what VBlank copies (platform_cgb_fx.c)
void cgb_bkg_changed(void) BANKED;
void cgb_update_spr(void) BANKED;

#endif

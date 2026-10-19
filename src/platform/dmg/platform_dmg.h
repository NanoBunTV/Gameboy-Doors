// What platform_dmg.c (bank 0: the VBlank copy) and platform_dmg_fx.c (banked) share. Not for anything else.
#ifndef PLATFORM_DMG_H
#define PLATFORM_DMG_H
#include <gb/gb.h>

extern uint8_t dmg_bgp_out, dmg_obp0_out, dmg_obp1_out, dmg_dirty;   // for the next VBlank

// Remakes the three palette registers from the fade, dim and flash
void dmg_update(void) BANKED;

#endif

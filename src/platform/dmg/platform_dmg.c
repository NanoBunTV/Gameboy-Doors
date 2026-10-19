#include <gb/gb.h>
#include "platform.h"
#include "platform_dmg.h"

// Bank 0: the VBlank copy. Fading, dimming and the flash are banked, in platform_dmg_fx.c (as on the GBC).

const char platform_name[] = "DMG";

uint8_t platform_fade;
uint8_t platform_dim;
uint8_t platform_flash;
uint8_t dmg_bgp_out, dmg_obp0_out, dmg_obp1_out, dmg_dirty;

void platform_init(void) {
    platform_fade = 0;
    dmg_update();
    platform_vblank();
}

uint8_t platform_vblank(void) {
    if (!dmg_dirty)
        return 0;   // three register writes: never in the way of the map writes
    dmg_dirty = 0;
    BGP_REG = dmg_bgp_out;
    OBP0_REG = dmg_obp0_out;
    OBP1_REG = dmg_obp1_out;
    return 0;
}

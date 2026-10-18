#include <gb/gb.h>
#include <string.h>
#include "sprites.h"
#include "platform.h"
#include "bg.h"

static uint8_t oam_next;   // next free shadow OAM entry this frame
static uint8_t oam_used;   // entries used last frame
static uint8_t *oam_ptr;   // its first byte (pointer writes: indexing the OAM structs cost ~9 DMG scanlines a sprite)

uint8_t spr_sheet_load(spr_sheet_t *rt, uint8_t bank, const sprite_sheet_t *src,
                       uint8_t base_tile, uint8_t base_pal, uint8_t *frame_buf) {
    uint8_t saved = CURRENT_BANK;
    SWITCH_ROM(bank);
    uint8_t count = src->tile_count;
    // STAT-safe: sheets are reloaded with the LCD on (game_start, after a jumpscare used their VRAM)
    for (uint8_t t = 0; t < count; t++)
        vram_write(0x8000u + ((uint16_t)(base_tile + t) << 4), src->tiles + ((uint16_t)t << 4), 16);
    memcpy(frame_buf, src->frames, (uint16_t)src->frame_count << 2);
#ifdef CGB
    platform_set_sprite_palettes(base_pal, src->palettes, src->palette_count);
#endif
    SWITCH_ROM(saved);
    rt->base_tile = base_tile;
    rt->base_pal = base_pal;
    rt->frames = frame_buf;
    return count;
}

void spr_begin(void) {
    oam_next = 0;
    oam_ptr = (uint8_t *)shadow_OAM;
}

static void put(const spr_sheet_t *sheet, uint8_t tile, uint8_t attr, int16_t sx, int16_t sy) {
    if (tile == 0xFF || oam_next >= 40)
        return;
    // On screen: -8 < sx < 160 and -16 < sy < 144, as one unsigned compare each
    uint16_t ox = (uint16_t)(sx + 8), oy = (uint16_t)(sy + 16);
    if (ox - 1 >= 167 || oy - 1 >= 159)
        return;
    uint8_t *o = oam_ptr;
    o[0] = (uint8_t)oy;
    o[1] = (uint8_t)ox;
    o[2] = sheet->base_tile + tile;
#ifdef CGB
    o[3] = attr + sheet->base_pal;
#else
    o[3] = attr;
#endif
    oam_ptr = o + 4;
    oam_next++;
}

void spr_draw(const spr_sheet_t *sheet, uint8_t frame, int16_t sx, int16_t sy, uint8_t flags) {
    const uint8_t *f = sheet->frames + (frame << 2);
    if (flags & SPR_FLIPX) {
        // Mirrored: the right column's tiles go on the left, both flipped
        put(sheet, f[2], f[3] ^ SPR_FLIPX, sx, sy);
        put(sheet, f[0], f[1] ^ SPR_FLIPX, sx + 8, sy);
    } else {
        put(sheet, f[0], f[1], sx, sy);
        put(sheet, f[2], f[3], sx + 8, sy);
    }
}

void spr_draw_big(const spr_sheet_t *sheet, uint8_t frame, uint8_t pw, uint8_t ph, int16_t sx, int16_t sy) {
    for (uint8_t j = 0; j < ph; j++, sy += 16) {
        int16_t x = sx;
        for (uint8_t i = 0; i < pw; i++, x += 16)
            spr_draw(sheet, frame++, x, sy, 0);
    }
}

void spr_end(void) {
    // Hide what was used last frame and not this frame (y = 0 is off screen)
    for (uint8_t i = oam_next; i < oam_used; i++)
        shadow_OAM[i].y = 0;
    oam_used = oam_next;
}

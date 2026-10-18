#include <gb/gb.h>
#include <string.h>
#include "sprites.h"
#include "platform.h"

static uint8_t oam_next;   // next free shadow OAM entry this frame
static uint8_t oam_used;   // entries used last frame

void spr_sheet_load(spr_sheet_t *rt, uint8_t bank, const sprite_sheet_t *src,
                    uint8_t base_tile, uint8_t base_pal, uint8_t *frame_buf) {
    uint8_t saved = CURRENT_BANK;
    SWITCH_ROM(bank);
    set_sprite_data(base_tile, src->tile_count, src->tiles);
    memcpy(frame_buf, src->frames, (uint16_t)src->frame_count << 2);
#ifdef CGB
    platform_set_sprite_palettes(base_pal, src->palettes, src->palette_count);
#endif
    SWITCH_ROM(saved);
    rt->base_tile = base_tile;
    rt->base_pal = base_pal;
    rt->frames = frame_buf;
}

void spr_begin(void) {
    oam_next = 0;
}

static void put(const spr_sheet_t *sheet, uint8_t tile, uint8_t attr, int16_t sx, int16_t sy) {
    if (tile == 0xFF || oam_next >= 40)
        return;
    if (sx <= -8 || sx >= 160 || sy <= -16 || sy >= 144)
        return;
    volatile OAM_item_t *o = &shadow_OAM[oam_next++];
    o->y = (uint8_t)(sy + 16);
    o->x = (uint8_t)(sx + 8);
    o->tile = sheet->base_tile + tile;
#ifdef CGB
    o->prop = attr + sheet->base_pal;
#else
    o->prop = attr;
#endif
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

void spr_end(void) {
    // Hide what was used last frame and not this frame (y = 0 is off screen)
    for (uint8_t i = oam_next; i < oam_used; i++)
        shadow_OAM[i].y = 0;
    oam_used = oam_next;
}

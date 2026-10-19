#include <gb/gb.h>
#include <string.h>
#include "sprites.h"
#include "platform.h"
#include "bg.h"
#include "sprite_player.h"

static uint8_t oam_next;   // next free shadow OAM entry this frame
static uint8_t oam_used;   // entries used last frame
static uint8_t *oam_ptr;   // its first byte (pointer writes: indexing the OAM structs cost ~9 DMG scanlines a sprite)
uint8_t spr_clip_y = 255;

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

// Room sheets, in the order asked for: slot_src[i] is what's in VRAM at slot i (0: unknown). Loading a slot
// clears every later one (they've moved), so a match means everything before it matched too.
// Not static: scripts/sprite-test.py checks VRAM and every drawn sprite against them.
#define SLOTS 8
const sprite_sheet_t *spr_slot_src[SLOTS];
uint8_t spr_slots;
#define slot_src spr_slot_src
#define slots spr_slots
static uint8_t next_tile, next_pal, next_frame;
uint8_t spr_high_from;
static uint8_t next_high;
// The room sheets' frame tables, packed like their tiles (one pool rather than a buffer per sheet: 2026-10-18,
// it freed ~200 bytes of the GBC's WRAM for Seek)
static uint8_t frame_pool[SPR_FRAME_POOL];

void spr_room_begin(void) {
    slots = 0;
    next_tile = SPR_PLAYER_TILES;
    next_high = spr_high_from;
    next_pal = 1;
    next_frame = 0;
}

void spr_forget(void) {
    memset(slot_src, 0, sizeof(slot_src));
}

// The place is fixed by the slot order and which sheets go high, so a slot that matches still has its sheet where
// the packing puts it (as long as the tileset hasn't changed: room.c forgets every sheet when it does)
static void need(spr_sheet_t *rt, uint8_t bank, const sprite_sheet_t *src, uint8_t high) {
    for (uint8_t i = 0; i < slots; i++)
        if (slot_src[i] == src)
            return;
    uint8_t *next = high && spr_high_from ? &next_high : &next_tile;
    if (slot_src[slots] != src) {
        spr_sheet_load(rt, bank, src, *next, next_pal, frame_pool + next_frame);
        slot_src[slots] = src;
        for (uint8_t i = slots + 1; i < SLOTS; i++)
            slot_src[i] = 0;
    }
    // Where the next goes (rt holds this one's place: from this load, or from the room it was loaded in)
    uint8_t saved = CURRENT_BANK;
    SWITCH_ROM(bank);
    *next += src->tile_count;
    next_frame += src->frame_count << 2;
#ifdef CGB
    next_pal += src->palette_count;
#endif
    SWITCH_ROM(saved);
    slots++;
}

void spr_need(spr_sheet_t *rt, uint8_t bank, const sprite_sheet_t *src) {
    need(rt, bank, src, 0);
}

void spr_need_high(spr_sheet_t *rt, uint8_t bank, const sprite_sheet_t *src) {
    need(rt, bank, src, 1);
}

void spr_begin(void) {
    oam_next = 0;
    oam_ptr = (uint8_t *)shadow_OAM;
}

// Both 8x16 columns in one function with no calls: a put() per column, with five arguments on the stack,
// cost ~3 DMG scanlines a sprite (Phase 5, Halt's hallway)
void spr_draw(const spr_sheet_t *sheet, uint8_t frame, int16_t sx, int16_t sy, uint8_t flags) {
    // On screen: -16 < sy < 144 and, per column, -8 < x < 160, as one unsigned compare each
    uint16_t oy = (uint16_t)(sy + 16);
    if (oy - 1 >= 159 || sy + 8 >= (int16_t)spr_clip_y)
        return;
    const uint8_t *f = sheet->frames + (frame << 2);
    uint8_t t0, a0, t1, a1;
    if (flags & SPR_FLIPX) {   // mirrored: the right column's tiles go on the left, both flipped
        t0 = f[2]; a0 = f[3] ^ SPR_FLIPX;
        t1 = f[0]; a1 = f[1] ^ SPR_FLIPX;
    } else {
        t0 = f[0]; a0 = f[1];
        t1 = f[2]; a1 = f[3];
    }
#ifdef CGB
    a0 += sheet->base_pal;
    a1 += sheet->base_pal;
#endif
    uint8_t base = sheet->base_tile, n = oam_next, y = (uint8_t)oy;
    uint8_t *o = oam_ptr;
    uint16_t ox = (uint16_t)(sx + 8);
    if (t0 != 0xFF && n < 40 && ox - 1 < 167) {
        *o++ = y;
        *o++ = (uint8_t)ox;
        *o++ = base + t0;
        *o++ = a0;
        n++;
    }
    ox += 8;
    if (t1 != 0xFF && n < 40 && ox - 1 < 167) {
        *o++ = y;
        *o++ = (uint8_t)ox;
        *o++ = base + t1;
        *o++ = a1;
        n++;
    }
    oam_ptr = o;
    oam_next = n;
}

void spr_draw_big(const spr_sheet_t *sheet, uint8_t frame, uint8_t pw, uint8_t ph, int16_t sx, int16_t sy,
                  uint8_t flags) {
    // Mirrored: each row's parts right to left, each mirrored (spr_draw swaps its two columns)
    int16_t first = (flags & SPR_FLIPX) ? sx + ((int16_t)(pw - 1) << 4) : sx;
    int8_t dx = (flags & SPR_FLIPX) ? -16 : 16;
    for (uint8_t j = 0; j < ph; j++, sy += 16) {
        int16_t x = first;
        for (uint8_t i = 0; i < pw; i++, x += dx)
            spr_draw(sheet, frame++, x, sy, flags);
    }
}

void spr_end(void) {
    // Hide what was used last frame and not this frame (y = 0 is off screen)
    for (uint8_t i = oam_next; i < oam_used; i++)
        shadow_OAM[i].y = 0;
    oam_used = oam_next;
}

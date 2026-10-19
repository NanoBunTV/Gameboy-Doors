#pragma bank 255
#include <gb/gb.h>
#include "room.h"
#include "bg.h"

// Door-number plates (moved out of room.c, 2026-10-19: bank 0 was 95% full). Only the base tiles' copy needs
// the tileset's bank: room_plate_base, in room.c.

// 3x5 digits for door plates, one row per byte (bit 2 = left column)
static const uint8_t plate_digits[10][5] = {
    { 7, 5, 5, 5, 7 }, { 2, 6, 2, 2, 7 }, { 7, 1, 7, 4, 7 }, { 7, 1, 3, 1, 7 }, { 5, 5, 7, 1, 1 },
    { 7, 4, 7, 1, 7 }, { 7, 4, 7, 5, 7 }, { 7, 1, 1, 2, 2 }, { 7, 5, 7, 5, 7 }, { 7, 5, 7, 1, 7 },
};

// Number geometry, matching the door art (ASSETS.md): brass digits on the dark header over the door, at x 2,
// 6, 10 and y 2-6 in the metatile. Colours are palette indices (light to dark, the same on both consoles):
// the art's placeholder "000" is painted over with the dark first.
#define PLATE_DIGIT_X 2
#define PLATE_DIGIT_Y 2
// Only the metatile's top two tiles are copied (buf), so the digits must end above pixel row 8. (Until
// 2026-10-18 they ran to row 8: each digit's bottom row went past buf instead of onto the door.)
#if PLATE_DIGIT_Y + 5 > 8
#error "door digits must fit in the top 8 pixel rows"
#endif
#define PLATE_BRASS 0
#define PLATE_BACK 3

static void set_pixel(uint8_t *two_tiles, uint8_t x, uint8_t y, uint8_t colour) {
    uint8_t *row = two_tiles + (x >> 3) * 16 + y * 2;  // left tile, then right tile
    uint8_t bit = 0x80 >> (x & 7);
    row[0] = (colour & 1) ? (row[0] | bit) : (row[0] & ~bit);
    row[1] = (colour & 2) ? (row[1] | bit) : (row[1] & ~bit);
}

uint8_t room_make_plate(uint8_t slot, uint8_t base, uint8_t number) BANKED {
    uint8_t buf[32];
    uint8_t mt = MT_DYN_FIRST + slot;
    uint8_t tile = TILE_PLATE_FIRST + slot * 2;
    uint16_t b = (uint16_t)base << 2, m = (uint16_t)mt << 2;

    // The base metatile's top two tiles, from ROM
    room_plate_base(base, buf);

    // A fake door without Dupe has no number; one Dupe fired from shows scribbles (Minecraft docs 06)
    static const uint8_t blank[5] = { 0, 0, 0, 0, 0 };
    static const uint8_t garbled[3][5] = { { 5, 2, 7, 1, 6 }, { 3, 6, 2, 5, 4 }, { 6, 1, 5, 2, 7 } };
    uint8_t digits[3] = { number / 100, (number / 10) % 10, number % 10 };
    for (uint8_t d = 0; d < 3; d++) {
        const uint8_t *g = number == PLATE_BLANK ? blank : number == PLATE_GARBLED ? garbled[d] : plate_digits[digits[d]];
        uint8_t x0 = PLATE_DIGIT_X + d * 4;
        for (uint8_t y = 0; y < 5; y++)
            for (uint8_t x = 0; x < 3; x++)
                set_pixel(buf, x0 + x, PLATE_DIGIT_Y + y, (g[y] & (4 >> x)) ? PLATE_BRASS : PLATE_BACK);
    }
    vram_write(bg_tile_addr(tile), buf, 32);

    mt_tiles[m] = tile;
    mt_tiles[m + 1] = tile + 1;
    mt_tiles[m + 2] = mt_tiles[b + 2];
    mt_tiles[m + 3] = mt_tiles[b + 3];
    mt_flags[mt] = mt_flags[base];
#ifdef CGB
    mt_attrs[m] = mt_attrs[b];
    mt_attrs[m + 1] = mt_attrs[b + 1];
    mt_attrs[m + 2] = mt_attrs[b + 2];
    mt_attrs[m + 3] = mt_attrs[b + 3];
#endif
    return mt;
}

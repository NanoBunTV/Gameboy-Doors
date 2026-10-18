#include <gb/gb.h>
#include <string.h>
#include "room.h"
#include "bg.h"
#include "platform.h"
#include "tileset_hotel.h"

uint8_t room_w, room_h;
uint16_t room_px_w, room_px_h;
uint8_t room_map[ROOM_MAX];
uint16_t room_row[ROOM_SIDE_MAX];   // offset of each metatile row in room_map (no multiply per lookup)
uint16_t room_spawn_x, room_spawn_y;
uint8_t room_obj_count;
room_obj_t room_objs[ROOM_OBJ_MAX];

uint8_t mt_tiles[METATILE_MAX * 4];
uint8_t mt_flags[METATILE_MAX];
#ifdef CGB
uint8_t mt_attrs[METATILE_MAX * 4];
#endif

static uint8_t loaded_tileset = 0xFF;
static uint8_t ts_bank;
static const tileset_t *ts;

// Loads a tileset's tiles into VRAM and its metatile tables into WRAM. Switches banks; the caller restores.
static void tileset_load(uint8_t id) {
    // Only TILESET_HOTEL so far. With more, switch on id here; BANK() can't go in a static table because
    // it isn't a constant expression for SDCC.
    (void)id;
    ts = &tileset_hotel;
    ts_bank = BANK(tileset_hotel);
    SWITCH_ROM(ts_bank);
    // BG tile data uses the 0x8800 area (LCDC bit 4 clear, main.c): tiles 0-127 at 0x9000, 128-255 at 0x8800.
    // Copied with vram_write (STAT-safe) because this can happen with the LCD on, behind a fade to black.
    for (uint8_t t = 0; t < ts->tile_count; t++)
        vram_write(bg_tile_addr(t), ts->tiles + (uint16_t)t * 16, 16);
    memcpy(mt_tiles, ts->mt_tiles, (uint16_t)ts->metatile_count * 4);
    memcpy(mt_flags, ts->mt_flags, ts->metatile_count);
#ifdef CGB
    memcpy(mt_attrs, ts->mt_attrs, (uint16_t)ts->metatile_count * 4);
    platform_set_bkg_palettes(0, ts->palettes, ts->palette_count);
#endif
    loaded_tileset = id;
}

void room_forget_tileset(void) {
    loaded_tileset = 0xFF;
}

void room_load(uint8_t bank, const room_def_t *def) {
    uint8_t saved = CURRENT_BANK;
    SWITCH_ROM(bank);
    room_w = def->width;
    room_h = def->height;
    room_spawn_x = def->spawn_x;
    room_spawn_y = def->spawn_y;
    uint8_t tileset = def->tileset;
    memcpy(room_map, def->map, (uint16_t)room_w * room_h);
    room_obj_count = def->obj_count > ROOM_OBJ_MAX ? ROOM_OBJ_MAX : def->obj_count;
    memcpy(room_objs, def->objects, room_obj_count * sizeof(room_obj_t));
    room_px_w = (uint16_t)room_w << 4;
    room_px_h = (uint16_t)room_h << 4;
    uint16_t o = 0;
    for (uint8_t y = 0; y < room_h; y++, o += room_w)
        room_row[y] = o;
    if (tileset != loaded_tileset)
        tileset_load(tileset);
    SWITCH_ROM(saved);
}

uint8_t room_metatile(int16_t mx, int16_t my) {
    if ((uint16_t)mx >= room_w || (uint16_t)my >= room_h)  // negative values wrap to large ones
        return 0;
    return room_map[room_row[(uint8_t)my] + (uint8_t)mx];
}

void room_set_metatile(uint8_t mx, uint8_t my, uint8_t mt) {
    room_map[room_row[my] + mx] = mt;
    bg_refresh_metatile(mx, my);
}

uint8_t room_solid_at(int16_t x, int16_t y) {
    if (x < 0 || y < 0)
        return 1;
    return mt_flags[room_metatile(x >> 4, y >> 4)] & MTF_SOLID;
}

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

uint8_t room_make_plate(uint8_t slot, uint8_t base, uint8_t number) {
    uint8_t buf[32];
    uint8_t mt = MT_DYN_FIRST + slot;
    uint8_t tile = TILE_PLATE_FIRST + slot * 2;
    uint16_t b = (uint16_t)base << 2, m = (uint16_t)mt << 2;

    // The base metatile's top two tiles, from ROM
    uint8_t saved = CURRENT_BANK;
    SWITCH_ROM(ts_bank);
    memcpy(buf, ts->tiles + (uint16_t)mt_tiles[b] * 16, 16);
    memcpy(buf + 16, ts->tiles + (uint16_t)mt_tiles[b + 1] * 16, 16);
    SWITCH_ROM(saved);

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

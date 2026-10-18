// Data types shared by the engine and the files scripts/assets.py generates into build/gen/.
// Generated data lives in banked ROM: switch to BANK(name) before reading it.
#ifndef ASSETS_H
#define ASSETS_H

#include <gb/gb.h>
#include <gb/cgb.h>
#include <stdint.h>

// Tilesets, numbered for room_def_t.tileset. Add one here when adding assets/tilesets/<name>.tsx,
// and a case to tileset_load() in bg.c.
#define TILESET_HOTEL 0

// Metatile flags (one byte per metatile, from the Tiled tileset's properties)
#define MTF_SOLID 0x01

// A tileset: 16x16 metatiles made of four 8x8 tiles (top left, top right, bottom left, bottom right)
typedef struct {
    uint8_t tile_count;          // unique 8x8 tiles
    const uint8_t *tiles;        // tile_count * 16 bytes, 2bpp
    uint8_t metatile_count;
    const uint8_t *mt_tiles;     // 4 tile indices per metatile
    const uint8_t *mt_flags;     // 1 byte per metatile, MTF_*
#ifdef CGB
    const uint8_t *mt_attrs;     // 4 BG attribute bytes per metatile (palette in bits 0-2)
    uint8_t palette_count;
    const palette_color_t *palettes;  // 4 colours each
#endif
} tileset_t;

// A room from Tiled
typedef struct {
    uint8_t width, height;       // in metatiles
    uint8_t tileset;             // TILESET_*
    uint16_t spawn_x, spawn_y;   // player start, pixels
    const uint8_t *map;          // width * height metatile numbers, row by row
} room_def_t;

// A sprite sheet of 16x16 frames, each drawn as two 8x16 hardware sprites
typedef struct {
    uint8_t tile_count;          // 8x8 tiles (always even)
    const uint8_t *tiles;
    uint8_t frame_count;
    const uint8_t *frames;       // per frame: left tile, left attr, right tile, right attr (tile 0xFF = none)
#ifdef CGB
    uint8_t palette_count;
    const palette_color_t *palettes;  // 4 colours each, colour 0 unused (transparent)
#endif
} sprite_sheet_t;

#endif

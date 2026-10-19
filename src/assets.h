// Data types shared by the engine and the files scripts/assets.py generates into build/gen/.
// Generated data lives in banked ROM: switch to BANK(name) before reading it.
#ifndef ASSETS_H
#define ASSETS_H

#include <gb/gb.h>
#include <gb/cgb.h>
#include <stdint.h>

// Tilesets, numbered for room_def_t.tileset. Add one here when adding assets/tilesets/<name>.tsx,
// and a case to tileset_load() in room.c.
#define TILESET_HOTEL 0

// Metatile flags (one byte per metatile, from the Tiled tileset's properties)
#define MTF_SOLID 0x01
#define MTF_BOOKCASE 0x02   // a bookcase's bottom half: where the Library's books can be (library.c)
#define MTF_FIRE 0x04       // walking into it kills (Seek's end hallway; game.c)
#define MTF_WALL 0x08       // a wall Seek's eyes can appear on before a chase (walleyes.c)

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

// Room objects from the Tiled object layer (ASSETS.md), in metatile units
#define OBJ_ENTRY   1   // the door you come in by; param = WALL_*
#define OBJ_EXIT    2   // the door to the next room; param = WALL_*
#define OBJ_DRAWER  3   // on a dresser: may hold the key
#define OBJ_KEYSPOT 4   // a place a key can lie in the open; param bit 0: fixed (the start room's key)
#define OBJ_SPAWN   5   // start / test rooms: where the player starts
#define OBJ_CLOSET  6   // a hiding spot (1x2, on closet_top/closet_bottom)
#define OBJ_BED     7   // a hiding spot (1x2, on bed_top/bed_bottom)
#define OBJ_PAPER   8   // the Library: the paper on the desk
#define OBJ_FIGURE  9   // the Library: where Figure starts
#define OBJ_SEEK    10  // a start hallway: where Seek rises (optional; seek.c)

#define WALL_TOP    1
#define WALL_BOTTOM 2
#define WALL_LEFT   3
#define WALL_RIGHT  4

#define ROOM_KIND_NORMAL 0
#define ROOM_KIND_START  1
#define ROOM_KIND_TEST   2
#define ROOM_KIND_HALT   3   // Halt's hallway: never planned, loaded by halt.c
#define ROOM_KIND_LIBRARY 4  // the Library: always room RUN_LIBRARY_ROOM (run.c)
#define ROOM_KIND_SEEK_START 5   // Seek's chases (run.c run_kind_at): the hallway he rises in,
#define ROOM_KIND_SEEK_4WAY  6   // the rooms with one real door and the rest fake,
#define ROOM_KIND_SEEK_END   7   // and the hallway with the fire and the door that slams

typedef struct {
    uint8_t type, x, y, w, h, param;
} room_obj_t;

// A room template from Tiled
typedef struct {
    uint8_t width, height;       // in metatiles
    uint8_t tileset;             // TILESET_*
    uint16_t spawn_x, spawn_y;   // player start (start/test rooms), pixels
    const uint8_t *map;          // width * height metatile numbers, row by row
    uint8_t obj_count;
    const room_obj_t *objects;   // sorted by type
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

// A still picture (the title logo): tiles, a tile map, GBC palettes per tile
typedef struct {
    uint8_t width, height;       // in tiles
    uint8_t tile_count;
    const uint8_t *tiles;
    const uint8_t *map;          // width * height tile numbers (from 0)
#ifdef CGB
    const uint8_t *attrs;        // width * height palette numbers
    uint8_t palette_count;
    const palette_color_t *palettes;
#endif
} image_t;

// The font: 8x8 tiles loaded at FONT_FIRST_TILE
typedef struct {
    uint8_t tile_count;
    const uint8_t *tiles;
    const uint8_t *ascii;        // tile number for ASCII 32..127
#ifdef CGB
    const palette_color_t *palette;   // loaded as BG palette 7
#endif
} font_t;

#endif

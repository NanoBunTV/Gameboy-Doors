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
#define TILESET_GARDEN 1    // the Courtyard and the Greenhouse (rooms 89-99): the hotel's metatile numbers for
                            // doors, walls and drawers (scripts/assets.py checks), its own art
#define TILESET_ELECTRICAL 2   // Room 100's warehouse (Phase 8b), the same rule
#define TILESET_SHOP 3   // Jeff's Shop (Phase 9b): the hotel's, less what it doesn't use, plus his doorway and counter

// Metatile flags (one byte per metatile, from the Tiled tileset's properties)
#define MTF_SOLID 0x01
#define MTF_BOOKCASE 0x02   // a bookcase's bottom half: where the Library's books can be (library.c)
#define MTF_FIRE 0x04       // walking into it kills (Seek's end hallway; game.c)
#define MTF_WALL 0x08       // a wall Seek's eyes can appear on before a chase (walleyes.c)
#define MTF_SNARE 0x10      // the Greenhouse's trap: stepping on it (garden.c)
#define MTF_SHELF 0x20      // Room 100: a shelf a fuse can be put on (electrical.c)
#define MTF_GATE  0x40      // Room 100: bars the lever takes away
#define MTF_SLIDE 0x80      // Room 100: the sliding door Figure waits behind, opened by the lever

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
#define OBJ_SNARE   11  // the Greenhouse: a rectangle of cells a Snare may be on (garden.c)
// Room 100 (electrical.c)
#define OBJ_LEVER   12  // 1x2 on lever_top/lever_bottom: opens the gates and lets Figure out
#define OBJ_BREAKER 13  // 1x2 on breaker_top/breaker_bottom: the fuses go in, then the Circuit Breaker
#define OBJ_BREAKER_DOOR 14  // 1x2 on breaker_door_top/_bottom: the breaker room's door (its key: the keyspot)
#define OBJ_FIGURE_DOOR  15  // 1x2 on copper_top/_bottom: Figure comes out of it once the power's back
#define OBJ_LIFT    16  // 2x2 on lift_*: the way out once the power's back
#define OBJ_LIGHT   17  // a rectangle: a pool of light on the floor that blinks on its own clock
#define OBJ_SAFE    18  // a rectangle: the breaker room, doorway included, where Figure never goes
#define OBJ_STOCK   19  // Jeff's Shop: a place on the counter (a 'counter' metatile) the run's stock lies on
#define OBJ_JEFF    20  // Jeff's Shop: where his eyes are; param: the pixel offset in the cell (x | y << 4)
#define OBJ_BELL    21  // a desk bell (a 'desk_bell' metatile): A rings it, as often as you like (owner, 2026-10-19)

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
#define ROOM_KIND_IS_CHASE(k) ((k) >= ROOM_KIND_SEEK_START && (k) <= ROOM_KIND_SEEK_END)
#define ROOM_KIND_COURTYARD  8   // room 89 (run.c run_kind_at): outdoors, safe, rain
#define ROOM_KIND_GREENHOUSE 9   // rooms 90-99: dark, Snares, rain on the glass
#define ROOM_KIND_ELECTRICAL 10  // room 100 (Phase 8b): the warehouse, the fuses, the breaker, the lift
#define ROOM_KIND_SHOP       11  // room 55 (Phase 9b): Jeff's Shop, lit and safe

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

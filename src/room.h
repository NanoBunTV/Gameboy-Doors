// The current room: its metatile map and objects in WRAM, the tileset's metatile tables in WRAM
#ifndef ROOM_H
#define ROOM_H

#include <stdint.h>
#include "assets.h"

#define ROOM_MAX 3072           // metatiles (3 KB of the DMG's 8 KB WRAM); scripts/assets.py checks rooms against it
#define ROOM_OBJ_MAX 24
#define METATILE_MAX 128

// Background VRAM plan (scripts/assets.py checks tilesets against it): tileset tiles from 0, door plates at
// 184-191 (2 tiles each), the font at 192-255. Metatiles 120-127 are made at runtime (door plates).
#define TILE_PLATE_FIRST 184
#define MT_DYN_FIRST 120
#define PAL_TEXT 7              // GBC background palette of the font / HUD

extern uint8_t room_w, room_h;          // metatiles
extern uint16_t room_px_w, room_px_h;   // pixels
extern uint8_t room_map[ROOM_MAX];
extern uint16_t room_spawn_x, room_spawn_y;
extern uint8_t room_obj_count;
extern room_obj_t room_objs[ROOM_OBJ_MAX];

// Copies of the current tileset's tables, so drawing never needs a bank switch
extern uint8_t mt_tiles[METATILE_MAX * 4];
extern uint8_t mt_flags[METATILE_MAX];
#ifdef CGB
extern uint8_t mt_attrs[METATILE_MAX * 4];
#endif

// Loads a room template: map and objects into WRAM, and its tileset if it isn't loaded yet (the first
// time, the LCD must be off). Doesn't draw anything.
void room_load(uint8_t bank, const room_def_t *def);

// Something else used the tileset's VRAM (the title logo): reload it with the next room
void room_forget_tileset(void);

// Metatile at metatile coordinates; 0 (void, solid) outside the room
uint8_t room_metatile(int16_t mx, int16_t my);

// Changes a metatile and redraws it if it's on screen (queued for the next VBlank)
void room_set_metatile(uint8_t mx, uint8_t my, uint8_t mt);

// Is the pixel at (x, y) inside something solid? Outside the room counts as solid.
uint8_t room_solid_at(int16_t x, int16_t y);

// Makes dynamic metatile MT_DYN_FIRST + slot: a copy of `base` (a door's top half, whose number plate is
// blank in the art) with `number` drawn on the plate as 3 digits. Returns its metatile number.
uint8_t room_make_plate(uint8_t slot, uint8_t base, uint8_t number);

#endif

// The current room: its metatile map in WRAM and the current tileset's metatile tables
#ifndef ROOM_H
#define ROOM_H

#include <stdint.h>
#include "assets.h"

#define ROOM_MAX 3072           // metatiles (3 KB of the DMG's 8 KB WRAM); scripts/assets.py checks rooms against it
#define METATILE_MAX 128

extern uint8_t room_w, room_h;          // metatiles
extern uint16_t room_px_w, room_px_h;   // pixels
extern uint8_t room_map[ROOM_MAX];
extern uint16_t room_spawn_x, room_spawn_y;

// Copies of the current tileset's tables, so drawing never needs a bank switch
extern uint8_t mt_tiles[METATILE_MAX * 4];
extern uint8_t mt_flags[METATILE_MAX];
#ifdef CGB
extern uint8_t mt_attrs[METATILE_MAX * 4];
#endif

// Loads a room (map, tileset tiles, palettes). The LCD must be off.
void room_load(uint8_t bank, const room_def_t *def);

// Metatile at metatile coordinates; 0 (void, solid) outside the room
uint8_t room_metatile(int16_t mx, int16_t my);

// Is the pixel at (x, y) inside something solid? Outside the room counts as solid.
uint8_t room_solid_at(int16_t x, int16_t y);

#endif

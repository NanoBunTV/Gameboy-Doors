// Dark rooms (DESIGN.md, "Light and darkness"): every background tile is black except the ones a light
// source reaches. The same on both consoles: an unlit tile is drawn as the font's blank (all black) tile,
// so darkness costs map writes, not palettes. bg.c asks light_lit() for every tile it draws; when a source
// moves, light_set() queues just the tiles that changed (bg_refresh_tile).
//
// Three sources: the player's (a small glow; the lighter's circle and the flashlight's cone, Phase 9),
// Eyes' glow, and the door the player is near (its top half, where the number is). Shapes are per-row spans around the source's tile, made once at boot (light_shapes.c).
#ifndef LIGHT_H
#define LIGHT_H

#include <gb/gb.h>
#include <stdint.h>
#include "font.h"
#include "room.h"

// An unlit tile: the font's first character, the space, is all black
#define LIGHT_DARK_TILE FONT_FIRST_TILE
#define LIGHT_DARK_ATTR PAL_TEXT

#define LIGHT_GLOW    0   // no item: a rounded 5x5-tile patch (owner, 2026-10-18)
#define LIGHT_LIGHTER 1   // radius 3 tiles (owner: lighter r=3)
#define LIGHT_FLASH   2   // + facing (DIR_*): a cone 7 tiles long, widening to 5, plus the glow
#define LIGHT_DOORWAY 10  // a door's two metatiles (2x4 tiles from its top-left tile)
#define LIGHT_SHAPES  11
#define LIGHT_OFF     0xFF

#define LIGHT_PLAYER  0   // sources
#define LIGHT_EYES    1
#define LIGHT_DOOR    2   // the door you're near, so its number reads in the dark (owner, 2026-10-18)
#define LIGHT_SOURCES 3

#define LIGHT_ROWS    17  // a shape covers rows -8..8 around its tile
#define LIGHT_REACH   8

extern uint8_t light_dark;   // the current room is dark
// Spans per shape and row (dx from the source's tile; l > r: nothing on that row), and each shape's first
// and last row that has anything
extern int8_t light_l[LIGHT_SHAPES][LIGHT_ROWS], light_r[LIGHT_SHAPES][LIGHT_ROWS];
extern uint8_t light_first[LIGHT_SHAPES], light_last[LIGHT_SHAPES];
// The sources (shape, middle tile) and the room's size in tiles: nothing outside the room is ever lit
extern uint8_t light_src_shape[LIGHT_SOURCES], light_src_x[LIGHT_SOURCES], light_src_y[LIGHT_SOURCES];
extern uint8_t light_tw, light_th;

void light_build_shapes(void) BANKED;   // once at boot (light_shapes.c)

// A room was loaded (before bg_draw_full): dark or not, all sources off (light_move.c)
void light_room(uint8_t dark) BANKED;

// Moves or changes a source (shape LIGHT_OFF: off); its middle tile, in the room. In a dark room the tiles
// that change are queued for redrawing (light_move.c).
void light_set(uint8_t source, uint8_t shape, uint8_t tx, uint8_t ty) BANKED;

// Does up to `rows` rows of the redraw the last light_set needs; once a frame (light_set itself only
// records it, and finishes any earlier one first)
#define LIGHT_ROWS_PER_FRAME 4
void light_step(uint8_t rows) BANKED;
extern uint8_t light_pending;

// Does a source light room tile (tx, ty)? (Tile in the room; the caller checks light_dark.)
uint8_t light_lit8(uint8_t tx, uint8_t ty);

// Blacks out the unlit tiles of a column (tx, rows ty..ty+n-1) or row (columns tx..tx+n-1, row ty) that
// bg.c has just built (attrs: GBC only). Only call in a dark room. n <= 24.
void light_mask_column(int16_t tx, int16_t ty, uint8_t n, uint8_t *tiles, uint8_t *attrs);
void light_mask_row(int16_t tx, int16_t ty, uint8_t n, uint8_t *tiles, uint8_t *attrs);


#endif

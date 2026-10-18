// Figure, in the Library (Minecraft docs 07 and 10; owner 2026-10-18): blind, hunts by sound. Walking is
// heard anywhere in the room; sneaking (B held), standing still and hiding aren't. Touching him is death.
#ifndef FIGURE_H
#define FIGURE_H

#include <gb/gb.h>
#include <stdint.h>
#include "sprites.h"

extern spr_sheet_t figure_marks;   // the book glint and the edge arrows (SPR_MARKS_*), loaded with him
extern int16_t figure_x, figure_y; // his feet, room pixels (for tests)
extern uint8_t figure_state;       // FIG_* (for tests)
extern uint8_t figure_hunting;     // walking at a noise (for tests)

#define FIG_OFF   0
#define FIG_FILL  1   // working out a route (he stands and listens)
#define FIG_WALK  2
#define FIG_PAUSE 3   // got there; a moment before the next wander

// His sheet and the marks into sprite VRAM from `tile` (palettes pal, pal + 1). Returns the tiles used.
uint8_t figure_load(uint8_t tile, uint8_t pal) BANKED;

// Puts him at metatile (mx, my), wandering, deaf for `grace` frames (the hint on entering)
void figure_start(uint8_t mx, uint8_t my, uint8_t grace) BANKED;
void figure_stop(void) BANKED;

// One frame: hearing, the route search, walking, footsteps. Returns DEATH_FIGURE if he touched you.
uint8_t figure_update(void) BANKED;

// Him, and an arrow at the screen's edge pointing to him while he's off screen
void figure_draw(void) BANKED;

#endif

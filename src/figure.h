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
#define FIG_CHASE 4   // Room 100's escape: deaf, on the player's trail (figure_escape)

// The Library's sheets for this room (sprites.h spr_need): the marks (glint, arrows), and him if he's here
void figure_need_sprites(void) BANKED;

// Puts him at metatile (mx, my), wandering, deaf for `grace` frames (the hint on entering)
void figure_start(uint8_t mx, uint8_t my, uint8_t grace) BANKED;
void figure_stop(void) BANKED;

// Room 100 (electrical.c). A rectangle of metatiles he never goes into, and never hears into (the breaker
// room, doorway included: Minecraft docs 10); w = 0: none. figure_start clears it.
void figure_nogo(uint8_t x, uint8_t y, uint8_t w, uint8_t h) BANKED;
// Sends him at a point (room pixels) as if he'd heard it there, without his roar: out of his cage
void figure_go(int16_t px, int16_t py) BANKED;
// The escape: he appears at metatile (mx, my) and follows the player's trail from there, deaf, at 0.75 of
// their running speed (Seek's numbers, owner). Closets don't stop him.
void figure_escape(uint8_t mx, uint8_t my) BANKED;

// One frame: hearing, the route search, walking, footsteps. Returns DEATH_FIGURE if he touched you.
uint8_t figure_update(void) BANKED;

// Him, and an arrow at the screen's edge pointing to him while he's off screen
void figure_draw(void) BANKED;

#endif

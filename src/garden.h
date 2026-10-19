// The Courtyard (room 89) and the Greenhouse (90-99), Minecraft docs 09 (owner, 2026-10-18): the rain (a
// sound in both, streaks over the Courtyard) and the Greenhouse's Snares.
//
// Snares: three a room (none in room 99), drawn per run from the floor cells inside the room's 'snare'
// rectangles, seen only where a light reaches (they're a floor metatile). Stepping on one costs 20 HP but
// never the last 10, and roots you for 5 s (no walking, no A), then 4 s in which no Snare catches you; the
// 4 s start when the root ends, so standing up off one can't be caught by it again (Minecraft docs 09, "The
// cooldown starts when the root ends").
#ifndef GARDEN_H
#define GARDEN_H

#include <gb/gb.h>
#include <stdint.h>

#define SNARE_DAMAGE 20
#define SNARE_MIN_HP 10          // snare.min-health-fraction: two hearts of twenty
#define SNARE_ROOT_FRAMES 300    // 5 s
#define SNARE_GRACE_FRAMES 240   // 4 s, from the end of the root
#define SNARES_PER_ROOM 3        // snare.per-room (scripts/assets.py checks there are enough cells)
#define RAIN_DROPS 8             // streaks over the Courtyard

extern uint8_t snare_x[SNARES_PER_ROOM], snare_y[SNARES_PER_ROOM];   // this room's (metatiles); 0xFF: none
extern uint16_t snare_root;      // frames of the root left
extern uint8_t snare_grace;      // frames after it in which no Snare catches you

// A new run: no root
void garden_reset(void) BANKED;

// The player came into a room: the rain on or off, no root
void garden_room_entered(void) BANKED;

// The room's map was (re)loaded, before it's drawn: this room's Snares into it
void garden_room_loaded(void) BANKED;

// The rain's sheet in the Courtyard (sprites.h)
void garden_need_sprites(void) BANKED;

// Is the player held by a Snare? (game.c: no walking, no A)
uint8_t garden_rooted(void) BANKED;

// Once a frame, after the player moved: a Snare under their feet springs; the root and its grace count down
void garden_update(void) BANKED;

void garden_draw(void) BANKED;

#endif

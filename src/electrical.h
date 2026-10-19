// Room 100, the warehouse (Phase 8b; Minecraft docs 10, owner 2026-10-18). One floor. Bar gates hold the
// shelving and the lever opens them: door 100 seals, the sliding door parts from the middle, Figure comes
// out at once. Ten fuses on the shelves and the breaker room's key in a far corner; A at the breaker box puts
// in what you carry. The tenth: Figure gone, the breaker room shut behind you, then the Circuit Breaker
// (three rounds). Solved: the power's back, the lift opens, and three seconds later Figure comes out of the
// door beside the box and you run for it.
#ifndef ELECTRICAL_H
#define ELECTRICAL_H

#include <gb/gb.h>
#include <stdint.h>

#define FUSES 10                // scripts/assets.py FUSES
#define BREAKER_ROUNDS 3

// electrical_state (for tests)
#define ELEC_WAIT    0          // the lever not pulled yet
#define ELEC_OPENING 1          // pulled: the sliding door parting, Figure on his way
#define ELEC_SEARCH  2          // the fuses and the key, Figure about
#define ELEC_SEALED  3          // all ten in: Figure gone, the breaker room shut, the box's panel
#define ELEC_POWER   4          // the breaker solved: the lift open, Figure coming in 3 s
#define ELEC_ESCAPE  5          // ... he's out: run for the lift
#define ELEC_WON     6          // in the lift

// electrical_frame()'s answers (as library_frame's)
#define ELEC_FREE 0
#define ELEC_HELD 1             // the breaker's panel has the buttons

extern uint8_t electrical_state;
extern uint8_t electrical_carried;     // fuses in your pockets (the HUD shows them)
extern uint8_t electrical_inserted;    // fuses in the box

void electrical_reset(void) BANKED;    // a new run

// The room was loaded into WRAM (also after a scare): what's been done goes back into the map
void electrical_room_loaded(void) BANKED;
// Entering room 100 (after the player is placed); leaving it for room 99 (only before the lever)
void electrical_enter(void) BANKED;
void electrical_exit(void) BANKED;
void electrical_need_sprites(void) BANKED;

// Each frame in Room 100, before the player moves: the breaker's panel
uint8_t electrical_frame(void) BANKED;
// ... after: Figure, the lights, the sliding door, the escape. Returns DEATH_* (DEATH_ESCAPE when he gets
// you in the escape).
uint8_t electrical_update(void) BANKED;
void electrical_draw(void) BANKED;

// A pressed at metatile (mx, my): a shelf, the lever, the box, the breaker door, the key. 1 if used.
uint8_t electrical_use_at(uint8_t mx, uint8_t my) BANKED;

// Sneaking works (B held) until the escape; closets don't save you in the escape
#define ELECTRICAL_RUNNING() (electrical_state >= ELEC_POWER)

#endif

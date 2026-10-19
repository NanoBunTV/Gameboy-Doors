// Screech (Minecraft docs 05, owner's answers 2026-10-18): only in dark rooms. A "psst", and he hangs just
// outside your glow in a direction you're not facing. Face him (walk his way) for 0.3 s and he leaves;
// don't, and after 3 s he bites for 40 HP (a full-screen jumpscare). Hiding sends him away silently.
// The chance rises the longer you linger in the dark; 20 s cooldown after each encounter.
#ifndef SCREECH_H
#define SCREECH_H

#include <gb/gb.h>
#include <stdint.h>

#define SCREECH_DAMAGE 40

extern uint8_t screech_state;   // SCREECH_* (scripts/bot.py reads it)
#define SCREECH_IDLE  0
#define SCREECH_HERE  1         // waiting to be looked at
#define SCREECH_GOING 2         // looked at: shuddering away
extern uint8_t screech_dir;     // where he is (DIR_*), while SCREECH_HERE

void screech_need_sprites(void) BANKED;   // after the room is lit: his sheet, if it's dark
void screech_reset(void) BANKED;          // a new run
void screech_room_entered(void) BANKED;   // gone, no cooldown
void screech_ward(void) BANKED;           // the crucifix: gone, and not back for a minute (owner)
uint8_t screech_update(void) BANKED;      // per frame; 1 if he bit
void screech_draw(void) BANKED;

#endif

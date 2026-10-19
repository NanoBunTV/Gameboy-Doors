// Dying: the jumpscare, then the Guiding Light (DESIGN.md): hint text for what killed you, then Try again
// or Title
#ifndef DEATH_H
#define DEATH_H

#include <gb/gb.h>
#include <stdint.h>

#define DEATH_NONE   0
#define DEATH_RUSH   1
#define DEATH_AMBUSH 2
#define DEATH_HIDE   3
#define DEATH_SCREECH 4
#define DEATH_EYES   5
#define DEATH_DUPE   6
#define DEATH_HALT   7
#define DEATH_FIGURE 8
#define DEATH_SEEK   9    // caught in a chase (seek.c)
#define DEATH_FIRE   10   // walked into fire (MTF_FIRE; game.c): his face too
#define DEATH_ESCAPE 11   // Figure caught you running for Room 100's lift (electrical.c): his face

#define DEATH_TRY_AGAIN 0
#define DEATH_TITLE     1

extern uint16_t death_seed;   // after DEATH_TRY_AGAIN: the new run's seed, from the player's timing

uint8_t death_run(uint8_t cause) BANKED;

// The full-screen face, scream and shake (Rush, Ambush, Screech), ending faded to black. Also used when
// Screech bites and you live: the room is redrawn after (game.c). Overwrites BG tiles from 0 and so sprite
// tiles 128 and up (game_init reloads them).
void death_jumpscare(uint8_t cause) BANKED;

#endif

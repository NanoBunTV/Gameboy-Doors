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

#define DEATH_TRY_AGAIN 0
#define DEATH_TITLE     1

extern uint16_t death_seed;   // after DEATH_TRY_AGAIN: the new run's seed, from the player's timing

uint8_t death_run(uint8_t cause) BANKED;

#endif

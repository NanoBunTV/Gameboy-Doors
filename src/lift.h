// The lift before a run (Phase 9b, owner 2026-10-19): a picture of the car going down with the Knobs shop
// under it, over Elevator Jam. Offers are picked with A and nothing is bought until BUY; Start leaves without
// buying, and so does the song ending (owner: it plays once). Then the doors open onto the reception.
#ifndef LIFT_H
#define LIFT_H

#include <gb/gb.h>
#include <stdint.h>

// What was bought this time (LIFT_* in items.h), handed over by game_start
extern uint8_t lift_bought;

void lift_run(void) BANKED;

#endif

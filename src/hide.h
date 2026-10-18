// Hiding in closets and under beds, and Hide (Minecraft docs 04): stay hidden 12 seconds and he throws you
// out for 40 HP and bars that spot for the rest of the run. A heartbeat from 7 seconds that speeds up is
// the only warning; there is no timer on screen.
#ifndef HIDE_H
#define HIDE_H

#include <gb/gb.h>
#include <stdint.h>
#include "assets.h"

extern uint8_t hide_hidden;      // 1 while in a closet / under a bed
extern uint16_t hide_frames;     // how long, this time

// Pressing A at a closet or bed: hides, or says something stops you (Hide banned it)
void hide_try(const room_obj_t *o) BANKED;

// Per frame while hidden: A or B leaves; Hide's timer. Returns DEATH_HIDE if his damage killed.
uint8_t hide_update(void) BANKED;

// Darkness while hidden (0 = none), for platform_set_dim
uint8_t hide_dim(void) BANKED;

// Hiding spots in the current room Hide hasn't banned
uint8_t hide_free_spots(void) BANKED;

void hide_reset(void) BANKED;    // not hidden (new room, new run)

#endif

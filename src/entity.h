// Rush and Ambush (Minecraft docs 04). A spawn rolled for a room (run.c) is queued and fires in the first
// newly entered room with a free hiding spot. Then: a short wait, the lights flicker (Ambush: green, with
// his own cue), and he travels the door-to-door route from well behind the player to the furthest door
// reached, killing anyone within his radius who isn't hidden, walls or not. Ambush turns at that door,
// goes back, waits, and comes again: 3-5 legs. While either is out, no door opens.
//
// His position is a room number and a distance along that room's route (entry door centre to exit door
// centre, scripts/assets.py), so he exists in rooms the player isn't in and is only drawn in this one.
#ifndef ENTITY_H
#define ENTITY_H

#include <gb/gb.h>
#include <stdint.h>

#define ENT_IDLE  0
#define ENT_WAIT  1   // fired; the flicker starts soon
#define ENT_WARN  2   // the lights flicker
#define ENT_MOVE  3   // travelling
#define ENT_PAUSE 4   // Ambush, waiting at the rear before coming again

#define ENTITY_RUSH   1
#define ENTITY_AMBUSH 2

extern uint8_t entity_state, entity_kind, entity_room, entity_queued;
extern int16_t entity_d;            // distance along entity_room's route, px
extern int16_t entity_x, entity_y;  // where he is in this room (valid when entity_room == run_room)

uint8_t entity_load(uint8_t first_tile, uint8_t first_pal) BANKED;  // sprite sheets; returns tiles used
void entity_reset(void) BANKED;                       // a new run
void entity_room_entered(uint8_t first_time) BANKED;  // after a room is loaded
uint8_t entity_update(void) BANKED;                   // per frame; DEATH_RUSH / DEATH_AMBUSH if he caught you
void entity_draw(void) BANKED;
void entity_lights(uint8_t *dim, uint8_t *tint) BANKED;   // the flicker this frame (dim 0 if none)

#define entity_active() (entity_state != ENT_IDLE)

#endif

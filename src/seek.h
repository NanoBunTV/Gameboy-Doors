// Seek (Minecraft docs 08, owner 2026-10-18): the chases at rooms 30-35 and 70-79 (run.h). In the start
// hallway he rises out of a puddle behind the player once they're three quarters of the way along (or past a
// 'seek' point); then the player runs at 1.4 times walking and he follows the door-to-door line Rush takes, at
// 0.75 of their speed, room after room. Only the real door of each room opens, by itself as you reach it; a
// four-way room's real door has its number and a blue glow. He catches anyone within his reach, or anyone who
// has ended up in a room behind him. In the end hallway he stops two metatiles short of its door, and when
// the player is through, it slams.
#ifndef SEEK_H
#define SEEK_H

#include <gb/gb.h>
#include <stdint.h>

#define SEEK_OFF   0
#define SEEK_WAIT  1   // the start hallway: waiting for the player to come far enough along
#define SEEK_RISE  2   // the puddle and his rise; the player can't move
#define SEEK_CHASE 3
#define SEEK_STOP  4   // stopped short of the end hallway's door

#define SEEK_AUTO_OPEN 24   // chasing, the real door opens when the player's feet are this near its middle (game.c)

extern uint8_t seek_state;
extern uint8_t seek_room;           // the room he's in (while chasing)
extern int16_t seek_d;              // ... and his distance along its route, px
extern int16_t seek_x, seek_y;      // his feet, when seek_room == run_room

void seek_reset(void) BANKED;            // a new run
void seek_room_entered(void) BANKED;     // after a room is loaded and the player placed
void seek_need_sprites(void) BANKED;     // load_room_sprites (game.c): his sheets in a chase room
// One frame (not while the player is frozen by his rise: see seek_frozen). Returns DEATH_SEEK if he caught you.
uint8_t seek_update(void) BANKED;
void seek_draw(void) BANKED;

#define seek_frozen() (seek_state == SEEK_RISE)   // the player stands and watches him rise
#define seek_chasing() (seek_state >= SEEK_CHASE)

#endif

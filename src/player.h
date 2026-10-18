// The player: 8-direction walking with collision, facing, walk animation
#ifndef PLAYER_H
#define PLAYER_H

#include <stdint.h>

// Facing, 8 directions, clockwise from down. DESIGN.md: the facing direction is where the player looks.
enum { DIR_DOWN, DIR_DOWN_LEFT, DIR_LEFT, DIR_UP_LEFT, DIR_UP, DIR_UP_RIGHT, DIR_RIGHT, DIR_DOWN_RIGHT };

extern int16_t player_x, player_y;   // top left of the 16x16 sprite, room pixels
extern uint8_t player_facing;        // DIR_*

void player_init(uint16_t x, uint16_t y);
void player_update(void);
void player_draw(void);

#endif

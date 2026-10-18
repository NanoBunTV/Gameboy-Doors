// The player: 8-direction walking with collision, facing, walk animation
#ifndef PLAYER_H
#define PLAYER_H

#include <stdint.h>

// Facing, 8 directions, clockwise from down. DESIGN.md: the facing direction is where the player looks.
enum { DIR_DOWN, DIR_DOWN_LEFT, DIR_LEFT, DIR_UP_LEFT, DIR_UP, DIR_UP_RIGHT, DIR_RIGHT, DIR_DOWN_RIGHT };

extern int16_t player_x, player_y;   // top left of the 16x16 sprite, room pixels
extern uint8_t player_facing;        // DIR_*

// Collision box (the feet), relative to the sprite's top left
#define HB_L 3
#define HB_R 12
#define HB_T 10
#define HB_B 15
// The middle of the feet, which decides which metatile the player stands on
#define PLAYER_FEET_X 8
#define PLAYER_FEET_Y 12

void player_load(void);                                  // sprite tiles, once
void player_place(int16_t x, int16_t y, uint8_t facing); // put the player somewhere, standing
void player_update(void);
void player_draw(void);

#endif

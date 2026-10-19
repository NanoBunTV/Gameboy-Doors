// Seek's eyes on the walls (owner, 2026-10-18): from WALL_EYES_ROOMS rooms before each chase, more each room
// (run_wall_eyes), on any wall metatile (MTF_WALL), blinking now and then. Harmless: they say he's near.
// Sprites, one per map row at most, so a screen line never has more than one.
#ifndef WALLEYES_H
#define WALLEYES_H

#include <gb/gb.h>
#include <stdint.h>

void walleyes_room_entered(void) BANKED;   // after a room is loaded: where this room's eyes are (from the seed)
void walleyes_need_sprites(void) BANKED;   // load_room_sprites (game.c)
void walleyes_draw(void) BANKED;

#endif

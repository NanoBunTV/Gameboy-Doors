// Eyes (Minecraft docs 05, owner's answers 2026-10-18): 5% of rooms from room 8. He hangs halfway along
// the room's door-to-door route and glows (lights the tiles around him in a dark room). Facing him, in the
// exact one of 8 directions he's in, drains 5 HP every 0.25 s after 0.5 s of looking. Hidden, you can't
// see him. He's gone for good the moment the room's exit door opens; he never blocks a door.
#ifndef EYES_H
#define EYES_H

#include <gb/gb.h>
#include <stdint.h>

extern uint8_t eyes_here;          // in the current room (scripts/bot.py reads it)
extern int16_t eyes_x, eyes_y;     // his middle, room pixels

void eyes_need_sprites(void) BANKED;   // after eyes_room_entered: his sheet, if he's in this room
void eyes_room_entered(void) BANKED;   // after the room is loaded and before it's drawn: sets his light
uint8_t eyes_update(void) BANKED;      // per frame; DEATH_EYES if he drained the last HP
void eyes_draw(void) BANKED;
void eyes_dismiss(void) BANKED;        // the exit door opened

#endif

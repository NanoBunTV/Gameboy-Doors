// Timothy (Minecraft docs 12, owner 2026-10-18): one drawer in 200, once a run, holds a spider instead of
// what was in it. He jumps out at you (10 HP, never the last), then scuttles off.
#ifndef TIMOTHY_H
#define TIMOTHY_H

#include <gb/gb.h>
#include <stdint.h>

void timothy_reset(void) BANKED;                                // a new room: he's gone
void timothy_need_sprites(void) BANKED;                         // his sheet, while he's out (spring asks too)
void timothy_spring(uint8_t mx, uint8_t my) BANKED;             // out of the drawer at metatile (mx, my)
void timothy_update(void) BANKED;
void timothy_draw(void) BANKED;

#endif

// Halt (Minecraft docs 06, owner 2026-10-18): a door armed by the planner (RF2_HALT) leads into a long
// sideways hallway, blue-lit on GBC. Halt walks at you; when the screen flashes and TURN AROUND shows, he's
// on the other side and you walk the other way. Forward legs are longer, so you get to the far door.
#ifndef HALT_H
#define HALT_H

#include <gb/gb.h>
#include <stdint.h>

#define HALT_DAMAGE 60

extern int8_t halt_dir;   // which way to walk now: +1 right, -1 left (scripts/bot.py reads it)

// The whole sequence, from the room's open exit door to the hallway's far end. Nothing else runs meanwhile.
// Returns DEATH_HALT if he killed you, else 0, with the screen black and the sprite sheets back as they were.
uint8_t halt_run(void) BANKED;

#endif

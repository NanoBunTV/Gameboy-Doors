// A run through the Hotel: the seed, which template each room uses, locks and keys, and what the player
// has changed in each room (opened doors and drawers, taken keys), so walking back finds rooms as left.
//
// Room n is entered through door n (DOORS' numbering, Minecraft docs 02): room 0 is the reception, its
// exit is door 1. Everything about room n is decided from the seed and n alone (run_plan), so a run is
// reproducible from its seed.
#ifndef RUN_H
#define RUN_H

#include <stdint.h>

#define RUN_LAST_ROOM 100           // Phase 2: room 100's exit doesn't open yet
#define RUN_ROOMS (RUN_LAST_ROOM + 1)

#define LOCK_CHANCE_PERCENT 18      // Minecraft config loot.lock-chance (0.18)
#define KEY_IN_DRAWER_PERCENT 75    // owner: keys mostly in drawers, sometimes in the open
#define NO_REPEAT_WINDOW 3          // a template isn't used again within 3 rooms (Minecraft no-repeat-window)

// run_flags[n]
#define RF_LOCKED       0x01        // the exit is locked; its key is in this room
#define RF_KEY_DRAWER   0x02        // the key is in drawer run_key_slot[n] (else on key spot run_key_slot[n])
#define RF_KEY_TAKEN    0x04
#define RF_EXIT_OPEN    0x08
#define RF_FINAL        0x10        // the last room: its exit stays shut (for now)
#define RF_KEY_FIXED    0x20        // the key is on the template's fixed key spot (the reception)

extern uint16_t run_seed;
extern uint8_t run_room;            // the room the player is in
extern uint8_t run_key_door;        // number of the door the carried key opens; 0 = no key
extern uint8_t run_template[RUN_ROOMS];
extern uint8_t run_flags[RUN_ROOMS];
extern uint8_t run_key_slot[RUN_ROOMS];
extern uint8_t run_drawers[RUN_ROOMS];  // opened drawers, one bit per drawer in object order

void run_start(uint16_t seed);

// Makes sure rooms 0..n are planned (in order: each depends on the one before)
void run_plan(uint8_t n);

#endif

// A run through the Hotel: the seed, which template each room uses, locks and keys, and what the player
// has changed in each room (opened doors and drawers, taken keys), so walking back finds rooms as left.
//
// Room n is entered through door n (DOORS' numbering, Minecraft docs 02): room 0 is the reception, its
// exit is door 1. Everything about room n is decided from the seed and n alone (run_plan), so a run is
// reproducible from its seed.
#ifndef RUN_H
#define RUN_H

#include <gb/gb.h>
#include <stdint.h>

#define RUN_LAST_ROOM 100           // room 100's exit doesn't open yet
#define RUN_ROOMS (RUN_LAST_ROOM + 1)

#define LOCK_CHANCE_PERCENT 18      // Minecraft config loot.lock-chance (0.18)
#define KEY_IN_DRAWER_PERCENT 75    // owner: keys mostly in drawers, sometimes in the open
#define NO_REPEAT_WINDOW 3          // a template isn't used again within 3 rooms (Minecraft no-repeat-window)

// Rush and Ambush (Minecraft config `entities`): entering a room for the first time rolls a spawn, which
// waits for a room with a free hiding spot (entity.c)
#define SPAWN_CHANCE_PERCENT 18     // entities.spawn-chance
#define AMBUSH_SHARE_PERCENT 10     // entities.ambush-share: of those, the share that are Ambush
#define ENTITY_FIRST_ROOM 3         // entities.first-room

// Dark rooms (Minecraft config dark-rooms, owner 2026-10-18): a roll per room from room 15
#define DARK_CHANCE_PERCENT 10
#define DARK_FIRST_ROOM 15
// Eyes (Minecraft config eyes): 5% per room from room 8, independent of everything else
#define EYES_CHANCE_PERCENT 5
#define EYES_FIRST_ROOM 8

// Dupe (Minecraft docs 06, owner 2026-10-18): rooms with more than one exit; the run picks the real one.
// 35% of those from room 6 hide Dupe behind the others.
#define DUPE_CHANCE_PERCENT 35
#define DUPE_FIRST_ROOM 6
#define DUPE_DAMAGE 40
// Halt: 3% per room arms its (plain) exit door, at most once a run
#define HALT_CHANCE_PERCENT 3
#define HALT_FIRST_ROOM 10
// Timothy: one drawer in 200, at most once a run; 10 HP, never the last
#define TIMOTHY_ODDS 200
#define TIMOTHY_DAMAGE 10

// The Library (Minecraft docs 07, owner 2026-10-18): always room 50. Room 49 has one door, on the top wall,
// into it. Nothing from the corridor follows you in: no Rush or Ambush (a queued one waits), Eyes, darkness,
// Dupe or Halt there, nor Halt on door 50.
#define RUN_LIBRARY_ROOM 50

// Seek's chases (Minecraft docs 08, owner 2026-10-18): 30 the start hallway he rises in, 31-34 four-way
// rooms, 35 the end hallway (the fire, and the door that slams); 70, 71-78, 79 the second. The room after each
// is safe. Only when the converter found all three kinds of chase room (ROOM_HAS_SEEK); else ordinary rooms.
// The room before a chase has one door, on top; nothing else (Rush, Eyes, the dark, Dupe, Halt, locks) is
// planned in a chase. Seek's eyes appear on the walls from WALL_EYES_ROOMS rooms before.
#define RUN_CHASES 2
#define RUN_CHASE_START(c) ((c) ? 70 : 30)
#define RUN_CHASE_END(c)   ((c) ? 79 : 35)
#define WALL_EYES_ROOMS 5

// The Courtyard and the Greenhouse (Minecraft docs 09, owner 2026-10-18): room 89 the Courtyard, outdoors
// and safe (nothing is planned there, and anything on its way is sent off: entity.c); 90-99 the Greenhouse,
// every room dark, Rush allowed in the dark (no flicker: his sound starts when the flicker would; an Ambush turns
// into a Rush), Snares, its four-way rooms always Dupe. Room 99 is safe. Room 88 has one door, on top. Only
// when the converter found both kinds (ROOM_HAS_GARDEN); else ordinary rooms. Rain in both (garden.c).
#define RUN_COURTYARD_ROOM 89
#define RUN_GREENHOUSE_LAST 99

#define MAX_HP 100

// run_flags[n]
#define RF_LOCKED       0x01        // the exit is locked; its key is in this room
#define RF_KEY_DRAWER   0x02        // the key is in drawer run_key_slot[n] (else on key spot run_key_slot[n])
#define RF_KEY_TAKEN    0x04
#define RF_EXIT_OPEN    0x08
#define RF_FINAL        0x10        // the last room: its exit stays shut (for now)
#define RF_KEY_FIXED    0x20        // the key is on the template's fixed key spot (the reception)
#define RF_SPAWN        0x40        // entering this room the first time queues Rush (or Ambush)
#define RF_AMBUSH       0x80        // ... Ambush

// run_flags2[n]
#define RF2_DARK        0x01        // the lights are out
#define RF2_EYES        0x02        // Eyes is in this room ...
#define RF2_EYES_GONE   0x04        // ... until a door opens (eyes.c)
#define RF2_DUPE        0x08        // the wrong doors hide Dupe (else they're numberless fakes)
#define RF2_DUPE_HIT    0x10        // first wrong door: Dupe has fired (shut, a garbled number) ...
                                    // ... 0x20 the second
#define RF2_HALT        0x40        // the exit leads into Halt's hallway first
#define RF2_HALT_DONE   0x80        // ... and the player came out of it

extern uint16_t run_seed;
extern uint8_t run_room;            // the room the player is in
extern uint8_t run_max_room;        // the furthest room entered
extern uint8_t run_key_door;        // number of the door the carried key opens; 0 = no key
extern uint8_t run_hp;              // 0 - MAX_HP
extern uint8_t run_template[RUN_ROOMS];
extern uint8_t run_flags[RUN_ROOMS];
extern uint8_t run_flags2[RUN_ROOMS];
extern uint8_t run_key_slot[RUN_ROOMS];
extern uint8_t run_drawers[RUN_ROOMS];  // opened drawers, one bit per drawer in object order
extern uint8_t run_banned[RUN_ROOMS];   // hiding spots Hide threw the player out of, one bit per spot
extern uint8_t run_exit[RUN_ROOMS];     // which of the template's exits is the real one (object order)
extern uint8_t run_timothy;             // Timothy has been met this run
extern uint16_t run_seconds;            // the run's clock, shown on the HUD (owner, 2026-10-18: instead of the door
                                        // number, which gave Dupe away); counted by hud_update from sys_time
extern uint16_t run_clock_last;         // sys_time when it last counted
extern uint8_t run_clock_frames;        // frames toward the next second

void run_start(uint16_t seed) BANKED;

// Makes sure rooms 0..n are planned (in order: each depends on the one before)
void run_plan(uint8_t n) BANKED;

// The number on room n's k-th wrong door (k = 0 or 1) when Dupe is there: one below the real door's or up
// to two above it, never the same twice in a room (Minecraft docs 06, "The wrong numbers")
uint8_t run_fake_number(uint8_t n, uint8_t k) BANKED;

// Is Timothy in room n's drawer d? (Once a run, 1 in TIMOTHY_ODDS, never the key's drawer.) From the seed,
// so a replay meets him in the same drawer.
uint8_t run_timothy_in(uint8_t n, uint8_t d) BANKED;

// The kind of room the plan puts at n (ROOM_KIND_*: the start room, the Library, a chase room, the Courtyard,
// the Greenhouse, else normal)
uint8_t run_kind_at(uint8_t n) BANKED;

// How many of Seek's eyes are on room n's walls: 1-5 in the rooms before a chase, 6 in its start hallway
uint8_t run_wall_eyes(uint8_t n) BANKED;

#endif

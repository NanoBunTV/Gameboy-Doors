// What the player carries (Phase 9a, DESIGN.md "Items and economy"; owner's choices 2026-10-18): gold, the
// lighter, flashlight and vitamins (held with Select, used with B), lockpicks, the skeleton key and the crucifix
// (all three work from the bag), and bandages and batteries, which are used the moment they're taken.
// Loot is rolled by run.c (run_loot); this file takes it, keeps the counts, runs the held light and the
// vitamins, and answers the crucifix.
#ifndef ITEMS_H
#define ITEMS_H

#include <gb/gb.h>
#include <stdint.h>
#include "run.h"

// The held item (Select cycles the ones carried)
#define HELD_NONE       0
#define HELD_LIGHTER    1
#define HELD_FLASHLIGHT 2
#define HELD_VITAMINS   3

#define LIGHTER_FUEL    3600   // frames: 1 minute burning (Minecraft), no refill
#define FLASH_CHARGE    7200   // 2 minutes a charge, a battery fills it
#define VITAMIN_FRAMES  600    // 1.3 times walking for 10 s (owner) ...
#define VITAMIN_HEAL    10     // ... and 10 HP
#define BANDAGE_HEAL    25
#define SKELETON_USES   5      // Minecraft: five turns
#define MAX_LOCKPICKS   3      // carry limits (Minecraft): 3 lockpicks, 3 vitamins, 1 of the rest
#define MAX_VITAMINS    3
#define GOLD_MAX        999
#define WARD_SECONDS    60     // the crucifix: Rush, Ambush, Screech, Eyes can't come back for a minute (owner) ...
#define FREEZE_FRAMES   600    // ... Seek and Figure stand still 10 s

extern uint16_t inv_gold;
extern uint8_t inv_lockpicks, inv_vitamins, inv_skeleton;   // inv_skeleton: uses left (0: none)
extern uint8_t inv_crucifix, inv_lighter, inv_flashlight;  // carried (0/1)
extern uint16_t inv_fuel, inv_charge;                      // the lighter's fuel and the flashlight's charge, frames
extern uint8_t inv_held;     // HELD_*
extern uint8_t inv_lit;      // the held light is on
extern uint16_t inv_vit_t;   // frames of vitamins left (player.c adds their speed)
// Until when (run_seconds) the crucifix keeps each away
extern uint16_t ward_rush, ward_eyes;   // (Screech's is his own cooldown: screech_ward)

void items_reset(void) BANKED;   // a new run: nothing carried

// Each playing frame: Select cycles, B uses the held item, the light's fuel, the vitamins, queued messages.
// ITEMS_SNEAK: the Library and Room 100 until the escape, where B sneaks and nothing is held (owner: items
// aren't needed there; what was held comes back after). ITEMS_HIDDEN: in a closet, the light out, no buttons.
#define ITEMS_FREE   0
#define ITEMS_SNEAK  1
#define ITEMS_HIDDEN 2
void items_frame(uint8_t mode) BANKED;

// The light the held item makes (LIGHT_LIGHTER, LIGHT_FLASH + facing), or LIGHT_GLOW
uint8_t items_light(void) BANKED;

// Takes what a drawer (loose 0) or a loose spot (loose 1) holds, as much as fits, and says what. Returns what's
// left behind: LOOT_LEFT_* bits.
#define LOOT_LEFT_ITEM    0x01
#define LOOT_LEFT_BATTERY 0x02
#define LOOT_LEFT_BANDAGE 0x04
#define LOOT_LEFT_GOLD    0x08
uint8_t items_take(const loot_t *l, uint8_t want) BANKED;   // want: LOOT_LEFT_* bits of l to try

// At a locked door you have no key for: a lockpick, else a use of the skeleton key. 1 if it opened.
uint8_t items_unlock(void) BANKED;

// An entity is hitting you (not a Snare, Timothy or Hide). If a crucifix is carried it's spent, the blow
// doesn't land, and 1 is returned: the caller sends the entity off (Minecraft docs 12, the owner's table).
uint8_t items_crucifix(void) BANKED;

// Start: paused on the list of what's carried, until Start or B
void items_pause(void) BANKED;

// Phase 9b. What the lift shop sold (lift.c), handed over as the run starts (Minecraft docs 12, the pre-run shop)
#define LIFT_LOCKPICKS  0x01   // two lockpicks
#define LIFT_LIGHTER    0x02
#define LIFT_FLASHLIGHT 0x04   // at half charge (Minecraft: "I have a flashlight FOR NOW")
#define LIFT_VITAMINS   0x08   // three
void items_lift(uint8_t bought) BANKED;

// Jeff's Shop: takes one item (LOOT_*) if it fits, quietly. 1 if taken (the caller takes the gold after).
uint8_t items_buy(uint8_t item) BANKED;

#endif

#include <gb/gb.h>
#include <string.h>
#include "run.h"
#include "assets.h"
#include "room_templates.h"

uint16_t run_seed;
uint8_t run_room;
uint8_t run_key_door;
uint8_t run_template[RUN_ROOMS];
uint8_t run_flags[RUN_ROOMS];
uint8_t run_key_slot[RUN_ROOMS];
uint8_t run_drawers[RUN_ROOMS];
static uint8_t planned;   // rooms 0 .. planned - 1 are planned

// xorshift16 (Brad Forschner's 7, 9, 8 triple), seeded per room from the run seed and the room number.
// A generator per room, not one shared stream, so a room's plan doesn't depend on anything but (seed, n)
// (the same reason the Minecraft version seeds per room, docs 02 "Locked doors").
static uint16_t rng;

static void rng_seed(uint16_t seed, uint8_t n) {
    rng = seed ^ ((uint16_t)n * 0x9E37u) ^ 0x5A5Au;
    if (!rng)
        rng = 1;
    for (uint8_t i = 0; i < 4; i++) {  // mix: nearby seeds and rooms shouldn't give similar first draws
        rng ^= rng << 7;
        rng ^= rng >> 9;
        rng ^= rng << 8;
    }
}

static uint16_t rnd(void) {
    rng ^= rng << 7;
    rng ^= rng >> 9;
    rng ^= rng << 8;
    return rng;
}

// 0 .. n-1
static uint8_t rnd_below(uint8_t n) {
    return (uint8_t)(rnd() % n);
}

static uint8_t opposite(uint8_t wall) {
    switch (wall) {
    case WALL_TOP: return WALL_BOTTOM;
    case WALL_LEFT: return WALL_RIGHT;
    case WALL_RIGHT: return WALL_LEFT;
    default: return WALL_TOP;
    }
}

void run_start(uint16_t seed) {
    run_seed = seed;
    run_room = 0;
    run_key_door = 0;
    planned = 0;
    memset(run_flags, 0, sizeof(run_flags));
    memset(run_drawers, 0, sizeof(run_drawers));
    run_plan(1);
}

static void plan_room(uint8_t n) {
    rng_seed(run_seed, n);
    uint8_t t = 0;
    if (n == 0) {
        for (uint8_t i = 0; i < ROOM_TEMPLATE_COUNT; i++)
            if (room_template_info[i][0] == ROOM_KIND_START)
                t = i;
    } else {
        uint8_t need = opposite(room_template_info[run_template[n - 1]][2]);
        uint8_t candidates[ROOM_TEMPLATE_COUNT], count = 0;
        // Twice: the second pass drops the no-repeat rule, in case the pool is too small for it
        for (uint8_t pass = 0; pass < 2 && !count; pass++)
            for (uint8_t i = 0; i < ROOM_TEMPLATE_COUNT; i++) {
                if (room_template_info[i][0] != ROOM_KIND_NORMAL || room_template_info[i][1] != need)
                    continue;
                uint8_t recent = 0;
                for (uint8_t back = 1; pass == 0 && back <= NO_REPEAT_WINDOW && back <= n; back++)
                    if (run_template[n - back] == i)
                        recent = 1;
                if (!recent)
                    candidates[count++] = i;
            }
        t = candidates[rnd_below(count)];  // the converter guarantees every exit has a matching entry
    }
    run_template[n] = t;

    const uint8_t *info = room_template_info[t];
    uint8_t flags = 0, slot = 0;
    if (n == RUN_LAST_ROOM) {
        flags = RF_FINAL;
    } else if (info[0] == ROOM_KIND_START) {
        flags = RF_LOCKED | RF_KEY_FIXED;  // door 1 is always locked; the key hangs behind the desk
    } else if (info[3] && (info[4] || info[5]) && rnd_below(100) < LOCK_CHANCE_PERCENT) {
        flags = RF_LOCKED;
        uint8_t drawer = info[4] && (!info[5] || rnd_below(100) < KEY_IN_DRAWER_PERCENT);
        if (drawer) {
            flags |= RF_KEY_DRAWER;
            slot = rnd_below(info[4]);
        } else {
            slot = rnd_below(info[5]);
        }
    }
    run_flags[n] = flags;
    run_key_slot[n] = slot;
}

void run_plan(uint8_t n) {
    if (n > RUN_LAST_ROOM)
        n = RUN_LAST_ROOM;
    while (planned <= n)
        plan_room(planned++);
}

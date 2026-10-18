#pragma bank 255
#include <gb/gb.h>
#include <string.h>
#include "run.h"
#include "assets.h"
#include "room_templates.h"

uint16_t run_seed;
uint8_t run_room;
uint8_t run_max_room;
uint8_t run_hp;
uint8_t run_key_door;
uint8_t run_template[RUN_ROOMS];
uint8_t run_flags[RUN_ROOMS];
uint8_t run_flags2[RUN_ROOMS];
uint8_t run_key_slot[RUN_ROOMS];
uint8_t run_drawers[RUN_ROOMS];
uint8_t run_banned[RUN_ROOMS];
uint8_t run_exit[RUN_ROOMS];
uint8_t run_timothy;
#if defined(AUTOTEST) || defined(DEBUG)
uint8_t run_force_timothy;   // scripts poke 1: the next drawer opened (not the key's) holds Timothy
#endif
static uint8_t halt_planned;   // a Halt is already armed in an earlier room
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

void run_start(uint16_t seed) BANKED {
    run_seed = seed;
    run_room = 0;
    run_max_room = 0;
    run_hp = MAX_HP;
    run_key_door = 0;
    planned = 0;
    memset(run_flags, 0, sizeof(run_flags));
    memset(run_flags2, 0, sizeof(run_flags2));
    memset(run_drawers, 0, sizeof(run_drawers));
    memset(run_banned, 0, sizeof(run_banned));
    memset(run_exit, 0, sizeof(run_exit));
    run_timothy = 0;
    halt_planned = 0;
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
        uint8_t need = opposite(room_template_exits[run_template[n - 1]][run_exit[n - 1]]);
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
    // Drawn after everything above, so adding this roll didn't change any room's layout or lock
    if (n >= ENTITY_FIRST_ROOM && n < RUN_LAST_ROOM && rnd_below(100) < SPAWN_CHANCE_PERCENT) {
        flags |= RF_SPAWN;
        if (rnd_below(100) < AMBUSH_SHARE_PERCENT)
            flags |= RF_AMBUSH;
    }
    // Phase 4's rolls, again after everything before them (Minecraft docs 05: a seed's layouts and locks
    // stay the same). Not the reception or the last room.
    uint8_t flags2 = 0;
    if (n >= DARK_FIRST_ROOM && n < RUN_LAST_ROOM && rnd_below(100) < DARK_CHANCE_PERCENT)
        flags2 |= RF2_DARK;
    if (n >= EYES_FIRST_ROOM && n < RUN_LAST_ROOM && rnd_below(100) < EYES_CHANCE_PERCENT)
        flags2 |= RF2_EYES;
    // Phase 5's: which exit is real (rooms with more than one), Dupe behind the others, Halt
    uint8_t exits = 0;
    while (exits < ROOM_EXITS_MAX && room_template_exits[t][exits])
        exits++;
    uint8_t real = 0;
    if (exits > 1) {
        real = rnd_below(exits);
        if (n >= DUPE_FIRST_ROOM && n < RUN_LAST_ROOM && rnd_below(100) < DUPE_CHANCE_PERCENT)
            flags2 |= RF2_DUPE;
    } else if (!halt_planned && n >= HALT_FIRST_ROOM && n < RUN_LAST_ROOM && !(flags & RF_LOCKED) &&
               rnd_below(100) < HALT_CHANCE_PERCENT) {
        flags2 |= RF2_HALT;   // only a plain door: not locked, not a Dupe room (owner)
        halt_planned = 1;
    }
    run_exit[n] = real;
    run_flags[n] = flags;
    run_flags2[n] = flags2;
    run_key_slot[n] = slot;
}

void run_plan(uint8_t n) BANKED {
    if (n > RUN_LAST_ROOM)
        n = RUN_LAST_ROOM;
    while (planned <= n)
        plan_room(planned++);
}

// Both draw from their own sequence (the planner reseeds per room, so borrowing its generator is safe)
uint8_t run_fake_number(uint8_t n, uint8_t k) BANKED {
    static const int8_t offsets[3] = { -1, 1, 2 };
    rng_seed(run_seed ^ 0xD0FEu, n);
    uint8_t a = rnd_below(3);
    if (k) {
        uint8_t b = rnd_below(2);   // one of the other two
        a = b >= a ? b + 1 : b;
    }
    return (uint8_t)(n + 1 + offsets[a]);
}

uint8_t run_timothy_in(uint8_t n, uint8_t d) BANKED {
    uint8_t f = run_flags[n];
    if (run_timothy || ((f & (RF_LOCKED | RF_KEY_DRAWER)) == (RF_LOCKED | RF_KEY_DRAWER) && run_key_slot[n] == d))
        return 0;
#if defined(AUTOTEST) || defined(DEBUG)
    if (run_force_timothy) {
        run_force_timothy = 0;
        return 1;
    }
#endif
    rng_seed(run_seed ^ 0x7177u ^ ((uint16_t)d << 12), n);
    return rnd() % TIMOTHY_ODDS == 0;
}

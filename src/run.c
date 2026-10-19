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
// In SRAM bank 1 (run.h, sram.h): run_start() clears or plan_room() sets each before it's read
uint8_t RUN_SRAM(0) run_template[RUN_ROOMS];
uint8_t RUN_SRAM(1) run_flags[RUN_ROOMS];
uint8_t RUN_SRAM(2) run_flags2[RUN_ROOMS];
uint8_t RUN_SRAM(3) run_key_slot[RUN_ROOMS];
uint8_t RUN_SRAM(4) run_drawers[RUN_ROOMS];
uint8_t RUN_SRAM(5) run_banned[RUN_ROOMS];
uint8_t RUN_SRAM(6) run_exit[RUN_ROOMS];
uint8_t run_timothy;
uint16_t run_seconds;
uint16_t run_clock_last;
uint8_t run_clock_frames;
uint16_t run_damage;
#if defined(AUTOTEST) || defined(DEBUG)
uint8_t run_force_timothy;   // scripts poke 1: the next drawer opened (not the key's) holds Timothy
#endif
static uint8_t halt_planned;   // a Halt is already armed in an earlier room
static uint8_t planned;   // rooms 0 .. planned - 1 are planned
// Opened drawers that still hold something (a full bag, full health, a full flashlight)
#define LEFT_MAX 6
static uint8_t left_room[LEFT_MAX], left_drawer[LEFT_MAX], left_what[LEFT_MAX], left_next;


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

uint8_t run_kind_at(uint8_t n) BANKED {
    if (n == 0)
        return ROOM_KIND_START;
    if (n == RUN_LIBRARY_ROOM)
        return ROOM_KIND_LIBRARY;
#if ROOM_HAS_ELECTRICAL
    if (n == RUN_LAST_ROOM)
        return ROOM_KIND_ELECTRICAL;
#endif
#if ROOM_HAS_SHOP
    if (n == RUN_SHOP_ROOM)
        return ROOM_KIND_SHOP;
#endif
#if ROOM_HAS_SEEK
    for (uint8_t c = 0; c < RUN_CHASES; c++) {
        if (n == RUN_CHASE_START(c))
            return ROOM_KIND_SEEK_START;
        if (n > RUN_CHASE_START(c) && n < RUN_CHASE_END(c))
            return ROOM_KIND_SEEK_4WAY;
        if (n == RUN_CHASE_END(c))
            return ROOM_KIND_SEEK_END;
    }
#endif
#if ROOM_HAS_GARDEN
    if (n == RUN_COURTYARD_ROOM)
        return ROOM_KIND_COURTYARD;
    if (n > RUN_COURTYARD_ROOM && n <= RUN_GREENHOUSE_LAST)
        return ROOM_KIND_GREENHOUSE;
#endif
    return ROOM_KIND_NORMAL;
}

uint8_t run_wall_eyes(uint8_t n) BANKED {
#if ROOM_HAS_SEEK
    for (uint8_t c = 0; c < RUN_CHASES; c++) {
        uint8_t s = RUN_CHASE_START(c);
        if (n <= s && n + WALL_EYES_ROOMS >= s)
            return WALL_EYES_ROOMS + 1 - (s - n);
    }
#endif
    (void)n;
    return 0;
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
    run_seconds = 0;
    run_damage = 0;
    run_clock_last = sys_time;
    run_clock_frames = 0;
    halt_planned = 0;
    memset(left_what, 0, sizeof(left_what));
    run_plan(1);
}

static void plan_room(uint8_t n) {
    rng_seed(run_seed, n);
    uint8_t t = 0;
    uint8_t kind = run_kind_at(n);
    // A chase room: nothing else is planned in it (Minecraft docs 08: noEntities, neverDark)
    uint8_t chase = ROOM_KIND_IS_CHASE(kind);
    // The Courtyard and the Greenhouse (Minecraft docs 09); room 99 is safe
    uint8_t garden = kind == ROOM_KIND_COURTYARD || kind == ROOM_KIND_GREENHOUSE;
    uint8_t safe = kind == ROOM_KIND_COURTYARD || (kind == ROOM_KIND_GREENHOUSE && n == RUN_GREENHOUSE_LAST) ||
                   kind == ROOM_KIND_SHOP;
    // The room before the Library, a chase, the Courtyard, Jeff's Shop or Room 100: one door, on the top wall, and
    // no Halt
    uint8_t next = n < RUN_LAST_ROOM ? run_kind_at(n + 1) : ROOM_KIND_NORMAL;
    uint8_t before_fixed = next == ROOM_KIND_LIBRARY || next == ROOM_KIND_SEEK_START || next == ROOM_KIND_COURTYARD ||
                           next == ROOM_KIND_ELECTRICAL || next == ROOM_KIND_SHOP;
    if (kind == ROOM_KIND_START || kind == ROOM_KIND_LIBRARY || kind == ROOM_KIND_ELECTRICAL || kind == ROOM_KIND_SHOP) {
        for (uint8_t i = 0; i < ROOM_TEMPLATE_COUNT; i++)
            if (room_template_info[i][0] == kind)
                t = i;
    } else {
        uint8_t need = opposite(room_template_exits[run_template[n - 1]][run_exit[n - 1]]);
        uint8_t candidates[ROOM_TEMPLATE_COUNT], count = 0;
        // Twice: the second pass drops the no-repeat rule, in case the pool is too small for it. Chase rooms
        // don't have it: a chase is the same rooms over and over (Minecraft docs 08).
        for (uint8_t pass = 0; pass < 2 && !count; pass++)
            for (uint8_t i = 0; i < ROOM_TEMPLATE_COUNT; i++) {
                if (room_template_info[i][0] != kind || room_template_info[i][1] != need)
                    continue;
                if (before_fixed && (room_template_exits[i][0] != WALL_TOP || room_template_exits[i][1]))
                    continue;   // (scripts/assets.py checks one exists for every entry wall)
                uint8_t recent = 0;
                for (uint8_t back = 1; pass == 0 && !chase && back <= NO_REPEAT_WINDOW && back <= n; back++)
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
    } else if (info[0] == ROOM_KIND_LIBRARY) {
        flags = RF_LOCKED;                 // the padlock (library.c); no key
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
    // Drawn after everything above, so adding this roll didn't change any room's layout or lock. The
    // Library's rolls are all skipped (nothing from the corridor follows you in).
    uint8_t lib = n == RUN_LIBRARY_ROOM || chase || safe;   // (the Library's rule, a chase's, the safe rooms')
    if (!lib && n >= ENTITY_FIRST_ROOM && n < RUN_LAST_ROOM && rnd_below(100) < SPAWN_CHANCE_PERCENT) {
        flags |= RF_SPAWN;
        if (rnd_below(100) < AMBUSH_SHARE_PERCENT)
            flags |= RF_AMBUSH;
    }
    // Phase 4's rolls, again after everything before them (Minecraft docs 05: a seed's layouts and locks
    // stay the same). Not the reception or the last room.
    uint8_t flags2 = 0;
    if (!lib && n >= DARK_FIRST_ROOM && n < RUN_LAST_ROOM && rnd_below(100) < DARK_CHANCE_PERCENT)
        flags2 |= RF2_DARK;
    if (kind == ROOM_KIND_GREENHOUSE)
        flags2 = RF2_DARK;   // every Greenhouse room (owner: fully dark), the roll above or not
    else if (kind == ROOM_KIND_COURTYARD)
        flags2 = 0;          // never dark
    if (!lib && n >= EYES_FIRST_ROOM && n < RUN_LAST_ROOM && rnd_below(100) < EYES_CHANCE_PERCENT)
        flags2 |= RF2_EYES;
    // Phase 5's: which exit is real (rooms with more than one), Dupe behind the others, Halt
    uint8_t exits = 0;
    while (exits < ROOM_EXITS_MAX && room_template_exits[t][exits])
        exits++;
    uint8_t real = 0;
    if (exits > 1) {
        real = rnd_below(exits);   // a four-way room: the real door; the others are numberless fakes
        if (!chase && !safe && n >= DUPE_FIRST_ROOM && n < RUN_LAST_ROOM &&
            (kind == ROOM_KIND_GREENHOUSE || rnd_below(100) < DUPE_CHANCE_PERCENT))
            flags2 |= RF2_DUPE;   // a Greenhouse four-way: always (Minecraft alwaysDupe)
    } else if (!halt_planned && !lib && !garden && !before_fixed && n >= HALT_FIRST_ROOM && n < RUN_LAST_ROOM &&
               !(flags & RF_LOCKED) && rnd_below(100) < HALT_CHANCE_PERCENT) {
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

// Which item (Minecraft docs 12: weights rather than percentages; owner: plus a rare skeleton key)
static const uint8_t item_weight[6] = { 35, 28, 18, 12, 4, 3 };   // LOOT_LOCKPICK .. LOOT_SKELETON

void run_loot(uint8_t n, uint8_t slot, uint8_t loose, uint8_t flash, loot_t *l) BANKED {
    rng_seed(run_seed ^ 0x6017u ^ ((uint16_t)slot << 11) ^ (loose ? 0x0480u : 0), n);
    l->gold = l->item = l->battery = l->bandage = 0;
    uint8_t gold = LOOT_GOLD_MIN + rnd_below(LOOT_GOLD_SPAN);   // drawn first, so it doesn't hang on the rest
    if (loose) {
        // One thing at most, the rarer first (Minecraft: a loose pile's rolls are a priority order), never an item
        // (Every roll is drawn whether or not it can count, so owning a flashlight changes no other roll)
        uint8_t battery = rnd_below(100) < LOOT_LOOSE_BATTERY, bandage = rnd_below(100) < LOOT_LOOSE_BANDAGE;
        uint8_t g = rnd_below(100) < LOOT_LOOSE_GOLD;
        if (flash && battery)
            l->battery = 1;
        else if (bandage)
            l->bandage = 1;
        else if (g)
            l->gold = gold;
        return;
    }
    // A drawer: every roll on its own, so one can hold all four
    if (rnd_below(100) < LOOT_DRAWER_GOLD)
        l->gold = gold;
    if (rnd_below(100) < LOOT_DRAWER_ITEM) {
        uint8_t w = rnd_below(100);   // the weights add up to 100
        uint8_t i = 0;
        while (w >= item_weight[i]) {
            w -= item_weight[i];
            i++;
        }
        l->item = i + 1;
    }
    if (rnd_below(100) < LOOT_DRAWER_BATTERY && flash)
        l->battery = 1;
    if (rnd_below(100) < LOOT_DRAWER_BANDAGE)
        l->bandage = 1;
}

uint8_t run_left(uint8_t n, uint8_t drawer) BANKED {
    for (uint8_t i = 0; i < LEFT_MAX; i++)
        if (left_what[i] && left_room[i] == n && left_drawer[i] == drawer)
            return left_what[i];
    return 0;
}

void run_set_left(uint8_t n, uint8_t drawer, uint8_t left) BANKED {
    uint8_t i = 0;
    while (i < LEFT_MAX && !(left_what[i] && left_room[i] == n && left_drawer[i] == drawer))
        i++;
    if (i == LEFT_MAX) {
        if (!left)
            return;
        i = left_next;   // the oldest goes
        left_next = left_next == LEFT_MAX - 1 ? 0 : left_next + 1;
    }
    left_room[i] = n;
    left_drawer[i] = drawer;
    left_what[i] = left;
}

uint8_t run_shop_item(uint8_t k) BANKED {
    // A shuffle of the six from (seed, 55): the first four are on the counter
    uint8_t items[6] = { LOOT_LOCKPICK, LOOT_VITAMINS, LOOT_LIGHTER, LOOT_FLASHLIGHT, LOOT_CRUCIFIX, LOOT_SKELETON };
    rng_seed(run_seed ^ 0x5409u, RUN_SHOP_ROOM);
    for (uint8_t i = 5; i; i--) {
        uint8_t j = rnd_below(i + 1), t = items[i];
        items[i] = items[j];
        items[j] = t;
    }
    return items[k];
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

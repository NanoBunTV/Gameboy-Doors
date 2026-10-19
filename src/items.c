#pragma bank 255
#include <gb/gb.h>
#include <string.h>
#include "items.h"
#include "run.h"
#include "hud.h"
#include "hud_state.h"
#include "input.h"
#include "audio.h"
#include "light.h"
#include "player.h"
#include "bg.h"

// Phase 9a (DESIGN.md "Items and economy", PROJECT.md for each choice)

uint16_t inv_gold;
uint8_t inv_lockpicks, inv_vitamins, inv_skeleton;
uint8_t inv_crucifix, inv_lighter, inv_flashlight;
uint16_t inv_fuel, inv_charge;
uint8_t inv_held, inv_lit;
uint16_t inv_vit_t;
uint16_t ward_rush, ward_eyes;

static uint8_t stashed;   // what was held before the Library or Room 100 put it away

// Messages wait their turn: a drawer can hold four things, and the key's message may be up already
#define SAY_MAX 4
#define SAY_FRAMES 90
enum { SAY_GOLD = 1, SAY_ITEM, SAY_FULL = SAY_ITEM + 6, SAY_BATTERY, SAY_BANDAGE, SAY_NOT_HURT, SAY_FLASH_FULL,
       SAY_LIGHTER_OUT, SAY_FLASH_DEAD, SAY_EMPTY };
static uint8_t say_q[SAY_MAX], say_n[SAY_MAX], say_len;
static char msg[21];

static const char *const item_names[6] = { "A LOCKPICK", "VITAMINS", "A LIGHTER", "A FLASHLIGHT", "A CRUCIFIX",
                                           "A SKELETON KEY" };

static void say(uint8_t what, uint8_t n) {
    if (say_len == SAY_MAX)
        return;
    say_q[say_len] = what;
    say_n[say_len] = n;
    say_len++;
}

static char *put(char *p, const char *s) {
    while (*s)
        *p++ = *s++;
    return p;
}

static char *put_num(char *p, uint16_t n) {
    if (n >= 100)
        *p++ = '0' + n / 100;
    if (n >= 10)
        *p++ = '0' + (n / 10) % 10;
    *p++ = '0' + n % 10;
    return p;
}

static void say_next(void) {
    uint8_t what = say_q[0], n = say_n[0];
    say_len--;
    for (uint8_t i = 0; i < say_len; i++) {
        say_q[i] = say_q[i + 1];
        say_n[i] = say_n[i + 1];
    }
    char *p = msg;
    if (what == SAY_GOLD) {
        p = put(p, "GOT ");
        p = put_num(p, n);
        p = put(p, " GOLD.");
    } else if (what >= SAY_ITEM && what < SAY_FULL) {
        p = put(p, "GOT ");
        p = put(p, item_names[what - SAY_ITEM]);
        *p++ = '.';
    } else {
        static const char *const lines[] = { "I CAN'T CARRY MORE.", "FLASHLIGHT CHARGED.", "USED A BANDAGE.",
                                             "I'M NOT HURT.", "FLASHLIGHT'S FULL.", "THE LIGHTER'S OUT.",
                                             "THE BATTERY'S DEAD.", "IT'S EMPTY." };
        p = put(p, lines[what - SAY_FULL]);
    }
    *p = 0;
    hud_message(msg, SAY_FRAMES);
}

void items_reset(void) BANKED {
    inv_gold = 0;
    inv_lockpicks = inv_vitamins = inv_skeleton = 0;
    inv_crucifix = inv_lighter = inv_flashlight = 0;
    inv_fuel = inv_charge = 0;
    inv_held = inv_lit = stashed = 0;
    inv_vit_t = 0;
    ward_rush = ward_eyes = 0;
    say_len = 0;
}

static uint8_t carried(uint8_t held) {
    switch (held) {
    case HELD_LIGHTER:    return inv_lighter;
    case HELD_FLASHLIGHT: return inv_flashlight;
    case HELD_VITAMINS:   return inv_vitamins;
    }
    return 1;   // HELD_NONE
}

static void light_out(void) {
    if (inv_lit) {
        inv_lit = 0;
        sfx_play(SFX_LIGHT);
    }
}

static void use_held(void) {
    switch (inv_held) {
    case HELD_LIGHTER:
    case HELD_FLASHLIGHT:
        if (inv_lit) {
            light_out();
        } else if (inv_held == HELD_LIGHTER ? inv_fuel : inv_charge) {
            inv_lit = 1;
            sfx_play(SFX_LIGHT);
        } else {
            say(SAY_EMPTY, 0);
        }
        break;
    case HELD_VITAMINS:
        // A second dose while one runs does nothing at all (Minecraft docs 12: it stacks on the game's speed,
        // not on itself)
        if (inv_vit_t)
            return;
        inv_vitamins--;
        inv_vit_t = VITAMIN_FRAMES;
        run_hp = run_hp + VITAMIN_HEAL > MAX_HP ? MAX_HP : run_hp + VITAMIN_HEAL;
        sfx_play(SFX_GULP);
        if (!inv_vitamins)
            inv_held = HELD_NONE;
        break;
    }
    hud_show(150);
}

void items_frame(uint8_t mode) BANKED {
    if (inv_vit_t)
        inv_vit_t--;
    if (say_len && !hud_msg_timer)
        say_next();
    if (mode == ITEMS_HIDDEN) {
        light_out();
        return;
    }
    // The Library and Room 100 until the escape: B sneaks, nothing is held (owner: items aren't needed there)
    if (mode == ITEMS_SNEAK) {
        if (inv_held) {
            light_out();
            stashed = inv_held;
            inv_held = HELD_NONE;
        }
        return;
    }
    if (stashed) {
        if (carried(stashed))
            inv_held = stashed;
        stashed = 0;
    }
    if (input_pressed & J_SELECT) {
        // The next of the three you carry, then nothing; a light goes out when it stops being held (Minecraft)
        uint8_t h = inv_held;
        do {
            h = h == HELD_VITAMINS ? HELD_NONE : h + 1;
        } while (!carried(h));
        if (h != inv_held) {
            light_out();
            inv_held = h;
            sfx_play(SFX_SELECT);
        }
    }
    if ((input_pressed & J_B) && inv_held)
        use_held();
    if (inv_lit) {
        if (inv_held == HELD_LIGHTER) {
            if (!--inv_fuel) {
                // Burnt out, and thrown away: another can be found (with a carry limit of one, an empty lighter
                // would keep a new one in its drawer)
                inv_lit = inv_lighter = 0;
                inv_held = HELD_NONE;
                say(SAY_LIGHTER_OUT, 0);
                hud_show(150);
            }
        } else if (!--inv_charge) {
            inv_lit = 0;   // the flashlight stays: a battery fills it
            say(SAY_FLASH_DEAD, 0);
            hud_show(150);
        }
    }
}

uint8_t items_light(void) BANKED {
    if (!inv_lit)
        return LIGHT_GLOW;
    return inv_held == HELD_LIGHTER ? LIGHT_LIGHTER : LIGHT_FLASH + player_facing;
}

// Takes one item if it fits. Returns 1 if taken.
static uint8_t take_item(uint8_t item) {
    switch (item) {
    case LOOT_LOCKPICK:
        if (inv_lockpicks >= MAX_LOCKPICKS)
            return 0;
        inv_lockpicks++;
        break;
    case LOOT_VITAMINS:
        if (inv_vitamins >= MAX_VITAMINS)
            return 0;
        inv_vitamins++;
        if (!inv_held)
            inv_held = HELD_VITAMINS;
        break;
    case LOOT_LIGHTER:
        if (inv_lighter)
            return 0;
        inv_lighter = 1;
        inv_fuel = LIGHTER_FUEL;
        if (!inv_held)
            inv_held = HELD_LIGHTER;
        break;
    case LOOT_FLASHLIGHT:
        if (inv_flashlight)
            return 0;
        inv_flashlight = 1;
        inv_charge = FLASH_CHARGE;
        if (!inv_held)
            inv_held = HELD_FLASHLIGHT;
        break;
    case LOOT_CRUCIFIX:
        if (inv_crucifix)
            return 0;
        inv_crucifix = 1;
        break;
    default:   // LOOT_SKELETON
        if (inv_skeleton)
            return 0;
        inv_skeleton = SKELETON_USES;
        break;
    }
    return 1;
}

uint8_t items_take(const loot_t *l, uint8_t want) BANKED {
    uint8_t left = 0;
    if ((want & LOOT_LEFT_GOLD) && l->gold) {
        if (inv_gold + l->gold > GOLD_MAX) {
            left |= LOOT_LEFT_GOLD;
            say(SAY_FULL, 0);
        } else {
            inv_gold += l->gold;
            say(SAY_GOLD, l->gold);
            sfx_play(SFX_GOLD);
        }
    }
    if ((want & LOOT_LEFT_ITEM) && l->item) {
        if (take_item(l->item)) {
            say(SAY_ITEM + l->item - 1, 0);
            sfx_play(SFX_KEY);
        } else {
            left |= LOOT_LEFT_ITEM;
            say(SAY_FULL, 0);
        }
    }
    if ((want & LOOT_LEFT_BATTERY) && l->battery) {
        if (!inv_flashlight || inv_charge == FLASH_CHARGE) {
            left |= LOOT_LEFT_BATTERY;   // nothing to put it in yet, or nothing to fill
            say(SAY_FLASH_FULL, 0);
        } else {
            inv_charge = FLASH_CHARGE;
            say(SAY_BATTERY, 0);
            sfx_play(SFX_GOLD);
        }
    }
    if ((want & LOOT_LEFT_BANDAGE) && l->bandage) {
        // At full health it stays where it is (Minecraft docs 12: taking it would throw it away)
        if (run_hp >= MAX_HP) {
            left |= LOOT_LEFT_BANDAGE;
            say(SAY_NOT_HURT, 0);
        } else {
            run_hp = run_hp + BANDAGE_HEAL > MAX_HP ? MAX_HP : run_hp + BANDAGE_HEAL;
            say(SAY_BANDAGE, 0);
            sfx_play(SFX_GULP);
        }
    }
    hud_show(150);
    return left;
}

void items_lift(uint8_t bought) BANKED {
    if (bought & LIFT_LOCKPICKS) {
        take_item(LOOT_LOCKPICK);
        take_item(LOOT_LOCKPICK);
    }
    if (bought & LIFT_LIGHTER)
        take_item(LOOT_LIGHTER);
    if ((bought & LIFT_FLASHLIGHT) && take_item(LOOT_FLASHLIGHT))
        inv_charge = FLASH_CHARGE / 2;
    if (bought & LIFT_VITAMINS)
        for (uint8_t i = 0; i < 3; i++)
            take_item(LOOT_VITAMINS);
}

uint8_t items_buy(uint8_t item) BANKED {
    if (!take_item(item))
        return 0;
    sfx_play(SFX_GOLD);
    hud_show(150);
    return 1;
}

uint8_t items_unlock(void) BANKED {
    // The cheaper first: a lockpick, then the skeleton key's uses (owner)
    if (inv_lockpicks) {
        inv_lockpicks--;
        hud_message("PICKED THE LOCK.", SAY_FRAMES);
        return 1;
    }
    if (inv_skeleton) {
        inv_skeleton--;
        char *p = put(msg, "SKELETON KEY: ");
        p = put_num(p, inv_skeleton);
        p = put(p, " LEFT");
        *p = 0;
        hud_message(msg, SAY_FRAMES);
        return 1;
    }
    return 0;
}

uint8_t items_crucifix(void) BANKED {
    if (!inv_crucifix)
        return 0;
    inv_crucifix = 0;
    sfx_play(SFX_CRUCIFIX);
    hud_message("THE CRUCIFIX BURNS.", SAY_FRAMES);
    return 1;
}

// ------------------------------------------------------------------------------------------ the pause panel

// "%LIGHTER    ####" etc.: an icon, the name, and the meter or the count at column 14
static uint8_t panel_y;

static void meter(char *p, uint16_t v, uint16_t full) {
    static const char bar[5] = { '*', '+', '{', '}', '#' };
    uint8_t px = (uint8_t)(((uint32_t)v * 32 + full - 1) / full);   // 0-32, a sliver showing until it's empty
    for (uint8_t i = 0; i < 4; i++) {
        uint8_t f = px > 8 ? 8 : px;
        px -= f;
        p[i] = bar[f >> 1];
    }
}

static void line(char icon, const char *name, uint16_t count, uint16_t v, uint16_t full) {
    char row[21];
    memset(row, ' ', 20);
    row[20] = 0;
    row[1] = icon;
    char *p;
    put(row + 3, name);
    if (full) {
        meter(row + 14, v, full);
    } else if (count != 0xFFFF) {
        p = row + 14;
        if (count < 1000) {
            if (icon != ICON_GOLD)
                *p++ = 'X';
            put_num(p, count);
        }
    }
    text_print_win(0, panel_y++, row);
}

void items_pause(void) BANKED {
    char row[21];
    uint8_t rows = 2;
    rows += (inv_lighter != 0) + (inv_flashlight != 0) + (inv_vitamins != 0) + (inv_lockpicks != 0) +
            (inv_skeleton != 0) + (inv_crucifix != 0) + 1 + 2;   // the gold, a gap and the time
    panel_y = 2;
    memset(row, ' ', 20);
    row[20] = 0;
    memcpy(row + 7, "PAUSED", 6);
    text_print_win(0, panel_y++, row);
    memset(row, ' ', 20);
    text_print_win(0, panel_y++, row);
    if (inv_lighter)
        line(ICON_LIGHTER, "LIGHTER", 0, inv_fuel, LIGHTER_FUEL);
    if (inv_flashlight)
        line(ICON_FLASHLIGHT, "FLASHLIGHT", 0, inv_charge, FLASH_CHARGE);
    if (inv_vitamins)
        line(ICON_VITAMINS, "VITAMINS", inv_vitamins, 0, 0);
    if (inv_lockpicks)
        line(ICON_LOCKPICK, "LOCKPICKS", inv_lockpicks, 0, 0);
    if (inv_skeleton)
        line(ICON_SKELETON, "SKELETON", inv_skeleton, 0, 0);
    if (inv_crucifix)
        line(ICON_CRUCIFIX, "CRUCIFIX", 0xFFFF, 0, 0);
    line(ICON_GOLD, "GOLD", inv_gold, 0, 0);
    text_print_win(0, panel_y++, row);
    uint16_t t = run_seconds < 5999 ? run_seconds : 5999;
    memcpy(row + 1, "TIME ", 5);
    row[6] = '0' + (t / 60) / 10;
    row[7] = '0' + (t / 60) % 10;
    row[8] = ':';
    row[9] = '0' + (t % 60) / 10;
    row[10] = '0' + (t % 60) % 10;
    text_print_win(0, panel_y++, row);

    hud_panel_set(rows);
    HIDE_SPRITES;   // they'd draw over the panel
    sfx_play(SFX_SELECT);
    // Paused: the clock stops (it counts sys_time, so it's moved past the pause after), the music plays on
    do {
        vsync();
        bg_flush();
        bg_idle();
        audio_frame();
        input_update();
    } while (!(input_pressed & (J_START | J_B)));
    hud_panel_set(0);
    SHOW_SPRITES;
    run_clock_last = sys_time;
}

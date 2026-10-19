#pragma bank 255
#include <gb/gb.h>
#include <string.h>
#include "hud.h"
#include "hud_state.h"
#include "bg.h"
#include "room.h"
#include "run.h"
#include "input.h"
#include "platform.h"
#include "electrical.h"
#include "items.h"

// The banked part of the HUD (moved out of bank 0, 2026-10-18): timers, the slide, the status line
#define SLIDE_SPEED 2

void hud_show(uint8_t frames) BANKED {
    if (hud_timer < frames)
        hud_timer = frames;
}

void hud_hide_now(void) BANKED {
    hud_timer = hud_msg_timer = 0;
    hud_panel = 0;
    bg_wy = HUD_Y_HIDDEN;
}

void hud_panel_set(uint8_t rows) BANKED {
    hud_panel = rows;
    if (!rows)
        hud_rebuild = 1;   // the panel was over the item row
    // At once both ways: the status line stays up a moment after a panel closes
    bg_wy = rows ? HUD_PANEL_ROW_Y(rows) : HUD_Y_SHOWN;
    hud_show(60);
}

static void two_digits(char *buf, uint8_t n) {
    buf[0] = '0' + n / 10;
    buf[1] = '0' + n % 10;
}

// " 12:34    @######## ~": the run's time (minutes and seconds, stopping at 99:59), health (a heart and an
// 8-tile bar) and the key if you carry one. No door number: it told you the real door's number in a Dupe room
// (owner, 2026-10-18). The bar's glyphs ('*' empty, '+' '{' '}' 2/4/6 px, '#' full) are in the font.
static void status_line(void) {
    static const char bar_glyph[5] = { '*', '+', '{', '}', '#' };
    char *want = hud_want;
    memset(want, ' ', 20);
    uint16_t t = run_seconds < 5999 ? run_seconds : 5999;
    two_digits(want + 1, (uint8_t)(t / 60));
    want[3] = ':';
    two_digits(want + 4, (uint8_t)(t % 60));
    want[10] = '@';
    uint8_t px = (uint8_t)(((uint16_t)run_hp * 64 + 50) / 100);   // 0-64 px
    for (uint8_t i = 0; i < 8; i++) {
        uint8_t fill = px > 8 ? 8 : px;
        px -= fill;
        want[11 + i] = bar_glyph[fill >> 1];
    }
    if (run_key_door)
        want[19] = '~';
    // Room 100: the fuses you carry ("F 3"), as the Minecraft version's events say how many you've found
    if (run_room == RUN_LAST_ROOM && electrical_carried) {
        want[7] = 'F';
        if (electrical_carried >= 10) {
            want[8] = '1';
            want[9] = '0';
        } else {
            want[9] = '0' + electrical_carried;
        }
    }
}

// The item row (Phase 9a, layout A of the owner's mock-up): the held item's icon and its meter (the lights: 4
// tiles, 2 px steps) or count (vitamins), and the gold at the right: " %  ####        ^123"
static uint8_t meter_px(uint16_t v, uint16_t full) {
    return (uint8_t)(((uint32_t)v * 32 + full - 1) / full);   // 0-32, a sliver until it's empty
}

static void item_line(uint8_t px) {
    static const char bar_glyph[5] = { '*', '+', '{', '}', '#' };
    static const char icon[4] = { ' ', ICON_LIGHTER, ICON_FLASHLIGHT, ICON_VITAMINS };
    char *want = hud_want2;
    memset(want, ' ', 20);
    want[1] = icon[inv_held];
    if (inv_held == HELD_VITAMINS) {
        want[3] = 'X';
        want[4] = '0' + inv_vitamins;
    } else if (inv_held) {
        for (uint8_t i = 0; i < 4; i++) {
            uint8_t fill = px > 8 ? 8 : px;
            px -= fill;
            want[3 + i] = bar_glyph[fill >> 1];
        }
    }
    want[16] = ICON_GOLD;
    uint16_t g = inv_gold;
    want[19] = '0' + g % 10;
    if (g >= 10)
        want[18] = '0' + (g / 10) % 10;
    if (g >= 100)
        want[17] = '0' + g / 100;
}

void hud_update(void) BANKED {
    static uint8_t last_hp;
    // What the status line was last built from: it's only rebuilt when one changes. Building it every
    // frame (three divisions and a 20-character compare) took ~45 of the DMG's 154 scanlines (Phase 4).
    static uint8_t built_hp = 0xFF, built_key, built_msg, built_fuses;
    static uint16_t built_seconds;
    uint8_t dirty = 0;
    // The run's clock, from the VBlank count: real time, so DMG's slow frames and the fades count too
    uint16_t now = sys_time;
    uint16_t passed = now - run_clock_last;
    run_clock_last = now;
    passed += run_clock_frames;
    while (passed >= 60) {
        passed -= 60;
        run_seconds++;
    }
    run_clock_frames = (uint8_t)passed;
    if (input_held & J_SELECT)
        hud_show(30);
    if (run_hp != last_hp) {   // health changed: show it (DESIGN.md, HUD)
        if (run_hp < last_hp)
            run_damage += last_hp - run_hp;   // for the results (nothing heals yet; a new run starts higher)
        last_hp = run_hp;
        hud_show(150);
    }
    if (hud_msg_timer) {
        hud_msg_timer--;
        if (hud_msg_new) {
            hud_msg_new = 0;
            built_msg = 1;
            dirty = 1;
        }
    } else if (built_msg || run_seconds != built_seconds || run_hp != built_hp || run_key_door != built_key ||
               electrical_carried != built_fuses) {
        built_msg = 0;
        built_seconds = run_seconds;
        built_hp = run_hp;
        built_key = run_key_door;
        built_fuses = electrical_carried;
        status_line();
        dirty = 1;
    }
    if (hud_timer)
        hud_timer--;
    // The item row, rebuilt when what it shows changes (never under a panel, which prints over it)
    static uint8_t built_held = 0xFF, built_px, built_lit, built_vit;
    static uint16_t built_gold;
    uint8_t px = inv_held == HELD_LIGHTER ? meter_px(inv_fuel, LIGHTER_FUEL)
               : inv_held == HELD_FLASHLIGHT ? meter_px(inv_charge, FLASH_CHARGE) : 0;
    uint8_t dirty2 = 0;
    if (hud_rebuild) {
        hud_rebuild = 0;
        built_held = 0xFF;
        memset(hud_shown2, 0, 20);   // matches nothing: every tile is written again
        memset(hud_shown, 0, 20);
        dirty = 1;
        if (!hud_msg_timer)
            status_line();
    }
    if (!hud_panel && (inv_held != built_held || px != built_px || inv_lit != built_lit ||
                       inv_vitamins != built_vit || inv_gold != built_gold)) {
        built_held = inv_held;
        built_px = px;
        built_lit = inv_lit;
        built_vit = inv_vitamins;
        built_gold = inv_gold;
        item_line(px);
        dirty2 = 1;
    }

    // Queue the characters that changed (the text row is window row 1)
    if (dirty)
        for (uint8_t i = 0; i < 20; i++)
            if (hud_shown[i] != hud_want[i]) {
                hud_shown[i] = hud_want[i];
                bg_queue_tile(WIN_MAP + 32 + i, hud_tile_for(hud_want[i]), PAL_TEXT);
            }
    if (dirty2)
        for (uint8_t i = 0; i < 20; i++)
            if (hud_shown2[i] != hud_want2[i]) {
                hud_shown2[i] = hud_want2[i];
                bg_queue_tile(WIN_MAP + 64 + i, hud_tile_for(hud_want2[i]), PAL_TEXT);
            }

    uint8_t target = hud_panel ? HUD_PANEL_ROW_Y(hud_panel) : hud_timer ? HUD_Y_SHOWN : HUD_Y_HIDDEN;
    if (bg_wy < target)
        bg_wy = (target - bg_wy > SLIDE_SPEED) ? bg_wy + SLIDE_SPEED : target;
    else if (bg_wy > target)
        bg_wy = (bg_wy - target > SLIDE_SPEED) ? bg_wy - SLIDE_SPEED : target;
}

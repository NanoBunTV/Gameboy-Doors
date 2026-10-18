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

// The banked part of the HUD (moved out of bank 0, 2026-10-18): timers, the slide, the status line
#define SLIDE_SPEED 2

void hud_show(uint8_t frames) BANKED {
    if (hud_timer < frames)
        hud_timer = frames;
}

void hud_hide_now(void) BANKED {
    hud_timer = hud_msg_timer = 0;
    bg_wy = HUD_Y_HIDDEN;
}

void text_3digits(char *buf, uint8_t n) BANKED {
    buf[0] = '0' + n / 100;
    buf[1] = '0' + (n / 10) % 10;
    buf[2] = '0' + n % 10;
}

// " DOOR 012 @######## ~": the room you're in, health (a heart and an 8-tile bar) and the key if you
// carry one. The bar's glyphs ('*' empty, '+' '{' '}' 2/4/6 px, '#' full) are in the font (font.json).
static void status_line(void) {
    static const char bar_glyph[5] = { '*', '+', '{', '}', '#' };
    char *want = hud_want;
    memset(want, ' ', 20);
    memcpy(want + 1, "DOOR", 4);
    text_3digits(want + 6, run_room);
    want[10] = '@';
    uint8_t px = (uint8_t)(((uint16_t)run_hp * 64 + 50) / 100);   // 0-64 px
    for (uint8_t i = 0; i < 8; i++) {
        uint8_t fill = px > 8 ? 8 : px;
        px -= fill;
        want[11 + i] = bar_glyph[fill >> 1];
    }
    if (run_key_door)
        want[19] = '~';
}

void hud_update(void) BANKED {
    static uint8_t last_hp;
    // What the status line was last built from: it's only rebuilt when one changes. Building it every
    // frame (three divisions and a 20-character compare) took ~45 of the DMG's 154 scanlines (Phase 4).
    static uint8_t built_room = 0xFF, built_hp, built_key, built_msg;
    uint8_t dirty = 0;
    if (input_held & J_SELECT)
        hud_show(30);
    if (run_hp != last_hp) {   // health changed: show it (DESIGN.md, HUD)
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
    } else if (built_msg || run_room != built_room || run_hp != built_hp || run_key_door != built_key) {
        built_msg = 0;
        built_room = run_room;
        built_hp = run_hp;
        built_key = run_key_door;
        status_line();
        dirty = 1;
    }
    if (hud_timer)
        hud_timer--;

    // Queue the characters that changed (the text row is window row 1)
    if (dirty)
        for (uint8_t i = 0; i < 20; i++)
            if (hud_shown[i] != hud_want[i]) {
                hud_shown[i] = hud_want[i];
                bg_queue_tile(WIN_MAP + 32 + i, hud_tile_for(hud_want[i]), PAL_TEXT);
            }

    uint8_t target = hud_timer ? HUD_Y_SHOWN : HUD_Y_HIDDEN;
    if (bg_wy < target)
        bg_wy = (target - bg_wy > SLIDE_SPEED) ? bg_wy + SLIDE_SPEED : target;
    else if (bg_wy > target)
        bg_wy = (bg_wy - target > SLIDE_SPEED) ? bg_wy - SLIDE_SPEED : target;
}

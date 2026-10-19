#pragma bank 255
#include <gb/gb.h>
#include "walleyes.h"
#include "run.h"
#include "room.h"
#include "bg.h"
#include "sprites.h"
#include "sprite_walleye.h"

#define EYES_MAX  6       // run_wall_eyes' most (the start hallway)
#define TRIES     40      // random starting cells to look from

uint8_t walleyes_count;   // not static: scripts/seek-test.py counts them
#define count walleyes_count
static uint8_t ex[EYES_MAX], ey[EYES_MAX];   // metatile
static uint8_t blink[EYES_MAX];              // frames to the next blink; the blink is its last 8
static uint16_t rng;
static spr_sheet_t sheet;

static uint16_t rnd(void) {
    rng ^= rng << 7;
    rng ^= rng >> 9;
    rng ^= rng << 8;
    return rng;
}

static uint8_t row_taken(uint8_t y) {
    for (uint8_t i = 0; i < count; i++)
        if (ey[i] == y)
            return 1;
    return 0;
}

void walleyes_room_entered(void) BANKED {
    count = 0;
    uint8_t want = run_wall_eyes(run_room);
    if (!want)
        return;
    rng = run_seed ^ 0xE7E5u ^ ((uint16_t)run_room << 8);
    if (!rng)
        rng = 1;
    uint16_t cells = (uint16_t)room_w * room_h;
    // From a random cell, the next wall cell (in map order, wrapping) in a row with no eye yet
    for (uint8_t tries = 0; tries < TRIES && count < want; tries++) {
        uint16_t c = rnd() % cells;
        for (uint16_t n = 0; n < cells; n++, c = c + 1 < cells ? c + 1 : 0) {
            uint8_t y = (uint8_t)(c / room_w);
            if (!(mt_flags[room_map[c]] & MTF_WALL) || row_taken(y))
                continue;
            ex[count] = (uint8_t)(c - room_row[y]);
            ey[count] = y;
            blink[count] = 60 + (uint8_t)(rnd() & 127);
            count++;
            break;
        }
    }
}

void walleyes_need_sprites(void) BANKED {
    if (count)
        spr_need(&sheet, BANK(sprite_walleye), &sprite_walleye);
}

void walleyes_draw(void) BANKED {
    for (uint8_t i = 0; i < count; i++) {
        uint8_t b = --blink[i];
        if (!b)
            b = blink[i] = 90 + (uint8_t)(rnd() & 127);
        // The blink: half, shut, half over the last 8 frames before the count runs out
        uint8_t f = b > 8 ? SPR_WALLEYE_OPEN : (b > 6 || b <= 2) ? SPR_WALLEYE_HALF : SPR_WALLEYE_SHUT;
        spr_draw(&sheet, f, ((int16_t)ex[i] << 4) - cam_x, ((int16_t)ey[i] << 4) - cam_y, 0);
    }
}

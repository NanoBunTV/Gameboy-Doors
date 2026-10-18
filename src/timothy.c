#pragma bank 255
#include <gb/gb.h>
#include "timothy.h"
#include "run.h"
#include "bg.h"
#include "hud.h"
#include "audio.h"
#include "player.h"
#include "room.h"
#include "sprites.h"
#include "sprite_timothy.h"

#define JUMP_FRAMES 20    // from the drawer to you
#define HOP_DIV     6     // the arc: t * (JUMP_FRAMES - t) / HOP_DIV px up, ~16 px at the top
#define FLEE_FRAMES 50    // then away, 2 px a frame, and gone
#define STATE_NONE  0
#define STATE_JUMP  1
#define STATE_FLEE  2

static spr_sheet_t sheet;
static uint8_t frames_buf[SPR_TIMOTHY_FRAME_COUNT * 4];
static uint8_t state, t;
static int16_t x0, y0, tx, ty, x, y;   // the spider's top left (an 8x8 sprite), room pixels
static int8_t fdx, fdy;

uint8_t timothy_load(uint8_t first_tile, uint8_t pal) BANKED {
    return spr_sheet_load(&sheet, BANK(sprite_timothy), &sprite_timothy, first_tile, pal, frames_buf);
}

void timothy_reset(void) BANKED {
    state = STATE_NONE;
}

void timothy_spring(uint8_t mx, uint8_t my) BANKED {
    x = x0 = ((int16_t)mx << 4) + 4;
    y = y0 = ((int16_t)my << 4) + 4;
    tx = player_x + 4;     // the player's middle
    ty = player_y + 4;
    state = STATE_JUMP;
    t = 0;
    sfx_play(SFX_TIMOTHY);
    hud_message("SOMETHING JUMPS OUT!", 90);
}

void timothy_update(void) BANKED {
    if (!state)
        return;
    t++;
    if (state == STATE_JUMP) {
        x = x0 + (int16_t)((tx - x0) * (int16_t)t / JUMP_FRAMES);
        y = y0 + (int16_t)((ty - y0) * (int16_t)t / JUMP_FRAMES) - (int16_t)(t * (JUMP_FRAMES - t) / HOP_DIV);
        if (t < JUMP_FRAMES)
            return;
        // He lands on you: 10 HP, but never the last (Minecraft: hurtSparing)
        run_hp = run_hp > TIMOTHY_DAMAGE ? run_hp - TIMOTHY_DAMAGE : 1;
        sfx_play(SFX_HURT);
        // Off along the floor toward the room's wider side (drawers stand against walls: back the way he
        // came would be into one)
        fdx = (uint16_t)(player_x + 8) < (room_px_w >> 1) ? 2 : -2;
        fdy = 0;
        state = STATE_FLEE;
        t = 0;
        return;
    }
    x += fdx;
    y += fdy;
    if (t >= FLEE_FRAMES)
        state = STATE_NONE;
}

void timothy_draw(void) BANKED {
    if (state)
        spr_draw(&sheet, SPR_TIMOTHY_F0, x - cam_x, y - cam_y + ((t >> 2) & 1), 0);   // legs scrabbling
}

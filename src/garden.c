#pragma bank 255
#include <gb/gb.h>
#include "garden.h"
#include "run.h"
#include "room.h"
#include "bg.h"
#include "hud.h"
#include "audio.h"
#include "player.h"
#include "sprites.h"
#include "tileset_garden.h"
#include "sprite_rain.h"

uint8_t snare_x[SNARES_PER_ROOM], snare_y[SNARES_PER_ROOM];
uint16_t snare_root;
uint8_t snare_grace;

static uint8_t courtyard;
static uint8_t drop_x[RAIN_DROPS], drop_y[RAIN_DROPS];   // screen pixels
static uint16_t rng;
static spr_sheet_t rain_sheet;

static uint16_t rnd(void) {
    rng ^= rng << 7;
    rng ^= rng >> 9;
    rng ^= rng << 8;
    return rng;
}

void garden_reset(void) BANKED {
    snare_root = 0;
    snare_grace = 0;
}

// The n-th candidate cell (map order within each 'snare' rectangle, rectangles in object order), or the count
// of them when n is 0xFFFF. Only floor cells count: the Snare is drawn on the Greenhouse floor.
static uint16_t candidate(uint16_t n, uint8_t *cx, uint8_t *cy) {
    uint16_t k = 0;
    for (uint8_t i = 0; i < room_obj_count; i++) {
        const room_obj_t *o = &room_objs[i];
        if (o->type != OBJ_SNARE)
            continue;
        for (uint8_t y = o->y; y < o->y + o->h; y++)
            for (uint8_t x = o->x; x < o->x + o->w; x++) {
                if (room_map[room_row[y] + x] != MT_GARDEN_FLOOR)
                    continue;
                if (k == n) {
                    *cx = x;
                    *cy = y;
                    return k;
                }
                k++;
            }
    }
    return k;
}

static void place_snares(void) {
    uint8_t x, y;
    uint16_t cells = candidate(0xFFFF, &x, &y);
    if (cells < SNARES_PER_ROOM)
        return;
    // From the seed and the room, so walking back finds them where they were (Minecraft: per session)
    rng = run_seed ^ 0x54A3u ^ ((uint16_t)run_room << 8);
    if (!rng)
        rng = 1;
    for (uint8_t s = 0; s < SNARES_PER_ROOM; s++) {
        uint16_t c = rnd() % cells;
        for (;;) {   // the next free candidate (the converter checked there are enough)
            candidate(c, &x, &y);
            if (room_map[room_row[y] + x] == MT_GARDEN_FLOOR)
                break;
            c = c + 1 < cells ? c + 1 : 0;
        }
        room_map[room_row[y] + x] = MT_GARDEN_SNARE;   // before the room is drawn: no redraw needed
        snare_x[s] = x;
        snare_y[s] = y;
    }
}

void garden_room_loaded(void) BANKED {
    for (uint8_t s = 0; s < SNARES_PER_ROOM; s++)
        snare_x[s] = snare_y[s] = 0xFF;
    if (run_kind_at(run_room) == ROOM_KIND_GREENHOUSE && run_room != RUN_GREENHOUSE_LAST)   // room 99 is safe
        place_snares();
}

void garden_room_entered(void) BANKED {
    uint8_t kind = run_kind_at(run_room);
    courtyard = kind == ROOM_KIND_COURTYARD;
    // Rain on the Courtyard and on the Greenhouse's glass (owner)
    audio_rain(courtyard || kind == ROOM_KIND_GREENHOUSE);
    snare_root = 0;
    snare_grace = 0;
    if (courtyard) {
        rng = run_seed ^ 0x2A1Du;
        if (!rng)
            rng = 1;
        for (uint8_t i = 0; i < RAIN_DROPS; i++) {
            drop_x[i] = (uint8_t)(rnd() % 168);
            drop_y[i] = (uint8_t)(rnd() % 152);
        }
    }
}

void garden_need_sprites(void) BANKED {
    if (courtyard)
        spr_need(&rain_sheet, BANK(sprite_rain), &sprite_rain);
}

uint8_t garden_rooted(void) BANKED {
    return snare_root != 0;
}

void garden_update(void) BANKED {
    if (snare_root) {
        if (!--snare_root)
            snare_grace = SNARE_GRACE_FRAMES;   // counted from now, not from the catch
        return;
    }
    if (snare_grace) {
        snare_grace--;
        return;
    }
    uint8_t mx = (uint8_t)((player_x + PLAYER_FEET_X) >> 4), my = (uint8_t)((player_y + PLAYER_FEET_Y) >> 4);
    if (!(mt_flags[room_map[room_row[my] + mx]] & MTF_SNARE))
        return;
    // It goes off with the damage, never before (Minecraft docs 09): no warning, and it can't kill
    snare_root = SNARE_ROOT_FRAMES;
    if (run_hp > SNARE_MIN_HP)
        run_hp = run_hp > SNARE_MIN_HP + SNARE_DAMAGE ? run_hp - SNARE_DAMAGE : SNARE_MIN_HP;
    sfx_play(SFX_SNARE);
    hud_message("WATCH YOUR STEP.", 180);
}

void garden_draw(void) BANKED {
    if (!courtyard)
        return;
    for (uint8_t i = 0; i < RAIN_DROPS; i++) {
        drop_y[i] += 5;
        drop_x[i] -= 1;
        if (drop_y[i] >= 152) {   // back to the top, somewhere new
            drop_y[i] = (uint8_t)(rnd() & 15);
            drop_x[i] = (uint8_t)(rnd() % 176);
        }
        spr_draw(&rain_sheet, SPR_RAIN_STREAK, (int16_t)drop_x[i] - 8, (int16_t)drop_y[i] - 8, 0);
    }
}

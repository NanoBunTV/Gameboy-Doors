#pragma bank 255
#include <gb/gb.h>
#include <stdlib.h>
#include "eyes.h"
#include "run.h"
#include "light.h"
#include "hide.h"
#include "death.h"
#include "audio.h"
#include "player.h"
#include "sprites.h"
#include "bg.h"
#include "room_templates.h"
#include "sprite_eyes.h"

// Owner, 2026-10-18: 5 HP per 0.25 s after 0.5 s of looking (about 5 s for a full bar)
#define GRACE_FRAMES 30
#define HIT_FRAMES   15
#define DAMAGE       5
#define CUE_EVERY    4     // the hit sound on every 4th hit: about once a second

uint8_t eyes_here;
int16_t eyes_x, eyes_y;
static uint8_t looking, hits, anim;

static spr_sheet_t sheet;

void eyes_need_sprites(void) BANKED {
    if (eyes_here)
        spr_need(&sheet, BANK(sprite_eyes), &sprite_eyes);
}

void eyes_room_entered(void) BANKED {
    uint8_t f = run_flags2[run_room];
    eyes_here = (f & RF2_EYES) && !(f & RF2_EYES_GONE);
    looking = 0;
    if (!eyes_here) {
        light_set(LIGHT_EYES, LIGHT_OFF, 0, 0);
        return;
    }
    // Halfway between the two doors: the line the player walks, which is open floor at both ends (Minecraft
    // docs 05). Top-down he floats, so furniture under the midpoint doesn't matter.
    uint8_t r[4];
    room_template_route(run_template[run_room], run_exit[run_room], r);
    eyes_x = ((int16_t)r[0] + r[2]) << 2;
    eyes_y = ((int16_t)r[1] + r[3]) << 2;
    light_set(LIGHT_EYES, LIGHT_LIGHTER, eyes_x >> 3, eyes_y >> 3);
    sfx_play(SFX_EYES);
}

void eyes_dismiss(void) BANKED {
    if (!eyes_here)
        return;
    eyes_here = 0;
    run_flags2[run_room] |= RF2_EYES_GONE;
    light_set(LIGHT_EYES, LIGHT_OFF, 0, 0);
}

// Which of the 8 directions (DIR_*) a vector points in: each covers 45 degrees (tan 22.5 = 0.41 ~ 2/5)
static uint8_t octant(int16_t dx, int16_t dy) {
    int16_t ax = abs(dx), ay = abs(dy);
    if (ay * 5 < ax * 2)
        return dx > 0 ? DIR_RIGHT : DIR_LEFT;
    if (ax * 5 < ay * 2)
        return dy > 0 ? DIR_DOWN : DIR_UP;
    if (dx > 0)
        return dy > 0 ? DIR_DOWN_RIGHT : DIR_UP_RIGHT;
    return dy > 0 ? DIR_DOWN_LEFT : DIR_UP_LEFT;
}

uint8_t eyes_update(void) BANKED {
    anim++;
    if (!eyes_here)
        return DEATH_NONE;
    int16_t dx = eyes_x - (player_x + 8), dy = eyes_y - (player_y + 8);
    // Looking at him: facing exactly his direction (owner: the narrow cone), or standing right under him
    uint8_t at = (abs(dx) < 8 && abs(dy) < 8) || octant(dx, dy) == player_facing;
    if (hide_hidden || !at) {
        looking = 0;
        return DEATH_NONE;
    }
    if (++looking < GRACE_FRAMES)
        return DEATH_NONE;
    looking = GRACE_FRAMES - HIT_FRAMES;   // the next hit in HIT_FRAMES, for as long as you keep looking
    if (run_hp <= DAMAGE) {
        run_hp = 0;
        return DEATH_EYES;
    }
    run_hp -= DAMAGE;
    if (!(++hits % CUE_EVERY))
        sfx_play(SFX_EYES_HIT);
    return DEATH_NONE;
}

void eyes_draw(void) BANKED {
    if (!eyes_here)
        return;
    uint8_t frame = (anim & 16) ? SPR_EYES_F1 : SPR_EYES_F0;
    spr_draw_big(&sheet, frame, 2, 2, eyes_x - 16 - cam_x, eyes_y - 16 - cam_y, 0);
}

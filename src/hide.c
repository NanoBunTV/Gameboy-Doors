#pragma bank 255
#include <gb/gb.h>
#include "hide.h"
#include "run.h"
#include "room.h"
#include "hud.h"
#include "input.h"
#include "audio.h"
#include "death.h"

// Minecraft config `hide`
#define HIDE_LIMIT      720    // limit-seconds 12
#define HEARTBEAT_FROM  420    // heartbeat-from-seconds 7
#define HIDE_DAMAGE     40     // damage-fraction 0.4 of 100 HP
#define BEAT_SLOW       60     // frames between beats at 7 s ...
#define BEAT_FAST       15     // ... and at 12 s
#define PULSE_FRAMES    6      // the screen darkens a step on each beat (the warning without sound)

uint8_t hide_hidden;
uint16_t hide_frames;
static uint8_t spot;           // index of the spot among the room's closets and beds
static uint8_t next_beat, pulse;

static uint8_t spot_index(const room_obj_t *o) {
    uint8_t n = 0;
    for (const room_obj_t *p = room_objs; p != o; p++)
        if (p->type == OBJ_CLOSET || p->type == OBJ_BED)
            n++;
    return n;
}

uint8_t hide_free_spots(void) BANKED {
    uint8_t n = 0, banned = run_banned[run_room];
    for (uint8_t i = 0; i < room_obj_count; i++)
        if (room_objs[i].type == OBJ_CLOSET || room_objs[i].type == OBJ_BED) {
            if (!(banned & 1))
                n++;
            banned >>= 1;
        }
    return n;
}

void hide_reset(void) BANKED {
    hide_hidden = 0;
    pulse = 0;
}

void hide_try(const room_obj_t *o) BANKED {
    uint8_t i = spot_index(o);
    if (run_banned[run_room] & (1 << i)) {
        // From the player's point of view, not the game's (Minecraft docs 04)
        hud_message("SOMETHING STOPS ME.", 120);
        sfx_play(SFX_LOCKED);
        return;
    }
    spot = i;
    hide_hidden = 1;
    hide_frames = 0;
    next_beat = 0;
    pulse = 0;
    sfx_play(SFX_HIDE);
}

uint8_t hide_update(void) BANKED {
    if (pulse)
        pulse--;
    if (input_pressed & (J_A | J_B)) {
        hide_hidden = 0;
        sfx_play(SFX_HIDE);
        return DEATH_NONE;
    }
    hide_frames++;
    if (hide_frames >= HEARTBEAT_FROM && next_beat)
        next_beat--;
    if (hide_frames >= HEARTBEAT_FROM && !next_beat) {
        sfx_play(SFX_HEARTBEAT);
        pulse = PULSE_FRAMES;
        next_beat = BEAT_SLOW - (uint8_t)(((uint16_t)(hide_frames - HEARTBEAT_FROM) * (BEAT_SLOW - BEAT_FAST)) /
                                          (HIDE_LIMIT - HEARTBEAT_FROM));
    }
    if (hide_frames < HIDE_LIMIT)
        return DEATH_NONE;

    // Hide: out, hurt, and never this spot again (the Hotel's rule: other spots, and spots you left
    // yourself, are still fine)
    hide_hidden = 0;
    pulse = 0;
    run_banned[run_room] |= 1 << spot;
    sfx_play(SFX_GET_OUT);
    if (run_hp <= HIDE_DAMAGE) {
        run_hp = 0;
        return DEATH_HIDE;
    }
    run_hp -= HIDE_DAMAGE;
    hud_message("GET OUT!", 120);
    return DEATH_NONE;
}

uint8_t hide_dim(void) BANKED {
    if (!hide_hidden)
        return 0;
    return pulse ? 3 : 2;
}

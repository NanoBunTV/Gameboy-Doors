#pragma bank 255
#include <gb/gb.h>
#include <string.h>
#include "ending.h"
#include "game.h"
#include "run.h"
#include "room.h"
#include "bg.h"
#include "hud.h"
#include "input.h"
#include "audio.h"
#include "image.h"
#include "figure.h"
#include "player.h"
#include "sprites.h"
#include "platform.h"
#include "font.h"
#include "tileset_electrical.h"

#define CLOSE_FRAMES  40      // the doors sliding shut
#define FIGURE_FRAMES 50      // he comes on at them
#define SLAM_FRAMES   30      // he hits them: the car shakes
#define FALL_FRAMES   120     // the car drops, shaking harder, fading out
#define BLACK_FRAMES  90      // the crash, in the dark

static const int8_t shake[8] = { 3, -2, 1, -3, 2, -1, 3, -2 };

static void frame(int8_t amount) {
    spr_begin();
    figure_draw();
    spr_end();
    figure_update();   // he keeps coming (contact means nothing now)
    int8_t sx = 0, sy = 0;
    static uint8_t t;
    t++;
    if (amount) {
        sx = (int8_t)(shake[t & 7] * amount / 3);
        sy = (int8_t)(shake[(t + 3) & 7] * amount / 3);
    }
    bg_set_scroll(cam_x + sx, cam_y + sy);
    game_wait_frames(1);
}

static void lift_shut(void) {
    for (uint8_t i = 0; i < room_obj_count; i++) {
        const room_obj_t *o = &room_objs[i];
        if (o->type != OBJ_LIFT)
            continue;
        room_set_metatile(o->x, o->y, MT_ELECTRICAL_LIFT_SHUT_TL);
        room_set_metatile(o->x + 1, o->y, MT_ELECTRICAL_LIFT_SHUT_TR);
        room_set_metatile(o->x, o->y + 1, MT_ELECTRICAL_LIFT_SHUT_BL);
        room_set_metatile(o->x + 1, o->y + 1, MT_ELECTRICAL_LIFT_SHUT_BR);
    }
}

static void print(uint8_t x, uint8_t y, const char *s) {
    text_queue(0x9800u + ((uint16_t)y << 5) + x, s);
}

static void four_hex(char *p, uint16_t v) {
    static const char hex[] = "0123456789ABCDEF";
    p[0] = hex[(v >> 12) & 15];
    p[1] = hex[(v >> 8) & 15];
    p[2] = hex[(v >> 4) & 15];
    p[3] = hex[v & 15];
}

static void results(void) {
    char line[21];
    screen_fill(FONT_FIRST_TILE, PAL_TEXT);   // the font's space
    bg_set_scroll(0, 0);
    music_play(MUSIC_ENDING);
    game_fade(FADE_BLACK, 0);
    print(4, 3, "YOU ESCAPED");
    print(4, 4, "THE HOTEL.");
    // "TIME        12:34" (stopping at 99:59, as the HUD)
    uint16_t t = run_seconds < 5999 ? run_seconds : 5999;
    memset(line, ' ', 20);
    line[20] = 0;
    memcpy(line, "TIME", 4);
    line[12] = '0' + (uint8_t)(t / 600);
    line[13] = '0' + (uint8_t)((t / 60) % 10);
    line[14] = ':';
    line[15] = '0' + (uint8_t)((t % 60) / 10);
    line[16] = '0' + (uint8_t)(t % 10);
    line[17] = 0;
    print(2, 7, line);
    memset(line, ' ', 20);
    memcpy(line, "DAMAGE TAKEN", 12);
    uint16_t d = run_damage > 9999 ? 9999 : run_damage;
    for (int8_t i = 16; i >= 13; i--) {
        line[i] = '0' + (uint8_t)(d % 10);
        d /= 10;
        if (!d)
            break;
    }
    line[17] = 0;
    print(2, 9, line);
    memset(line, ' ', 20);
    memcpy(line, "SEED", 4);
    four_hex(line + 13, run_seed);
    line[17] = 0;
    print(2, 11, line);
    print(4, 15, "PRESS START");
    uint8_t wait = 60;   // not straight past it with a held button
    while (1) {
        game_wait_frames(1);
        input_update();
        if (wait) {
            wait--;
            continue;
        }
        if (input_pressed & (J_START | J_A))
            break;
    }
    sfx_play(SFX_SELECT);
    game_fade(0, FADE_BLACK);
    music_play(MUSIC_NONE);
}

void ending_run(void) BANKED {
    game_state = GAME_TRANSITION;
    hud_hide_now();
    // In the car: the doors shut on the room, the player inside (not drawn)
    for (uint8_t i = 0; i < CLOSE_FRAMES; i++)
        frame(0);
    lift_shut();
    sfx_play(SFX_DOOR);
    for (uint8_t i = 0; i < FIGURE_FRAMES; i++)
        frame(0);
    sfx_play(SFX_SLAM);   // he hits the doors (DOORS: he slams them as it goes)
    for (uint8_t i = 0; i < SLAM_FRAMES; i++)
        frame(3);
    figure_stop();
    sfx_rumble(10, RUMBLE_RUSH);   // the car dropping
    for (uint8_t i = 0; i < FALL_FRAMES; i++) {
        if (i == FALL_FRAMES - 30)
            game_fade(0, FADE_BLACK);
        frame(i < 60 ? 2 : 4);
    }
    platform_set_fade(FADE_BLACK);
    sfx_rumble(0, RUMBLE_RUSH);
    sfx_play(SFX_CRASH);
    spr_begin();
    spr_end();
    game_wait_frames(BLACK_FRAMES);
    results();
    player_fast = 0;
}

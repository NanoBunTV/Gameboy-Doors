#pragma bank 255
#include <gb/gb.h>
#include <stdlib.h>
#include "halt.h"
#include "game.h"
#include "run.h"
#include "room.h"
#include "bg.h"
#include "hud.h"
#include "input.h"
#include "audio.h"
#include "death.h"
#include "light.h"
#include "player.h"
#include "sprites.h"
#include "platform.h"
#include "room_templates.h"
#include "tileset_hotel.h"
#include "sprite_halt.h"
#include "sprite_letters.h"
#include "sprite_player.h"

// The spacing sum (Minecraft docs 06, "The spacing sum"): each leg starts him far enough away that
// reacting GRACE frames late is exactly survivable, d = speed * grace + (leg - grace) * (speed - walk).
// Scaled to the screen: the Minecraft distances (11-13 blocks) would start him three screens away, and
// even 1.5 px/frame with a 1 s allowance kept him just off the screen's edge for a player who turns at once.
#define WALK        0x0140   // player.c WALK_SPEED, 1.25 px/frame
#define SPEED       0x0168   // 1.4 px/frame (Minecraft: 1.3 times walking; slower here, see GRACE)
#define GRACE       40       // how late you may turn (2/3 s): with SPEED, legs start him 73-82 px away,
                             // at the screen's edge (it shows 80 px either side of you)
#define LEG_BACK    150      // frames walking back toward the entry
#define LEG_FORWARD 203      // 35% longer (halt.forward-extra): the difference is all your progress
#define LEG_START(leg) ((int16_t)(((uint32_t)SPEED * GRACE + (uint32_t)((leg) - GRACE) * (SPEED - WALK)) >> 8))
#define GLYPH_FRAMES 60      // TURN AROUND on screen
#define FLASH_FRAMES 4
#define TOUCH_X     10       // contact: his middle within this of yours, across and ...
#define TOUCH_Y     14       // ... up and down

// The hallway's sprite VRAM (halt_run): no other entity runs here, so only these
SPR_CHECK(SPR_PLAYER_TILES + SPR_HALT_TILES + SPR_LETTERS_TILES <= SPR_TILE_LIMIT(TILESET_HOTEL_TILES),
          halt_sprites_outgrow_sprite_vram);
SPR_CHECK(SPR_PLAYER_PALETTES + SPR_HALT_PALETTES + SPR_LETTERS_PALETTES <= SPR_PALETTE_LIMIT,
          halt_sprites_need_over_8_gbc_palettes);
SPR_CHECK((SPR_HALT_FRAME_COUNT + SPR_LETTERS_FRAME_COUNT) * 4 <= SPR_FRAME_POOL, halt_frame_tables_outgrow_the_pool);

static spr_sheet_t halt_sheet, letters_sheet;
static int16_t hx, hy;            // his middle, room pixels
static uint8_t hsub;              // fraction of a pixel across
int8_t halt_dir;                  // +1: the player must walk right (he's behind, on the left); -1: left
static uint8_t leg, glyph, flash, anim;

static int16_t player_mid_x(void) { return player_x + 8; }
static int16_t player_mid_y(void) { return player_y + 8; }

// At the start of a leg: the leg's starting distance away, on the side the player is walking away from
static void place_for_leg(void) {
    int16_t d = LEG_START(halt_dir > 0 ? LEG_FORWARD : LEG_BACK);
    hx = player_mid_x() - halt_dir * d;
    hy = player_mid_y();
    hsub = 0;
    leg = halt_dir > 0 ? LEG_FORWARD : LEG_BACK;
}

static void draw(void) {
    spr_begin();
    if (glyph) {   // two words, at most 6 sprites on a line (8x16 letters)
        spr_draw(&letters_sheet, SPR_LETTERS_TU, 64, 12, 0);
        spr_draw(&letters_sheet, SPR_LETTERS_RN, 80, 12, 0);
        spr_draw(&letters_sheet, SPR_LETTERS_AR, 56, 30, 0);
        spr_draw(&letters_sheet, SPR_LETTERS_OU, 72, 30, 0);
        spr_draw(&letters_sheet, SPR_LETTERS_ND, 88, 30, 0);
    }
    player_draw();
    // 24x32 in a 32x32 frame (its right column is empty); the hem ripples, the whole of him jitters
    spr_draw_big(&halt_sheet, (anim & 8) ? SPR_HALT_F1 : SPR_HALT_F0, 2, 2,
                 hx - 12 - cam_x + ((anim >> 1) & 1), hy - 16 - cam_y, 0);
    spr_end();
}


#define ESCAPED 0xFF

// One frame of the chase. Returns DEATH_HALT, ESCAPED at the far door, else 0.
static uint8_t step(void) {
    input_update();
    player_update();
    anim++;
    // Across at his speed, up and down at half a pixel, toward the player
    int16_t px = player_mid_x(), py = player_mid_y();
    uint16_t s = (uint16_t)hsub + (SPEED & 0xFF);
    hsub = (uint8_t)s;
    int16_t move = (SPEED >> 8) + (s >> 8);
    if (hx < px)
        hx = hx + move > px ? px : hx + move;
    else if (hx > px)
        hx = hx - move < px ? px : hx - move;
    if ((anim & 1) && hy != py)
        hy += hy < py ? 1 : -1;

    if (glyph)
        glyph--;
    if (flash && !--flash)
        platform_set_flash(0);
    if (!--leg) {
        // Turn around: he's on the other side now
        halt_dir = -halt_dir;
        place_for_leg();
        glyph = GLYPH_FRAMES;
        flash = FLASH_FRAMES;
        platform_set_flash(1);
        sfx_play(SFX_HALT);
    }
    if (abs(hx - px) < TOUCH_X && abs(hy - py) < TOUCH_Y) {
        if (run_hp <= HALT_DAMAGE) {
            run_hp = 0;
            return DEATH_HALT;
        }
        run_hp -= HALT_DAMAGE;
        sfx_play(SFX_HURT);
        flash = FLASH_FRAMES;
        platform_set_flash(1);
        place_for_leg();   // back to the leg's start, not left standing on you (Minecraft docs 06)
    }
    // The far door: an open doorway in the right wall
    uint8_t mx = (uint8_t)((player_x + PLAYER_FEET_X) >> 4);
    if (mx >= room_w - 1)
        return ESCAPED;
    hud_update();
    game_follow_camera();
    draw();
    return 0;
}

uint8_t halt_run(void) BANKED {
    game_state = GAME_TRANSITION;
    game_fade(platform_fade, FADE_BLACK);
    hud_hide_now();
    spr_begin();
    spr_end();
    sfx_rumble(0, RUMBLE_RUSH);

    // The hallway: the door behind you shut (and numberless), the far one open
    uint8_t bank;
    const room_def_t *def = room_template(ROOM_T_HALT, &bank);
    room_load(bank, def);
    for (uint8_t i = 0; i < room_obj_count; i++) {
        const room_obj_t *o = &room_objs[i];
        if (o->type == OBJ_ENTRY) {
            room_set_metatile(o->x, o->y, room_make_plate(3, MT_HOTEL_SIDE_DOOR_TOP, PLATE_BLANK));
            room_set_metatile(o->x, o->y + 1, MT_HOTEL_SIDE_DOOR_BOTTOM);
        } else if (o->type == OBJ_EXIT) {
            // As every open side door (game.c door_set): the open doorway, here with a blank plate
            room_set_metatile(o->x, o->y, room_make_plate(0, MT_HOTEL_DOOR_OPEN_TOP, PLATE_BLANK));
            room_set_metatile(o->x, o->y + 1, MT_HOTEL_DOOR_OPEN_BOTTOM);
            player_place(16 + 2, ((int16_t)o->y << 4) + 16 - 10, DIR_RIGHT);   // as through a left-wall door
        }
    }
    light_room(0);
    spr_room_begin();   // the hallway is a room of its own (the next door asks for that room's sheets)
    spr_need(&halt_sheet, BANK(sprite_halt), &sprite_halt);
    spr_need(&letters_sheet, BANK(sprite_letters), &sprite_letters);
    platform_set_dim(0, TINT_BLUE);   // owner: blue-lit, as in DOORS (grey on DMG)

    halt_dir = 1;   // he comes from behind first
    place_for_leg();
    glyph = flash = anim = 0;
    game_draw_room();
    draw();
    game_wait_frames(1);
    game_fade(FADE_BLACK, 0);
    game_state = GAME_HALT;

    uint8_t r;
    do {   // as main()'s loop: the VBlank work first, the audio last
        vsync();
        bg_flush();
        r = step();
        audio_frame();
    } while (!r);
    platform_set_flash(0);
    if (r == DEATH_HALT)
        return DEATH_HALT;
    // Out: back to the corridor (the caller enters the next room, which loads its own sprite sheets)
    game_state = GAME_TRANSITION;
    game_fade(0, FADE_BLACK);
    platform_set_dim(0, TINT_NONE);
    return 0;
}

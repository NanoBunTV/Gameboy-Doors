#pragma bank 255
#include <gb/gb.h>
#include "game.h"
#include "run.h"
#include "room.h"
#include "bg.h"
#include "hud.h"
#include "input.h"
#include "player.h"
#include "sprites.h"
#include "platform.h"
#include "audio.h"
#include "entity.h"
#include "hide.h"
#include "death.h"
#include "room_templates.h"
#include "tileset_hotel.h"
#include "sprite_key.h"

#define SCREEN_W 160
#define SCREEN_H 144

// How the player arrives in a room
#define VIA_START 0    // the run starts here (spawn object)
#define VIA_ENTRY 1    // through the room's entry door, coming from the room before
#define VIA_EXIT  2    // back through the room's exit door, from the room after

#define FADE_STEP_FRAMES 3
#define HUD_ENTER_FRAMES 120
#define HUD_MESSAGE_FRAMES 120

uint8_t game_state;
uint8_t game_death;

static spr_sheet_t key_sheet;
static uint8_t key_frames[SPR_KEY_FRAME_COUNT * 4];
static const room_obj_t *entry_door, *exit_door;
static const room_obj_t *key_spot;   // where the key lies in the open, or 0

#ifdef AUTOTEST
// Worst case of how far into VBlank bg_flush() got, in scanlines (VBlank is lines 144-153, i.e. 0-9 here;
// 10 or more means it ran into the next frame). Read by scripts/vblank-budget.py.
uint8_t autotest_flush_lines;
static void measure_flush(void) {
    uint8_t ly = LY_REG;
    uint8_t lines = ly >= 144 ? ly - 144 : ly + 10;
    if (lines > autotest_flush_lines)
        autotest_flush_lines = lines;
}
#else
#define measure_flush()
#endif

void game_wait_frames(uint8_t n) BANKED {
    while (n--) {
        vsync();
        bg_flush();
        bg_idle();
        audio_frame();
    }
}

void game_init(void) BANKED {
    player_load();
    // Player: sprite tiles 0-35, palette 0. Key: tiles from 36, palette 1. Rush and Ambush after it,
    // palettes 2 and 3.
    uint8_t t = 36 + spr_sheet_load(&key_sheet, BANK(sprite_key), &sprite_key, 36, 1, key_frames);
    entity_load(t, 2);
}

// ------------------------------------------------------------------------------------------ camera

// Camera centred on the player, stopping at the room's edges (owner, 2026-10-17); a room smaller than the
// screen is centred
static int16_t camera_axis(int16_t centre, uint16_t room_size, uint8_t screen) {
    if (room_size <= screen)
        return -(int16_t)((screen - room_size) >> 1);
    int16_t c = centre - (screen >> 1);
    if (c < 0)
        return 0;
    if (c > (int16_t)(room_size - screen))
        return room_size - screen;
    return c;
}

static int16_t camera_target_x(void) { return camera_axis(player_x + 8, room_px_w, SCREEN_W); }
static int16_t camera_target_y(void) { return camera_axis(player_y + 8, room_px_h, SCREEN_H); }

// Clamps a camera move to 4 px per axis per frame, the most bg_set_camera can stream (bg.h)
static int16_t step_toward(int16_t from, int16_t to) {
    int16_t d = to - from;
    if (d > 4) d = 4;
    if (d < -4) d = -4;
    return from + d;
}

// ------------------------------------------------------------------------------------------ objects

// The n-th object of a type (for key spots, only the non-fixed ones), or 0
static const room_obj_t *find_obj(uint8_t type, uint8_t n) {
    for (uint8_t i = 0; i < room_obj_count; i++) {
        const room_obj_t *o = &room_objs[i];
        if (o->type != type || (type == OBJ_KEYSPOT && (o->param & 1)))
            continue;
        if (!n--)
            return o;
    }
    return 0;
}

static const room_obj_t *find_fixed_keyspot(void) {
    for (uint8_t i = 0; i < room_obj_count; i++)
        if (room_objs[i].type == OBJ_KEYSPOT && (room_objs[i].param & 1))
            return &room_objs[i];
    return 0;
}

static uint8_t drawer_index(const room_obj_t *o) {
    uint8_t n = 0;
    for (const room_obj_t *p = room_objs; p != o; p++)
        if (p->type == OBJ_DRAWER)
            n++;
    return n;
}

#define DOOR_OPEN   0
#define DOOR_CLOSED 1
#define DOOR_LOCKED 2

// Sets a door's metatiles for its state. Closed doors get their number on the plate (dynamic metatile).
static void door_set(const room_obj_t *o, uint8_t state, uint8_t number) {
    uint8_t x = o->x, y = o->y;
    switch (o->param) {
    case WALL_TOP:
        if (state == DOOR_OPEN) {
            room_set_metatile(x, y, MT_HOTEL_DOOR_OPEN_TOP);
            room_set_metatile(x, y + 1, MT_HOTEL_DOOR_OPEN_BOTTOM);
        } else {
            room_set_metatile(x, y, room_make_plate(0, MT_HOTEL_DOOR_TOP, number));
            room_set_metatile(x, y + 1, state == DOOR_LOCKED ? MT_HOTEL_DOOR_BOTTOM_LOCKED : MT_HOTEL_DOOR_BOTTOM);
        }
        break;
    case WALL_LEFT:
    case WALL_RIGHT:
        if (state == DOOR_OPEN) {
            room_set_metatile(x, y, MT_HOTEL_SIDE_OPEN_TOP);
            room_set_metatile(x, y + 1, MT_HOTEL_SIDE_OPEN_BOTTOM);
        } else {
            room_set_metatile(x, y, room_make_plate(0, MT_HOTEL_SIDE_DOOR_TOP, number));
            room_set_metatile(x, y + 1, state == DOOR_LOCKED ? MT_HOTEL_SIDE_DOOR_BOTTOM_LOCKED : MT_HOTEL_SIDE_DOOR_BOTTOM);
        }
        break;
    default:  // WALL_BOTTOM: an entry, always an open doorway
        room_set_metatile(x, y, MT_HOTEL_DOORWAY_S);
        break;
    }
}

static uint8_t exit_state(void) {
    uint8_t f = run_flags[run_room];
    if (f & RF_EXIT_OPEN)
        return DOOR_OPEN;
    return (f & (RF_LOCKED | RF_FINAL)) ? DOOR_LOCKED : DOOR_CLOSED;
}

// Puts the room's saved state into the freshly loaded map: doors, opened drawers, where the key is
static void apply_room_state(void) {
    entry_door = find_obj(OBJ_ENTRY, 0);
    exit_door = find_obj(OBJ_EXIT, 0);
    if (exit_door)
        door_set(exit_door, exit_state(), run_room + 1);
    if (entry_door)
        door_set(entry_door, DOOR_OPEN, run_room);

    uint8_t opened = run_drawers[run_room];
    for (uint8_t i = 0; opened; i++, opened >>= 1)
        if (opened & 1) {
            const room_obj_t *d = find_obj(OBJ_DRAWER, i);
            if (d)
                room_set_metatile(d->x, d->y, MT_HOTEL_DRESSER_OPEN);
        }

    uint8_t f = run_flags[run_room];
    key_spot = 0;
    if ((f & RF_LOCKED) && !(f & (RF_KEY_TAKEN | RF_KEY_DRAWER)))
        key_spot = (f & RF_KEY_FIXED) ? find_fixed_keyspot() : find_obj(OBJ_KEYSPOT, run_key_slot[run_room]);
}

// Where the player stands after coming through a door: just inside it, facing into the room
static void place_at_door(const room_obj_t *o) {
    int16_t x = (int16_t)o->x << 4, y = (int16_t)o->y << 4;
    switch (o->param) {
    case WALL_TOP:     player_place(x, y + 32 - 8, DIR_DOWN); break;          // feet in the row below the door
    case WALL_BOTTOM:  player_place(x, y - 16 - 2, DIR_UP); break;            // feet in the row above it
    case WALL_LEFT:    player_place(x + 16 + 2, y + 16 - 10, DIR_RIGHT); break;
    default:           player_place(x - 16 - 2, y + 16 - 10, DIR_LEFT); break; // WALL_RIGHT
    }
}

// ------------------------------------------------------------------------------------------ rooms

static void draw_sprites(void) {
    spr_begin();
    if (!hide_hidden)
        player_draw();
    entity_draw();
    if (key_spot)
        spr_draw(&key_sheet, SPR_KEY_KEY, ((int16_t)key_spot->x << 4) - cam_x, ((int16_t)key_spot->y << 4) - cam_y, 0);
    spr_end();
}

void game_fade(uint8_t from, uint8_t to) BANKED {
    while (from != to) {
        from = from < to ? from + 1 : from - 1;
        platform_set_fade(from);
        game_wait_frames(FADE_STEP_FRAMES);
    }
}

// Loads room n with the screen black and fades in. `via`: how the player arrives.
static void enter_room(uint8_t n, uint8_t via) {
    game_state = GAME_TRANSITION;
    if (platform_fade != FADE_BLACK)
        game_fade(platform_fade, FADE_BLACK);
    hud_hide_now();
    hide_reset();
    platform_set_dim(0, TINT_NONE);
    spr_begin();
    spr_end();

    run_plan(n + 1);
    run_room = n;
    uint8_t first_time = n > run_max_room;
    if (first_time)
        run_max_room = n;
    uint8_t bank;
    const room_def_t *def = room_template(run_template[n], &bank);
    room_load(bank, def);
    apply_room_state();
    if (via == VIA_START)
        player_place(room_spawn_x, room_spawn_y, DIR_UP);
    else
        place_at_door(via == VIA_ENTRY ? entry_door : exit_door);

    bg_draw_full(camera_target_x(), camera_target_y());
    draw_sprites();
    hud_show(HUD_ENTER_FRAMES);
    entity_room_entered(first_time);
    game_wait_frames(1);
    game_fade(FADE_BLACK, 0);
    game_state = GAME_PLAYING;
}

void game_start(uint16_t seed) BANKED {
    run_start(seed);
    entity_reset();
    hide_reset();
    music_play(MUSIC_HOTEL);
    enter_room(0, VIA_START);
}

// ------------------------------------------------------------------------------------------ playing

static uint8_t inside(const room_obj_t *o, uint8_t mx, uint8_t my) {
    return o && mx >= o->x && mx < o->x + o->w && my >= o->y && my < o->y + o->h;
}

// Walking into an open door's doorway takes you through it
static uint8_t check_doors(void) {
    uint8_t mx = (uint8_t)((player_x + PLAYER_FEET_X) >> 4), my = (uint8_t)((player_y + PLAYER_FEET_Y) >> 4);
    if (inside(exit_door, mx, my) && exit_state() == DOOR_OPEN && run_room < RUN_LAST_ROOM) {
        enter_room(run_room + 1, VIA_ENTRY);
        return 1;
    }
    if (run_room > 0 && inside(entry_door, mx, my)) {
        enter_room(run_room - 1, VIA_EXIT);
        return 1;
    }
    return 0;
}

static void message_key(void) {
    static char msg[] = "GOT KEY FOR DOOR 000";
    text_3digits(msg + 17, run_room + 1);
    hud_message(msg, HUD_MESSAGE_FRAMES);
}

static void take_key(void) {
    sfx_play(SFX_KEY);
    run_flags[run_room] |= RF_KEY_TAKEN;
    run_key_door = run_room + 1;
    key_spot = 0;
    message_key();
}

static void use_exit(void) {
    uint8_t *f = &run_flags[run_room];
    if (*f & RF_EXIT_OPEN)
        return;
    if (entity_active()) {   // Minecraft docs 04: no door opens while something is travelling
        hud_message("IT WON'T BUDGE.", HUD_MESSAGE_FRAMES);
        sfx_play(SFX_LOCKED);
        return;
    }
    if (*f & RF_FINAL) {
        hud_message("TO BE CONTINUED...", HUD_MESSAGE_FRAMES);
        return;
    }
    if (*f & RF_LOCKED) {
        if (run_key_door != run_room + 1) {
            hud_message("IT'S LOCKED.", HUD_MESSAGE_FRAMES);
            sfx_play(SFX_LOCKED);
            return;
        }
        run_key_door = 0;  // the key is used up (Minecraft version: "Take the key when it turns")
        hud_message("UNLOCKED.", HUD_MESSAGE_FRAMES);
        sfx_play(SFX_UNLOCK);
    } else {
        sfx_play(SFX_DOOR);
    }
    *f |= RF_EXIT_OPEN;
    door_set(exit_door, DOOR_OPEN, 0);
}

static void use_drawer(const room_obj_t *o) {
    uint8_t bit = 1 << drawer_index(o);
    if (run_drawers[run_room] & bit)
        return;
    run_drawers[run_room] |= bit;
    room_set_metatile(o->x, o->y, MT_HOTEL_DRESSER_OPEN);
    sfx_play(SFX_DRAWER);
    uint8_t f = run_flags[run_room];
    if ((f & (RF_LOCKED | RF_KEY_DRAWER | RF_KEY_TAKEN)) == (RF_LOCKED | RF_KEY_DRAWER) &&
        drawer_index(o) == run_key_slot[run_room])
        take_key();
    else
        hud_message("NOTHING.", 60);
}

// Uses whatever is at room pixel (px, py). Returns 1 if there was something.
static uint8_t interact_at(int16_t px, int16_t py) {
    if (px < 0 || py < 0)
        return 0;
    uint8_t mx = (uint8_t)(px >> 4), my = (uint8_t)(py >> 4);
    if (key_spot && inside(key_spot, mx, my)) {
        take_key();
        return 1;
    }
    for (uint8_t i = 0; i < room_obj_count; i++) {
        const room_obj_t *o = &room_objs[i];
        if (!inside(o, mx, my))
            continue;
        switch (o->type) {
        case OBJ_EXIT:
            use_exit();
            return 1;
        case OBJ_DRAWER:
            use_drawer(o);
            return 1;
        case OBJ_CLOSET:
        case OBJ_BED:
            hide_try(o);
            return 1;
        }
    }
    return 0;
}

// Pressing A: whatever is in front of the player (12 px ahead of the feet, in the facing direction), or
// failing that 6 px to either side of that point: walls and furniture nudge the player off the line of
// what they're facing (player.c), and lining up to the pixel shouldn't be needed.
static void interact(void) {
    static const int8_t ahead_x[8] = { 0, -9, -12, -9, 0, 9, 12, 9 };
    static const int8_t ahead_y[8] = { 12, 9, 0, -9, -12, -9, 0, 9 };
    int8_t ax = ahead_x[player_facing], ay = ahead_y[player_facing];
    int16_t px = player_x + PLAYER_FEET_X + ax;
    int16_t py = player_y + PLAYER_FEET_Y + ay;
    int8_t sx = -ay >> 1, sy = ax >> 1;   // sideways, half the distance ahead
    if (!interact_at(px, py) && !interact_at(px + sx, py + sy))
        interact_at(px - sx, py - sy);
}

void game_frame(void) BANKED {
    measure_flush();
    input_update();
    uint8_t death;
    if (hide_hidden) {
        death = hide_update();
    } else {
        player_update();
        // No door takes you anywhere while Rush or Ambush is out (the open ones too)
        if (!entity_active() && check_doors())
            return;
        if (input_pressed & J_A)
            interact();
        death = DEATH_NONE;
    }
    if (!death)
        death = entity_update();
    if (death) {
        game_death = death;
        game_state = GAME_DEAD;
        return;
    }
    uint8_t dim, tint, hd = hide_dim();
    entity_lights(&dim, &tint);
    platform_set_dim(hd > dim ? hd : dim, tint);
    hud_update();
    bg_set_camera(step_toward(cam_x, camera_target_x()), step_toward(cam_y, camera_target_y()));
    draw_sprites();
}

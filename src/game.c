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
#include "light.h"
#include "screech.h"
#include "eyes.h"
#include "halt.h"
#include "timothy.h"
#include "library.h"
#include "figure.h"
#include "seek.h"
#include "walleyes.h"
#include "garden.h"
#include "electrical.h"
#include "items.h"
#include "lift.h"
#include "shop.h"
#include "room_templates.h"
#include "tileset_hotel.h"
#include "tileset_garden.h"
#include "tileset_shop.h"
#include "sprite_key.h"
#include "sprite_player.h"
#include "sprite_rush.h"
#include "sprite_ambush.h"
#include "sprite_eyes.h"
#include "sprite_screech.h"
#include "sprite_timothy.h"
#include "sprite_walleye.h"
#include "sprite_seek.h"
#include "sprite_puddle.h"
#include "sprite_guide.h"
#include "sprite_rain.h"
#include "sprite_jeff.h"

#define SCREEN_W 160
#define SCREEN_H 144

// How the player arrives in a room
#define VIA_START 0    // the run starts here (spawn object)
#define VIA_ENTRY 1    // through the room's entry door, coming from the room before
#define VIA_EXIT  2    // back through the room's exit door, from the room after

#define IN_LIBRARY (run_room == RUN_LIBRARY_ROOM)
static uint8_t in_electrical;   // Room 100 (set at the door: run_kind_at is a banked call)

#define FADE_STEP_FRAMES 3
#define HUD_MESSAGE_FRAMES 120

uint8_t game_state;
uint8_t game_death;

static spr_sheet_t key_sheet;
static const room_obj_t *entry_door, *exit_door;    // exit_door: this run's real exit
static const room_obj_t *exits[ROOM_EXITS_MAX];      // every exit, in object order (Dupe rooms have 2-3)
static uint8_t exit_count;
static const room_obj_t *key_spot;   // where the key lies in the open, or 0
// Loot lying in the open (Phase 9a) on the key spots the key isn't on: what each holds (rolled at the door, so
// finding a flashlight meanwhile doesn't change what's lying there) and its frame, or 0xFF
#define LOOSE_MAX 4   // key spots with loot per room (scripts/assets.py LOOSE_MAX; 2 until the owner's rooms, 2026-10-20)
static const room_obj_t *loose[LOOSE_MAX];
static loot_t loose_loot[LOOSE_MAX];
static uint8_t loose_frame[LOOSE_MAX];

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

#if defined(AUTOTEST) || defined(DEBUG)
// Debug builds only: with nothing held, B cycles the player's light between the glow, the lighter and the
// flashlight (owner, 2026-10-18; the play-testing bot's recorded run lights dark rooms all three ways). Not in
// the Library or Room 100, where B is sneaking.
#define LIGHT_DEBUG
uint8_t game_light_tool;
// Scripts poke a room number: the next frame enters it through its entry door (scripts/bot.py --warp)
uint8_t game_warp;
#endif

void game_init(void) BANKED {
    // The player's sheet at tile 0, palette 0; every other sheet is loaded per room (load_room_sprites).
    // Sprite tiles 128 and up share VRAM with background tiles the title and a jumpscare use, so after one
    // every sheet is loaded again.
    player_load();
    spr_forget();
}

// The most one hotel room can ask for at once (load_room_sprites, Timothy mid-room): dark, Eyes, Rush on his
// way, a key in the open and a Timothy drawer can all fall in one room. Rush and Ambush never come together.
#define MAX_(a, b) ((a) > (b) ? (a) : (b))
// Seek's eyes on the walls can be there too (the rooms before a chase). Rush or Ambush go after the tileset when
// it passes 128 tiles (spr_need_high, 2026-10-19: the owner's decorations), the rest below 128.
#define HOTEL_LOW_TILES (SPR_PLAYER_TILES + SPR_KEY_TILES + SPR_EYES_TILES + SPR_SCREECH_TILES + SPR_TIMOTHY_TILES +                          SPR_WALLEYE_TILES)
#define HOTEL_HIGH_TILES MAX_(SPR_RUSH_TILES, SPR_AMBUSH_TILES)
SPR_CHECK(TILESET_HOTEL_TILES > 128 ? HOTEL_LOW_TILES <= 128 && HOTEL_HIGH_TILES <= SPR_HIGH_ROOM(TILESET_HOTEL_TILES)
                                    : HOTEL_LOW_TILES + HOTEL_HIGH_TILES <= 184,
          hotel_room_sprites_outgrow_sprite_vram);
SPR_CHECK(SPR_PLAYER_PALETTES + SPR_KEY_PALETTES + MAX_(SPR_RUSH_PALETTES, SPR_AMBUSH_PALETTES) +
          SPR_EYES_PALETTES + SPR_SCREECH_PALETTES + SPR_TIMOTHY_PALETTES + SPR_WALLEYE_PALETTES <= SPR_PALETTE_LIMIT,
          hotel_room_sprites_need_over_8_gbc_palettes);
SPR_CHECK((SPR_KEY_FRAME_COUNT + MAX_(SPR_RUSH_FRAME_COUNT, SPR_AMBUSH_FRAME_COUNT) + SPR_EYES_FRAME_COUNT +
           SPR_SCREECH_FRAME_COUNT + SPR_TIMOTHY_FRAME_COUNT + SPR_WALLEYE_FRAME_COUNT) * 4 <= SPR_FRAME_POOL,
          hotel_room_frame_tables_outgrow_the_pool);
// A Courtyard or Greenhouse room: the hotel's set less the wall eyes (no chase near), plus the Courtyard's rain
SPR_CHECK(SPR_PLAYER_TILES + SPR_KEY_TILES + MAX_(SPR_RUSH_TILES, SPR_AMBUSH_TILES) + SPR_EYES_TILES +
          SPR_SCREECH_TILES + SPR_TIMOTHY_TILES + SPR_RAIN_TILES <= SPR_TILE_LIMIT(TILESET_GARDEN_TILES),
          garden_room_sprites_outgrow_sprite_vram);
SPR_CHECK(SPR_PLAYER_PALETTES + SPR_KEY_PALETTES + MAX_(SPR_RUSH_PALETTES, SPR_AMBUSH_PALETTES) +
          SPR_EYES_PALETTES + SPR_SCREECH_PALETTES + SPR_TIMOTHY_PALETTES + SPR_RAIN_PALETTES <= SPR_PALETTE_LIMIT,
          garden_room_sprites_need_over_8_gbc_palettes);
SPR_CHECK((SPR_KEY_FRAME_COUNT + MAX_(SPR_RUSH_FRAME_COUNT, SPR_AMBUSH_FRAME_COUNT) + SPR_EYES_FRAME_COUNT +
           SPR_SCREECH_FRAME_COUNT + SPR_TIMOTHY_FRAME_COUNT + SPR_RAIN_FRAME_COUNT) * 4 <= SPR_FRAME_POOL,
          garden_room_frame_tables_outgrow_the_pool);
// A chase room: Seek, his puddle (the start hallway), the guide's glow, the wall eyes
SPR_CHECK(SPR_PLAYER_TILES + SPR_SEEK_TILES + SPR_PUDDLE_TILES + SPR_GUIDE_TILES + SPR_WALLEYE_TILES <=
          SPR_TILE_LIMIT(TILESET_HOTEL_TILES), chase_sprites_outgrow_sprite_vram);
SPR_CHECK(SPR_PLAYER_PALETTES + SPR_SEEK_PALETTES + SPR_PUDDLE_PALETTES + SPR_GUIDE_PALETTES +
          SPR_WALLEYE_PALETTES <= SPR_PALETTE_LIMIT, chase_sprites_need_over_8_gbc_palettes);
SPR_CHECK((SPR_SEEK_FRAME_COUNT + SPR_PUDDLE_FRAME_COUNT + SPR_GUIDE_FRAME_COUNT + SPR_WALLEYE_FRAME_COUNT) * 4 <=
          SPR_FRAME_POOL, chase_frame_tables_outgrow_the_pool);

// Jeff's Shop: the player and his eyes
SPR_CHECK(SPR_PLAYER_TILES + SPR_JEFF_TILES <= SPR_TILE_LIMIT(TILESET_SHOP_TILES), shop_sprites_outgrow_sprite_vram);
SPR_CHECK(SPR_PLAYER_PALETTES + SPR_JEFF_PALETTES <= SPR_PALETTE_LIMIT, shop_sprites_need_over_8_gbc_palettes);

static uint8_t player_light(void) {
#ifdef LIGHT_DEBUG
    if (game_light_tool == 1)
        return LIGHT_LIGHTER;
    if (game_light_tool == 2)
        return LIGHT_FLASH + player_facing;
#endif
    return items_light();
}

static void light_player(void) {
    light_set(LIGHT_PLAYER, player_light(), (player_x + 8) >> 3, (player_y + 8) >> 3);
}

// ------------------------------------------------------------------------------------------ fire

#define FIRE_FRAMES 12        // between flips of the flames on screen
#define DOOR_LIGHT_NEAR 3     // metatiles from a door at which its top half lights in the dark
static uint8_t room_fire;     // the room has fire metatiles
static uint8_t fire_t;

// The flames flicker: every fire metatile on screen swaps between the two (owner, 2026-10-18: the two
// textures were drawn but never moved). Only the visible cells: the rest stream in as they are.
static void fire_flicker(void) {
    if (!room_fire || ++fire_t < FIRE_FRAMES)
        return;
    fire_t = 0;
    uint8_t x0 = cam_x < 0 ? 0 : (uint8_t)(cam_x >> 4), y0 = cam_y < 0 ? 0 : (uint8_t)(cam_y >> 4);
    uint8_t x1 = x0 + 11, y1 = y0 + 10;
    if (x1 > room_w)
        x1 = room_w;
    if (y1 > room_h)
        y1 = room_h;
    for (uint8_t y = y0; y < y1; y++) {
        const uint8_t *m = room_map + room_row[y];
        for (uint8_t x = x0; x < x1; x++) {
            uint8_t mt = m[x];
            if (mt == MT_HOTEL_FIRE_A)
                room_set_metatile(x, y, MT_HOTEL_FIRE_B);
            else if (mt == MT_HOTEL_FIRE_B)
                room_set_metatile(x, y, MT_HOTEL_FIRE_A);
        }
    }
}

static void find_fire(void) {
    room_fire = 0;
    uint16_t n = (uint16_t)room_w * room_h;
    for (uint16_t c = 0; c < n; c++)
        if (mt_flags[room_map[c]] & MTF_FIRE) {
            room_fire = 1;
            return;
        }
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

void game_draw_room(void) BANKED {
    bg_draw_full(camera_target_x(), camera_target_y());
}

void game_follow_camera(void) BANKED {
    bg_set_camera(step_toward(cam_x, camera_target_x()), step_toward(cam_y, camera_target_y()));
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

// Which exit (object order) a door is, for its plate slot
static uint8_t exit_index(const room_obj_t *o) {
    for (uint8_t k = 0; k < exit_count; k++)
        if (exits[k] == o)
            return k;
    return 3;   // the entry: its own plate slot (an open side entry shows its number too)
}

// Sets a door's metatiles for its state. Closed doors get their number on the plate (dynamic metatile,
// one per exit).
static void door_set(const room_obj_t *o, uint8_t state, uint8_t number) {
    uint8_t x = o->x, y = o->y, slot = exit_index(o);
    uint8_t locked_mt = MT_HOTEL_DOOR_BOTTOM_LOCKED;
    if (state == DOOR_LOCKED && IN_LIBRARY && o->type == OBJ_EXIT) {
        locked_mt = MT_HOTEL_DOOR_BOTTOM_PADLOCK;   // the combination padlock, and no number (owner's mock-up)
        number = PLATE_BLANK;
    }
    // Open, every wall's door is the top wall's open doorway: the number stays on the header over it (owner,
    // 2026-10-18: side doors lost theirs to a plain floor gap)
    if (state == DOOR_OPEN && o->param != WALL_BOTTOM) {
        room_set_metatile(x, y, room_make_plate(slot, MT_HOTEL_DOOR_OPEN_TOP, number));
        room_set_metatile(x, y + 1, MT_HOTEL_DOOR_OPEN_BOTTOM);
        return;
    }
    switch (o->param) {
    case WALL_TOP:
        room_set_metatile(x, y, room_make_plate(slot, MT_HOTEL_DOOR_TOP, number));
        room_set_metatile(x, y + 1, state == DOOR_LOCKED ? locked_mt : MT_HOTEL_DOOR_BOTTOM);
        break;
    case WALL_LEFT:
    case WALL_RIGHT:
        {
            room_set_metatile(x, y, room_make_plate(slot, MT_HOTEL_SIDE_DOOR_TOP, number));
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

// A wrong door (Dupe room): its index among the wrong ones (0 or 1)
static uint8_t fake_index(const room_obj_t *o) {
    uint8_t k = exit_index(o);
    return k < run_exit[run_room] ? k : k - 1;
}

// What a wrong door shows: no number without Dupe, his wrong number, scribbles once he's fired
static uint8_t fake_label(const room_obj_t *o) {
    uint8_t f2 = run_flags2[run_room], i = fake_index(o);
    if (!(f2 & RF2_DUPE))
        return PLATE_BLANK;
    if (f2 & (RF2_DUPE_HIT << i))
        return PLATE_GARBLED;
    return run_fake_number(run_room, i);
}

// Puts the room's saved state into the freshly loaded map: doors, opened drawers, where the key is
static void apply_room_state(void) {
    entry_door = find_obj(OBJ_ENTRY, 0);
    exit_count = 0;
    while (exit_count < ROOM_EXITS_MAX && (exits[exit_count] = find_obj(OBJ_EXIT, exit_count)))
        exit_count++;
    exit_door = exits[run_exit[run_room]];
    // The wrong doors look like the real one: padlocked when it is, and shut
    for (uint8_t k = 0; k < exit_count; k++)
        if (exits[k] != exit_door)
            door_set(exits[k], (run_flags[run_room] & RF_LOCKED) ? DOOR_LOCKED : DOOR_CLOSED, fake_label(exits[k]));
    if (exit_door)
        door_set(exit_door, exit_state(), run_room + 1);
    if (entry_door)
        door_set(entry_door, DOOR_OPEN, run_room);
    // Door 50 seals behind you: the Library is entered, not opened into (Minecraft docs 07). So do a chase's
    // doors (Minecraft docs 08), and the end hallway's, slammed: a bottom-wall doorway becomes wall, a side
    // door shuts.
    uint8_t k = run_kind_at(run_room);
    if (entry_door && (IN_LIBRARY || k == ROOM_KIND_SEEK_4WAY || k == ROOM_KIND_SEEK_END ||
                       run_kind_at(run_room - 1) == ROOM_KIND_SEEK_END)) {
        if (entry_door->param == WALL_BOTTOM)
            room_set_metatile(entry_door->x, entry_door->y, MT_HOTEL_WALL_TOP);
        else
            door_set(entry_door, DOOR_CLOSED, run_room);
    }

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
    // The other key spots may hold loot; the key's holds only the key (Minecraft docs 12), taken or not
    uint8_t key_k = (f & (RF_LOCKED | RF_KEY_DRAWER | RF_KEY_FIXED)) == RF_LOCKED ? run_key_slot[run_room] : 0xFF;
    for (uint8_t k = 0; k < LOOSE_MAX; k++) {
        loose[k] = 0;
        const room_obj_t *o = find_obj(OBJ_KEYSPOT, k);
        if (!o || k == key_k || (run_drawers[run_room] & RUN_LOOSE_BIT(k)) || run_kind_at(run_room) != ROOM_KIND_NORMAL)
            continue;
        loot_t *l = &loose_loot[k];
        run_loot(run_room, k, 1, inv_flashlight, l);
        if (l->battery || l->bandage || l->gold) {
            loose[k] = o;
            loose_frame[k] = l->battery ? SPR_KEY_BATTERY : l->bandage ? SPR_KEY_BANDAGE : SPR_KEY_GOLD;
        }
    }
}

// In the dark, the top half of the door you're within DOOR_LIGHT_NEAR metatiles of lights up so its number
// reads (owner, 2026-10-18: the glow didn't reach a top-wall door's header). The nearest such door; doors in
// the bottom wall are open doorways with no number.
static void light_door(void) {
    if (!light_dark)
        return;
    uint8_t fx = (uint8_t)((player_x + PLAYER_FEET_X) >> 4), fy = (uint8_t)((player_y + PLAYER_FEET_Y) >> 4);
    const room_obj_t *best = 0;
    uint8_t best_d = DOOR_LIGHT_NEAR + 1;
    for (uint8_t k = 0; k <= exit_count; k++) {
        const room_obj_t *o = k < exit_count ? exits[k] : entry_door;
        if (!o || o->param == WALL_BOTTOM)
            continue;
        // Distance from the feet's cell to the door's 1x2 cells, the larger of across and up/down
        uint8_t dx = fx > o->x ? fx - o->x : o->x - fx;
        uint8_t dy = fy > o->y + 1 ? fy - (o->y + 1) : fy < o->y ? o->y - fy : 0;
        uint8_t d = dx > dy ? dx : dy;
        if (d < best_d) {
            best_d = d;
            best = o;
        }
    }
    if (best)
        light_set(LIGHT_DOOR, LIGHT_DOORWAY, best->x << 1, best->y << 1);
    else
        light_set(LIGHT_DOOR, LIGHT_OFF, 0, 0);
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
    seek_draw();
    if (IN_LIBRARY)
        library_draw();
    if (in_electrical)
        electrical_draw();
    shop_draw();
    screech_draw();
    timothy_draw();
    entity_draw();
    eyes_draw();
    if (key_spot)
        spr_draw(&key_sheet, SPR_KEY_KEY, ((int16_t)key_spot->x << 4) - cam_x, ((int16_t)key_spot->y << 4) - cam_y, 0);
    for (uint8_t k = 0; k < LOOSE_MAX; k++)
        if (loose[k])
            spr_draw(&key_sheet, loose_frame[k], ((int16_t)loose[k]->x << 4) - cam_x, ((int16_t)loose[k]->y << 4) - cam_y, 0);
    garden_draw();     // the Courtyard's rain
    walleyes_draw();   // last: anything else on the same line wins the hardware's 10
    spr_end();
}

void game_fade(uint8_t from, uint8_t to) BANKED {
    while (from != to) {
        from = from < to ? from + 1 : from - 1;
        platform_set_fade(from);
        game_wait_frames(FADE_STEP_FRAMES);
    }
}

// The current room's template and saved state into WRAM (and its tileset into VRAM if needed)
static void load_room(void) {
    uint8_t bank;
    const room_def_t *def = room_template(run_template[run_room], &bank);
    room_load(bank, def);
    apply_room_state();
    garden_room_loaded();   // the Greenhouse's Snares into the map
    electrical_room_loaded();   // Room 100's fuses, and what the lever and the power changed
    shop_room_loaded();         // Jeff's stock on his counter
    find_fire();
}

// The loaded room's lights, and who's in it (behind black)
static void light_up_room(void) {
    light_room(run_flags2[run_room] & RF2_DARK);
    light_player();
    light_door();
    eyes_room_entered();
}

// Only the sheets this room can show (sprites.h), in the order that changes least from door to door: Rush
// stays for a few rooms, the rest come and go. Timothy asks for his when he springs (timothy.c).
static void load_room_sprites(void) {
    spr_room_begin();
    entity_need_sprites();
    // The key and the loose loot share the key's sheet: any of the LOOSE_MAX spots holding something needs it (until
    // 2026-10-20 only spots 0 and 1 were asked, so loot on a third or fourth spot drew from another sheet)
    uint8_t want_key = key_spot != 0;
    for (uint8_t k = 0; k < LOOSE_MAX; k++)
        if (loose[k])
            want_key = 1;
    if (want_key)
        spr_need(&key_sheet, BANK(sprite_key), &sprite_key);
    eyes_need_sprites();
    screech_need_sprites();
    timothy_need_sprites();   // only if he's out (after a Screech bite mid-jump)
    if (IN_LIBRARY)
        library_need_sprites();
    if (in_electrical)
        electrical_need_sprites();
    shop_need_sprites();
    seek_need_sprites();
    walleyes_need_sprites();
    garden_need_sprites();
}

// Draws the lit room around the player
static void draw_room(void) {
    load_room_sprites();
    bg_draw_full(camera_target_x(), camera_target_y());
    draw_sprites();
}

// Loads room n with the screen black and fades in. `via`: how the player arrives.
static void enter_room(uint8_t n, uint8_t via) {
    game_state = GAME_TRANSITION;
    if (platform_fade != FADE_BLACK)
        game_fade(platform_fade, FADE_BLACK);
    hud_hide_now();
    hide_reset();
    timothy_reset();
    platform_set_dim(0, TINT_NONE);
    spr_begin();
    spr_end();

    run_plan(n + 1);
    if (IN_LIBRARY && n != RUN_LIBRARY_ROOM)
        library_exit();   // through the padlocked door: Figure and the music stay behind
    if (in_electrical && n != run_room)
        electrical_exit();   // back to room 99, before the lever
    if (run_room == RUN_SHOP_ROOM && n != run_room)
        shop_exit();         // Jeff's jingle stays in his shop
    run_room = n;
    in_electrical = run_kind_at(n) == ROOM_KIND_ELECTRICAL;
    uint8_t first_time = n > run_max_room;
    if (first_time)
        run_max_room = n;
    uint8_t was_dark = light_dark;
    load_room();
    if (via == VIA_START)
        player_place(room_spawn_x, room_spawn_y, DIR_UP);
    else
        place_at_door(via == VIA_ENTRY ? entry_door : exit_door);
    screech_room_entered();
    if (IN_LIBRARY)
        library_enter();
    if (in_electrical)
        electrical_enter();
    shop_enter();
    seek_room_entered();
    walleyes_room_entered();
    garden_room_entered();
    if (!(run_flags2[run_room] & RF2_DARK))
        items_room_lit();   // out of the dark: the light goes out by itself (owner)
    light_up_room();
    // The darkness cue marks the lights going out, so a run of dark rooms announces once (Minecraft docs 05)
    if (light_dark && !was_dark)
        sfx_play(SFX_DARK);
    entity_room_entered(first_time);   // before the sprites: whether he fires decides his sheet
    draw_room();
    game_wait_frames(1);
    game_fade(FADE_BLACK, 0);
    game_state = GAME_PLAYING;
}

// Screech bit (or Dupe got you) and you lived: the face, then back to the room as it was (the picture used
// the tileset's and some sprites' VRAM)
static void survived_scare(uint8_t cause) {
    game_state = GAME_TRANSITION;
    death_jumpscare(cause);
    game_init();
    load_room();
    light_up_room();
    draw_room();
    hud_show(150);
    game_wait_frames(1);
    game_fade(FADE_BLACK, 0);
    game_state = GAME_PLAYING;
}

void game_start(uint16_t seed) BANKED {
    run_start(seed);
    items_reset();
    items_lift(lift_bought);   // what the lift shop sold
    lift_bought = 0;
    game_init();   // sprite tiles: a jumpscare may have used their VRAM
    entity_reset();
    screech_reset();
    hide_reset();
    library_reset();
    figure_stop();
    seek_reset();
    garden_reset();
    electrical_reset();
    light_room(0);
    music_play(MUSIC_NONE);   // no music in the rooms (owner, 2026-10-18: the ambience is gone)
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
        uint8_t *f2 = &run_flags2[run_room];
        if ((*f2 & (RF2_HALT | RF2_HALT_DONE)) == RF2_HALT) {
            // Halt's hallway first; you come out at the next room as if through the door (Minecraft docs 06)
            uint8_t death = halt_run();
            if (death) {
                game_death = death;
                game_state = GAME_DEAD;
                return 1;
            }
            *f2 |= RF2_HALT_DONE;
        }
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
    hud_message("GOT THE KEY.", HUD_MESSAGE_FRAMES);   // no number: it would name the real door (owner)
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
    if (IN_LIBRARY && (*f & RF_LOCKED)) {
        library_open_lock();   // no key turns in it: the code (library.c)
        return;
    }
    if (*f & RF_LOCKED) {
        if (run_key_door == run_room + 1) {
            run_key_door = 0;  // the key is used up (Minecraft version: "Take the key when it turns")
            hud_message("UNLOCKED.", HUD_MESSAGE_FRAMES);
        } else if (items_unlock()) {   // no key: a lockpick, else the skeleton key (owner: the key first)
            *f |= RF_KEY_TAKEN;   // its key has nothing left to open: gone (Minecraft docs 02, discardKeysFor)
            key_spot = 0;
        } else {
            hud_message("IT'S LOCKED.", HUD_MESSAGE_FRAMES);
            sfx_play(SFX_LOCKED);
            return;
        }
        sfx_play(SFX_UNLOCK);
    } else {
        sfx_play(SFX_DOOR);
    }
    *f |= RF_EXIT_OPEN;
    door_set(exit_door, DOOR_OPEN, run_room + 1);
    eyes_dismiss();   // progress is the answer to Eyes
}

// A wrong door in a Dupe room (Minecraft docs 06, "The refusal order"): checked before any lock, and it
// spends no key
// The Library's code was right
static void open_padlock(void) {
    run_flags[run_room] = (run_flags[run_room] & ~RF_LOCKED) | RF_EXIT_OPEN;
    door_set(exit_door, DOOR_OPEN, run_room + 1);
    sfx_play(SFX_UNLOCK);
    hud_message("THE PADLOCK OPENS.", HUD_MESSAGE_FRAMES);
}

static void use_fake(const room_obj_t *o) {
    uint8_t *f2 = &run_flags2[run_room];
    uint8_t hit = RF2_DUPE_HIT << fake_index(o);
    if (entity_active()) {
        hud_message("IT WON'T BUDGE.", HUD_MESSAGE_FRAMES);
        sfx_play(SFX_LOCKED);
        return;
    }
    if (!(*f2 & RF2_DUPE) || (*f2 & hit)) {
        hud_message((*f2 & hit) ? "IT WON'T OPEN." : "IT'S FAKE.", HUD_MESSAGE_FRAMES);
        sfx_play(SFX_LOCKED);
        return;
    }
    // The crucifix: the door stays shut and he's spent on it (its scribbles come all the same)
    if (items_crucifix()) {
        *f2 |= hit;
        door_set(o, DOOR_CLOSED, fake_label(o));
        return;
    }
    // The door opens on him: a beat, then his face (owner: full-screen) and 40 HP. Not progress, so Eyes
    // stays. The door then shuts with scribbles where its number was.
    game_state = GAME_TRANSITION;
    sfx_play(SFX_DOOR);
    door_set(o, DOOR_OPEN, fake_label(o));   // still his wrong number while it swings open
    *f2 |= hit;
    draw_sprites();
    game_wait_frames(20);
    if (run_hp > DUPE_DAMAGE) {
        run_hp -= DUPE_DAMAGE;
        survived_scare(DEATH_DUPE);
    } else {
        run_hp = 0;
        game_death = DEATH_DUPE;
        game_state = GAME_DEAD;
    }
}

static uint8_t drawer_left(uint8_t d);

static void use_drawer(const room_obj_t *o) {
    uint8_t bit = 1 << drawer_index(o);
    if (run_drawers[run_room] & bit) {
        drawer_left(drawer_index(o));
        return;
    }
    run_drawers[run_room] |= bit;
    room_set_metatile(o->x, o->y, MT_HOTEL_DRESSER_OPEN);
    sfx_play(SFX_DRAWER);
    if (run_timothy_in(run_room, drawer_index(o))) {   // he took whatever was in it
        run_timothy = 1;
        timothy_spring(o->x, o->y);
        return;
    }
    uint8_t f = run_flags[run_room], d = drawer_index(o), got = 0;
    if ((f & (RF_LOCKED | RF_KEY_DRAWER | RF_KEY_TAKEN)) == (RF_LOCKED | RF_KEY_DRAWER) && d == run_key_slot[run_room]) {
        take_key();
        got = 1;
    }
    // The key goes in first and the loot rolls after (Minecraft docs 12): a drawer can hold both
    loot_t l;
    run_loot(run_room, d, 0, inv_flashlight, &l);
    if (l.gold || l.item || l.battery || l.bandage)
        run_set_left(run_room, d, items_take(&l, 0x0F));
    else if (!got)
        hud_message("NOTHING.", 60);
}

// An opened drawer that kept what didn't fit: A again tries that
static uint8_t drawer_left(uint8_t d) {
    uint8_t left = run_left(run_room, d);
    if (!left)
        return 0;
    loot_t l;
    run_loot(run_room, d, 0, inv_flashlight, &l);
    run_set_left(run_room, d, items_take(&l, left));
    return 1;
}

// Loot lying in the open: taken unless it doesn't fit (a bandage at full health stays put)
static void take_loose(uint8_t k) {
    if (items_take(&loose_loot[k], 0x0F))
        return;
    run_drawers[run_room] |= RUN_LOOSE_BIT(k);
    loose[k] = 0;
}

// Uses whatever is at room pixel (px, py). Returns 1 if there was something.
static uint8_t interact_at(int16_t px, int16_t py) {
    if (px < 0 || py < 0)
        return 0;
    uint8_t mx = (uint8_t)(px >> 4), my = (uint8_t)(py >> 4);
    if (IN_LIBRARY && library_use_at(mx, my))
        return 1;
    if (in_electrical && electrical_use_at(mx, my))
        return 1;
    if (shop_use_at(mx, my))
        return 1;
    if (key_spot && inside(key_spot, mx, my)) {
        take_key();
        return 1;
    }
    for (uint8_t k = 0; k < LOOSE_MAX; k++)
        if (loose[k] && inside(loose[k], mx, my)) {
            take_loose(k);
            return 1;
        }
    for (uint8_t i = 0; i < room_obj_count; i++) {
        const room_obj_t *o = &room_objs[i];
        if (!inside(o, mx, my))
            continue;
        switch (o->type) {
        case OBJ_EXIT:
            if (o == exit_door)
                use_exit();
            else
                use_fake(o);
            return 1;
        case OBJ_DRAWER:
            use_drawer(o);
            return 1;
        case OBJ_BELL:   // rings every time, however fast it's pressed (owner): each press restarts the sound
            sfx_play(SFX_BELL);
            sfx_play(SFX_BELL_HI);
            return 1;
        case OBJ_CLOSET:
        case OBJ_BED:
            if (in_electrical && ELECTRICAL_RUNNING()) {   // owner: closets don't save you in the escape
                hud_message("NO TIME TO HIDE.", HUD_MESSAGE_FRAMES);
                return 1;
            }
            hide_try(o);
            return 1;
        case OBJ_PAPER:
            library_read_paper();
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
    // A key you're standing on comes first (owner, 2026-10-18: only the cell ahead was looked at, so a key
    // underfoot had to be stepped off to be picked up). The feet's cell or the body's middle: in 3/4 view a
    // player drawn over the key can have their feet in the cell below it.
    uint8_t fx = (uint8_t)((player_x + PLAYER_FEET_X) >> 4);
    uint8_t fy = (uint8_t)((player_y + PLAYER_FEET_Y) >> 4), my = (uint8_t)((player_y + 6) >> 4);
    if (key_spot && (inside(key_spot, fx, fy) || inside(key_spot, fx, my))) {
        take_key();
        return;
    }
    for (uint8_t k = 0; k < LOOSE_MAX; k++)   // loot underfoot too
        if (loose[k] && (inside(loose[k], fx, fy) || inside(loose[k], fx, my))) {
            take_loose(k);
            return;
        }
    if (in_electrical && electrical_use_at(fx, (uint8_t)((player_y + PLAYER_FEET_Y) >> 4)))
        return;   // Room 100's key underfoot (the feet's cell is floor: nothing else is used there)
    int8_t ax = ahead_x[player_facing], ay = ahead_y[player_facing];
    int16_t px = player_x + PLAYER_FEET_X + ax;
    int16_t py = player_y + PLAYER_FEET_Y + ay;
    int8_t sx = -ay >> 1, sy = ax >> 1;   // sideways, half the distance ahead
    if (!interact_at(px, py) && !interact_at(px + sx, py + sy))
        interact_at(px - sx, py - sy);
}

// Fire (MTF_FIRE, Seek's end hallway): the feet's metatile or the one at the body's middle
static uint8_t on_fire(void) {
    int16_t x = player_x + PLAYER_FEET_X;
    uint8_t mx = (uint8_t)(x >> 4);
    return (mt_flags[room_map[room_row[(uint8_t)((player_y + PLAYER_FEET_Y) >> 4)] + mx]] |
            mt_flags[room_map[room_row[(uint8_t)((player_y + 6) >> 4)] + mx]]) & MTF_FIRE;
}

// Chasing, the real door opens by itself as you reach it (Minecraft docs 08: a door that has to be aimed at
// while running from something is a death with no mechanic under it). Measured to the door's inner edge.
static void chase_door(void) {
    uint8_t *f = &run_flags[run_room];
    if ((*f & RF_EXIT_OPEN) || !exit_door || !ROOM_KIND_IS_CHASE(run_kind_at(run_room)))
        return;
    const room_obj_t *o = exit_door;
    int16_t x = ((int16_t)o->x << 4) + (o->w << 3), y = ((int16_t)o->y << 4) + (o->h << 3);
    switch (o->param) {
    case WALL_TOP:  y = (int16_t)(o->y + o->h) << 4; break;
    case WALL_LEFT: x = (int16_t)(o->x + o->w) << 4; break;
    default:        x = (int16_t)o->x << 4; break;
    }
    int16_t dx = x - (player_x + PLAYER_FEET_X), dy = y - (player_y + PLAYER_FEET_Y);
    if (dx > -SEEK_AUTO_OPEN && dx < SEEK_AUTO_OPEN && dy > -SEEK_AUTO_OPEN && dy < SEEK_AUTO_OPEN) {
        *f |= RF_EXIT_OPEN;
        door_set(exit_door, DOOR_OPEN, run_room + 1);
        sfx_play(SFX_DOOR);
    }
}

void game_frame(void) BANKED {
    measure_flush();
    input_update();
#if defined(AUTOTEST) || defined(DEBUG)
    if (game_warp) {
        uint8_t n = game_warp;
        game_warp = 0;
        enter_room(n, VIA_ENTRY);
        return;
    }
#endif
    uint8_t death;
    // The Library's padlock takes the buttons while its panel is open (Start opens the notes)
    uint8_t lib = IN_LIBRARY ? library_frame() : LIB_FREE;
    if (lib == LIB_SOLVED)
        open_padlock();
    if (in_electrical && electrical_frame() == ELEC_HELD)
        lib = LIB_HELD;   // the breaker's panel has the buttons
    if (hide_hidden) {
        items_frame(ITEMS_HIDDEN);   // a light goes out in a closet (Minecraft docs 04); B is getting out
        death = hide_update();
    } else if (seek_frozen()) {
        player_moving = 0;   // watching him rise
        death = DEATH_NONE;
    } else if (lib == LIB_HELD) {
        player_moving = 0;   // standing at the lock: silent
        death = DEATH_NONE;
    } else if (garden_rooted()) {
        player_moving = 0;   // a Snare holds you: no walking, no A (Minecraft docs 09)
        garden_update();
        death = DEATH_NONE;
    } else {
        // Owner: hold B to sneak in the Library (items don't need B there); Room 100 the same, until the escape.
        // Nothing is held there (owner, Phase 9a).
        uint8_t sneak_room = IN_LIBRARY || (in_electrical && !ELECTRICAL_RUNNING());
        player_sneak = sneak_room && (input_held & J_B);
#ifdef LIGHT_DEBUG
        if ((input_pressed & J_B) && !sneak_room && !inv_held)
            game_light_tool = game_light_tool == 2 ? 0 : game_light_tool + 1;
#endif
        items_frame(sneak_room ? ITEMS_SNEAK : ITEMS_FREE);
        // Start pauses on what you carry (the Library's Start is its notes)
        if ((input_pressed & J_START) && !IN_LIBRARY) {
            items_pause();
            return;
        }
        player_update();
        if (on_fire()) {
            sfx_play(SFX_FIRE);
            game_death = DEATH_FIRE;
            game_state = GAME_DEAD;
            return;
        }
        garden_update();   // a Snare under your feet
        if (seek_chasing())
            chase_door();
        // No door takes you anywhere while Rush or Ambush is out (the open ones too)
        if (!entity_active() && check_doors())
            return;
        if (input_pressed & J_A) {
            interact();
            if (game_state != GAME_PLAYING)   // Dupe's last blow
                return;
        }
        death = DEATH_NONE;
    }
    if (!death && IN_LIBRARY)
        death = library_update();
    if (!death && in_electrical) {
        death = electrical_update();
        if (electrical_state == ELEC_WON) {
            game_state = GAME_WON;
            return;
        }
    }
    if (!death)
        death = seek_update();
    if (!death)
        death = entity_update();
    if (!death)
        death = eyes_update();
    if (!death && screech_update()) {
        if (items_crucifix()) {
            screech_ward();   // gone, and kept away a minute (owner)
        } else if (run_hp > SCREECH_DAMAGE) {
            run_hp -= SCREECH_DAMAGE;
            survived_scare(DEATH_SCREECH);
            return;
        } else {
            run_hp = 0;
            death = DEATH_SCREECH;
        }
    }
    if (death) {
        game_death = death;
        game_state = GAME_DEAD;
        return;
    }
    timothy_update();
    uint8_t dim, tint, hd = hide_dim();
    entity_lights(&dim, &tint);
    platform_set_dim(hd > dim ? hd : dim, tint);
    hud_update();
    game_follow_camera();
    // The light follows the player a frame late when the camera just built a column or row: the player
    // crosses a tile edge on the same frame the centred camera does, and both together overran a DMG frame
    if (!bg_col_n && !bg_row_n) {
        light_player();
        // The door's light only between the player's redraws: light_set finishes a pending one at once
        if (!light_pending)
            light_door();
        light_step(LIGHT_ROWS_PER_FRAME);
    }
    fire_flicker();
    draw_sprites();
}

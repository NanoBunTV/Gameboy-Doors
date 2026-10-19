#pragma bank 255
#include <gb/gb.h>
#include <string.h>
#include "electrical.h"
#include "figure.h"
#include "run.h"
#include "room.h"
#include "bg.h"
#include "hud.h"
#include "input.h"
#include "audio.h"
#include "death.h"
#include "player.h"
#include "sprites.h"
#include "tileset_hotel.h"
#include "tileset_electrical.h"
#include "sprite_key.h"
#include "sprite_marks.h"
#include "sprite_figure.h"
#include "sprite_player.h"

// After the Minecraft version's Room 100 (docs 10), on one floor (owner, 2026-10-18).
//
// What the room's author places (ASSETS.md): the lever, the breaker box, the breaker room's door, Figure's
// door, the lift, Figure's start, key spots, the 'safe' rectangle (the breaker room, where he never goes),
// pools of light; gate, slide and shelf metatiles. The run picks the fuses' shelves and the key's spot from the
// seed, so a replay finds them in the same places.
//
// Scratch (fuse places, the light pools, the breaker's answer) lives in the room map's tail after Figure's
// route search and its queue: Room 100 is at most (ROOM_MAX - 768) / 2 metatiles (scripts/assets.py).

#define SLIDE_STEP     40      // frames between the sliding door's steps (it parts from the middle outward);
                               // more than Figure's route search takes for the room (32 cells a frame)
#define MUSIC_AFTER    120     // the song starts this long after the lever (Minecraft: 5 s, the gate's moment)
#define FIGURE_AFTER   180     // the power's back: he comes out this long after (Minecraft: 3 s)
#define LIGHTS_MAX     6       // scripts/assets.py LIGHTS_MAX
#define PANEL_ROWS     7
#define MSG_FRAMES     120
// The Circuit Breaker (Minecraft docs 10): one number at a time, on or off, through all ten in a random
// order, a blank pause, and the same answer again; three rounds, each faster. Nothing is checked until all ten
// match at once.
#define SHOW_PAUSE     60      // the display blank between passes
#define SOLVED_FRAMES  180     // both tiles blank after a round is solved; the switches go off halfway
static const uint8_t show_frames[BREAKER_ROUNDS] = { 54, 36, 22 };

SPR_CHECK(SPR_PLAYER_TILES + SPR_MARKS_TILES + SPR_FIGURE_TILES + SPR_KEY_TILES <=
          SPR_TILE_LIMIT(TILESET_ELECTRICAL_TILES), room100_sprites_outgrow_sprite_vram);
SPR_CHECK(SPR_PLAYER_PALETTES + SPR_MARKS_PALETTES + SPR_FIGURE_PALETTES + SPR_KEY_PALETTES <= SPR_PALETTE_LIMIT,
          room100_sprites_need_over_8_gbc_palettes);
SPR_CHECK((SPR_MARKS_FRAME_COUNT + SPR_FIGURE_FRAME_COUNT + SPR_KEY_FRAME_COUNT) * 4 <= SPR_FRAME_POOL,
          room100_frame_tables_outgrow_the_pool);

uint8_t electrical_state;
uint8_t electrical_carried, electrical_inserted;
// Not static: scripts/electrical-test.py reads them
uint16_t elec_taken;             // fuses picked up, a bit each (in plan order)
uint8_t elec_key_slot;           // which key spot holds the key (non-fixed ones, object order)
uint8_t elec_key_taken;
uint8_t elec_round;              // the breaker: round 0-2
uint16_t elec_switch;            // the breaker's switches, bit k = switch k+1 on
uint16_t elec_answer;            // ... what they must be

uint8_t elec_cursor;             // the breaker panel's switch (0-9)
static uint8_t timer, music_t, slide_k, slide_lo, slide_hi, slide_axis, panel, show_i, show_t, solved_t;
static uint8_t lights;           // pools in this room
static uint16_t rng;
static spr_sheet_t key_sheet;

// The scratch in the room map's tail
typedef struct {
    uint8_t fuse_x[FUSES], fuse_y[FUSES];
    uint8_t light_on[LIGHTS_MAX], light_t[LIGHTS_MAX], light_blinks[LIGHTS_MAX];
    uint8_t order[FUSES];        // the breaker's display order
    const room_obj_t *light_obj[LIGHTS_MAX];
    char row[21];                // a line of text (the panel, the messages)
} scratch_t;
static scratch_t *sc;

static const room_obj_t *lever, *breaker, *bdoor, *fdoor, *lift, *safe, *key_obj;
// In the scratch, to spare the GBC's WRAM bank 1 (the stack's too)
#define row (sc->row)
#define light_obj (sc->light_obj)

static uint16_t rnd(void) {
    rng ^= rng << 7;
    rng ^= rng >> 9;
    rng ^= rng << 8;
    return rng;
}

static void seed(uint16_t salt) {
    rng = run_seed ^ salt;
    if (!rng)
        rng = 1;
    for (uint8_t i = 0; i < 4; i++)
        rnd();
}

void electrical_reset(void) BANKED {
    electrical_state = ELEC_WAIT;
    electrical_carried = electrical_inserted = 0;
    elec_taken = 0;
    elec_key_taken = 0;
    panel = 0;
}

static uint8_t inside(const room_obj_t *o, uint8_t mx, uint8_t my) {
    return o && (uint8_t)(mx - o->x) < o->w && (uint8_t)(my - o->y) < o->h;
}

static uint8_t fuse_candidate(uint8_t x, uint8_t y) {
    uint16_t c = room_row[y] + x;
    return (mt_flags[room_map[c]] & MTF_SHELF) && y + 1 < room_h && !(mt_flags[room_map[c + room_w]] & MTF_SOLID) &&
           !inside(safe, x, y);
}

// The ten fuses' shelves, from the seed alone and the room's map (as the Library's books)
static void plan_fuses(void) {
    seed(0xF05Eu);
    uint8_t count = 0;
    for (uint8_t y = 0; y < room_h; y++)
        for (uint8_t x = 0; x < room_w; x++)
            if (fuse_candidate(x, y))
                count++;   // 10-255 (scripts/assets.py)
    uint8_t pick[FUSES];
    for (uint8_t i = 0; i < FUSES; i++) {
        uint8_t k, again;
        do {
            k = (uint8_t)(rnd() % count);
            again = 0;
            for (uint8_t j = 0; j < i; j++)
                if (pick[j] == k)
                    again = 1;
        } while (again);
        pick[i] = k;
    }
    uint8_t n = 0;
    for (uint8_t y = 0; y < room_h; y++)
        for (uint8_t x = 0; x < room_w; x++)
            if (fuse_candidate(x, y)) {
                for (uint8_t i = 0; i < FUSES; i++)
                    if (pick[i] == n) {
                        sc->fuse_x[i] = x;
                        sc->fuse_y[i] = y;
                    }
                n++;
            }
    // The key: one of the room's (non-fixed) key spots
    uint8_t spots = 0;
    for (uint8_t i = 0; i < room_obj_count; i++)
        if (room_objs[i].type == OBJ_KEYSPOT && !(room_objs[i].param & 1))
            spots++;
    elec_key_slot = (uint8_t)(rnd() % spots);
}

static void set_mt(const room_obj_t *o, uint8_t top, uint8_t bottom) {
    room_set_metatile(o->x, o->y, top);
    room_set_metatile(o->x, o->y + 1, bottom);
}

static void lift_gates(uint8_t mt) {
    room_set_metatile(lift->x, lift->y + 2, mt);
    room_set_metatile(lift->x + 1, lift->y + 2, mt);
}

static void lift_open(void) {
    room_set_metatile(lift->x, lift->y, MT_ELECTRICAL_LIFT_OPEN_TL);
    room_set_metatile(lift->x + 1, lift->y, MT_ELECTRICAL_LIFT_OPEN_TR);
    room_set_metatile(lift->x, lift->y + 1, MT_ELECTRICAL_LIFT_OPEN_BL);
    room_set_metatile(lift->x + 1, lift->y + 1, MT_ELECTRICAL_LIFT_OPEN_BR);
}

// Every gate but the lift's two (they go when the power comes back), and door 100 sealed behind you
// The map passes walk it with one pointer: a row lookup per cell made the lever's frame several frames long
static void open_gates(void) {
    const uint8_t *m = room_map;
    for (uint8_t y = 0; y < room_h; y++)
        for (uint8_t x = 0; x < room_w; x++, m++)
            if ((mt_flags[*m] & MTF_GATE) && !(y == lift->y + 2 && (uint8_t)(x - lift->x) < 2))
                room_set_metatile(x, y, MT_ELECTRICAL_FLOOR);
    for (uint8_t i = 0; i < room_obj_count; i++)
        if (room_objs[i].type == OBJ_ENTRY)
            room_set_metatile(room_objs[i].x, room_objs[i].y, MT_HOTEL_WALL_TOP);
}

// The sliding door: its cells' extent along its long axis (found once, at the lever)
static void find_slide(void) {
    uint8_t x0 = 255, x1 = 0, y0 = 255, y1 = 0;
    const uint8_t *m = room_map;
    for (uint8_t y = 0; y < room_h; y++)
        for (uint8_t x = 0; x < room_w; x++, m++)
            if (mt_flags[*m] & MTF_SLIDE) {
                if (x < x0) x0 = x;
                if (x > x1) x1 = x;
                if (y < y0) y0 = y;
                if (y > y1) y1 = y;
            }
    slide_k = 0;
    if (x0 == 255) {
        slide_lo = 1;   // none
        slide_hi = 0;
        return;
    }
    slide_axis = (x1 - x0) >= (y1 - y0) ? 0 : 1;
    slide_lo = slide_axis ? y0 : x0;
    slide_hi = slide_axis ? y1 : x1;
}

// One step: the cells within step k of the middle go, both sides together (Minecraft docs 10: a pair of doors
// parting in step, not one dragged back and forth). Returns 1 when nothing's left.
static uint8_t slide_step(void) {
    if (slide_lo > slide_hi)
        return 1;
    uint8_t any = 0, reach = 2 * slide_k + 1;
    const uint8_t *m = room_map;
    for (uint8_t y = 0; y < room_h; y++)
        for (uint8_t x = 0; x < room_w; x++, m++)
            if (mt_flags[*m] & MTF_SLIDE) {
                uint8_t p = slide_axis ? y : x;
                int16_t d = (int16_t)2 * p - slide_lo - slide_hi;
                if (d < 0)
                    d = -d;
                if (d <= reach)
                    room_set_metatile(x, y, MT_ELECTRICAL_FLOOR);
                else
                    any = 1;
            }
    slide_k++;
    return !any;
}

static void find_objects(void) {
    lever = breaker = bdoor = fdoor = lift = safe = key_obj = 0;
    lights = 0;
    uint8_t spot = 0;
    for (uint8_t i = 0; i < room_obj_count; i++) {
        const room_obj_t *o = &room_objs[i];
        switch (o->type) {
        case OBJ_LEVER: lever = o; break;
        case OBJ_BREAKER: breaker = o; break;
        case OBJ_BREAKER_DOOR: bdoor = o; break;
        case OBJ_FIGURE_DOOR: fdoor = o; break;
        case OBJ_LIFT: lift = o; break;
        case OBJ_SAFE: safe = o; break;
        case OBJ_LIGHT:
            if (lights < LIGHTS_MAX)
                light_obj[lights++] = o;
            break;
        case OBJ_KEYSPOT:
            if (!(o->param & 1) && spot++ == elec_key_slot)
                key_obj = o;
            break;
        }
    }
}

void electrical_room_loaded(void) BANKED {
    if (run_room != RUN_LAST_ROOM || run_kind_at(RUN_LAST_ROOM) != ROOM_KIND_ELECTRICAL)
        return;
    uint16_t cells = (uint16_t)room_w * room_h;
    sc = (scratch_t *)(room_map + 2 * cells + 512);
    for (uint8_t i = 0; i < room_obj_count; i++)
        if (room_objs[i].type == OBJ_SAFE)
            safe = &room_objs[i];
    plan_fuses();
    find_objects();
    // The fuses not yet taken, on their shelves (the map is fresh from ROM)
    for (uint8_t i = 0; i < FUSES; i++)
        if (!(elec_taken & (1 << i)))
            room_map[room_row[sc->fuse_y[i]] + sc->fuse_x[i]] = MT_ELECTRICAL_SHELF_FUSE;
    for (uint8_t i = 0; i < LIGHTS_MAX; i++) {
        sc->light_on[i] = 0;
        sc->light_blinks[i] = 0;
        sc->light_t[i] = 30 + (uint8_t)(rnd() % 200);   // each on its own clock from the start
    }
    if (electrical_state == ELEC_WAIT)
        return;
    // The lever's been pulled (you can't leave after it; a reload is a death's new run, so this is the room
    // being redrawn)
    room_map[room_row[lever->y] + lever->x] = MT_ELECTRICAL_LEVER_PULLED;
    open_gates();
    find_slide();
    while (!slide_step())
        ;
    if (electrical_state == ELEC_SEALED)
        set_mt(bdoor, MT_ELECTRICAL_COPPER_TOP, MT_ELECTRICAL_COPPER_BOTTOM);
    else if (elec_key_taken >= 2 || electrical_state >= ELEC_POWER)
        set_mt(bdoor, MT_ELECTRICAL_COPPER_OPEN_TOP, MT_ELECTRICAL_COPPER_OPEN_BOTTOM);
    if (electrical_state >= ELEC_POWER) {
        lift_gates(MT_ELECTRICAL_FLOOR);
        lift_open();
    }
}

void electrical_need_sprites(void) BANKED {
    figure_need_sprites();
    if (!elec_key_taken)
        spr_need(&key_sheet, BANK(sprite_key), &sprite_key);
}

void electrical_enter(void) BANKED {
    panel = 0;
    spr_clip_y = 255;
    if (electrical_state != ELEC_WAIT)
        return;
    // Figure waits behind the sliding door until the lever (Minecraft: upstairs)
    for (uint8_t i = 0; i < room_obj_count; i++)
        if (room_objs[i].type == OBJ_FIGURE)
            figure_start(room_objs[i].x, room_objs[i].y, 0);
    figure_nogo(safe->x, safe->y, safe->w, safe->h);
    music_play(MUSIC_NONE);
}

void electrical_exit(void) BANKED {
    figure_stop();
    panel = 0;
    hud_panel_set(0);
    spr_clip_y = 255;
}

// ------------------------------------------------------------------------------------------ the panel

static void row_clear(void) {
    memset(row, ' ', 20);
    row[20] = 0;
}

static void draw_display(void) {
    // "-- 7 ON --": the number shown and whether it's on; blank between passes (the font has no squares: its
    // [ ] are the Library's shapes, # * the health bar's)
    row_clear();
    memcpy(row + 4, "--        --", 12);
    if (!solved_t && show_i < FUSES) {
        uint8_t k = sc->order[show_i];   // switch k (0-9)
        if (k == 9) {
            row[7] = '1';
            row[8] = '0';
        } else {
            row[8] = '1' + k;
        }
        if (elec_answer & (1 << k))
            memcpy(row + 10, "ON", 2);
        else
            memcpy(row + 10, "OFF", 3);
    }
    text_print_win(0, 4, row);
}

static void draw_switches(void) {
    // ">10I" cells of 4: the cursor, the number (two places), I on or O off (a switch's own marks)
    for (uint8_t r = 0; r < 2; r++) {
        row_clear();
        for (uint8_t c = 0; c < 5; c++) {
            uint8_t k = r * 5 + c;
            char *p = row + c * 4;
            p[0] = k == elec_cursor ? '>' : ' ';
            if (k == 9) {
                p[1] = '1';
                p[2] = '0';
            } else {
                p[2] = '1' + k;
            }
            p[3] = (elec_switch & (1 << k)) ? 'I' : 'O';
        }
        text_print_win(0, 6 + r, row);
    }
}

static void draw_panel(void) {
    row_clear();
    memcpy(row + 1, "CIRCUIT BREAKER", 15);
    row[17] = '1' + elec_round;
    row[18] = '/';
    row[19] = '0' + BREAKER_ROUNDS;
    text_print_win(0, 2, row);
    row_clear();
    text_print_win(0, 3, row);
    draw_display();
    row_clear();
    text_print_win(0, 5, row);
    draw_switches();
    row_clear();
    memcpy(row + 1, "A: FLIP   B: LEAVE", 18);
    text_print_win(0, 8, row);
}

// A round's answer: each switch on or off, shown in a shuffled order
static void new_round(void) {
    elec_answer = rnd() & 0x03FF;
    for (uint8_t i = 0; i < FUSES; i++)
        sc->order[i] = i;
    for (uint8_t i = FUSES - 1; i; i--) {
        uint8_t j = (uint8_t)(rnd() % (i + 1)), t = sc->order[i];
        sc->order[i] = sc->order[j];
        sc->order[j] = t;
    }
    show_i = 0;
    show_t = show_frames[elec_round];
}

static void open_panel(void) {
    panel = 1;
    elec_cursor = 0;
    draw_panel();
    hud_panel_set(PANEL_ROWS);
}

static void close_panel(void) {
    panel = 0;
    hud_panel_set(0);
}

static void power_on(void) {
    electrical_state = ELEC_POWER;
    timer = FIGURE_AFTER;
    lift_gates(MT_ELECTRICAL_FLOOR);
    lift_open();
    set_mt(bdoor, MT_ELECTRICAL_COPPER_OPEN_TOP, MT_ELECTRICAL_COPPER_OPEN_BOTTOM);
    player_fast = 1;   // run (Seek's numbers, owner: you at 1.4 times walking, he at 0.75 of that)
    sfx_play(SFX_POWER);
    hud_message("THE POWER IS BACK.", MSG_FRAMES);
}

// The display's step: the number for show_frames, the next, a pause after the tenth, the same again
static void breaker_tick(void) {
    if (solved_t) {
        if (--solved_t == SOLVED_FRAMES / 2) {
            elec_switch = 0;   // off halfway (at the start it would look like the board refusing)
            if (panel)
                draw_switches();
        }
        if (!solved_t) {
            if (++elec_round == BREAKER_ROUNDS) {
                close_panel();
                power_on();
                return;
            }
            new_round();
            if (panel)
                draw_panel();
        }
        return;
    }
    if (--show_t)
        return;
    show_i++;
    if (show_i == FUSES) {
        show_t = SHOW_PAUSE;
    } else {
        if (show_i > FUSES)
            show_i = 0;
        show_t = show_frames[elec_round];
        if (panel)
            sfx_play((elec_answer & (1 << sc->order[show_i])) ? SFX_BREAKER_ON : SFX_BREAKER_OFF);
    }
    if (panel)
        draw_display();
}

uint8_t electrical_frame(void) BANKED {
    if (electrical_state == ELEC_SEALED)
        breaker_tick();
    if (!panel)
        return ELEC_FREE;
    uint8_t p = input_pressed;
    if (p & J_B) {
        close_panel();
    } else if ((p & J_A) && !solved_t) {
        elec_switch ^= 1 << elec_cursor;
        sfx_play(SFX_SWITCH);
        draw_switches();
        if (elec_switch == elec_answer) {   // all ten at once: nothing else is checked (Minecraft docs 10)
            solved_t = SOLVED_FRAMES;
            sfx_play(SFX_UNLOCK);
            draw_display();
        }
    } else if (p & (J_LEFT | J_RIGHT | J_UP | J_DOWN)) {
        if (p & J_LEFT)
            elec_cursor = elec_cursor % 5 ? elec_cursor - 1 : elec_cursor + 4;
        if (p & J_RIGHT)
            elec_cursor = elec_cursor % 5 == 4 ? elec_cursor - 4 : elec_cursor + 1;
        if (p & (J_UP | J_DOWN))
            elec_cursor = elec_cursor < 5 ? elec_cursor + 5 : elec_cursor - 5;
        draw_switches();
    }
    return ELEC_HELD;
}

// ------------------------------------------------------------------------------------------ using things

static void message_count(const char *what, uint8_t n) {
    // "<what> 7 OF 10."
    uint8_t len = (uint8_t)strlen(what);
    memcpy(row, what, len);
    char *p = row + len;
    if (n >= 10)
        *p++ = '1', *p++ = '0';
    else
        *p++ = '0' + n;
    memcpy(p, " OF 10.", 8);
    hud_message(row, MSG_FRAMES);
}

static void pull_lever(void) {
    room_set_metatile(lever->x, lever->y, MT_ELECTRICAL_LEVER_PULLED);
    sfx_play(SFX_LEVER);
    hud_message("SOMETHING'S COMING.", MSG_FRAMES);
    open_gates();
    find_slide();
    slide_step();
    electrical_state = ELEC_OPENING;
    timer = SLIDE_STEP;
    music_t = MUSIC_AFTER;
    // He comes at once (Minecraft docs 10: not when the door's finished), at where the lever was pulled
    figure_go(((int16_t)lever->x << 4) + 8, ((int16_t)(lever->y + 2) << 4) + 8);
}

static void use_breaker(void) {
    if (electrical_state == ELEC_SEALED) {
        open_panel();
        return;
    }
    if (electrical_state == ELEC_WAIT || electrical_state == ELEC_OPENING || electrical_state == ELEC_SEARCH) {
        if (!electrical_carried) {
            message_count("IT NEEDS FUSES. ", electrical_inserted);
            return;
        }
        electrical_inserted += electrical_carried;
        electrical_carried = 0;
        sfx_play(SFX_FUSE_IN);
        if (electrical_inserted < FUSES) {
            message_count("FUSES IN: ", electrical_inserted);
            return;
        }
        // The tenth: the search is over (Minecraft docs 10). He's gone, the breaker room shut behind you.
        electrical_state = ELEC_SEALED;
        figure_stop();
        set_mt(bdoor, MT_ELECTRICAL_COPPER_TOP, MT_ELECTRICAL_COPPER_BOTTOM);
        sfx_play(SFX_SLAM);
        hud_message("THE DOOR SHUTS.", MSG_FRAMES);
        seed(0xB4E4u ^ run_seconds);   // the breaker's answers (not replayed: nothing before them depends on them)
        elec_round = 0;
        elec_switch = 0;
        solved_t = 0;
        new_round();
    }
}

static void use_bdoor(void) {
    if (elec_key_taken == 2 || electrical_state >= ELEC_SEALED)
        return;   // open, or shut for good until the power
    if (!elec_key_taken) {
        hud_message("IT'S LOCKED.", MSG_FRAMES);
        sfx_play(SFX_LOCKED);
        return;
    }
    elec_key_taken = 2;   // the key's spent turning it (Minecraft docs 10)
    run_key_door = 0;
    set_mt(bdoor, MT_ELECTRICAL_COPPER_OPEN_TOP, MT_ELECTRICAL_COPPER_OPEN_BOTTOM);
    sfx_play(SFX_UNLOCK);
    hud_message("UNLOCKED.", MSG_FRAMES);
}

uint8_t electrical_use_at(uint8_t mx, uint8_t my) BANKED {
    if (key_obj && !elec_key_taken && inside(key_obj, mx, my)) {
        elec_key_taken = 1;
        run_key_door = 0xFF;   // the HUD's key; no door has this number
        sfx_play(SFX_KEY);
        hud_message("THE BREAKER KEY.", MSG_FRAMES);
        return 1;
    }
    if (inside(lever, mx, my)) {
        if (electrical_state == ELEC_WAIT)
            pull_lever();
        return 1;
    }
    if (inside(breaker, mx, my)) {
        use_breaker();
        return 1;
    }
    if (inside(bdoor, mx, my)) {
        use_bdoor();
        return 1;
    }
    if (inside(fdoor, mx, my))
        return 1;   // it won't open from this side
    for (uint8_t i = 0; i < FUSES; i++)
        if (sc->fuse_x[i] == mx && sc->fuse_y[i] == my && !(elec_taken & (1 << i))) {
            elec_taken |= 1 << i;
            electrical_carried++;
            room_set_metatile(mx, my, MT_ELECTRICAL_SHELF_BOTTOM);
            sfx_play(SFX_FUSE);
            message_count("A FUSE. ", electrical_carried + electrical_inserted);
            hud_show(MSG_FRAMES);
            return 1;
        }
    return 0;
}

// ------------------------------------------------------------------------------------------ each frame

// The pools of light: dead is the resting state; now and then two or three quick blinks, each pool on its
// own clock (Minecraft docs 10, "The warehouse's lights are already dead"). One pool changes a frame, so the
// tile writes stay a pool's worth.
static void light_pool(uint8_t i, uint8_t on) {
    const room_obj_t *o = light_obj[i];
    uint8_t mt = on ? MT_ELECTRICAL_FLOOR_LIT : MT_ELECTRICAL_FLOOR;
    for (uint8_t y = o->y; y < o->y + o->h; y++)
        for (uint8_t x = o->x; x < o->x + o->w; x++) {
            uint8_t m = room_map[room_row[y] + x];
            if (m == MT_ELECTRICAL_FLOOR || m == MT_ELECTRICAL_FLOOR_LIT)
                room_set_metatile(x, y, mt);
        }
    sc->light_on[i] = on;
}

static void lights_update(void) {
    uint8_t changed = 0;
    for (uint8_t i = 0; i < lights; i++) {
        if (sc->light_t[i] > 1) {
            sc->light_t[i]--;
            continue;
        }
        if (changed)
            continue;   // next frame
        changed = 1;
        if (sc->light_on[i]) {
            light_pool(i, 0);
            sc->light_t[i] = sc->light_blinks[i] ? 3 + (uint8_t)(rnd() & 7) : 120 + (uint8_t)(rnd() % 120);
        } else {
            if (!sc->light_blinks[i])
                sc->light_blinks[i] = 2 + (uint8_t)(rnd() & 1);
            sc->light_blinks[i]--;
            light_pool(i, 1);
            sc->light_t[i] = 3 + (uint8_t)(rnd() & 7);
        }
    }
}

uint8_t electrical_update(void) BANKED {
    spr_clip_y = panel ? HUD_PANEL_ROW_Y(PANEL_ROWS) : 255;
    lights_update();
    if (music_t && !--music_t)
        music_play(MUSIC_ELECTRICAL);
    switch (electrical_state) {
    case ELEC_OPENING:
        if (!--timer) {
            timer = SLIDE_STEP;
            uint8_t done = slide_step();
            sfx_play(SFX_SLIDE);
            // The way out of his cage grows each step: work his route out again (Minecraft docs 10), unless
            // he's heard you meanwhile and is on his way to that
            if (!figure_hunting || figure_state != FIG_WALK)
                figure_go(((int16_t)lever->x << 4) + 8, ((int16_t)(lever->y + 2) << 4) + 8);
            if (done)
                electrical_state = ELEC_SEARCH;
        }
        return figure_update();
    case ELEC_SEARCH:
        return figure_update();
    case ELEC_POWER:
    case ELEC_ESCAPE:
        if (electrical_state == ELEC_POWER && !--timer) {
            electrical_state = ELEC_ESCAPE;
            set_mt(fdoor, MT_ELECTRICAL_COPPER_OPEN_TOP, MT_ELECTRICAL_COPPER_OPEN_BOTTOM);
            sfx_play(SFX_DOOR);
            figure_escape(fdoor->x, fdoor->y + 2);
        }
        if (inside(lift, (uint8_t)((player_x + PLAYER_FEET_X) >> 4), (uint8_t)((player_y + PLAYER_FEET_Y) >> 4))) {
            electrical_state = ELEC_WON;
            return DEATH_NONE;
        }
        if (electrical_state == ELEC_ESCAPE && figure_update() != DEATH_NONE)
            return DEATH_ESCAPE;
        return DEATH_NONE;
    }
    return DEATH_NONE;
}

void electrical_draw(void) BANKED {
    // Not before the lever: behind the sliding door he's out of sight (the Minecraft version: upstairs), and
    // his edge arrow would give his cage away
    if ((electrical_state != ELEC_WAIT && electrical_state <= ELEC_SEARCH) || electrical_state == ELEC_ESCAPE)
        figure_draw();
    if (key_obj && !elec_key_taken)
        spr_draw(&key_sheet, SPR_KEY_KEY, ((int16_t)key_obj->x << 4) - cam_x, ((int16_t)key_obj->y << 4) - cam_y, 0);
}

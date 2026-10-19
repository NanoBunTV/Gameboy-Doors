#pragma bank 255
#include <gb/gb.h>
#include <string.h>
#include "library.h"
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
#include "sprite_marks.h"
#include "sprite_figure.h"
#include "sprite_player.h"
#include "tileset_hotel.h"

// The puzzle is three pieces and none of them is the answer (Minecraft docs 07): eight books each say one
// shape's digit, the paper says which shape goes on which wheel, and only the two together open the
// padlock. Nothing rolls a code: the run draws eight books (a shape each, all different; a digit each,
// repeats allowed) and five of them for the paper. Finding six books leaves two wheels to guess.
//
// Books can be on any bookcase metatile (MTF_BOOKCASE) with floor below it, where the player stands to read
// one; a shelf with an unread book in it glints once you're near (owner: about 3 metatiles). An empty shelf
// says nothing. The paper is the desk's 'paper' object. What's been read is kept for the run and shown on
// the notes (Start: a panel over the bottom of the screen; the game doesn't pause, owner's choice).

#define GLINT_RANGE  3          // metatiles
#define GLINT_MAX    2          // drawn at once (sprites per line: Figure 4, the player 2, the arrow 1)
#define DEAF_FRAMES  180        // Figure can't hear you for the first 3 s (Minecraft figure-grace-seconds)
#define HINT_FRAMES  120        // each of its two lines (hint_t counts to 2 * HINT_FRAMES + 1, a byte)
#define PANEL_ROWS   7          // window rows 2-8

// The Library's sprite VRAM (library_need_sprites): nothing from the corridor comes in, so only these
SPR_CHECK(SPR_PLAYER_TILES + SPR_MARKS_TILES + SPR_FIGURE_TILES <= SPR_TILE_LIMIT(TILESET_HOTEL_TILES),
          library_sprites_outgrow_sprite_vram);
SPR_CHECK(SPR_PLAYER_PALETTES + SPR_MARKS_PALETTES + SPR_FIGURE_PALETTES <= SPR_PALETTE_LIMIT,
          library_sprites_need_over_8_gbc_palettes);
SPR_CHECK((SPR_MARKS_FRAME_COUNT + SPR_FIGURE_FRAME_COUNT) * 4 <= SPR_FRAME_POOL, library_frame_tables_outgrow_the_pool);

#define PANEL_NONE  0
#define PANEL_NOTES 1
#define PANEL_LOCK  2

// The shapes' characters in the font (assets/fonts/font.json): circle, square, heart, star, X, cross,
// triangle, diamond
static const char shape_char[LIB_BOOKS] = { '%', '[', '&', '$', '(', ')', '^', ']' };

uint8_t library_left;
static uint8_t planned, hint_t;
// Not static: scripts/library-test.py reads them
uint8_t lib_book_x[LIB_BOOKS], lib_book_y[LIB_BOOKS], lib_book_shape[LIB_BOOKS], lib_book_digit[LIB_BOOKS];
uint8_t lib_paper[LIB_CODE];   // the book whose shape goes on each wheel
static uint8_t read_books, paper_read;
static uint8_t panel, wheel[LIB_CODE], cursor, glint_t;
static uint16_t rng;
static char row[21];
static char msg[21];

static uint16_t rnd(void) {
    rng ^= rng << 7;
    rng ^= rng >> 9;
    rng ^= rng << 8;
    return rng;
}

void library_reset(void) BANKED {
    planned = 0;
    library_left = 0;
    read_books = paper_read = 0;
    hint_t = 0;
    panel = PANEL_NONE;
    spr_clip_y = 255;
}

static uint8_t candidate(uint8_t x, uint8_t y) {
    uint16_t c = room_row[y] + x;
    return (mt_flags[room_map[c]] & MTF_BOOKCASE) && y + 1 < room_h && !(mt_flags[room_map[c + room_w]] & MTF_SOLID);
}

static void shuffle(uint8_t *a) {
    for (uint8_t i = 0; i < LIB_BOOKS; i++)
        a[i] = i;
    for (uint8_t i = LIB_BOOKS - 1; i; i--) {
        uint8_t j = (uint8_t)(rnd() % (i + 1)), t = a[i];
        a[i] = a[j];
        a[j] = t;
    }
}

// From the seed alone (and the room's map), so a replay finds the same books
static void plan_books(void) {
    rng = run_seed ^ 0xB00Cu;
    if (!rng)
        rng = 1;
    for (uint8_t i = 0; i < 4; i++)
        rnd();
    uint8_t count = 0;
    for (uint8_t y = 0; y < room_h; y++)
        for (uint8_t x = 0; x < room_w; x++)
            if (candidate(x, y))
                count++;   // 8-255 (scripts/assets.py)
    uint8_t pick[LIB_BOOKS];
    for (uint8_t i = 0; i < LIB_BOOKS; i++) {
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
            if (candidate(x, y)) {
                for (uint8_t i = 0; i < LIB_BOOKS; i++)
                    if (pick[i] == n) {
                        lib_book_x[i] = x;
                        lib_book_y[i] = y;
                    }
                n++;
            }
    // Shapes without repeats (two books with one shape would make the code ambiguous); digits may repeat
    shuffle(lib_book_shape);
    for (uint8_t i = 0; i < LIB_BOOKS; i++)
        lib_book_digit[i] = (uint8_t)(rnd() % 10);
    uint8_t order[LIB_BOOKS];
    shuffle(order);
    memcpy(lib_paper, order, LIB_CODE);
}

void library_need_sprites(void) BANKED {
    figure_need_sprites();
}

void library_enter(void) BANKED {
    if (!planned) {
        plan_books();
        planned = 1;
    }
    panel = PANEL_NONE;
    spr_clip_y = 255;
    if (library_left)
        return;   // you've been through: he and the music are gone
    for (uint8_t i = 0; i < room_obj_count; i++)
        if (room_objs[i].type == OBJ_FIGURE)
            figure_start(room_objs[i].x, room_objs[i].y, hint_t ? 0 : DEAF_FRAMES);
    music_play(MUSIC_LIBRARY);
    if (!hint_t)
        hint_t = 1;   // the hint starts on the first frame (the only tutorial: Minecraft docs 07)
}

void library_exit(void) BANKED {
    figure_stop();
    library_left = 1;
    panel = PANEL_NONE;
    spr_clip_y = 255;
    music_play(MUSIC_NONE);
}

// ------------------------------------------------------------------------------------------ the panels

static void row_clear(void) {
    memset(row, ' ', 20);
    row[20] = 0;
}

static void row_print(uint8_t y) {
    text_print_win(0, y, row);
}

static void eq(char *p, uint8_t book) {
    p[0] = shape_char[lib_book_shape[book]];
    p[1] = '=';
    p[2] = '0' + lib_book_digit[book];
}

static void draw_notes(void) {
    row_clear();
    memcpy(row + 1, "PAPER", 5);
    if (!paper_read)
        memcpy(row + 8, "NOT FOUND", 9);
    row_print(2);
    row_clear();
    if (paper_read)
        for (uint8_t k = 0; k < LIB_CODE; k++) {   // "1=^ 2=( 3=& 4=$ 5=%"
            row[1 + k * 4] = '1' + k;
            row[2 + k * 4] = '=';
            row[3 + k * 4] = shape_char[lib_book_shape[lib_paper[k]]];
        }
    row_print(3);
    row_clear();
    row_print(4);
    uint8_t n = 0;
    for (uint8_t b = 0; b < LIB_BOOKS; b++)
        if (read_books & (1 << b))
            n++;
    memcpy(row + 1, "BOOKS  0 OF 8", 13);
    row[8] = '0' + n;
    row_print(5);
    // Up to four a row, in the order they're in the room
    uint8_t k = 0;
    for (uint8_t y = 6; y <= 7; y++) {
        row_clear();
        for (uint8_t col = 0; col < 4 && k < LIB_BOOKS; k++)
            if (read_books & (1 << k)) {
                eq(row + 1 + col * 5, k);
                col++;
            }
        row_print(y);
    }
    row_clear();
    memcpy(row + 1, "START: CLOSE", 12);
    row_print(8);
}

static void draw_wheels(void) {
    row_clear();
    row[5 + cursor * 2] = '^';
    row_print(4);
    row_clear();
    for (uint8_t k = 0; k < LIB_CODE; k++)
        row[5 + k * 2] = '0' + wheel[k];
    row_print(5);
}

static void draw_lock(void) {
    row_clear();
    memcpy(row + 6, "PADLOCK", 7);
    row_print(2);
    row_clear();
    row_print(3);
    draw_wheels();
    row_clear();
    row_print(6);
    memcpy(row + 1, "UP/DOWN: TURN", 13);
    row_print(7);
    row_clear();
    memcpy(row + 1, "A: TRY   B: LEAVE", 17);
    row_print(8);
}

static void open_panel(uint8_t which) {
    panel = which;
    if (which == PANEL_LOCK)
        draw_lock();
    else
        draw_notes();
    hud_panel_set(PANEL_ROWS);
}

static void close_panel(void) {
    panel = PANEL_NONE;
    hud_panel_set(0);
}

void library_open_lock(void) BANKED {
    cursor = 0;
    open_panel(PANEL_LOCK);
}

uint8_t library_frame(void) BANKED {
    if (panel == PANEL_LOCK) {
        uint8_t p = input_pressed;
        if (p & J_B) {
            close_panel();
        } else if (p & J_A) {
            uint8_t right = 1;
            for (uint8_t k = 0; k < LIB_CODE; k++)
                if (wheel[k] != lib_book_digit[lib_paper[k]])
                    right = 0;
            if (right) {
                close_panel();
                return LIB_SOLVED;
            }
            // Wrong: all five back to 0, and nothing about which was wrong (Minecraft docs 07)
            memset(wheel, 0, sizeof(wheel));
            draw_wheels();
            sfx_play(SFX_LOCKED);
            hud_message("WRONG CODE.", 90);
        } else if (p & (J_LEFT | J_RIGHT | J_UP | J_DOWN)) {
            if (p & J_LEFT)
                cursor = cursor ? cursor - 1 : LIB_CODE - 1;
            if (p & J_RIGHT)
                cursor = cursor == LIB_CODE - 1 ? 0 : cursor + 1;
            if (p & J_UP)
                wheel[cursor] = wheel[cursor] == 9 ? 0 : wheel[cursor] + 1;
            if (p & J_DOWN)
                wheel[cursor] = wheel[cursor] ? wheel[cursor] - 1 : 9;
            if (p & (J_UP | J_DOWN))
                sfx_play(SFX_WHEEL);
            draw_wheels();
        }
        return LIB_HELD;
    }
    if (input_pressed & J_START) {
        if (panel == PANEL_NOTES)
            close_panel();
        else
            open_panel(PANEL_NOTES);
    }
    return LIB_FREE;
}

// ------------------------------------------------------------------------------------------ reading

static uint8_t book_at(uint8_t mx, uint8_t my) {
    for (uint8_t b = 0; b < LIB_BOOKS; b++)
        if (lib_book_x[b] == mx && lib_book_y[b] == my)
            return b;
    return 0xFF;
}

uint8_t library_use_at(uint8_t mx, uint8_t my) BANKED {
    uint8_t b = book_at(mx, my);
    if (b == 0xFF)
        return 0;   // an empty shelf says nothing (Minecraft docs 07, "Clicking things")
    read_books |= 1 << b;
    sfx_play(SFX_BOOK);
    memcpy(msg, "A BOOK SAYS: ", 13);
    eq(msg + 13, b);
    msg[16] = 0;
    hud_message(msg, 150);
    if (panel == PANEL_NOTES)
        draw_notes();
    return 1;
}

void library_read_paper(void) BANKED {
    paper_read = 1;
    sfx_play(SFX_BOOK);
    memset(msg, ' ', 20);
    for (uint8_t k = 0; k < LIB_CODE; k++) {
        msg[k * 4] = '1' + k;
        msg[1 + k * 4] = '=';
        msg[2 + k * 4] = shape_char[lib_book_shape[lib_paper[k]]];
    }
    msg[19] = 0;
    hud_message(msg, 180);
    if (panel == PANEL_NOTES)
        draw_notes();
}

// ------------------------------------------------------------------------------------------ each frame

uint8_t library_update(void) BANKED {
    spr_clip_y = panel ? HUD_PANEL_ROW_Y(PANEL_ROWS) : 255;
    glint_t++;
    if (hint_t && hint_t < 2 * HINT_FRAMES + 1) {
        if (hint_t == 1)
            hud_message("IT CAN'T SEE YOU.", HINT_FRAMES);
        else if (hint_t == HINT_FRAMES + 1)
            hud_message("HOLD B TO SNEAK.", HINT_FRAMES);
        hint_t++;
    }
    if (library_left)
        return DEATH_NONE;
    return figure_update();
}

void library_draw(void) BANKED {
    figure_draw();
    // The glint: an unread book's shelf near the player, twinkling
    if ((glint_t & 0x30) == 0x30)
        return;
    uint8_t px = (uint8_t)((player_x + PLAYER_FEET_X) >> 4), py = (uint8_t)((player_y + PLAYER_FEET_Y) >> 4);
    uint8_t drawn = 0;
    for (uint8_t b = 0; b < LIB_BOOKS && drawn < GLINT_MAX; b++) {
        if (read_books & (1 << b))
            continue;
        uint8_t dx = lib_book_x[b] > px ? lib_book_x[b] - px : px - lib_book_x[b];
        uint8_t dy = lib_book_y[b] > py ? lib_book_y[b] - py : py - lib_book_y[b];
        if (dx > GLINT_RANGE || dy > GLINT_RANGE)
            continue;
        spr_draw(&figure_marks, SPR_MARKS_GLINT, ((int16_t)lib_book_x[b] << 4) + 4 - cam_x,
                 ((int16_t)lib_book_y[b] << 4) + 2 - cam_y, 0);
        drawn++;
    }
}

#pragma bank 255
#include <gb/gb.h>
#include <string.h>
#include "lift.h"
#include "items.h"
#include "save.h"
#include "game.h"
#include "hud.h"
#include "bg.h"
#include "image.h"
#include "room.h"
#include "input.h"
#include "audio.h"
#include "sprites.h"
#include "platform.h"
#include "font.h"
#include "image_lift.h"

// Elevator Jam's notes end after this many frames (scripts: 304 rows of 6 and 7 ticks); the doors open then
#define SONG_FRAMES  1954
#define OPEN_FRAMES  45      // the doors open, then the fade
#define SAY_FRAMES   60
#define PICTURE_ROWS 9       // the picture's top half (shut doors), its bottom half the same open
#define PANEL_Y      9
#define OFFER_Y      11      // four offer rows, then BUY two rows under
#define BUY_Y        16
#define OFFERS       4

uint8_t lift_bought;

// Minecraft config pregame-shop.offers (owner: those four)
static const uint8_t offer_bit[OFFERS] = { LIFT_LOCKPICKS, LIFT_LIGHTER, LIFT_FLASHLIGHT, LIFT_VITAMINS };
static const uint8_t offer_price[OFFERS] = { 50, 50, 100, 100 };
static const char offer_icon[OFFERS] = { ICON_LOCKPICK, ICON_LIGHTER, ICON_FLASHLIGHT, ICON_VITAMINS };
static const char *const offer_name[OFFERS] = { "LOCKPICKS X2", "LIGHTER", "FLASHLIGHT", "VITAMINS X3" };

uint8_t lift_picked, lift_cursor;   // LIFT_* bits picked; the row the cursor is on (tests read them)
static uint8_t say_t;

static uint16_t total(void) {
    uint16_t t = 0;
    for (uint8_t i = 0; i < OFFERS; i++)
        if (lift_picked & offer_bit[i])
            t += offer_price[i];
    return t;
}

static void row_blank(char *row) {
    memset(row, ' ', 20);
    row[20] = 0;
}

// The knob and a number right-aligned to column 19
static void knobs_right(char *row, uint16_t v) {
    save_num(row + 19, v);
    uint8_t k = 19;
    while (row[k] != ' ')
        k--;
    row[k] = KNOB_CHAR;
}

static void draw_panel(void) {
    char row[21];
    row_blank(row);
    memcpy(row + 1, "PRE-RUN SHOP", 12);
    knobs_right(row, save_knobs);
    text_print_bg(0, PANEL_Y, row);
    for (uint8_t i = 0; i < OFFERS; i++) {
        row_blank(row);
        row[0] = lift_cursor == i ? '>' : ' ';
        row[1] = (lift_picked & offer_bit[i]) ? KNOB_CHAR : ' ';
        row[2] = offer_icon[i];
        const char *n = offer_name[i];
        for (uint8_t k = 0; n[k]; k++)
            row[4 + k] = n[k];
        save_num(row + 19, offer_price[i]);
        text_print_bg(0, OFFER_Y + i, row);
    }
    row_blank(row);
    row[0] = lift_cursor == OFFERS ? '>' : ' ';
    memcpy(row + 2, "BUY", 3);
    knobs_right(row, total());
    text_print_bg(0, BUY_Y, row);
    row_blank(row);
    memcpy(row + 1, "START: LEAVE", 12);
    text_print_bg(0, BUY_Y + 1, row);
}

static void say(const char *s) {
    char row[21];
    row_blank(row);
    for (uint8_t k = 0; s[k]; k++)
        row[1 + k] = s[k];
    text_print_bg(0, PANEL_Y + 1, row);
    say_t = SAY_FRAMES;
}

void lift_run(void) BANKED {
    game_state = GAME_LIFT;
    if (platform_fade != FADE_BLACK)
        game_fade(platform_fade, FADE_BLACK);
    platform_set_dim(0, TINT_NONE);
    hud_hide_now();
    spr_begin();
    spr_end();
    hud_icons(1);   // the offers' icons (a Library death leaves its shapes in)
    screen_fill(FONT_FIRST_TILE, PAL_TEXT);   // the font's space
    image_show_rows(BANK(image_lift), &image_lift, 0, 0, 0, PICTURE_ROWS);
    bg_set_scroll(0, 0);
    lift_picked = 0;
    lift_cursor = 0;
    say_t = 0;
    lift_bought = 0;
    draw_panel();
    music_play(MUSIC_ELEVATOR);
    game_fade(FADE_BLACK, 0);

    uint16_t t = 0;
    while (1) {
        game_wait_frames(1);
        input_update();
        if (++t >= SONG_FRAMES)
            break;   // the song is over: the doors open, nothing bought that wasn't confirmed (owner)
        if (say_t && !--say_t)
            say("");
        if (input_pressed & J_START)
            break;
        if (input_pressed & (J_UP | J_DOWN)) {
            lift_cursor = (input_pressed & J_UP) ? (lift_cursor ? lift_cursor - 1 : OFFERS) : (lift_cursor == OFFERS ? 0 : lift_cursor + 1);
            sfx_play(SFX_SELECT);
            draw_panel();
        } else if (input_pressed & J_A) {
            if (lift_cursor < OFFERS) {
                uint8_t bit = offer_bit[lift_cursor];
                if (!(lift_picked & bit) && total() + offer_price[lift_cursor] > save_knobs) {
                    say("NOT ENOUGH KNOBS.");
                    sfx_play(SFX_LOCKED);
                } else {
                    lift_picked ^= bit;
                    sfx_play(SFX_SELECT);
                    draw_panel();
                }
            } else if (lift_picked) {
                // Nothing is bought until here (Minecraft: one confirm buys it all)
                save_spend(total());
                lift_bought = lift_picked;
                sfx_play(SFX_GOLD);
                break;
            } else {
                break;   // BUY with nothing lift_picked: off you go
            }
        }
    }
    // The doors open on the reception
    image_show_rows(BANK(image_lift), &image_lift, 0, 0, PICTURE_ROWS, PICTURE_ROWS);
    sfx_play(SFX_DOOR);
    game_wait_frames(OPEN_FRAMES);
    game_fade(0, FADE_BLACK);
    music_play(MUSIC_NONE);
}

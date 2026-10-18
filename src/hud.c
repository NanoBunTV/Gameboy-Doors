#include <gb/gb.h>
#include <string.h>
#include "hud.h"
#include "bg.h"
#include "room.h"
#include "run.h"
#include "input.h"
#include "platform.h"
#include "font.h"

#define WIN_MAP 0x9C00u
#define HUD_ROWS 2
#define HUD_Y_SHOWN (144 - HUD_ROWS * 8)
#define HUD_Y_HIDDEN 144
#define SLIDE_SPEED 2

static uint8_t ascii[96];          // WRAM copy of the font's ASCII table
static char shown[21];             // what the text row currently holds (or is queued to)
static char want[21];
static uint8_t timer;              // frames left to show
static uint8_t msg_timer;          // frames left of the message (0 = show the status line)

static uint8_t tile_for(char c) {
    uint8_t code = (uint8_t)c;
    return (code >= 32 && code < 128) ? ascii[code - 32] : ascii[0];
}

void hud_init(void) {
    uint8_t saved = CURRENT_BANK;
    SWITCH_ROM(BANK(font));
    set_bkg_data(FONT_FIRST_TILE, font.tile_count, font.tiles);
    memcpy(ascii, font.ascii, sizeof(ascii));
#ifdef CGB
    platform_set_bkg_palettes(PAL_TEXT, font.palette, 1);
#endif
    SWITCH_ROM(saved);

    // Window map: row 0 a border line, row 1 the text
    uint8_t row[20];
    memset(row, tile_for('='), 20);
    set_win_tiles(0, 0, 20, 1, row);
    memset(row, tile_for(' '), 20);
    set_win_tiles(0, 1, 20, 1, row);
#ifdef CGB
    VBK_REG = VBK_ATTRIBUTES;
    memset(row, PAL_TEXT, 20);
    set_win_tiles(0, 0, 20, 1, row);
    set_win_tiles(0, 1, 20, 1, row);
    VBK_REG = VBK_TILES;
#endif
    memset(shown, ' ', 20);
    shown[20] = 0;
    WX_REG = 7;
    bg_wy = HUD_Y_HIDDEN;
    WY_REG = HUD_Y_HIDDEN;
}

void hud_show(uint8_t frames) {
    if (timer < frames)
        timer = frames;
}

void hud_message(const char *text, uint8_t frames) {
    uint8_t n = 0;
    while (n < 20 && text[n])
        n++;
    memset(want, ' ', 20);
    memcpy(want + ((20 - n) >> 1), text, n);  // centred
    msg_timer = frames;
    hud_show(frames);
}

void hud_hide_now(void) {
    timer = msg_timer = 0;
    bg_wy = HUD_Y_HIDDEN;
}

void text_3digits(char *buf, uint8_t n) {
    buf[0] = '0' + n / 100;
    buf[1] = '0' + (n / 10) % 10;
    buf[2] = '0' + n % 10;
}

// " DOOR 012       ~ " : the room you're in, and the key if you carry one
static void status_line(void) {
    memset(want, ' ', 20);
    memcpy(want + 1, "DOOR", 4);
    text_3digits(want + 6, run_room);
    if (run_key_door)
        want[18] = '~';
}

void hud_update(void) {
    if (input_held & J_SELECT)
        hud_show(30);
    if (msg_timer)
        msg_timer--;
    else
        status_line();
    if (timer)
        timer--;

    // Queue the characters that changed (the text row is window row 1)
    for (uint8_t i = 0; i < 20; i++)
        if (shown[i] != want[i]) {
            shown[i] = want[i];
            bg_queue_tile(WIN_MAP + 32 + i, tile_for(want[i]), PAL_TEXT);
        }

    uint8_t target = timer ? HUD_Y_SHOWN : HUD_Y_HIDDEN;
    if (bg_wy < target)
        bg_wy = (target - bg_wy > SLIDE_SPEED) ? bg_wy + SLIDE_SPEED : target;
    else if (bg_wy > target)
        bg_wy = (bg_wy - target > SLIDE_SPEED) ? bg_wy - SLIDE_SPEED : target;
}

void text_queue(uint16_t addr, const char *text) {
    while (*text)
        bg_queue_tile(addr++, tile_for(*text++), PAL_TEXT);
}

void text_print_bg(uint8_t x, uint8_t y, const char *text) {
    uint8_t row[20];
    uint8_t n = 0;
    while (text[n] && x + n < 20) {
        row[n] = tile_for(text[n]);
        n++;
    }
    set_bkg_tiles(x, y, n, 1, row);
#ifdef CGB
    VBK_REG = VBK_ATTRIBUTES;
    memset(row, PAL_TEXT, n);
    set_bkg_tiles(x, y, n, 1, row);
    VBK_REG = VBK_TILES;
#endif
}

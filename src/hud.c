#include <gb/gb.h>
#include <string.h>
#include "hud.h"
#include "hud_state.h"
#include "bg.h"
#include "room.h"
#include "platform.h"
#include "font.h"

// Bank 0: what switches banks to read the font, and what reads text passed in from the caller's bank (a
// string literal lives in the caller's bank, which is only mapped while bank 0 code runs). The rest of the
// HUD is in hud_status.c.

uint8_t hud_ascii[96];
char hud_shown[21];
char hud_want[21];
uint8_t hud_timer, hud_msg_timer, hud_msg_new;

uint8_t hud_tile_for(char c) {
    uint8_t code = (uint8_t)c;
    return (code >= 32 && code < 128) ? hud_ascii[code - 32] : hud_ascii[0];
}

void hud_init(void) {
    uint8_t saved = CURRENT_BANK;
    SWITCH_ROM(BANK(font));
    set_bkg_data(FONT_FIRST_TILE, font.tile_count, font.tiles);
    memcpy(hud_ascii, font.ascii, sizeof(hud_ascii));
#ifdef CGB
    platform_set_bkg_palettes(PAL_TEXT, font.palette, 1);
#endif
    SWITCH_ROM(saved);

    // Window map: row 0 a border line, row 1 the text
    uint8_t row[20];
    memset(row, hud_tile_for('='), 20);
    set_win_tiles(0, 0, 20, 1, row);
    memset(row, hud_tile_for(' '), 20);
    set_win_tiles(0, 1, 20, 1, row);
#ifdef CGB
    VBK_REG = VBK_ATTRIBUTES;
    memset(row, PAL_TEXT, 20);
    set_win_tiles(0, 0, 20, 1, row);
    set_win_tiles(0, 1, 20, 1, row);
    VBK_REG = VBK_TILES;
#endif
    memset(hud_shown, ' ', 20);
    hud_shown[20] = 0;
    WX_REG = 7;
    bg_wy = HUD_Y_HIDDEN;
    WY_REG = HUD_Y_HIDDEN;
}

void hud_message(const char *text, uint8_t frames) {
    uint8_t n = 0;
    while (n < 20 && text[n])
        n++;
    memset(hud_want, ' ', 20);
    memcpy(hud_want + ((20 - n) >> 1), text, n);  // centred
    hud_msg_timer = frames;
    hud_msg_new = 1;
    hud_show(frames);
}

void text_queue(uint16_t addr, const char *text) {
    while (*text)
        bg_queue_tile(addr++, hud_tile_for(*text++), PAL_TEXT);
}

void text_print_bg(uint8_t x, uint8_t y, const char *text) {
    uint8_t row[20];
    uint8_t n = 0;
    while (text[n] && x + n < 20) {
        row[n] = hud_tile_for(text[n]);
        n++;
    }
    uint16_t addr = 0x9800u + ((uint16_t)y << 5) + x;
    vram_write(addr, row, n);
#ifdef CGB
    VBK_REG = VBK_ATTRIBUTES;
    memset(row, PAL_TEXT, n);
    vram_write(addr, row, n);
    VBK_REG = VBK_TILES;
#endif
}

void hud_font_palette(void) {
#ifdef CGB
    uint8_t saved = CURRENT_BANK;
    SWITCH_ROM(BANK(font));
    platform_set_bkg_palettes(PAL_TEXT, font.palette, 1);
    SWITCH_ROM(saved);
#endif
}

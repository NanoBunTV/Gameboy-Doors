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

char hud_shown[21];
char hud_want[21];
uint8_t hud_timer, hud_msg_timer, hud_msg_new;
uint8_t hud_panel;

// Read from the font's ROM table (a WRAM copy cost 96 bytes of the GBC's stack until Phase 8)
uint8_t hud_tile_for(char c) {
    uint8_t code = (uint8_t)c;
    uint8_t saved = CURRENT_BANK;
    SWITCH_ROM(BANK(font));
    uint8_t t = font.ascii[(code >= 32 && code < 128) ? code - 32 : 0];
    SWITCH_ROM(saved);
    return t;
}

void hud_init(void) {
    uint8_t saved = CURRENT_BANK;
    SWITCH_ROM(BANK(font));
    set_bkg_data(FONT_FIRST_TILE, font.tile_count, font.tiles);
#ifdef CGB
    platform_set_bkg_palettes(PAL_TEXT, font.palette, 1);
#endif
    SWITCH_ROM(saved);

    // Window map: row 0 a border line, row 1 the text, rows 2 and down the Library's panel (hud_panel_set)
    uint8_t row[20];
    memset(row, hud_tile_for('_'), 20);
    set_win_tiles(0, 0, 20, 1, row);
    memset(row, hud_tile_for(' '), 20);
    for (uint8_t y = 1; y < HUD_ROWS + HUD_PANEL_MAX; y++)
        set_win_tiles(0, y, 20, 1, row);
#ifdef CGB
    VBK_REG = VBK_ATTRIBUTES;
    memset(row, PAL_TEXT, 20);
    for (uint8_t y = 0; y < HUD_ROWS + HUD_PANEL_MAX; y++)
        set_win_tiles(0, y, 20, 1, row);
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

static void print_at(uint16_t map, uint8_t x, uint8_t y, const char *text) {
    uint8_t row[20];
    uint8_t n = 0;
    while (text[n] && x + n < 20) {
        row[n] = hud_tile_for(text[n]);
        n++;
    }
    uint16_t addr = map + ((uint16_t)y << 5) + x;
    vram_write(addr, row, n);
#ifdef CGB
    VBK_REG = VBK_ATTRIBUTES;
    memset(row, PAL_TEXT, n);
    vram_write(addr, row, n);
    VBK_REG = VBK_TILES;
#endif
}

void text_print_bg(uint8_t x, uint8_t y, const char *text) {
    print_at(0x9800u, x, y, text);
}

void text_print_win(uint8_t x, uint8_t y, const char *text) {
    print_at(WIN_MAP, x, y, text);
}

void hud_font_palette(void) {
#ifdef CGB
    uint8_t saved = CURRENT_BANK;
    SWITCH_ROM(BANK(font));
    platform_set_bkg_palettes(PAL_TEXT, font.palette, 1);
    SWITCH_ROM(saved);
#endif
}

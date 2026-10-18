#include <gb/gb.h>
#include "title.h"
#include "hud.h"
#include "bg.h"
#include "room.h"
#include "input.h"
#include "game.h"
#include "platform.h"
#include "font.h"
#include "image_logo.h"

#define BLINK_FRAMES 32

uint16_t title_run(void) {
    game_state = GAME_TITLE;
    DISPLAY_OFF;   // waits for VBlank
    fill_bkg_rect(0, 0, 32, 32, FONT_FIRST_TILE);   // the font's first character is the space
#ifdef CGB
    VBK_REG = VBK_ATTRIBUTES;
    fill_bkg_rect(0, 0, 32, 32, PAL_TEXT);
    VBK_REG = VBK_TILES;
#endif
    // The logo uses BG tiles from 0, over the room tileset, which is reloaded for the first room
    uint8_t saved = CURRENT_BANK;
    SWITCH_ROM(BANK(image_logo));
    set_bkg_data(0, image_logo.tile_count, image_logo.tiles);
    set_bkg_tiles(1, 3, image_logo.width, image_logo.height, image_logo.map);
#ifdef CGB
    VBK_REG = VBK_ATTRIBUTES;
    set_bkg_tiles(1, 3, image_logo.width, image_logo.height, image_logo.attrs);
    VBK_REG = VBK_TILES;
    platform_set_bkg_palettes(0, image_logo.palettes, image_logo.palette_count);
#endif
    SWITCH_ROM(saved);
    room_forget_tileset();

    text_print_bg(2, 10, "HOTEL - FLOOR 1");
    text_print_bg(4, 13, "PRESS START");
    text_print_bg(17, 17, platform_name);
    bg_set_scroll(0, 0);
    platform_set_fade(0);
    platform_vblank();
    DISPLAY_ON;

    // The seed: how long the title was shown, mixed with the divider register at the moment Start is
    // pressed. A Game Boy powers on the same way every time, so a person's timing is the only randomness.
    uint16_t frames = 0;
    while (1) {
        game_wait_frames(1);
        input_update();
        frames++;
        if ((frames & (BLINK_FRAMES - 1)) == 0)
            text_queue(0x9800u + 13 * 32 + 4, (frames & BLINK_FRAMES) ? "           " : "PRESS START");
        if (input_pressed & J_START)
            break;
    }
#ifdef AUTOTEST
    (void)frames;
    return 0xC0DE;   // tests replay a fixed run
#else
    return frames ^ ((uint16_t)DIV_REG << 8) ^ DIV_REG;
#endif
}

#pragma bank 255
#include <gb/gb.h>
#include <string.h>
#include "title.h"
#include "hud.h"
#include "bg.h"
#include "room.h"
#include "image.h"
#include "input.h"
#include "game.h"
#include "audio.h"
#include "platform.h"
#include "font.h"
#include "image_logo.h"
#include "save.h"

#define BLINK_FRAMES 32
#define ERASE_HOLD   180   // Select + B held this long asks to erase the save (owner, Phase 9b)

// The Knobs balance in the bottom-left corner: the knob glyph and the number
static void show_knobs(void) {
    char row[7];
    memset(row, ' ', 6);
    row[6] = 0;
    save_num(row + 5, save_knobs);
    uint8_t k = 0;
    while (row[k] == ' ')
        k++;
    memmove(row + 1, row + k, 6 - k);   // "|95   ": the number just after the knob
    memset(row + 7 - k, ' ', k - 1);
    row[0] = KNOB_CHAR;
    text_print_bg(1, 17, row);
}

// "ERASE SAVE?" over PRESS START; A erases, B keeps it
static void erase_prompt(void) {
    text_print_bg(4, 13, "ERASE SAVE? ");
    text_print_bg(4, 14, "A=YES  B=NO");
    sfx_play(SFX_SELECT);
    while (1) {
        game_wait_frames(1);
        input_update();
        if (input_pressed & J_A) {
            save_erase();
            sfx_play(SFX_UNLOCK);
            break;
        }
        if (input_pressed & J_B)
            break;
    }
    text_print_bg(4, 14, "           ");
    show_knobs();
}

uint16_t title_run(void) BANKED {
    game_state = GAME_TITLE;
    // At boot the LCD is still off; coming back from a run it's on, so draw behind black instead (turning
    // the LCD off would flash white on a DMG)
    uint8_t lcd_on = LCDC_REG & LCDCF_ON;
    if (lcd_on && platform_fade != FADE_BLACK)
        game_fade(platform_fade, FADE_BLACK);
    platform_set_dim(0, TINT_NONE);
    hud_hide_now();
    screen_fill(FONT_FIRST_TILE, PAL_TEXT);   // the font's first character is the space
    // The logo uses BG tiles from 0, over the room tileset, which is reloaded for the first room
    image_show(BANK(image_logo), &image_logo, 1, 3);
    text_print_bg(2, 10, "HOTEL - FLOOR 1");
    text_print_bg(4, 13, "PRESS START");
    text_print_bg(17, 17, platform_name);
    show_knobs();
    bg_set_scroll(0, 0);
    music_play(MUSIC_TITLE);
    if (lcd_on) {
        game_fade(FADE_BLACK, 0);
    } else {
        platform_set_fade(0);
        platform_vblank();
        DISPLAY_ON;
    }

    // The seed: how long the title was shown, mixed with the divider register at the moment Start is
    // pressed. A Game Boy powers on the same way every time, so a person's timing is the only randomness.
    uint16_t frames = 0;
    uint8_t erase = 0;
    while (1) {
        game_wait_frames(1);
        input_update();
        frames++;
        if ((input_held & (J_SELECT | J_B)) == (J_SELECT | J_B)) {
            if (++erase == ERASE_HOLD) {
                erase_prompt();
                erase = 0;
                continue;
            }
        } else {
            erase = 0;
        }
        if ((frames & (BLINK_FRAMES - 1)) == 0)
            text_queue(0x9800u + 13 * 32 + 4, (frames & BLINK_FRAMES) ? "           " : "PRESS START");
        if (input_pressed & J_START)
            break;
    }
    music_play(MUSIC_NONE);   // cut the song (and its last held note) before the blip
    sfx_play(SFX_SELECT);
#ifdef AUTOTEST
    (void)frames;
    return AUTOTEST_SEED;   // tests replay a fixed run
#else
    return frames ^ ((uint16_t)DIV_REG << 8) ^ DIV_REG;
#endif
}

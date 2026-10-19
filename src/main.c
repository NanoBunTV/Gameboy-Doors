// DOORS for Game Boy
#include <gb/gb.h>
#include <stdint.h>
#include "platform.h"
#include "hud.h"
#include "bg.h"
#include "audio.h"
#include "game.h"
#include "title.h"
#include "death.h"
#include "ending.h"
#include "light.h"

void main(void) {
    DISPLAY_OFF;  // waits for VBlank
    platform_init();
    audio_init();
    // BG tiles at 0x8800-0x97FF (signed), BG map at 0x9800, window map at 0x9C00 (the HUD),
    // 8x16 sprites with tiles at 0x8000-0x87FF
    LCDC_REG = LCDCF_OFF | LCDCF_BG8800 | LCDCF_BG9800 | LCDCF_WIN9C00 | LCDCF_WINON |
               LCDCF_OBJ16 | LCDCF_OBJON | LCDCF_BGON;
    hud_init();
    light_build_shapes();   // sprite sheets: game_start (game_init)

    uint16_t seed = title_run();
    while (1) {
        game_start(seed);
        while (game_state != GAME_DEAD && game_state != GAME_WON) {
            vsync();
            bg_flush();   // scroll, palettes and VRAM writes first, while VBlank lasts
            game_frame();
            audio_frame();
        }
        if (game_state == GAME_WON) {
            ending_run();   // the lift, the results; then the title
            seed = title_run();
        } else if (death_run(game_death) == DEATH_TRY_AGAIN)
            seed = death_seed;
        else
            seed = title_run();
    }
}

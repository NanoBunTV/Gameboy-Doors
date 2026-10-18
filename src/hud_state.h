// State shared by hud.c (bank 0: the font, and the functions that read text from the caller's bank) and
// hud_status.c (banked: the timers, the slide and the status line). Not for other files.
#ifndef HUD_STATE_H
#define HUD_STATE_H

#include <stdint.h>

#define WIN_MAP 0x9C00u
#define HUD_ROWS 2
#define HUD_Y_SHOWN (144 - HUD_ROWS * 8)
#define HUD_Y_HIDDEN 144

extern uint8_t hud_ascii[96];   // WRAM copy of the font's ASCII table
extern char hud_shown[21];      // what the text row currently holds (or is queued to)
extern char hud_want[21];
extern uint8_t hud_timer;       // frames left to show
extern uint8_t hud_msg_timer;   // frames left of the message (0 = show the status line)
extern uint8_t hud_msg_new;     // hud_message filled hud_want since the last update

// The font tile for a character (bank 0, hud.c)
uint8_t hud_tile_for(char c);

#endif

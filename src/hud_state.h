// State shared by hud.c (bank 0: the font, and the functions that read text from the caller's bank) and
// hud_status.c (banked: the timers, the slide and the status line); items.c reads hud_msg_timer to queue its
// messages behind the one showing.
#ifndef HUD_STATE_H
#define HUD_STATE_H

#include <stdint.h>

#define WIN_MAP 0x9C00u
#define HUD_ROWS 3               // the border, the status line, the held item and gold (Phase 9a)
#define HUD_Y_SHOWN (144 - HUD_ROWS * 8)
#define HUD_Y_HIDDEN 144
#define HUD_PANEL_MAX 12         // rows a panel can have under the status line (it covers the item row)

extern uint8_t hud_panel;       // rows of panel shown under the status line (0: none)

extern char hud_shown[21];      // what the text row currently holds (or is queued to)
extern char hud_want[21];
extern char hud_shown2[21];     // ... and the item row (window row 2)
extern char hud_want2[21];
extern uint8_t hud_rebuild;     // a panel was over the item row: build both rows again
extern uint8_t hud_timer;       // frames left to show
extern uint8_t hud_msg_timer;   // frames left of the message (0 = show the status line)
extern uint8_t hud_msg_new;     // hud_message filled hud_want since the last update

// The font tile for a character (bank 0, hud.c)
uint8_t hud_tile_for(char c);

#endif

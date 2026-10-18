// Playing a run: entering rooms, doors, drawers and keys, the camera, and the frame loop
#ifndef GAME_H
#define GAME_H

#include <stdint.h>

#define GAME_TITLE      0
#define GAME_PLAYING    1
#define GAME_TRANSITION 2
extern uint8_t game_state;   // for tests (scripts/bot.py reads it)

void game_init(void);                // once at boot: sprite sheets
void game_start(uint16_t seed);      // a new run, starting in the reception
void game_frame(void);               // one frame of play (call after vsync + bg_flush)

// Waits for the next frame, doing the VBlank work (for loops outside game_frame: title, fades)
void game_wait_frames(uint8_t n);

#endif

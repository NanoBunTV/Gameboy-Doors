// Playing a run: entering rooms, doors, drawers and keys, hiding, the camera, and the frame loop.
// Lives in banked ROM (GBDK BANKED calls).
#ifndef GAME_H
#define GAME_H

#include <gb/gb.h>
#include <stdint.h>

#define GAME_TITLE      0
#define GAME_PLAYING    1
#define GAME_TRANSITION 2
#define GAME_DEAD       3
extern uint8_t game_state;   // for tests (scripts/bot.py reads it)
extern uint8_t game_death;   // DEATH_* (death.h) once game_state is GAME_DEAD

void game_init(void) BANKED;              // once at boot: sprite sheets
void game_start(uint16_t seed) BANKED;    // a new run, starting in the reception
void game_frame(void) BANKED;             // one frame of play (call after vsync + bg_flush)

// Waits for the next frame, doing the VBlank work and the audio (for loops outside game_frame: title,
// fades, the death screen)
void game_wait_frames(uint8_t n) BANKED;

// Fades from one level to another (FADE_BLACK = black), a step every few frames
void game_fade(uint8_t from, uint8_t to) BANKED;

#endif

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
// The seed AUTOTEST builds always use (title and Try again). 0x01D9 has dark rooms (15, 17, 21), Eyes in a
// dark room (17), Ambush (5) and a spawn rolled in a dark room (21) within the recorded 30 rooms.
#define AUTOTEST_SEED 0x01D9

extern uint8_t game_state;   // for tests (scripts/bot.py reads it)
extern uint8_t game_death;   // DEATH_* (death.h) once game_state is GAME_DEAD

void game_init(void) BANKED;              // sprite sheets (game_start does it; STAT-safe)
void game_start(uint16_t seed) BANKED;    // a new run, starting in the reception
void game_frame(void) BANKED;             // one frame of play (call after vsync + bg_flush)

// Waits for the next frame, doing the VBlank work and the audio (for loops outside game_frame: title,
// fades, the death screen)
void game_wait_frames(uint8_t n) BANKED;

// Fades from one level to another (FADE_BLACK = black), a step every few frames
void game_fade(uint8_t from, uint8_t to) BANKED;

#endif

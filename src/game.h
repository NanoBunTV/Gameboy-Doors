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
#define GAME_HALT       4   // in Halt's hallway (halt.c)
// The seed AUTOTEST builds always use (title and Try again). 0x1331 (Phase 5) has, within the recorded 30
// rooms: Ambush (5), Dupe rooms (9, 12, 18) and ones without him (2, 7, 13, ...), Halt's door (10), Eyes
// (11, and 18 in the dark), dark rooms from 15.
#define AUTOTEST_SEED 0x1331

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

// For Halt's hallway (halt.c), with the hallway loaded and the player placed: draws the room around the
// player with the screen black / moves the camera toward the player (4 px a frame at most)
void game_draw_room(void) BANKED;
void game_follow_camera(void) BANKED;

#endif

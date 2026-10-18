// The Library, room 50 (Minecraft docs 07; owner 2026-10-18): eight books, each saying "shape = digit", a
// paper on the desk giving the order of five shapes, and a five-wheel padlock on the exit. Figure is in
// there (figure.c). Nothing from the corridor follows you in (run.c), and door 50 seals behind you.
#ifndef LIBRARY_H
#define LIBRARY_H

#include <gb/gb.h>
#include <stdint.h>

#define LIB_BOOKS 8        // scripts/assets.py LIBRARY_BOOKS
#define LIB_CODE  5

// library_frame()'s answers
#define LIB_FREE   0       // the player moves as usual
#define LIB_HELD   1       // the padlock has the buttons
#define LIB_SOLVED 2       // the code was right: the caller opens the door

extern uint8_t library_left;      // walked out through the padlocked door: Figure and the music are gone

void library_reset(void) BANKED;  // a new run

// Room 50 has been loaded (and the player placed): books, Figure, the music, the hint the first time.
// The sprite VRAM Figure borrows is given back by game_init() when leaving.
void library_enter(void) BANKED;
// Sprite sheets only (after a full-screen picture used sprite VRAM)
void library_load_sprites(void) BANKED;
// Leaving room 50 for the next room (through the padlocked door)
void library_exit(void) BANKED;

// Each frame in the Library, before the player moves: Start (the notes) and the padlock's buttons
uint8_t library_frame(void) BANKED;
// ... after: Figure. Returns DEATH_*.
uint8_t library_update(void) BANKED;
void library_draw(void) BANKED;

// A pressed at metatile (mx, my): a book? Returns 1 if there was one.
uint8_t library_use_at(uint8_t mx, uint8_t my) BANKED;
void library_read_paper(void) BANKED;
// The padlocked door used: opens the padlock's panel
void library_open_lock(void) BANKED;

#endif

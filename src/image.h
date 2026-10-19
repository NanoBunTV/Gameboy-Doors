// Full-screen pictures (the title logo, jumpscares) and clearing the screen. Bank 0. Every write waits
// for STAT, so these work with the LCD on (behind a fade; they take a few frames) or off.
#ifndef IMAGE_H
#define IMAGE_H

#include <stdint.h>
#include "assets.h"

// Loads a picture's tiles over the room tileset (BG tiles from 0) and draws it at tile (x, y) of the map.
// On GBC its palettes replace BG palettes 0+. The next room reloads its tileset.
void image_show(uint8_t bank, const image_t *img, uint8_t x, uint8_t y);

// The same for `rows` of the picture's rows from `first` (all its tiles are loaded): the lift's doors, shut and
// open, are the two halves of one picture
void image_show_rows(uint8_t bank, const image_t *img, uint8_t x, uint8_t y, uint8_t first, uint8_t rows);

// Fills the whole 32x32 BG map with one tile (and on GBC one attribute byte)
void screen_fill(uint8_t tile, uint8_t attr);

#endif

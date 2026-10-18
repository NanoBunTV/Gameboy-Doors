// Background: the camera and streaming the room into the 32x32-tile hardware map as it scrolls.
//
// The hardware map is a 32x32-tile ring: room tile (tx, ty) lives at map slot (tx & 31, ty & 31). The
// screen shows up to 21x19 tiles; the map is kept correct for those plus one tile beyond each edge
// (23x21), so a column or row is written before it scrolls into view. Each frame bg_set_camera() builds
// at most ONE column or row into a buffer, and bg_flush() writes it right after the next vsync together
// with the new scroll position. One per frame keeps the VRAM writes well inside the DMG's VBlank
// (Docs/ARCHITECTURE.md, "Scrolling"); the one-tile margin is what makes that enough for a camera moving
// up to 4 px per frame on each axis.
#ifndef BG_H
#define BG_H

#include <stdint.h>

extern int16_t cam_x, cam_y;   // top left of the screen in room pixels
extern uint8_t bg_wy;          // window Y register value, applied in bg_flush()
extern uint8_t bg_col_n, bg_row_n;   // a column / row is built and waits for the next VBlank (non-zero)

// Draws the whole screen at a camera position. Safe with the LCD on (every write waits for STAT) but
// takes a frame or two, so do it while the screen is faded to black.
void bg_draw_full(int16_t x, int16_t y);

// Moves the camera (at most 4 px per axis per frame) and queues what comes into view. Once per frame.
void bg_set_camera(int16_t x, int16_t y);

// Sets the scroll without streaming anything (screens that aren't a room)
void bg_set_scroll(int16_t x, int16_t y);

// For frames without a camera (title screen): lets queued single-tile writes through. Once per frame.
void bg_idle(void);

// Call right after vsync(): sets SCX/SCY/WY, applies palette changes, writes what was queued
void bg_flush(void);

// Queues one write to the BG or window map (and on GBC its attribute byte), done in a later VBlank
void bg_queue_tile(uint16_t addr, uint8_t tile, uint8_t attr);

// Queues a redraw of one room metatile from room_map, if it's in the part of the map that's drawn
void bg_refresh_metatile(uint8_t mx, uint8_t my);

// The same for tiles x0..x1 of tile row y (room tile coordinates, in the room if lit): their room tiles if
// lit, else black (light.h)
void bg_refresh_span(int16_t x0, int16_t x1, int16_t y, uint8_t lit);

// VRAM address of a background tile's pixel data (0x8800 addressing, main.c)
uint16_t bg_tile_addr(uint8_t tile);

// Copies to VRAM, waiting for STAT before each byte: safe with the LCD on, slow (for small amounts)
void vram_write(uint16_t dst, const uint8_t *src, uint8_t n);

#endif

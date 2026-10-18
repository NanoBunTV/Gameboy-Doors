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

// Draws the whole screen at a camera position. The LCD must be off.
void bg_draw_full(int16_t x, int16_t y);

// Moves the camera (at most 8 px per axis per frame) and queues what comes into view
void bg_set_camera(int16_t x, int16_t y);

// Call right after vsync(): writes the queued column/row and sets SCX/SCY
void bg_flush(void);

#endif

// The on-demand HUD (DESIGN.md): a 2-row strip on the window layer that slides up from the bottom when
// something changes or while Select is held, then slides away. Also the font and text printing.
#ifndef HUD_H
#define HUD_H

#include <stdint.h>

// Once at boot with the LCD off: loads the font into VRAM, sets up the window map
void hud_init(void);

// Shows the status line (door number, health, key) for `frames` frames
void hud_show(uint8_t frames);

// Shows a message instead of the status line for `frames` frames (at most 20 characters)
void hud_message(const char *text, uint8_t frames);

// Hides the HUD at once (no slide), e.g. for a room transition
void hud_hide_now(void);

// Once per frame: timers, slide, and the status line's contents
void hud_update(void);

// Prints to the background map at tile (x, y), straight to VRAM (waiting for STAT: safe with the LCD on)
void text_print_bg(uint8_t x, uint8_t y, const char *text);

// Puts the font's own palette back in GBC BG palette 7 (after the Guiding Light's blue)
void hud_font_palette(void);

// Queues text to a BG or window map address (written in later VBlanks; safe with the LCD on)
void text_queue(uint16_t addr, const char *text);

// Formats a number as 3 digits into buf (no terminator)
void text_3digits(char *buf, uint8_t n);

#endif

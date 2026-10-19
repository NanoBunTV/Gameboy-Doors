// The on-demand HUD (DESIGN.md): a 2-row strip on the window layer that slides up from the bottom when
// something changes or while Select is held, then slides away. Also the font and text printing.
// The functions that take text stay in bank 0 (hud.c): a string literal is in its caller's bank.
#ifndef HUD_H
#define HUD_H

#include <gb/gb.h>
#include <stdint.h>

// Once at boot with the LCD off: loads the font into VRAM, sets up the window map
void hud_init(void);

// Shows the status line (door number, health, key) for `frames` frames
void hud_show(uint8_t frames) BANKED;

// Shows a message instead of the status line for `frames` frames (at most 20 characters)
void hud_message(const char *text, uint8_t frames);

// Hides the HUD at once (no slide), e.g. for a room transition
void hud_hide_now(void) BANKED;

// Once per frame: timers, slide, and the status line's contents
void hud_update(void) BANKED;

// Prints to the background map at tile (x, y), straight to VRAM (waiting for STAT: safe with the LCD on)
void text_print_bg(uint8_t x, uint8_t y, const char *text);

// The same into the window map (the HUD and the Library's panel: rows 2 and down are the panel's)
void text_print_win(uint8_t x, uint8_t y, const char *text);

// Opens a panel of `rows` rows (at most 12) under the status line at once, or closes it (0). The Library's
// notes and padlock, the breaker and the pause print into window rows 2 and down with text_print_win (over the
// item row, which is built again after).
void hud_panel_set(uint8_t rows) BANKED;

// Window Y of the panel's first row when it's open (sprites below it are hidden: spr_clip_y)
#define HUD_PANEL_ROW_Y(rows) (144 - ((rows) + 2) * 8)

// The item icons (Phase 9a) are the Library's eight shape glyphs drawn differently: everywhere but the Library
// those characters show the icons (hud_icons(1)), in the Library the shapes (font.json alt_chars)
#define ICON_LIGHTER    '%'
#define ICON_FLASHLIGHT '['
#define ICON_VITAMINS   '&'
#define ICON_LOCKPICK   '$'
#define ICON_SKELETON   '('
#define ICON_CRUCIFIX   ')'
#define ICON_GOLD       '^'
#define ICON_BANDAGE    ']'
void hud_icons(uint8_t on);   // STAT-safe; does nothing if they're already so

// Puts the font's own palette back in GBC BG palette 7 (after the Guiding Light's blue)
void hud_font_palette(void);

// Queues text to a BG or window map address (written in later VBlanks; safe with the LCD on)
void text_queue(uint16_t addr, const char *text);

#endif

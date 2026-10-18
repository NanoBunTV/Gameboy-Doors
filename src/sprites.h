// Hardware sprites: sprite sheets in VRAM and a per-frame allocator for shadow OAM.
// Sprites are 8x16 (LCDC bit 2); sprite tiles use 0x8000-0x87FF (tiles 0-127), which the background never
// uses (video.c puts BG tiles at 0x8800-0x97FF).
#ifndef SPRITES_H
#define SPRITES_H

#include <stdint.h>
#include "assets.h"

#define SPR_FLIPX 0x20

typedef struct {
    uint8_t base_tile;     // first VRAM tile of the sheet
    uint8_t base_pal;      // first GBC sprite palette of the sheet (unused on DMG)
    uint8_t *frames;       // WRAM copy of the frame table (4 bytes per frame)
} spr_sheet_t;

// Loads a sheet's tiles, palettes and frame table. frame_buf must hold 4 bytes per frame (per 16x16 part
// for big frames). Returns the number of tiles it used from base_tile.
uint8_t spr_sheet_load(spr_sheet_t *rt, uint8_t bank, const sprite_sheet_t *src,
                       uint8_t base_tile, uint8_t base_pal, uint8_t *frame_buf);

// Screen line below which no sprite is drawn (one at least half below it is skipped): the Library's panel.
// 255: none.
extern uint8_t spr_clip_y;

// Per frame: spr_begin(), any number of spr_draw(), spr_end()
void spr_begin(void);
void spr_draw(const spr_sheet_t *sheet, uint8_t frame, int16_t sx, int16_t sy, uint8_t flags);
void spr_end(void);

// A frame bigger than 16x16: pw x ph parts of 16x16, starting at part `frame` (scripts/assets.py)
void spr_draw_big(const spr_sheet_t *sheet, uint8_t frame, uint8_t pw, uint8_t ph, int16_t sx, int16_t sy);

#endif

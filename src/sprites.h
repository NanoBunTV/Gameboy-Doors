// Hardware sprites: sprite sheets in VRAM and a per-frame allocator for shadow OAM.
// Sprites are 8x16 (LCDC bit 2); sprite tiles are 0x8000-0x8FFF. Tiles 0-127 the background never uses;
// 128 and up are also background tiles 128 and up (video.c: BG tiles at 0x8800-0x97FF), see SPR_TILE_LIMIT.
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
// for big frames). Returns the number of tiles it used from base_tile. STAT-safe.
uint8_t spr_sheet_load(spr_sheet_t *rt, uint8_t bank, const sprite_sheet_t *src,
                       uint8_t base_tile, uint8_t base_pal, uint8_t *frame_buf);

// Sprite VRAM per room. The player's sheet stays at tile 0 / palette 0 (player_load). After it come only the
// sheets the current room can show, packed in the order they're asked for: game.c's load_room_sprites() for
// a room (during the door's fade), Halt's hallway and the Library for theirs, Timothy when he springs.
// A sheet already in the same place from the room before isn't written again, so a door costs only what
// changed. Same layout on both consoles (the GBC's second VRAM bank is left for later).
void spr_room_begin(void);   // a new room: nothing asked for yet
// Makes sure the sheet is in VRAM for this room (rt: as spr_sheet_load; its frame table goes in a pool shared
// by the room's sheets). STAT-safe, so it also works mid-room. Asking again in the same room does nothing.
void spr_need(spr_sheet_t *rt, uint8_t bank, const sprite_sheet_t *src);
// The same for Rush and Ambush: when the room's tileset has more than 128 tiles (its tiles 128 and up are sprite
// tiles 128 and up), these go just after the tileset's last tile, up to 184, and every other sheet below 128
// (2026-10-19: the hotel tileset passed 128 with the owner's decorations). Otherwise as spr_need.
void spr_need_high(spr_sheet_t *rt, uint8_t bank, const sprite_sheet_t *src);
// Where spr_need_high's sheets start: the loaded tileset's tile count if it's over 128, else 0 (room.c sets it,
// and forgets every sheet, whenever it loads a tileset). Not static: scripts/sprite-test.py follows the packing.
extern uint8_t spr_high_from;
void spr_forget(void);       // sprite VRAM was drawn over (the title, a jumpscare): reload every sheet

// Where room sheets start (after the player's), and where they must end: background tiles 128 and up are
// sprite tiles 128 and up, so sprites may go there only while the tileset (bg_tiles) stays below 128; 184 is
// where the door numbers and the font begin. Checked at build time for each set of sheets that can be in
// VRAM together (SPR_CHECK in game.c, halt.c, library.c), from the SPR_*_TILES / SPR_*_PALETTES the
// converter writes.
#define SPR_TILE_LIMIT(bg_tiles) ((bg_tiles) > 128 ? 128 : 184)
// What spr_need_high's sheets have, after a tileset of more than 128 tiles (0: none, they're packed with the rest)
#define SPR_HIGH_ROOM(bg_tiles) ((bg_tiles) > 128 ? 184 - (bg_tiles) : 0)
#define SPR_PALETTE_LIMIT 8   // GBC
// The room sheets' frame tables, 4 bytes a frame (16x16 part): checked with the rest (SPR_*_FRAME_COUNT * 4)
#define SPR_FRAME_POOL 128
// Fails the build (error 195, "Size of array <name> is negative") unless cond holds. SDCC's _Static_assert
// is only a warning.
#define SPR_CHECK(cond, name) typedef char name[(cond) ? 1 : -1]

// Screen line below which no sprite is drawn (one at least half below it is skipped): the Library's panel.
// 255: none.
extern uint8_t spr_clip_y;

// Per frame: spr_begin(), any number of spr_draw(), spr_end()
void spr_begin(void);
void spr_draw(const spr_sheet_t *sheet, uint8_t frame, int16_t sx, int16_t sy, uint8_t flags);
void spr_end(void);

// A frame bigger than 16x16: pw x ph parts of 16x16, starting at part `frame` (scripts/assets.py).
// flags: 0, or SPR_FLIPX for the whole frame mirrored.
void spr_draw_big(const spr_sheet_t *sheet, uint8_t frame, uint8_t pw, uint8_t ph, int16_t sx, int16_t sy,
                  uint8_t flags);

#endif

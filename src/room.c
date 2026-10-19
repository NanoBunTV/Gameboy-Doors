#include <gb/gb.h>
#include <string.h>
#include "room.h"
#include "bg.h"
#include "platform.h"
#include "sprites.h"
#include "tileset_hotel.h"
#include "tileset_garden.h"
#include "tileset_electrical.h"
#include "tileset_shop.h"

uint8_t room_w, room_h;
uint16_t room_px_w, room_px_h;
uint8_t room_map[ROOM_MAX];
uint16_t room_row[ROOM_SIDE_MAX];   // offset of each metatile row in room_map (no multiply per lookup)
uint16_t room_spawn_x, room_spawn_y;
uint8_t room_obj_count;
room_obj_t room_objs[ROOM_OBJ_MAX];

uint8_t mt_tiles[METATILE_MAX * 4];
uint8_t mt_flags[METATILE_MAX];
#ifdef CGB
uint8_t mt_attrs[METATILE_MAX * 4];
#endif

static uint8_t loaded_tileset = 0xFF;
static uint8_t ts_bank;
static const tileset_t *ts;

// Loads a tileset's tiles into VRAM and its metatile tables into WRAM. Switches banks; the caller restores.
static void tileset_load(uint8_t id) {
    // BANK() can't go in a static table because it isn't a constant expression for SDCC
    if (id == TILESET_GARDEN) {
        ts = &tileset_garden;
        ts_bank = BANK(tileset_garden);
    } else if (id == TILESET_ELECTRICAL) {
        ts = &tileset_electrical;
        ts_bank = BANK(tileset_electrical);
    } else if (id == TILESET_SHOP) {
        ts = &tileset_shop;
        ts_bank = BANK(tileset_shop);
    } else {
        ts = &tileset_hotel;
        ts_bank = BANK(tileset_hotel);
    }
    SWITCH_ROM(ts_bank);
    // BG tile data uses the 0x8800 area (LCDC bit 4 clear, main.c): tiles 0-127 at 0x9000, 128-255 at 0x8800.
    // Copied with vram_write (STAT-safe) because this can happen with the LCD on, behind a fade to black.
    for (uint8_t t = 0; t < ts->tile_count; t++)
        vram_write(bg_tile_addr(t), ts->tiles + (uint16_t)t * 16, 16);
    memcpy(mt_tiles, ts->mt_tiles, (uint16_t)ts->metatile_count * 4);
    memcpy(mt_flags, ts->mt_flags, ts->metatile_count);
#ifdef CGB
    memcpy(mt_attrs, ts->mt_attrs, (uint16_t)ts->metatile_count * 4);
    platform_set_bkg_palettes(0, ts->palettes, ts->palette_count);
#endif
    // A tileset over 128 tiles takes sprite tiles 128 and up: Rush and Ambush go after it (spr_need_high). Either
    // way the sheets' places may change and its tiles may have overwritten some: every sheet loads again.
    spr_high_from = ts->tile_count > 128 ? ts->tile_count : 0;
    spr_forget();
    loaded_tileset = id;
}

void room_forget_tileset(void) {
    loaded_tileset = 0xFF;
}

void room_load(uint8_t bank, const room_def_t *def) {
    uint8_t saved = CURRENT_BANK;
    SWITCH_ROM(bank);
    room_w = def->width;
    room_h = def->height;
    room_spawn_x = def->spawn_x;
    room_spawn_y = def->spawn_y;
    uint8_t tileset = def->tileset;
    memcpy(room_map, def->map, (uint16_t)room_w * room_h);
    room_obj_count = def->obj_count > ROOM_OBJ_MAX ? ROOM_OBJ_MAX : def->obj_count;
    memcpy(room_objs, def->objects, room_obj_count * sizeof(room_obj_t));
    room_px_w = (uint16_t)room_w << 4;
    room_px_h = (uint16_t)room_h << 4;
    uint16_t o = 0;
    for (uint8_t y = 0; y < room_h; y++, o += room_w)
        room_row[y] = o;
    if (tileset != loaded_tileset)
        tileset_load(tileset);
    SWITCH_ROM(saved);
}

uint8_t room_metatile(int16_t mx, int16_t my) {
    if ((uint16_t)mx >= room_w || (uint16_t)my >= room_h)  // negative values wrap to large ones
        return 0;
    return room_map[room_row[(uint8_t)my] + (uint8_t)mx];
}

void room_set_metatile(uint8_t mx, uint8_t my, uint8_t mt) {
    room_map[room_row[my] + mx] = mt;
    bg_refresh_metatile(mx, my);
}

uint8_t room_solid_at(int16_t x, int16_t y) {
    if (x < 0 || y < 0)
        return 1;
    return mt_flags[room_metatile(x >> 4, y >> 4)] & MTF_SOLID;
}

// The door plate's base: the top two tiles of metatile `base`, from the tileset's bank (room_plate.c draws the
// number, banked)
void room_plate_base(uint8_t base, uint8_t *buf) {
    uint16_t b = (uint16_t)base << 2;
    uint8_t saved = CURRENT_BANK;
    SWITCH_ROM(ts_bank);
    memcpy(buf, ts->tiles + (uint16_t)mt_tiles[b] * 16, 16);
    memcpy(buf + 16, ts->tiles + (uint16_t)mt_tiles[b + 1] * 16, 16);
    SWITCH_ROM(saved);
}

#include <gb/gb.h>
#include <string.h>
#include "room.h"
#include "platform.h"
#include "tileset_hotel.h"

uint8_t room_w, room_h;
uint16_t room_px_w, room_px_h;
uint8_t room_map[ROOM_MAX];
uint16_t room_spawn_x, room_spawn_y;

uint8_t mt_tiles[METATILE_MAX * 4];
uint8_t mt_flags[METATILE_MAX];
#ifdef CGB
uint8_t mt_attrs[METATILE_MAX * 4];
#endif

// Loads a tileset's tiles into VRAM and its metatile tables into WRAM. Switches banks; the caller restores.
static void tileset_load(uint8_t id) {
    // Only TILESET_HOTEL so far. With more, switch on id here; BANK() can't go in a static table because
    // it isn't a constant expression for SDCC.
    (void)id;
    const tileset_t *ts = &tileset_hotel;
    SWITCH_ROM(BANK(tileset_hotel));
    // BG tile data uses the 0x8800 area (LCDC bit 4 clear, video.c): tiles 0-127 at 0x9000, 128-255 at 0x8800
    set_bkg_data(0, ts->tile_count, ts->tiles);
    memcpy(mt_tiles, ts->mt_tiles, (uint16_t)ts->metatile_count * 4);
    memcpy(mt_flags, ts->mt_flags, ts->metatile_count);
#ifdef CGB
    memcpy(mt_attrs, ts->mt_attrs, (uint16_t)ts->metatile_count * 4);
    platform_set_bkg_palettes(ts->palettes, ts->palette_count);
#endif
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
    room_px_w = (uint16_t)room_w << 4;
    room_px_h = (uint16_t)room_h << 4;
    tileset_load(tileset);
    SWITCH_ROM(saved);
}

uint8_t room_metatile(int16_t mx, int16_t my) {
    if ((uint16_t)mx >= room_w || (uint16_t)my >= room_h)  // negative values wrap to large ones
        return 0;
    return room_map[(uint16_t)my * room_w + (uint16_t)mx];
}

uint8_t room_solid_at(int16_t x, int16_t y) {
    if (x < 0 || y < 0)
        return 1;
    return mt_flags[room_metatile(x >> 4, y >> 4)] & MTF_SOLID;
}

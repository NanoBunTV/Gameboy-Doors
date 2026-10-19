#include <gb/gb.h>
#include <string.h>
#include "image.h"
#include "bg.h"
#include "room.h"
#include "platform.h"

// Bank 0: reads banked image data, which banked callers (title.c, death.c) can't switch to themselves

void image_show(uint8_t bank, const image_t *img, uint8_t x, uint8_t y) {
    image_show_rows(bank, img, x, y, 0, 0xFF);
}

void image_show_rows(uint8_t bank, const image_t *img, uint8_t x, uint8_t y, uint8_t first, uint8_t rows) {
    uint8_t saved = CURRENT_BANK;
    SWITCH_ROM(bank);
    uint8_t w = img->width, h = img->height, n = img->tile_count;
    if (rows > h - first)
        rows = h - first;
    for (uint8_t t = 0; t < n; t++)
        vram_write(bg_tile_addr(t), img->tiles + (uint16_t)t * 16, 16);
    uint16_t skip = (uint16_t)first * w;
    const uint8_t *map = img->map + skip;
    for (uint8_t row = 0; row < rows; row++, map += w)
        vram_write(0x9800u + ((uint16_t)(y + row) << 5) + x, map, w);
#ifdef CGB
    VBK_REG = VBK_ATTRIBUTES;
    const uint8_t *attrs = img->attrs + skip;
    for (uint8_t row = 0; row < rows; row++, attrs += w)
        vram_write(0x9800u + ((uint16_t)(y + row) << 5) + x, attrs, w);
    VBK_REG = VBK_TILES;
    platform_set_bkg_palettes(0, img->palettes, img->palette_count);
#endif
    SWITCH_ROM(saved);
    room_forget_tileset();   // the picture's tiles replaced the room tileset's
}

void screen_fill(uint8_t tile, uint8_t attr) {
    uint8_t row[32];
    memset(row, tile, 32);
    for (uint8_t y = 0; y < 32; y++)
        vram_write(0x9800u + ((uint16_t)y << 5), row, 32);
#ifdef CGB
    memset(row, attr, 32);
    VBK_REG = VBK_ATTRIBUTES;
    for (uint8_t y = 0; y < 32; y++)
        vram_write(0x9800u + ((uint16_t)y << 5), row, 32);
    VBK_REG = VBK_TILES;
#else
    (void)attr;
#endif
}

#include <gb/gb.h>
#include "bg.h"
#include "room.h"

#define VIS_COLS 21   // tile columns the screen can show at once (20 + 1 when scrolled mid-tile)
#define VIS_ROWS 19
#define MAX_COLS (VIS_COLS + 2)  // plus a one-tile margin on each side
#define MAX_ROWS (VIS_ROWS + 2)

int16_t cam_x, cam_y;
// The tile region the map holds correctly: columns ld_l..ld_r, rows ld_t..ld_b
static int16_t ld_l, ld_r, ld_t, ld_b;
static uint8_t scx, scy;

// The queue bg_flush_vram() writes. Not static: the assembly reads them by name.
uint8_t bg_col_n, bg_row_n;
uint8_t *bg_col_dst, *bg_row_dst;
uint8_t bg_col_tiles[MAX_ROWS], bg_row_tiles[MAX_COLS];
#ifdef CGB
uint8_t bg_col_attrs[MAX_ROWS], bg_row_attrs[MAX_COLS];
#endif

static uint8_t *map_slot(int16_t tx, int16_t ty) {
    return (uint8_t *)(0x9800u + (((uint16_t)ty & 31) << 5) + ((uint16_t)tx & 31));
}

// Tile column tx, rows ty .. ty + count - 1, into bg_col_*
static void build_column(int16_t tx, int16_t ty, uint8_t count) {
    int16_t mx = tx >> 1, my = ty >> 1;
    uint8_t xsub = (uint8_t)tx & 1;
    uint8_t ysub = (uint8_t)ty & 1;  // 1: start at the bottom half of the first metatile
    uint8_t in_x = (uint16_t)mx < room_w;
    // Modular pointer: wrong while my is out of range, but only dereferenced when it's in range
    const uint8_t *p = room_map + (uint16_t)my * room_w + (uint16_t)mx;
    uint8_t *t = bg_col_tiles;
#ifdef CGB
    uint8_t *a = bg_col_attrs;
#endif
    uint8_t n = count;
    while (1) {
        uint8_t m = (in_x && (uint16_t)my < room_h) ? *p : 0;
        uint16_t k = ((uint16_t)m << 2) + xsub;
        if (!ysub) {
            *t++ = mt_tiles[k];
#ifdef CGB
            *a++ = mt_attrs[k];
#endif
            if (!--n) break;
        }
        *t++ = mt_tiles[k + 2];
#ifdef CGB
        *a++ = mt_attrs[k + 2];
#endif
        if (!--n) break;
        ysub = 0;
        my++;
        p += room_w;
    }
    bg_col_dst = map_slot(tx, ty);
    bg_col_n = count;
}

// Tile row ty, columns tx .. tx + count - 1, into bg_row_*
static void build_row(int16_t tx, int16_t ty, uint8_t count) {
    int16_t mx = tx >> 1, my = ty >> 1;
    uint8_t xsub = (uint8_t)tx & 1;
    uint8_t ysub = ((uint8_t)ty & 1) << 1;
    uint8_t in_y = (uint16_t)my < room_h;
    const uint8_t *p = room_map + (uint16_t)my * room_w + (uint16_t)mx;
    uint8_t *t = bg_row_tiles;
#ifdef CGB
    uint8_t *a = bg_row_attrs;
#endif
    uint8_t n = count;
    while (1) {
        uint8_t m = (in_y && (uint16_t)mx < room_w) ? *p : 0;
        uint16_t k = ((uint16_t)m << 2) + ysub;
        if (!xsub) {
            *t++ = mt_tiles[k];
#ifdef CGB
            *a++ = mt_attrs[k];
#endif
            if (!--n) break;
        }
        *t++ = mt_tiles[k + 1];
#ifdef CGB
        *a++ = mt_attrs[k + 1];
#endif
        if (!--n) break;
        xsub = 0;
        mx++;
        p++;
    }
    bg_row_dst = map_slot(tx, ty);
    bg_row_n = count;
}

// Writes the queued column and row to the BG map (and on GBC the attribute map in VRAM bank 1).
// Every byte waits for STAT to say VRAM is accessible, so this is safe at any time; called right after
// vsync() it normally finishes inside VBlank without waiting. After the STAT check passes there are at
// least the 20 cycles of mode 2 before VRAM locks, and the write follows within 3.
static void bg_flush_vram(void) __naked {
    __asm
        ld a, (#_bg_col_n)
        or a
        jr z, 10$
        ld c, a
        ld hl, #_bg_col_dst
        ld a, (hl+)
        ld e, a
        ld d, (hl)
        ld hl, #_bg_col_tiles
        call bgf_column
#ifdef CGB
        ld a, #1
        ldh (_VBK_REG), a
        ld a, (#_bg_col_n)
        ld c, a
        ld hl, #_bg_col_dst
        ld a, (hl+)
        ld e, a
        ld d, (hl)
        ld hl, #_bg_col_attrs
        call bgf_column
        xor a
        ldh (_VBK_REG), a
#endif
        xor a
        ld (#_bg_col_n), a
10$:
        ld a, (#_bg_row_n)
        or a
        ret z
        ld c, a
        ld hl, #_bg_row_dst
        ld a, (hl+)
        ld e, a
        ld d, (hl)
        ld hl, #_bg_row_tiles
        call bgf_row
#ifdef CGB
        ld a, #1
        ldh (_VBK_REG), a
        ld a, (#_bg_row_n)
        ld c, a
        ld hl, #_bg_row_dst
        ld a, (hl+)
        ld e, a
        ld d, (hl)
        ld hl, #_bg_row_attrs
        call bgf_row
        xor a
        ldh (_VBK_REG), a
#endif
        xor a
        ld (#_bg_row_n), a
        ret

    ; HL = source, DE = map address, C = count. Steps down one map row (32 bytes), wrapping 0x9C00 to 0x9800
bgf_column:
1$:
        ld a, (hl+)
        ld b, a
2$:
        ldh a, (_STAT_REG)
        and #0x02
        jr nz, 2$
        ld a, b
        ld (de), a
        ld a, e
        add a, #32
        ld e, a
        jr nc, 3$
        inc d
        ld a, d
        cp #0x9C
        jr nz, 3$
        ld d, #0x98
3$:
        dec c
        jr nz, 1$
        ret

    ; HL = source, DE = map address, C = count. Steps right, wrapping within the 32-byte map row
bgf_row:
1$:
        ld a, (hl+)
        ld b, a
2$:
        ldh a, (_STAT_REG)
        and #0x02
        jr nz, 2$
        ld a, b
        ld (de), a
        inc e
        ld a, e
        and #31
        jr nz, 3$
        ld a, e
        sub #32
        ld e, a
3$:
        dec c
        jr nz, 1$
        ret
    __endasm;
}

void bg_draw_full(int16_t x, int16_t y) {
    int16_t l = x >> 3, t = y >> 3;
    ld_l = l - 1;
    ld_r = l + VIS_COLS;
    ld_t = t - 1;
    ld_b = t + VIS_ROWS;
    bg_row_n = 0;
    for (int16_t c = ld_l; c <= ld_r; c++) {
        build_column(c, ld_t, MAX_ROWS);
        bg_flush_vram();
    }
    cam_x = x;
    cam_y = y;
    SCX_REG = scx = (uint8_t)x;
    SCY_REG = scy = (uint8_t)y;
}

void bg_set_camera(int16_t x, int16_t y) {
    int16_t l = x >> 3, t = y >> 3;
    // Forget what's outside the wanted region (visible + margin). Nothing to write: the slots are reused.
    if (ld_l < l - 1) ld_l = l - 1;
    if (ld_r > l + VIS_COLS) ld_r = l + VIS_COLS;
    if (ld_t < t - 1) ld_t = t - 1;
    if (ld_b > t + VIS_ROWS) ld_b = t + VIS_ROWS;

    // Then add one column or row: first anything visible that's missing (only after a camera jump of more
    // than a tile), then the margins. A new column covers the loaded rows and vice versa, so the region
    // stays a rectangle.
    uint8_t cols = (uint8_t)(ld_r - ld_l + 1), rows = (uint8_t)(ld_b - ld_t + 1);
    if (ld_r < l + VIS_COLS - 1)
        build_column(++ld_r, ld_t, rows);
    else if (ld_l > l)
        build_column(--ld_l, ld_t, rows);
    else if (ld_b < t + VIS_ROWS - 1)
        build_row(ld_l, ++ld_b, cols);
    else if (ld_t > t)
        build_row(ld_l, --ld_t, cols);
    else if (ld_r < l + VIS_COLS)
        build_column(++ld_r, ld_t, rows);
    else if (ld_l > l - 1)
        build_column(--ld_l, ld_t, rows);
    else if (ld_b < t + VIS_ROWS)
        build_row(ld_l, ++ld_b, cols);
    else if (ld_t > t - 1)
        build_row(ld_l, --ld_t, cols);

    cam_x = x;
    cam_y = y;
    scx = (uint8_t)x;
    scy = (uint8_t)y;
}

void bg_flush(void) {
    SCX_REG = scx;
    SCY_REG = scy;
    bg_flush_vram();
}

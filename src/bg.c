#include <gb/gb.h>
#include "bg.h"
#include "room.h"
#include "platform.h"

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

uint8_t bg_wy = 144;        // window Y, written in VBlank (hud.c slides it)

static uint8_t *map_slot(int16_t tx, int16_t ty) {
    return (uint8_t *)(0x9800u + (((uint16_t)ty & 31) << 5) + ((uint16_t)tx & 31));
}

// --- Single-tile writes: changed metatiles and HUD text. A ring of (address, tile, GBC attribute); each
// frame that has no column or row to stream, up to VQ_PER_FRAME of them go into bg_vq_buf for VBlank.
#define VQ_MAX 64           // power of two
#define VQ_PER_FRAME 20     // fits the DMG's VBlank with GBDK's interrupt (ARCHITECTURE.md)
static uint16_t vq_addr[VQ_MAX];
static uint8_t vq_tile[VQ_MAX];
#ifdef CGB
static uint8_t vq_attr[VQ_MAX];
#endif
static uint8_t vq_head, vq_tail;   // entries vq_tail .. vq_head - 1
uint8_t bg_vq_n;
uint8_t bg_vq_buf[VQ_PER_FRAME * 3];
#ifdef CGB
uint8_t bg_vq_abuf[VQ_PER_FRAME * 3];
#endif

void bg_queue_tile(uint16_t addr, uint8_t tile, uint8_t attr) {
    uint8_t next = (vq_head + 1) & (VQ_MAX - 1);
    if (next == vq_tail)
        return;  // full: drop rather than overwrite what's queued (64 is several frames of work)
    vq_addr[vq_head] = addr;
    vq_tile[vq_head] = tile;
#ifdef CGB
    vq_attr[vq_head] = attr;
#else
    (void)attr;
#endif
    vq_head = next;
}

// Moves up to VQ_PER_FRAME queued writes into the buffer bg_flush() writes
static void vq_prepare(void) {
    uint8_t n = 0, *b = bg_vq_buf;
#ifdef CGB
    uint8_t *a = bg_vq_abuf;
#endif
    while (vq_tail != vq_head && n < VQ_PER_FRAME) {
        uint16_t addr = vq_addr[vq_tail];
        *b++ = (uint8_t)addr;
        *b++ = addr >> 8;
        *b++ = vq_tile[vq_tail];
#ifdef CGB
        *a++ = (uint8_t)addr;
        *a++ = addr >> 8;
        *a++ = vq_attr[vq_tail];
#endif
        vq_tail = (vq_tail + 1) & (VQ_MAX - 1);
        n++;
    }
    bg_vq_n = n;
}

// Drops queued writes to the background map (not the window's), after a full redraw made them stale
static void vq_drop_bg(void) {
    uint8_t i = vq_tail, j = vq_tail;
    while (i != vq_head) {
        if (vq_addr[i] >= 0x9C00u) {
            vq_addr[j] = vq_addr[i];
            vq_tile[j] = vq_tile[i];
#ifdef CGB
            vq_attr[j] = vq_attr[i];
#endif
            j = (j + 1) & (VQ_MAX - 1);
        }
        i = (i + 1) & (VQ_MAX - 1);
    }
    vq_head = j;
}

void bg_refresh_metatile(uint8_t mx, uint8_t my) {
    int16_t tx = (int16_t)mx << 1, ty = (int16_t)my << 1;
    uint16_t k = (uint16_t)room_map[(uint16_t)my * room_w + mx] << 2;
    for (uint8_t j = 0; j < 2; j++)
        for (uint8_t i = 0; i < 2; i++) {
            int16_t x = tx + i, y = ty + j;
            if (x < ld_l || x > ld_r || y < ld_t || y > ld_b)
                continue;  // not in the map yet; streaming will draw it from room_map
#ifdef CGB
            bg_queue_tile((uint16_t)map_slot(x, y), mt_tiles[k + (j << 1) + i], mt_attrs[k + (j << 1) + i]);
#else
            bg_queue_tile((uint16_t)map_slot(x, y), mt_tiles[k + (j << 1) + i], 0);
#endif
        }
}

uint16_t bg_tile_addr(uint8_t tile) {
    return tile < 128 ? 0x9000u + ((uint16_t)tile << 4) : 0x8800u + ((uint16_t)(tile - 128) << 4);
}

// vram_write: copies n bytes to VRAM, waiting for STAT before each byte, so it's safe with the LCD on
static uint8_t *vw_dst;
static const uint8_t *vw_src;
static uint8_t vw_n;
static void vram_write_asm(void) __naked {
    __asm
        ld hl, #_vw_dst
        ld a, (hl+)
        ld e, a
        ld d, (hl)
        ld hl, #_vw_src
        ld a, (hl+)
        ld h, (hl)
        ld l, a
        ld a, (#_vw_n)
        ld c, a
1$:
        ld a, (hl+)
        ld b, a
2$:
        ldh a, (_STAT_REG)
        and #0x02
        jr nz, 2$
        ld a, b
        ld (de), a
        inc de
        dec c
        jr nz, 1$
        ret
    __endasm;
}

void vram_write(uint16_t dst, const uint8_t *src, uint8_t n) {
    if (!n)
        return;
    vw_dst = (uint8_t *)dst;
    vw_src = src;
    vw_n = n;
    vram_write_asm();
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
        jr z, 20$
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
20$:
        ld a, (#_bg_vq_n)
        or a
        ret z
        ld c, a
        ld hl, #_bg_vq_buf
        call bgf_scatter
#ifdef CGB
        ld a, #1
        ldh (_VBK_REG), a
        ld a, (#_bg_vq_n)
        ld c, a
        ld hl, #_bg_vq_abuf
        call bgf_scatter
        xor a
        ldh (_VBK_REG), a
#endif
        xor a
        ld (#_bg_vq_n), a
        ret

    ; HL = list of (address low, address high, value), C = count
bgf_scatter:
1$:
        ld a, (hl+)
        ld e, a
        ld a, (hl+)
        ld d, a
        ld a, (hl+)
        ld b, a
2$:
        ldh a, (_STAT_REG)
        and #0x02
        jr nz, 2$
        ld a, b
        ld (de), a
        dec c
        jr nz, 1$
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
    bg_flush_vram();   // whatever was prepared for the last VBlank (harmless: all redrawn below)
    vq_drop_bg();      // queued map writes belong to the old picture; the window's stay
    ld_l = l - 1;
    ld_r = l + VIS_COLS;
    ld_t = t - 1;
    ld_b = t + VIS_ROWS;
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
    cam_x = x;
    cam_y = y;
    scx = (uint8_t)x;
    scy = (uint8_t)y;
    // Last frame's writes are still waiting (bg_flush gave VBlank to the palettes): build nothing new over
    // them. The one-tile margin covers a frame's delay.
    if (bg_col_n || bg_row_n || bg_vq_n)
        return;
    // Forget what's outside the wanted region (visible + margin). Nothing to write: the slots are reused.
    if (ld_l < l - 1) ld_l = l - 1;
    if (ld_r > l + VIS_COLS) ld_r = l + VIS_COLS;
    if (ld_t < t - 1) ld_t = t - 1;
    if (ld_b > t + VIS_ROWS) ld_b = t + VIS_ROWS;

    // Then add one column or row: first anything visible that's missing (only after a camera jump of more
    // than a tile), then the margins. A new column covers the loaded rows and vice versa, so the region
    // stays a rectangle.
    // Queued single-tile writes go first when only margin work is due; they get a frame of their own
    // because a column/row and a full batch of them together wouldn't fit the DMG's VBlank.
    uint8_t cols = (uint8_t)(ld_r - ld_l + 1), rows = (uint8_t)(ld_b - ld_t + 1);
    uint8_t queued = vq_head != vq_tail;
    if (ld_r < l + VIS_COLS - 1)
        build_column(++ld_r, ld_t, rows);
    else if (ld_l > l)
        build_column(--ld_l, ld_t, rows);
    else if (ld_b < t + VIS_ROWS - 1)
        build_row(ld_l, ++ld_b, cols);
    else if (ld_t > t)
        build_row(ld_l, --ld_t, cols);
    else if (queued)
        vq_prepare();
    else if (ld_r < l + VIS_COLS)
        build_column(++ld_r, ld_t, rows);
    else if (ld_l > l - 1)
        build_column(--ld_l, ld_t, rows);
    else if (ld_b < t + VIS_ROWS)
        build_row(ld_l, ++ld_b, cols);
    else if (ld_t > t - 1)
        build_row(ld_l, --ld_t, cols);
}

void bg_set_scroll(int16_t x, int16_t y) {
    cam_x = x;
    cam_y = y;
    SCX_REG = scx = (uint8_t)x;
    SCY_REG = scy = (uint8_t)y;
}

void bg_idle(void) {
    if (vq_head != vq_tail && !bg_vq_n)
        vq_prepare();
}

void bg_flush(void) {
    SCX_REG = scx;
    SCY_REG = scy;
    WY_REG = bg_wy;
    // GBC palette RAM can only be written in VBlank, so the palettes go first; on a frame they take up much
    // of it, the map writes wait for the next one (they're STAT-safe anyway, but would run past VBlank)
    if (!platform_vblank())
        bg_flush_vram();
}

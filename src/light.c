#include <gb/gb.h>
#include <string.h>
#include "light.h"
#include "bg.h"

// Bank 0: what bg.c needs while it streams the room in. Moving a light is light_move.c (banked).
//
// This runs inside a DMG frame that's already busy, and SDCC's 16-bit code is slow on the SM83, so it works
// in 8-bit tile coordinates wherever it can: rooms are at most 127 metatiles a side (scripts/assets.py).

uint8_t light_dark;
int8_t light_l[LIGHT_SHAPES][LIGHT_ROWS], light_r[LIGHT_SHAPES][LIGHT_ROWS];
uint8_t light_first[LIGHT_SHAPES], light_last[LIGHT_SHAPES];

// The two sources (not static: scripts/scroll-test.py reads them)
uint8_t light_src_shape[2] = { LIGHT_OFF, LIGHT_OFF };
uint8_t light_src_x[2], light_src_y[2];
uint8_t light_tw, light_th;

uint8_t light_lit8(uint8_t tx, uint8_t ty) {
    for (uint8_t s = 0; s < 2; s++) {
        uint8_t shape = light_src_shape[s];
        if (shape == LIGHT_OFF)
            continue;
        uint8_t row = (uint8_t)(ty - light_src_y[s] + LIGHT_REACH);
        if (row >= LIGHT_ROWS)
            continue;
        int8_t dx = (int8_t)(uint8_t)(tx - light_src_x[s]);
        if (dx >= light_l[shape][row] && dx <= light_r[shape][row])
            return 1;
    }
    return 0;
}

// Column tx, rows ty .. ty+n-1. Per source, its rows that fall in the column; then a byte loop.
void light_mask_column(int16_t tx, int16_t ty, uint8_t n, uint8_t *tiles, uint8_t *attrs) {
    uint8_t lit[24];
    memset(lit, 0, n);
    if ((uint16_t)tx < light_tw)
        for (uint8_t s = 0; s < 2; s++) {
            uint8_t shape = light_src_shape[s];
            if (shape == LIGHT_OFF)
                continue;
            int16_t dx = tx - light_src_x[s];
            if (dx < -LIGHT_REACH || dx > LIGHT_REACH)
                continue;
            int8_t d = (int8_t)dx;
            const int8_t *l = light_l[shape], *r = light_r[shape];
            // shape row `row` is tile row light_src_y - REACH + row, i.e. index k0 + row in the column
            int16_t k0 = (int16_t)light_src_y[s] - LIGHT_REACH - ty;
            for (uint8_t row = light_first[shape]; row <= light_last[shape]; row++) {
                uint16_t k = (uint16_t)(k0 + row);
                if (k < n && d >= l[row] && d <= r[row] && (uint16_t)(ty + k) < light_th)
                    lit[(uint8_t)k] = 1;
            }
        }
    for (uint8_t i = 0; i < n; i++)
        if (!lit[i]) {
            tiles[i] = LIGHT_DARK_TILE;
#ifdef CGB
            attrs[i] = LIGHT_DARK_ATTR;
#else
            (void)attrs;
#endif
        }
}

// Source s's lit span on tile row ty, as indexes from tx clipped to 0..n-1: *a..*b, or *a > *b for none
static void row_span(uint8_t s, int16_t tx, int16_t ty, uint8_t n, uint8_t *a, uint8_t *b) {
    *a = 1;
    *b = 0;
    uint8_t shape = light_src_shape[s];
    if (shape == LIGHT_OFF || (uint16_t)ty >= light_th)
        return;
    uint8_t row = (uint8_t)((uint8_t)ty - light_src_y[s] + LIGHT_REACH);
    if (row >= LIGHT_ROWS || light_l[shape][row] > light_r[shape][row])
        return;
    int16_t l = (int16_t)light_src_x[s] + light_l[shape][row], r = (int16_t)light_src_x[s] + light_r[shape][row];
    if (l < 0)
        l = 0;
    if (r >= light_tw)
        r = light_tw - 1;
    l -= tx;
    r -= tx;
    if (r < 0 || l >= n || l > r)
        return;
    *a = l < 0 ? 0 : (uint8_t)l;
    *b = r >= n ? n - 1 : (uint8_t)r;
}

void light_mask_row(int16_t tx, int16_t ty, uint8_t n, uint8_t *tiles, uint8_t *attrs) {
    // At most one lit span per source on a row: darken everything outside both
    uint8_t a0, b0, a1, b1;
    row_span(0, tx, ty, n, &a0, &b0);
    row_span(1, tx, ty, n, &a1, &b1);
    for (uint8_t i = 0; i < n; i++) {
        if ((i >= a0 && i <= b0) || (i >= a1 && i <= b1))
            continue;
        tiles[i] = LIGHT_DARK_TILE;
#ifdef CGB
        attrs[i] = LIGHT_DARK_ATTR;
#else
        (void)attrs;
#endif
    }
}

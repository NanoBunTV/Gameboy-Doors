#pragma bank 255
#include <gb/gb.h>
#include "light.h"
#include "bg.h"

// Moving a light (banked; light.c has what bg.c needs every frame)

extern uint8_t light_pending;

void light_room(uint8_t dark) BANKED {
    light_dark = dark;
    light_src_shape[0] = light_src_shape[1] = light_src_shape[2] = LIGHT_OFF;
    light_tw = room_w << 1;
    light_th = room_h << 1;
    light_pending = 0;   // the room is about to be drawn whole
}

// Everything per row is 8-bit: rows are tile rows inside the room (0..light_th-1), spans are clipped to its
// columns (0..light_tw-1) as they're made. With 16-bit maths this took ~26 DMG scanlines per move.

static uint8_t sl, sr;   // span8's result

// A shape's span on row y for a source at (sx, sy), clipped to the room: 1 and sl..sr, or 0 for none
static uint8_t span8(uint8_t shape, uint8_t sx, uint8_t sy, uint8_t y) {
    if (shape == LIGHT_OFF)
        return 0;
    uint8_t row = (uint8_t)(y - sy + LIGHT_REACH);
    if (row >= LIGHT_ROWS)
        return 0;
    int8_t l = light_l[shape][row], r = light_r[shape][row];
    if (l > r)
        return 0;
    if (r < 0 && sx < (uint8_t)(-r))
        return 0;                      // all left of the room
    uint16_t a = (l < 0 && sx < (uint8_t)(-l)) ? 0 : (uint16_t)(sx + l);   // 16 bits: a 254-tile room can
    uint16_t b = (uint16_t)(sx + r);                                       // reach past 255
    if (a >= light_tw)
        return 0;                      // all right of the room
    sl = (uint8_t)a;
    sr = b >= light_tw ? light_tw - 1 : (uint8_t)b;
    return 1;
}

static uint8_t row_y, has_p, pl, pr, has_q, ql, qr;

// Redraws a..b (a <= b) of row_y as dark, except what the second other source lights (ql..qr)
static void dark_minus_q(uint8_t a, uint8_t b) {
    if (!has_q || qr < a || ql > b) {
        bg_refresh_span(a, b, row_y, 0);
        return;
    }
    if (ql > a)
        bg_refresh_span(a, ql - 1, row_y, 0);
    if (qr < b)
        bg_refresh_span(qr + 1, b, row_y, 0);
}

// ... and what the first lights (pl..pr)
static void dark_minus_other(uint8_t a, uint8_t b) {
    if (!has_p || pr < a || pl > b) {
        dark_minus_q(a, b);
        return;
    }
    if (pl > a)
        dark_minus_q(a, pl - 1);
    if (pr < b)
        dark_minus_q(pr + 1, b);
}

// The redraw a move needs, done a few rows per frame by light_step (all at once it overran DMG frames):
// which source, its shape and place before the move, and the rows left
static uint8_t pend_s, pend_os, pend_ox, pend_oy, pend_y, pend_last;
uint8_t light_pending;   // a redraw isn't finished (not static: scripts/scroll-test.py waits for it)

void light_step(uint8_t rows) BANKED {
    if (!light_pending)
        return;
    uint8_t s = pend_s, os = pend_os, ox = pend_ox, oy = pend_oy;
    uint8_t shape = light_src_shape[s], tx = light_src_x[s], ty = light_src_y[s];
    // The other two sources
    uint8_t o1 = s == 0 ? 1 : 0, o2 = s == 2 ? 1 : 2;
    uint8_t po = light_src_shape[o1], px = light_src_x[o1], py = light_src_y[o1];
    uint8_t qo = light_src_shape[o2], qx = light_src_x[o2], qy = light_src_y[o2];
    while (rows--) {
        uint8_t y = pend_y;
        row_y = y;
        uint8_t has_o = span8(os, ox, oy, y), ol = sl, or_ = sr;
        uint8_t has_n = span8(shape, tx, ty, y), nl = sl, nr = sr;
        has_p = span8(po, px, py, y);
        pl = sl;
        pr = sr;
        has_q = span8(qo, qx, qy, y);
        ql = sl;
        qr = sr;
        // New minus old: lit
        if (has_n) {
            if (!has_o) {
                bg_refresh_span(nl, nr, y, 1);
            } else {
                if (nl < ol)
                    bg_refresh_span(nl, nr < ol ? nr : ol - 1, y, 1);
                if (nr > or_)
                    bg_refresh_span(nl > or_ ? nl : or_ + 1, nr, y, 1);
            }
        }
        // Old minus new: dark, unless another source lights it
        if (has_o) {
            if (!has_n) {
                dark_minus_other(ol, or_);
            } else {
                if (ol < nl)
                    dark_minus_other(ol, or_ < nl ? or_ : nl - 1);
                if (or_ > nr)
                    dark_minus_other(ol > nr ? ol : nr + 1, or_);
            }
        }
        if (y == pend_last) {
            light_pending = 0;
            return;
        }
        pend_y = y + 1;
    }
}

void light_set(uint8_t s, uint8_t shape, uint8_t tx, uint8_t ty) BANKED {
    uint8_t os = light_src_shape[s], ox = light_src_x[s], oy = light_src_y[s];
    if (shape == os && tx == ox && ty == oy)
        return;
    if (light_pending)
        light_step(LIGHT_ROWS * 2);   // finish the last move first: its "before" is this one's
    light_src_shape[s] = shape;
    light_src_x[s] = tx;
    light_src_y[s] = ty;
    if (!light_dark)
        return;
    // Rows that either shape touches, in the room. light_step then goes row by row, as intervals: what
    // the new shape adds is lit; what the old one loses goes dark, except where the other source lights
    // it (so nothing per tile needs to ask what's lit).
    int16_t top = 0x7FFF, bottom = -0x7FFF;
    if (os != LIGHT_OFF) {
        top = (int16_t)oy - LIGHT_REACH + light_first[os];
        bottom = (int16_t)oy - LIGHT_REACH + light_last[os];
    }
    if (shape != LIGHT_OFF) {
        int16_t t = (int16_t)ty - LIGHT_REACH + light_first[shape], b = (int16_t)ty - LIGHT_REACH + light_last[shape];
        if (t < top) top = t;
        if (b > bottom) bottom = b;
    }
    if (top < 0)
        top = 0;
    if (bottom >= light_th)
        bottom = light_th - 1;
    if (top > bottom)
        return;
    pend_s = s;
    pend_os = os;
    pend_ox = ox;
    pend_oy = oy;
    pend_y = (uint8_t)top;
    pend_last = (uint8_t)bottom;
    light_pending = 1;
}

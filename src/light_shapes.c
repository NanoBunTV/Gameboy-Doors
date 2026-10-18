#pragma bank 255
#include <gb/gb.h>
#include "light.h"

// Builds the light shapes once at boot (a few frames of work, while the LCD is off): the glow and the
// lighter are rounded squares, the flashlight is the glow plus a cone in each of the 8 facing directions.
// Distances in 16ths of a tile.

// Facing (player.h DIR_*, clockwise from down) as a step
static const int8_t dir_x[8] = { 0, -1, -1, -1, 0, 1, 1, 1 };
static const int8_t dir_y[8] = { 1, 1, 0, -1, -1, -1, 0, 1 };

#define GLOW_R2    5     // dx^2 + dy^2 <= 5: 5x5 without its corners
#define LIGHTER_R2 10    // 7x7, rounded
#define CONE_LEN   120   // 7.5 tiles
// Half-width 0.5 tile at the source, 2.5 at the far end: lateral <= 8 + along * 2 / 7 (in 16ths)

static uint8_t lit(uint8_t shape, int8_t dx, int8_t dy) {
    int16_t d2 = dx * dx + dy * dy;
    if (shape == LIGHT_LIGHTER)
        return d2 <= LIGHTER_R2;
    if (d2 <= GLOW_R2)
        return 1;
    if (shape == LIGHT_GLOW)
        return 0;
    uint8_t f = shape - LIGHT_FLASH;
    int8_t fx = dir_x[f], fy = dir_y[f];
    int16_t along = dx * fx + dy * fy;
    int16_t lateral = dx * fy - dy * fx;
    if (lateral < 0)
        lateral = -lateral;
    // In 16ths of a tile: a diagonal step is 1.41 tiles long, so diagonal values scale by 16/1.41 = 11.3
    uint8_t unit = (fx && fy) ? 11 : 16;
    along *= unit;
    lateral *= unit;
    return along > 0 && along <= CONE_LEN && 7 * lateral <= 56 + 2 * along;
}

void light_build_shapes(void) BANKED {
    for (uint8_t s = 0; s < LIGHT_SHAPES; s++) {
        light_first[s] = 0xFF;
        light_last[s] = 0;
        for (uint8_t row = 0; row < LIGHT_ROWS; row++) {
            int8_t dy = (int8_t)row - LIGHT_REACH;
            int8_t l = 1, r = 0;
            for (int8_t dx = -LIGHT_REACH; dx <= LIGHT_REACH; dx++)
                if (lit(s, dx, dy)) {
                    if (l > r)
                        l = dx;   // first lit tile of the row
                    r = dx;
                }
            // A row is stored as one span; the shapes are convex enough that it has no gaps
            light_l[s][row] = l;
            light_r[s][row] = r;
            if (l <= r) {
                if (light_first[s] == 0xFF)
                    light_first[s] = row;
                light_last[s] = row;
            }
        }
    }
}

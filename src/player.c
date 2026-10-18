#include <gb/gb.h>
#include "player.h"
#include "input.h"
#include "room.h"
#include "bg.h"
#include "sprites.h"
#include "sprite_player.h"

// Walking speed in 1/256 px per frame. 1.25 px/frame = 75 px/s = about 4.7 metatiles (blocks) a second.
// Diagonals move 1/sqrt(2) as fast on each axis, so the speed is the same in every direction.
#define WALK_SPEED      0x0140
#define WALK_SPEED_DIAG 0x00E2

// Collision box: the feet, relative to the sprite's top left (3/4 view: the head may overlap walls)
#define HB_L 3
#define HB_R 12
#define HB_T 10
#define HB_B 15

// Walk cycle: idle, step A, idle, step B; each for 8 frames
#define ANIM_SHIFT 3

int16_t player_x, player_y;
uint8_t player_facing;
static uint8_t sub_x, sub_y;
static uint8_t anim_timer;
static uint8_t moving;

static spr_sheet_t sheet;
static uint8_t frame_buf[SPR_PLAYER_FRAME_COUNT * 4];

void player_init(uint16_t x, uint16_t y) {
    player_x = x;
    player_y = y;
    sub_x = sub_y = 0x80;
    player_facing = DIR_DOWN;
    anim_timer = 0;
    moving = 0;
    spr_sheet_load(&sheet, BANK(sprite_player), &sprite_player, 0, 0, frame_buf);
}

static uint8_t blocked(int16_t x, int16_t y) {
    return room_solid_at(x + HB_L, y + HB_T) || room_solid_at(x + HB_R, y + HB_T) ||
           room_solid_at(x + HB_L, y + HB_B) || room_solid_at(x + HB_R, y + HB_B);
}

// Moves one axis by speed (1/256 px) in direction dir (-1 or 1), one pixel at a time, stopping at walls
static void move_x(int8_t dir, uint16_t speed) {
    uint16_t acc = (dir > 0 ? sub_x : (uint8_t)~sub_x) + speed;
    uint8_t steps = acc >> 8;
    sub_x = dir > 0 ? (uint8_t)acc : (uint8_t)~(uint8_t)acc;
    while (steps--) {
        if (blocked(player_x + dir, player_y)) {
            sub_x = dir > 0 ? 0xFF : 0x00;  // pressed against the wall
            return;
        }
        player_x += dir;
    }
}

static void move_y(int8_t dir, uint16_t speed) {
    uint16_t acc = (dir > 0 ? sub_y : (uint8_t)~sub_y) + speed;
    uint8_t steps = acc >> 8;
    sub_y = dir > 0 ? (uint8_t)acc : (uint8_t)~(uint8_t)acc;
    while (steps--) {
        if (blocked(player_x, player_y + dir)) {
            sub_y = dir > 0 ? 0xFF : 0x00;
            return;
        }
        player_y += dir;
    }
}

// Facing for each D-pad combination (index: right, left, up, down bits as in J_*); 0xFF = no direction
static const uint8_t facing_for[16] = {
    0xFF,          // none
    DIR_RIGHT,     // J_RIGHT
    DIR_LEFT,      // J_LEFT
    0xFF,          // left + right
    DIR_UP,        // J_UP
    DIR_UP_RIGHT,
    DIR_UP_LEFT,
    DIR_UP,
    DIR_DOWN,      // J_DOWN
    DIR_DOWN_RIGHT,
    DIR_DOWN_LEFT,
    DIR_DOWN,
    0xFF,          // up + down
    DIR_RIGHT,
    DIR_LEFT,
    0xFF,
};

void player_update(void) {
    int8_t dx = 0, dy = 0;
    if (input_held & J_LEFT) dx--;
    if (input_held & J_RIGHT) dx++;
    if (input_held & J_UP) dy--;
    if (input_held & J_DOWN) dy++;

    uint8_t f = facing_for[input_held & 0x0F];
    if (f != 0xFF)
        player_facing = f;

    moving = dx || dy;
    if (moving) {
        uint16_t speed = (dx && dy) ? WALK_SPEED_DIAG : WALK_SPEED;
        if (dx) move_x(dx, speed);
        if (dy) move_y(dy, speed);
        anim_timer++;
    } else {
        anim_timer = 0;
    }
}

void player_draw(void) {
    static const uint8_t step[4] = { 0, 1, 0, 2 };  // idle, a, idle, b
    uint8_t s = moving ? step[(anim_timer >> ANIM_SHIFT) & 3] : 0;
    uint8_t frame, flags = 0;
    switch (player_facing) {
    case DIR_UP:
        frame = SPR_PLAYER_UP_IDLE;
        break;
    case DIR_DOWN:
        frame = SPR_PLAYER_DOWN_IDLE;
        break;
    case DIR_LEFT: case DIR_UP_LEFT: case DIR_DOWN_LEFT:
        frame = SPR_PLAYER_LEFT_IDLE;
        break;
    default:  // right and the right diagonals: the left frames mirrored
        frame = SPR_PLAYER_LEFT_IDLE;
        flags = SPR_FLIPX;
        break;
    }
    spr_draw(&sheet, frame + s, player_x - cam_x, player_y - cam_y, flags);
}

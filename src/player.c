#pragma bank 255
#include <gb/gb.h>
#include "player.h"
#include "items.h"
#include "input.h"
#include "room.h"
#include "bg.h"
#include "sprites.h"
#include "sprite_player.h"

// Walking speed in 1/256 px per frame. 1.25 px/frame = 75 px/s = about 4.7 metatiles (blocks) a second.
// Diagonals move 1/sqrt(2) as fast on each axis, so the speed is the same in every direction.
#define WALK_SPEED      0x0140
#define WALK_SPEED_DIAG 0x00E2
// Sneaking (B held where Figure is, game.c): half speed, and Figure doesn't hear it
#define SNEAK_SPEED      0x00A0
#define SNEAK_SPEED_DIAG 0x0071
// Running from Seek: 1.4 times walking for the whole chase (Minecraft docs 08: the Hotel has no sprinting, so
// the chase hands out Speed)
#define CHASE_SPEED      0x01C0
#define CHASE_SPEED_DIAG 0x013C
// Running for Room 100's lift (player_fast 2): 1.2 times walking (owner, 2026-10-18: only just faster than him)
#define ESCAPE_SPEED      0x0180
#define ESCAPE_SPEED_DIAG 0x010F
// Vitamins (Phase 9a, owner): 0.3 of walking on top of whatever the speed is, so they work in a chase too
#define VITAMIN_BOOST      0x0060
#define VITAMIN_BOOST_DIAG 0x0044

// Walk cycle: idle, step A, idle, step B; each for 8 frames
#define ANIM_SHIFT 3

int16_t player_x, player_y;
uint8_t player_facing;
static uint8_t sub_x, sub_y;
static uint8_t anim_timer;
uint8_t player_moving;
uint8_t player_sneak;
uint8_t player_fast;

static spr_sheet_t sheet;
static uint8_t frame_buf[SPR_PLAYER_FRAME_COUNT * 4];

// How far (px) the player is slid sideways when walking into the edge of a gap, such as a doorway
#define NUDGE_MAX 6

void player_load(void) BANKED {
    spr_sheet_load(&sheet, BANK(sprite_player), &sprite_player, 0, 0, frame_buf);
}

void player_place(int16_t x, int16_t y, uint8_t facing) BANKED {
    player_x = x;
    player_y = y;
    sub_x = sub_y = 0x80;
    player_facing = facing;
    anim_timer = 0;
    player_moving = 0;
}

static uint8_t blocked(int16_t x, int16_t y) {
    return room_solid_at(x + HB_L, y + HB_T) || room_solid_at(x + HB_R, y + HB_T) ||
           room_solid_at(x + HB_L, y + HB_B) || room_solid_at(x + HB_R, y + HB_B);
}

// Moves one axis by speed (1/256 px) in direction dir (-1 or 1), one pixel at a time, stopping at walls.
// Returns 1 if a wall stopped it.
static uint8_t move_x(int8_t dir, uint16_t speed) {
    uint16_t acc = (dir > 0 ? sub_x : (uint8_t)~sub_x) + speed;
    uint8_t steps = acc >> 8;
    sub_x = dir > 0 ? (uint8_t)acc : (uint8_t)~(uint8_t)acc;
    while (steps--) {
        if (blocked(player_x + dir, player_y)) {
            sub_x = dir > 0 ? 0xFF : 0x00;  // pressed against the wall
            return 1;
        }
        player_x += dir;
    }
    return 0;
}

static uint8_t move_y(int8_t dir, uint16_t speed) {
    uint16_t acc = (dir > 0 ? sub_y : (uint8_t)~sub_y) + speed;
    uint8_t steps = acc >> 8;
    sub_y = dir > 0 ? (uint8_t)acc : (uint8_t)~(uint8_t)acc;
    while (steps--) {
        if (blocked(player_x, player_y + dir)) {
            sub_y = dir > 0 ? 0xFF : 0x00;
            return 1;
        }
        player_y += dir;
    }
    return 0;
}

// Walking straight into a wall next to a gap (a doorway, the space between two dressers): if the gap is
// within NUDGE_MAX px to one side, slide 1 px toward it. Saves lining up a 10 px wide player with a
// 16 px door by hand.
static void nudge_sideways_for_y(int8_t dy) {
    for (int8_t k = 1; k <= NUDGE_MAX; k++) {
        if (!blocked(player_x - k, player_y + dy) && !blocked(player_x - 1, player_y)) {
            player_x--;
            return;
        }
        if (!blocked(player_x + k, player_y + dy) && !blocked(player_x + 1, player_y)) {
            player_x++;
            return;
        }
    }
}

static void nudge_sideways_for_x(int8_t dx) {
    for (int8_t k = 1; k <= NUDGE_MAX; k++) {
        if (!blocked(player_x + dx, player_y - k) && !blocked(player_x, player_y - 1)) {
            player_y--;
            return;
        }
        if (!blocked(player_x + dx, player_y + k) && !blocked(player_x, player_y + 1)) {
            player_y++;
            return;
        }
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

void player_update(void) BANKED {
    int8_t dx = 0, dy = 0;
    if (input_held & J_LEFT) dx--;
    if (input_held & J_RIGHT) dx++;
    if (input_held & J_UP) dy--;
    if (input_held & J_DOWN) dy++;

    uint8_t f = facing_for[input_held & 0x0F];
    if (f != 0xFF)
        player_facing = f;

    player_moving = dx || dy;
    if (player_moving) {
        uint16_t speed = player_sneak ? ((dx && dy) ? SNEAK_SPEED_DIAG : SNEAK_SPEED)
                       : player_fast == 2 ? ((dx && dy) ? ESCAPE_SPEED_DIAG : ESCAPE_SPEED)
                       : player_fast  ? ((dx && dy) ? CHASE_SPEED_DIAG : CHASE_SPEED)
                                      : ((dx && dy) ? WALK_SPEED_DIAG : WALK_SPEED);
        if (inv_vit_t && !player_sneak)
            speed += (dx && dy) ? VITAMIN_BOOST_DIAG : VITAMIN_BOOST;
        if (dx && move_x(dx, speed) && !dy)
            nudge_sideways_for_x(dx);
        if (dy && move_y(dy, speed) && !dx)
            nudge_sideways_for_y(dy);
        anim_timer++;
    } else {
        anim_timer = 0;
    }
}

void player_draw(void) BANKED {
    static const uint8_t step[4] = { 0, 1, 0, 2 };  // idle, a, idle, b
    uint8_t s = player_moving ? step[(anim_timer >> (ANIM_SHIFT + player_sneak)) & 3] : 0;
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

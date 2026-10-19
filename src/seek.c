#pragma bank 255
#include <gb/gb.h>
#include <stdlib.h>
#include "seek.h"
#include "run.h"
#include "room.h"
#include "bg.h"
#include "hud.h"
#include "audio.h"
#include "death.h"
#include "player.h"
#include "sprites.h"
#include "room_templates.h"
#include "items.h"
#include "sprite_seek.h"
#include "sprite_puddle.h"
#include "sprite_guide.h"

// The Minecraft numbers, scaled to the screen (one block = 16 px):
#define SPEED        0x0150   // 0.75 of the player's chase speed (player.c CHASE_SPEED 0x01C0): 1.31 px/frame
#define REACH        24       // he catches within this (the Minecraft 2-block radius, less: the camera shows
                              // only 72 px behind you)
#define STOP_SHORT   32       // he stops two blocks short of the end hallway's door
#define RISE_BEHIND  64       // the puddle is this far behind the player along the route: on screen
#define RISE_FRAMES  170      // the puddle (0), bigger (40), his head (80), all of him (130), the chase (170)
#define HOLD_FRAMES  45       // then he stands a moment while the music starts: the player's head start
#define STEP_FRAMES  14       // between his footsteps
#define HEAR_DISTANCE 600     // px of route at which his footsteps start
#define GLOW_IN      20       // the guide's glow, this far into the room from the real door's middle

uint8_t seek_state;
uint8_t seek_room;
int16_t seek_d;
int16_t seek_x, seek_y;

static uint8_t done;                     // bit per chase: it's over (the door slammed)
static uint8_t chase;                    // which one (0, 1)
static uint8_t sub, t, hold, step_t_, anim;
static uint16_t frozen;                  // the crucifix stopped him where he stands (owner: 10 s)
static int16_t rise_d, rise_x, rise_y;
static int16_t trigger;                  // the player's feet passing this along `axis` start his rise
static uint8_t axis;                     // 0 x rising, 1 x falling, 2 y rising, 3 y falling
static uint8_t glow;                     // the guide's glow is shown in this room
static int16_t glow_x, glow_y;
static uint8_t placed_room = 0xFF;       // the room xa/ya are for
static int16_t placed_d;
static int32_t xa, ya;                   // his feet, 24.8
static int16_t sx, sy;                   // px moved per px of route, 8.8
static uint8_t face;                     // his frame for the route's direction; bit 7: mirrored

static spr_sheet_t sheet, puddle, guide;

static uint8_t kind_of(uint8_t room) {
    return room_template_info[run_template[room]][0];
}

static int16_t route_length(uint8_t room) {
    return (int16_t)room_template_length(run_template[room], run_exit[room]);
}

// The route's door centres (copied out of their bank: room_templates.h)
static uint8_t route_buf[4];
static const uint8_t *route(uint8_t room) {
    room_template_route(run_template[room], run_exit[room], route_buf);
    return route_buf;
}

void seek_reset(void) BANKED {
    seek_state = SEEK_OFF;
    done = 0;
    frozen = 0;
    player_fast = 0;
}

void seek_need_sprites(void) BANKED {
    uint8_t k = kind_of(run_room);
    if (!ROOM_KIND_IS_CHASE(k))
        return;
    spr_need(&sheet, BANK(sprite_seek), &sprite_seek);
    if (k == ROOM_KIND_SEEK_START)
        spr_need(&puddle, BANK(sprite_puddle), &sprite_puddle);
    if (k == ROOM_KIND_SEEK_4WAY)
        spr_need(&guide, BANK(sprite_guide), &sprite_guide);
}

// The point d px along room's route (one division each way: only on entering a room)
static int16_t along(int16_t a, int16_t b, int16_t d, int16_t len) {
    return a + (int16_t)(((int32_t)(b - a) * d) / len);
}

// The start hallway: where he'll rise, and where the player's feet start it
static void plan_rise(void) {
    const uint8_t *r = route(run_room);
    int16_t ax = (int16_t)r[0] << 3, ay = (int16_t)r[1] << 3, bx = (int16_t)r[2] << 3, by = (int16_t)r[3] << 3;
    int16_t len = route_length(run_room);
    rise_d = len - (len >> 2) - RISE_BEHIND;   // three quarters along, less the distance behind the player
    for (uint8_t i = 0; i < room_obj_count; i++)
        if (room_objs[i].type == OBJ_SEEK) {     // the room's own spot: its distance along the route
            int16_t px = ((int16_t)room_objs[i].x << 4) + 8, py = ((int16_t)room_objs[i].y << 4) + 8;
            rise_d = (int16_t)((((int32_t)(px - ax) * (bx - ax)) + ((int32_t)(py - ay) * (by - ay))) / len);
        }
    if (rise_d < 16)
        rise_d = 16;
    rise_x = along(ax, bx, rise_d, len);
    rise_y = along(ay, by, rise_d, len);
    int16_t tx = along(ax, bx, rise_d + RISE_BEHIND, len), ty = along(ay, by, rise_d + RISE_BEHIND, len);
    if (abs(bx - ax) > abs(by - ay)) {
        axis = bx > ax ? 0 : 1;
        trigger = tx;
    } else {
        axis = by > ay ? 2 : 3;
        trigger = ty;
    }
}

// The real door of a four-way room: its glow, a little way into the room
static void place_glow(void) {
    glow = 0;
    if (kind_of(run_room) != ROOM_KIND_SEEK_4WAY)
        return;
    uint8_t k = 0;
    for (uint8_t i = 0; i < room_obj_count; i++) {
        const room_obj_t *o = &room_objs[i];
        if (o->type != OBJ_EXIT || k++ != run_exit[run_room])
            continue;
        glow_x = ((int16_t)o->x << 4) + (o->w << 3);
        glow_y = ((int16_t)o->y << 4) + (o->h << 3);
        switch (o->param) {
        case WALL_TOP:   glow_y += GLOW_IN; break;
        case WALL_LEFT:  glow_x += GLOW_IN; break;
        default:         glow_x -= GLOW_IN; break;
        }
        glow = 1;
    }
}

static void slam(void) {
    seek_state = SEEK_OFF;
    done |= 1 << chase;
    player_fast = 0;
    music_play(MUSIC_NONE);
    sfx_play(SFX_SLAM);
}

void seek_room_entered(void) BANKED {
    uint8_t k = kind_of(run_room);
    placed_room = 0xFF;
    place_glow();
    if (seek_state <= SEEK_WAIT) {
        seek_state = SEEK_OFF;
        if (k != ROOM_KIND_SEEK_START)
            return;
        chase = run_room >= RUN_CHASE_START(1);
        if (done & (1 << chase))
            return;
        plan_rise();
        seek_state = SEEK_WAIT;
        return;
    }
    // Chasing: through the end hallway's door, it slams behind you
    if (run_room == RUN_CHASE_END(chase) + 1)
        slam();
}

static uint8_t passed_trigger(void) {
    int16_t fx = player_x + PLAYER_FEET_X, fy = player_y + PLAYER_FEET_Y;
    switch (axis) {
    case 0: return fx >= trigger;
    case 1: return fx <= trigger;
    case 2: return fy >= trigger;
    default: return fy <= trigger;
    }
}

// Along the route by one frame's speed; stops short of the end hallway's door
static void travel(void) {
    uint16_t s = (uint16_t)sub + (SPEED & 0xFF);
    sub = (uint8_t)s;
    seek_d += (int16_t)(SPEED >> 8) + (int16_t)(s >> 8);
    for (;;) {
        int16_t len = route_length(seek_room);
        if (kind_of(seek_room) == ROOM_KIND_SEEK_END) {
            if (seek_d >= len - STOP_SHORT) {
                seek_d = len - STOP_SHORT;
                seek_state = SEEK_STOP;
            }
            return;
        }
        if (seek_d < len)
            return;
        seek_d -= len;
        seek_room++;
    }
}

// Where he is in this room: worked out once a room (two divisions), then stepped (entity.c's way)
static void place(void) {
    if (placed_room != seek_room) {
        const uint8_t *r = route(seek_room);
        int16_t ax = (int16_t)r[0] << 3, ay = (int16_t)r[1] << 3, bx = (int16_t)r[2] << 3, by = (int16_t)r[3] << 3;
        int16_t len = route_length(seek_room);
        xa = ((int32_t)ax << 8) + (((int32_t)(bx - ax) * seek_d << 8) / len);
        ya = ((int32_t)ay << 8) + (((int32_t)(by - ay) * seek_d << 8) / len);
        sx = (int16_t)(((int32_t)(bx - ax) << 8) / len);
        sy = (int16_t)(((int32_t)(by - ay) << 8) / len);
        // His frames: his back going up, his side going across (mirrored to the right), his front going down
        if (abs(by - ay) >= abs(bx - ax))
            face = by < ay ? SPR_SEEK_BACK_A : SPR_SEEK_FRONT;
        else
            face = bx < ax ? SPR_SEEK_LEFT_A : (SPR_SEEK_LEFT_A | 0x80);
        placed_room = seek_room;
    } else {
        int16_t moved = seek_d - placed_d;
        xa += (int16_t)(sx * moved);
        ya += (int16_t)(sy * moved);
    }
    placed_d = seek_d;
    seek_x = (int16_t)(xa >> 8);
    seek_y = (int16_t)(ya >> 8);
}

// How far he is behind the player, roughly (the player counts as at their room's entry, plus a little)
static uint16_t distance(void) {
    if (seek_room == run_room)
        return (uint16_t)(abs(seek_x - (player_x + 8)) + abs(seek_y - (player_y + PLAYER_FEET_Y)));
    uint16_t d = (uint16_t)(route_length(seek_room) - seek_d) + 32;
    for (uint8_t r = seek_room + 1; r < run_room && d < HEAR_DISTANCE; r++)
        d += route_length(r);
    return d;
}

// He has you, unless the crucifix stops him where he stands for 10 s (Minecraft docs 12: he's the room, so he's
// stopped rather than sent off)
static uint8_t caught(void) {
    if (items_crucifix()) {
        frozen = FREEZE_FRAMES;
        return 0;
    }
    return DEATH_SEEK;
}

uint8_t seek_update(void) BANKED {
    anim++;
    switch (seek_state) {
    case SEEK_WAIT:
        if (!passed_trigger())
            return 0;
        seek_state = SEEK_RISE;
        t = 0;
        hud_hide_now();
        music_play(MUSIC_NONE);
        sfx_play(SFX_SEEK_RISE);
        return 0;
    case SEEK_RISE:
        if (++t < RISE_FRAMES)
            return 0;
        seek_state = SEEK_CHASE;
        seek_room = run_room;
        seek_d = rise_d;
        sub = 0;
        hold = HOLD_FRAMES;
        step_t_ = 0;
        placed_room = 0xFF;
        player_fast = 1;
        music_play(MUSIC_SEEK);
        return 0;
    case SEEK_CHASE:
    case SEEK_STOP:
        break;
    default:
        return 0;
    }
    if (frozen) {
        frozen--;
        return 0;
    }
    if (hold)
        hold--;
    else if (seek_state == SEEK_CHASE)
        travel();
    // Behind him is the same mistake as being caught by him (only while he moves: once stopped, the door
    // ends it, Minecraft docs 08)
    if (seek_state == SEEK_CHASE && seek_room > run_room)
        return caught();
    if (seek_room == run_room) {
        place();
        int16_t dx = abs(seek_x - (player_x + 8)), dy = abs(seek_y - (player_y + PLAYER_FEET_Y));
        if (dx < REACH && dy < REACH && (uint16_t)(dx * dx) + (uint16_t)(dy * dy) < (uint16_t)REACH * REACH)
            return caught();
    } else {
        placed_room = 0xFF;
    }
    if (seek_state == SEEK_CHASE && !hold && ++step_t_ >= STEP_FRAMES) {
        step_t_ = 0;
        uint16_t d = distance();
        if (d < HEAR_DISTANCE)
            sfx_play_vol(SFX_SEEK_STEP, 15 - (uint8_t)(d / (HEAR_DISTANCE / 14)));
    }
    return 0;
}

void seek_draw(void) BANKED {
    if (seek_state == SEEK_RISE || (seek_state == SEEK_CHASE && hold && seek_room == run_room)) {
        // The puddle first (drawn over his feet), then him coming up out of it
        int16_t px = rise_x - 16 - cam_x, py = rise_y - 8 - cam_y;
        spr_draw_big(&puddle, seek_state == SEEK_RISE && t < 40 ? SPR_PUDDLE_SMALL : SPR_PUDDLE_LARGE, 2, 1,
                     px, py, 0);
        if (seek_state == SEEK_RISE) {
            if (t >= 130)
                spr_draw_big(&sheet, SPR_SEEK_FRONT, 2, 2, rise_x - 12 - cam_x, rise_y - 30 - cam_y, 0);
            else if (t >= 80)
                spr_draw_big(&sheet, SPR_SEEK_FRONT, 2, 1, rise_x - 12 - cam_x, rise_y - 16 - cam_y, 0);
            return;
        }
    }
    if (glow && seek_chasing())
        spr_draw(&guide, (anim & 8) ? SPR_GUIDE_SMALL : SPR_GUIDE_GLOW, glow_x - 8 - cam_x, glow_y - 8 - cam_y, 0);
    if (!seek_chasing() || seek_room != run_room)
        return;
    uint8_t f = face & 0x7F;
    if (f != SPR_SEEK_FRONT && seek_state == SEEK_CHASE && !hold && (anim & 8))
        f += SPR_SEEK_BACK_B - SPR_SEEK_BACK_A;   // the other step (back_b, left_b follow back_a, left_a)
    // 24 wide at the left of a 32x32 frame: mirrored, the empty column comes first
    int16_t x = seek_x - 12 - cam_x - ((face & 0x80) ? 8 : 0);
    spr_draw_big(&sheet, f, 2, 2, x, seek_y - 30 - cam_y, (face & 0x80) ? SPR_FLIPX : 0);
}

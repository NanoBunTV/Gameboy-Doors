#pragma bank 255
#include <gb/gb.h>
#include <string.h>
#include <stdlib.h>
#include "figure.h"
#include "run.h"
#include "room.h"
#include "bg.h"
#include "player.h"
#include "hide.h"
#include "audio.h"
#include "death.h"
#include "sprite_figure.h"
#include "sprite_marks.h"

// He follows the rules of the Minecraft version's Figure (docs 10):
// - Walking is heard anywhere in the room (his hearing has no range); sneaking makes no sound at all.
// - He finishes what he started: a noise he's walking at isn't replaced by a newer one until he's got there,
//   unless the newer one is in the same cell (the same player still walking there).
// - Wandering he's slower than walking and faster than sneaking; hunting he's faster than walking, so once
//   he's coming a closet is the answer. He never finds a hidden player.
//
// Finding his way: a flood fill from his goal over the room's metatiles gives every floor cell its distance
// in steps, and he walks downhill. It runs FILL_PER_FRAME cells a frame while he stands (about a third of a
// second for the Library), and lives in the room map's unused tail: the Library is at most half of ROOM_MAX
// less the queue (scripts/assets.py checks). An A* would need a heap; the fill is a ring of cells.

#define WANDER_SPEED  0x00C0   // 0.75 px/frame (sneaking 0.63, walking 1.25)
#define HUNT_SPEED    0x0170   // 1.44 px/frame
#define NOISE_EVERY   8        // frames between the noises a walking player makes
#define HEAR_COOLDOWN 180      // his roar on hearing you, at most every 3 s (Minecraft figure.scare)
#define FILL_PER_FRAME 32       // cells a frame (more ran past the DMG's frame)
#define STEP_WANDER   28       // frames between footsteps
#define STEP_HUNT     14
#define TOUCH_X       10       // contact: his feet within this of yours, across and ...
#define TOUCH_Y       8        // ... up and down
#define FAR           255      // not reached by the fill
#define NOGO          254      // never entered (Room 100's breaker room): never FAR, never downhill
#define QUEUE_BYTES   512      // 256 cells (x, y); scripts/assets.py LIBRARY_MAX_CELLS assumes this
// The escape (Room 100): he follows the trail the player's feet leave, a point every TRAIL_STEP px, kept in
// the route search's space (it isn't needed then): 256 points of 4 bytes, at most 2,048 px behind
#define CHASE_SPEED   0x0150   // 1.31 px/frame: 0.75 of the player's run (seek.c's SPEED)
#define TRAIL_STEP    8
#define CHASE_SCARE   180      // his roar every 3 s while he comes (Minecraft figure.scare, the escape's metronome)

spr_sheet_t figure_marks;
static spr_sheet_t sheet;

uint8_t figure_state;
int16_t figure_x, figure_y;
uint8_t figure_hunting;          // walking at a noise (else wandering)
static uint8_t sub, grace, noise_t, hear_cool, step_t_, pause, anim;
static uint16_t speed;
static uint8_t goal_mx, goal_my;
static int16_t goal_x, goal_y;
static uint8_t *dist;            // per cell: steps to the goal (room map's tail)
static uint8_t *queue;           // the fill's ring, x then y per cell
static uint8_t qh, qt;
static uint16_t rng;
static uint8_t nogo_x, nogo_y, nogo_w, nogo_h;
static int16_t *trail;           // the escape: x, y per point (in dist's place)
static uint8_t th, tt;           // head (next written), tail (next he walks to)
static int16_t last_x, last_y;   // the last point written

static uint16_t rnd(void) {
    rng ^= rng << 7;
    rng ^= rng >> 9;
    rng ^= rng << 8;
    return rng;
}

void figure_need_sprites(void) BANKED {
    spr_need(&figure_marks, BANK(sprite_marks), &sprite_marks);   // the glints, even once he's gone
    if (figure_state != FIG_OFF)
        spr_need(&sheet, BANK(sprite_figure), &sprite_figure);
}

static uint8_t walkable(uint16_t c) {
    return !(mt_flags[room_map[c]] & MTF_SOLID);
}

static uint8_t in_nogo(uint8_t mx, uint8_t my) {
    return (uint8_t)(mx - nogo_x) < nogo_w && (uint8_t)(my - nogo_y) < nogo_h;
}

void figure_nogo(uint8_t x, uint8_t y, uint8_t w, uint8_t h) BANKED {
    nogo_x = x;
    nogo_y = y;
    nogo_w = w;
    nogo_h = h;
}

static void fill_start(uint8_t mx, uint8_t my) {
    uint16_t cells = (uint16_t)room_w * room_h;
    memset(dist, FAR, cells);
    for (uint8_t y = 0; y < nogo_h; y++)   // the fill never enters the no-go area, so no route crosses it
        memset(dist + room_row[nogo_y + y] + nogo_x, NOGO, nogo_w);
    dist[room_row[my] + mx] = 0;
    queue[0] = mx;
    queue[1] = my;
    qt = 0;
    qh = 1;
    goal_mx = mx;
    goal_my = my;
    figure_state = FIG_FILL;
}

// One cell's neighbour: reached, unless it's a wall, reached already, or the ring is full (never in the
// Library's aisles). No bounds checks: only floor cells are queued, and a room's edge is always wall.
#define TRY(dp, mp, nx, ny)                                                              if (*(dp) == FAR && !(mt_flags[*(mp)] & MTF_SOLID) && (uint8_t)(qh + 1) != qt) {         *(dp) = d;                                                                           uint8_t *o = queue + ((uint16_t)qh << 1);                                            o[0] = (nx);                                                                         o[1] = (ny);                                                                         qh++;                                                                            }

static void fill_step(void) {
    uint8_t w = room_w;
    for (uint8_t n = FILL_PER_FRAME; n && qt != qh; n--) {
        uint8_t *q = queue + ((uint16_t)qt << 1);
        uint8_t x = q[0], y = q[1];
        qt++;
        uint16_t c = room_row[y] + x;
        uint8_t *dp = dist + c;
        const uint8_t *mp = room_map + c;
        uint8_t d = *dp + 1;
        TRY(dp - 1, mp - 1, x - 1, y)
        TRY(dp + 1, mp + 1, x + 1, y)
        TRY(dp - w, mp - w, x, y - 1)
        TRY(dp + w, mp + w, x, y + 1)
    }
    if (qt == qh)
        figure_state = FIG_WALK;
}

static void wander(void) {
    uint8_t fx = (uint8_t)(figure_x >> 4), fy = (uint8_t)(figure_y >> 4);
    for (uint8_t i = 0; i < 24; i++) {
        uint8_t mx = (uint8_t)(rnd() % room_w), my = (uint8_t)(rnd() % room_h);
        if (walkable(room_row[my] + mx) && !in_nogo(mx, my) && abs((int8_t)(mx - fx)) + abs((int8_t)(my - fy)) > 4) {
            figure_hunting = 0;
            speed = WANDER_SPEED;
            goal_x = ((int16_t)mx << 4) + 8;
            goal_y = ((int16_t)my << 4) + 8;
            fill_start(mx, my);
            return;
        }
    }
    pause = 30;   // try again shortly
}

static void hear(int16_t px, int16_t py) {
    uint8_t mx = (uint8_t)(px >> 4), my = (uint8_t)(py >> 4);
    if (in_nogo(mx, my))
        return;   // he can't hear into the breaker room (Minecraft docs 10: it's the one place to breathe)
    if (!hear_cool) {   // heard whether or not he acts on it (Minecraft docs 10, "The audio")
        sfx_play(SFX_FIGURE_HEAR);
        hear_cool = HEAR_COOLDOWN;
    }
    if (figure_hunting && figure_state != FIG_PAUSE) {
        if (mx == goal_mx && my == goal_my) {   // the same player still walking there
            goal_x = px;
            goal_y = py;
        }
        return;   // committed
    }
    figure_hunting = 1;
    speed = HUNT_SPEED;
    goal_x = px;
    goal_y = py;
    fill_start(mx, my);
}

static void arrived(void) {
    figure_state = FIG_PAUSE;
    pause = figure_hunting ? 40 : 30 + (uint8_t)(rnd() % 60);
    figure_hunting = 0;
}

// One pixel downhill along the fill (along the middle of each cell, so he stays on the floor). Returns 1
// when he has got there, or can't (no route from where he is).
static uint8_t step_once(void) {
    uint8_t mx = (uint8_t)(figure_x >> 4), my = (uint8_t)(figure_y >> 4);
    uint16_t c = room_row[my] + mx;
    uint8_t d = dist[c];
    int16_t tx, ty, cx = ((int16_t)mx << 4) + 8, cy = ((int16_t)my << 4) + 8;
    if (d == FAR)
        return 1;
    if (!d) {
        tx = goal_x;
        ty = goal_y;
    } else if (mx + 1 < room_w && dist[c + 1] < d) {
        tx = cx + 16;
        ty = cy;
    } else if (mx && dist[c - 1] < d) {
        tx = cx - 16;
        ty = cy;
    } else if (my + 1 < room_h && dist[c + room_w] < d) {
        tx = cx;
        ty = cy + 16;
    } else {
        tx = cx;
        ty = cy - 16;
    }
    // Across first to the cell's middle, then along
    if (d && tx != cx && figure_y != cy)
        tx = figure_x, ty = cy;
    else if (d && ty != cy && figure_x != cx)
        tx = cx, ty = figure_y;
    if (figure_x != tx)
        figure_x += figure_x < tx ? 1 : -1;
    else if (figure_y != ty)
        figure_y += figure_y < ty ? 1 : -1;
    else
        return !d;
    return 0;
}

static void walk(void) {
    uint16_t s = (uint16_t)sub + (speed & 0xFF);
    sub = (uint8_t)s;
    uint8_t steps = (uint8_t)(speed >> 8) + (uint8_t)(s >> 8);
    while (steps--)
        if (step_once()) {
            arrived();
            return;
        }
}

static uint16_t player_dist(void) {
    return (uint16_t)abs(figure_x - (player_x + PLAYER_FEET_X)) + (uint16_t)abs(figure_y - (player_y + PLAYER_FEET_Y));
}

void figure_start(uint8_t mx, uint8_t my, uint8_t deaf) BANKED {
    uint16_t cells = (uint16_t)room_w * room_h;
    dist = room_map + cells;
    queue = dist + cells;
    figure_x = ((int16_t)mx << 4) + 8;
    figure_y = ((int16_t)my << 4) + 8;
    sub = 0;
    figure_hunting = 0;
    grace = deaf;
    noise_t = hear_cool = step_t_ = anim = 0;
    rng = run_seed ^ 0xF16Eu;
    if (!rng)
        rng = 1;
    figure_state = FIG_PAUSE;
    pause = 60;
    nogo_w = 0;
}

void figure_stop(void) BANKED {
    figure_state = FIG_OFF;
}

void figure_go(int16_t px, int16_t py) BANKED {
    figure_hunting = 1;
    speed = HUNT_SPEED;
    goal_x = px;
    goal_y = py;
    fill_start((uint8_t)(px >> 4), (uint8_t)(py >> 4));
}

void figure_escape(uint8_t mx, uint8_t my) BANKED {
    uint16_t cells = (uint16_t)room_w * room_h;
    trail = (int16_t *)(room_map + cells);
    figure_x = ((int16_t)mx << 4) + 8;
    figure_y = ((int16_t)my << 4) + 8;
    sub = 0;
    th = tt = 0;
    last_x = figure_x;
    last_y = figure_y;
    figure_hunting = 1;
    hear_cool = 0;
    step_t_ = 0;
    figure_state = FIG_CHASE;
}

// The escape: the player's feet leave a point every TRAIL_STEP px; he walks from point to point (each a
// few px from the last, so never through a wall), and straight at them once he's on the newest
static void chase(void) {
    int16_t px = player_x + PLAYER_FEET_X, py = player_y + PLAYER_FEET_Y;
    if ((uint16_t)abs(px - last_x) + (uint16_t)abs(py - last_y) >= TRAIL_STEP && (uint8_t)(th + 1) != tt) {
        int16_t *p = trail + ((uint16_t)th << 1);
        p[0] = last_x = px;
        p[1] = last_y = py;
        th++;
    }
    uint16_t s = (uint16_t)sub + (CHASE_SPEED & 0xFF);
    sub = (uint8_t)s;
    uint8_t steps = (uint8_t)(CHASE_SPEED >> 8) + (uint8_t)(s >> 8);
    while (steps--) {
        int16_t tx = px, ty = py;
        if (tt != th) {
            const int16_t *p = trail + ((uint16_t)tt << 1);
            tx = p[0];
            ty = p[1];
            if (abs(tx - figure_x) + abs(ty - figure_y) <= 1) {
                tt++;
                continue;
            }
        }
        // A pixel along whichever axis is further off
        int16_t dx = tx - figure_x, dy = ty - figure_y;
        if (abs(dx) >= abs(dy)) {
            if (dx)
                figure_x += dx > 0 ? 1 : -1;
        } else {
            figure_y += dy > 0 ? 1 : -1;
        }
    }
    if (!hear_cool) {
        sfx_play(SFX_FIGURE_HEAR);
        hear_cool = CHASE_SCARE;
    }
}

uint8_t figure_update(void) BANKED {
    if (figure_state == FIG_OFF)
        return DEATH_NONE;
    anim++;
    if (grace)
        grace--;
    if (hear_cool)
        hear_cool--;
    // A walking player makes a noise now and every NOISE_EVERY frames while they keep walking (not in the
    // escape: he's deaf then, Minecraft docs 10)
    if (figure_state != FIG_CHASE && player_moving && !player_sneak && !hide_hidden && !grace) {
        if (!noise_t) {
            noise_t = NOISE_EVERY;
            hear(player_x + PLAYER_FEET_X, player_y + PLAYER_FEET_Y);
        }
        noise_t--;
    } else {
        noise_t = 0;
    }
    switch (figure_state) {
    case FIG_FILL:
        fill_step();
        break;
    case FIG_CHASE:
    case FIG_WALK:
        if (figure_state == FIG_CHASE)
            chase();
        else
            walk();
        if (++step_t_ >= (figure_hunting ? STEP_HUNT : STEP_WANDER)) {
            // Louder as he gets closer: full volume within a screen's quarter, faint across the room
            step_t_ = 0;
            uint16_t d = player_dist() >> 5;
            sfx_play_vol(SFX_FIGURE_STEP, d >= 14 ? 1 : 15 - (uint8_t)d);
        }
        break;
    default:   // FIG_PAUSE
        if (!--pause)
            wander();
        break;
    }
    if ((!hide_hidden || figure_state == FIG_CHASE) && abs(figure_x - (player_x + PLAYER_FEET_X)) < TOUCH_X &&
        abs(figure_y - (player_y + PLAYER_FEET_Y)) < TOUCH_Y)
        return DEATH_FIGURE;
    return DEATH_NONE;
}

void figure_draw(void) BANKED {
    if (figure_state == FIG_OFF)
        return;
    // 32x48, feet at the bottom middle. Walking: stand, left foot up, stand, right foot up (owner, 2026-10-18:
    // with two frames only one foot ever lifted)
    int16_t sx = figure_x - 16 - cam_x, sy = figure_y - 46 - cam_y;
    uint8_t f = SPR_FIGURE_F0;
    if (figure_state == FIG_WALK || figure_state == FIG_CHASE) {
        uint8_t step = (figure_hunting ? anim >> 3 : anim >> 4) & 3;
        if (step == 1)
            f = SPR_FIGURE_F1;
        else if (step == 3)
            f = SPR_FIGURE_F2;
    }
    spr_draw_big(&sheet, f, 2, 3, sx, sy, 0);
    // Off screen: an arrow at the edge nearest him (owner: footsteps and an edge arrow)
    int16_t ox = sx < -32 ? -32 - sx : sx > 160 ? sx - 160 : 0;
    int16_t oy = sy < -48 ? -48 - sy : sy > 144 ? sy - 144 : 0;
    if (!ox && !oy)
        return;
    int16_t bottom = (spr_clip_y < 144 ? (int16_t)spr_clip_y : 128) - 16;
    int16_t ay = figure_y - 24 - cam_y, ax = figure_x - 4 - cam_x;
    ay = ay < 0 ? 0 : ay > bottom ? bottom : ay;
    ax = ax < 0 ? 0 : ax > 152 ? 152 : ax;
    if (ox >= oy)
        spr_draw(&figure_marks, SPR_MARKS_ARROW_L, sx < 0 ? 0 : 152, ay, sx < 0 ? 0 : SPR_FLIPX);
    else
        spr_draw(&figure_marks, sy < 0 ? SPR_MARKS_ARROW_U : SPR_MARKS_ARROW_D, ax, sy < 0 ? 0 : bottom, 0);
}

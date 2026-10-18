#pragma bank 255
#include <gb/gb.h>
#include <stdlib.h>
#include "entity.h"
#include "run.h"
#include "room.h"
#include "bg.h"
#include "hide.h"
#include "death.h"
#include "audio.h"
#include "player.h"
#include "sprites.h"
#include "platform.h"
#include "room_templates.h"
#include "sprite_rush.h"
#include "sprite_ambush.h"

// Numbers from the Minecraft config (rush, ambush, entities), one block = 16 px, 20 ticks = 60 frames
#define RUSH_SPEED    0x0555   // 1 block/tick = 5.33 px/frame (8.8 fixed point)
#define AMBUSH_SPEED  0x0888   // 1.6 blocks/tick
#define RUSH_RADIUS   96       // kill-radius 6 blocks
#define AMBUSH_RADIUS 128      // 8 blocks
#define RUSH_WARN     120      // warning-seconds 2
#define AMBUSH_WARN   240      // 4
#define AMBUSH_PAUSE  300      // rear-pause-seconds 5
#define QUEUE_PATIENCE 8       // entities.queue-patience (rooms)
#define FIRE_DELAY    45       // frames between walking in and the flicker
#define START_BEHIND  1000     // px of route behind the player's door he starts from (about 3 s for Rush)
#define HEAR_DISTANCE 768      // px of route at which his rumble starts (near-distance is 25-30 blocks)

uint8_t entity_state, entity_kind, entity_room, entity_queued;
int16_t entity_d;
int16_t entity_x, entity_y;

static uint8_t patience;
static uint8_t sub;            // fraction of a pixel
static int8_t dir;             // +1 toward the far door, -1 back
static uint8_t end_room, start_room;
static int16_t start_d;
static uint16_t timer;
static uint8_t legs;           // Ambush: traverses left, one per direction (min/max-movements 3-5)
static uint8_t anim;

static spr_sheet_t rush_sheet, ambush_sheet;
static uint8_t rush_frames[SPR_RUSH_FRAME_COUNT * 4], ambush_frames[SPR_AMBUSH_FRAME_COUNT * 4];

uint8_t entity_load(uint8_t first_tile, uint8_t first_pal) BANKED {
    first_tile += spr_sheet_load(&rush_sheet, BANK(sprite_rush), &sprite_rush, first_tile, first_pal, rush_frames);
    first_tile += spr_sheet_load(&ambush_sheet, BANK(sprite_ambush), &sprite_ambush, first_tile, first_pal + 1,
                                 ambush_frames);
    return first_tile;
}

void entity_reset(void) BANKED {
    entity_state = ENT_IDLE;
    entity_queued = 0;
    sfx_rumble(0, RUMBLE_RUSH);
}

static int16_t route_length(uint8_t room) {
    return (int16_t)room_template_length[run_template[room]];
}

static void fire(void) {
    entity_kind = entity_queued;
    entity_queued = 0;
    entity_state = ENT_WAIT;
    timer = FIRE_DELAY;
    end_room = run_room;
    // Start START_BEHIND px back along the route from this room's entry (or at the very start of the run)
    int16_t back = START_BEHIND;
    uint8_t r = run_room;
    start_room = 0;
    start_d = 0;
    while (r > 0) {
        r--;
        int16_t len = route_length(r);
        if (back <= len) {
            start_room = r;
            start_d = len - back;
            break;
        }
        back -= len;
    }
    entity_room = start_room;
    entity_d = start_d;
    sub = 0;
    dir = 1;
    // Ambush: 3-5 legs, from the seed so a run replays the same (an odd count ends at the far door)
    legs = entity_kind == ENTITY_AMBUSH ? 3 + (uint8_t)((run_seed ^ run_room) % 3) : 1;
}

void entity_room_entered(uint8_t first_time) BANKED {
    if (!first_time || entity_state != ENT_IDLE)
        return;
    if (!entity_queued && (run_flags[run_room] & RF_SPAWN)) {
        entity_queued = (run_flags[run_room] & RF_AMBUSH) ? ENTITY_AMBUSH : ENTITY_RUSH;
        patience = QUEUE_PATIENCE;
    }
    if (!entity_queued)
        return;
    if (hide_free_spots())
        fire();
    else if (!--patience)
        entity_queued = 0;   // nowhere to hide for too long: give up rather than block every later roll
}

// Moves along the route by one frame's speed. Returns 1 at the end of a leg.
static uint8_t travel(void) {
    uint16_t speed = entity_kind == ENTITY_AMBUSH ? AMBUSH_SPEED : RUSH_SPEED;
    uint16_t s = (uint16_t)sub + (speed & 0xFF);
    sub = (uint8_t)s;
    int16_t step = (int16_t)(speed >> 8) + (int16_t)(s >> 8);
    if (dir > 0) {
        entity_d += step;
        while (entity_d >= route_length(entity_room)) {
            if (entity_room == end_room) {
                entity_d = route_length(entity_room);
                return 1;
            }
            entity_d -= route_length(entity_room);
            entity_room++;
        }
    } else {
        entity_d -= step;
        while (entity_room > start_room && entity_d < 0) {
            entity_room--;
            entity_d += route_length(entity_room);
        }
        if (entity_room == start_room && entity_d <= start_d) {
            entity_d = start_d;
            return 1;
        }
    }
    return 0;
}

// Where he is in the current room, from the route's two door centres
static void place(void) {
    uint8_t t = run_template[entity_room];
    const uint8_t *r = room_template_route[t];
    int16_t ax = (int16_t)r[0] << 3, ay = (int16_t)r[1] << 3;
    int16_t bx = (int16_t)r[2] << 3, by = (int16_t)r[3] << 3;
    int16_t len = route_length(entity_room);
    entity_x = ax + (int16_t)(((int32_t)(bx - ax) * entity_d) / len);
    entity_y = ay + (int16_t)(((int32_t)(by - ay) * entity_d) / len);
}

// How far he is from the player along the route, in px (roughly: the player counts as at their room's
// entry door, plus their straight distance from it)
static uint16_t route_distance(void) {
    if (entity_room == run_room) {
        int16_t dx = abs(entity_x - (player_x + 8)), dy = abs(entity_y - (player_y + 8));
        return (uint16_t)(dx + dy);
    }
    uint16_t d = 0;
    if (entity_room < run_room) {
        d = (uint16_t)(route_length(entity_room) - entity_d);
        for (uint8_t r = entity_room + 1; r < run_room && d < HEAR_DISTANCE; r++)
            d += route_length(r);
    } else {
        d = (uint16_t)entity_d;
        for (uint8_t r = run_room + 1; r < entity_room && d < HEAR_DISTANCE; r++)
            d += route_length(r);
    }
    return d;
}

static uint8_t caught(void) {
    if (entity_room != run_room || hide_hidden)
        return 0;
    int16_t dx = abs(entity_x - (player_x + 8)), dy = abs(entity_y - (player_y + 8));
    uint8_t radius = entity_kind == ENTITY_AMBUSH ? AMBUSH_RADIUS : RUSH_RADIUS;
    if (dx >= radius || dy >= radius)
        return 0;
    return (uint16_t)(dx * dx) + (uint16_t)(dy * dy) < (uint16_t)radius * radius;
}

static void finish(void) {
    entity_state = ENT_IDLE;
    sfx_rumble(0, RUMBLE_RUSH);
}

uint8_t entity_update(void) BANKED {
    anim++;
    switch (entity_state) {
    case ENT_WAIT:
        if (--timer)
            return 0;
        entity_state = ENT_WARN;
        if (entity_kind == ENTITY_AMBUSH) {
            timer = AMBUSH_WARN;
            sfx_play(SFX_AMBUSH);
        } else {
            timer = RUSH_WARN;
            sfx_play(SFX_FLICKER);
        }
        return 0;
    case ENT_WARN:
        if (--timer)
            return 0;
        entity_state = ENT_MOVE;
        break;
    case ENT_PAUSE:
        sfx_rumble(0, RUMBLE_AMBUSH);
        if (--timer)
            return 0;
        entity_state = ENT_MOVE;
        dir = 1;
        break;
    case ENT_MOVE:
        break;
    default:
        return 0;
    }

    // ENT_MOVE
    if (travel()) {
        if (!--legs) {
            finish();
            return 0;
        }
        if (dir > 0) {
            dir = -1;              // turned at the far door
        } else {
            entity_state = ENT_PAUSE;  // back at the rear: wait, then come again
            timer = AMBUSH_PAUSE;
        }
    }
    if (entity_room == run_room)
        place();
    uint16_t d = route_distance();
    sfx_rumble(d >= HEAR_DISTANCE ? 0 : 15 - (uint8_t)(d / (HEAR_DISTANCE / 15)),
               entity_kind == ENTITY_AMBUSH ? RUMBLE_AMBUSH : RUMBLE_RUSH);
    if (caught()) {
        finish();
        return entity_kind == ENTITY_AMBUSH ? DEATH_AMBUSH : DEATH_RUSH;
    }
    return 0;
}

void entity_draw(void) BANKED {
    if (entity_state != ENT_MOVE || entity_room != run_room)
        return;
    spr_sheet_t *sh = entity_kind == ENTITY_AMBUSH ? &ambush_sheet : &rush_sheet;
    uint8_t frame = (anim & 4) ? SPR_RUSH_F1 : SPR_RUSH_F0;   // both sheets: two 32x32 frames, same layout
    spr_draw_big(sh, frame, 2, 2, entity_x - 16 - cam_x, entity_y - 16 - cam_y);
}

// The lights stutter while he's announced and while he's in the room: an irregular pattern, not a switch
static const uint8_t flicker[16] = { 0, 2, 0, 0, 1, 3, 0, 2, 0, 0, 3, 1, 0, 2, 2, 0 };

void entity_lights(uint8_t *dim, uint8_t *tint) BANKED {
    *dim = 0;
    *tint = TINT_NONE;
    if (entity_state == ENT_WARN || (entity_state == ENT_MOVE && entity_room == run_room)) {
        *dim = flicker[(anim >> 2) & 15];
        if (entity_kind == ENTITY_AMBUSH)
            *tint = TINT_GREEN;
    }
}

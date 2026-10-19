#pragma bank 255
#include <gb/gb.h>
#include <stdlib.h>
#include "entity.h"
#include "run.h"
#include "room.h"
#include "bg.h"
#include "hide.h"
#include "light.h"
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
#define RUSH_WARN     120      // warning-seconds 2
#define AMBUSH_WARN   240      // 4
#define AMBUSH_PAUSE  300      // rear-pause-seconds 5
#define QUEUE_PATIENCE 8       // entities.queue-patience (rooms)
#define FIRE_DELAY    45       // frames between walking in and the flicker
#define START_BEHIND  1000     // px of route behind the player's door he starts from (about 3 s for Rush)
#define HEAR_DISTANCE 768      // px of route at which his rumble starts (near-distance is 25-30 blocks)
#define UNLIT_FAR_VOLUME 3     // the Greenhouse: no flicker, so his rumble is heard from the warning on (owner)

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
static uint8_t unlit;          // fired in the Greenhouse: no flicker, the rumble from the start (Minecraft docs 09)
static uint8_t anim;
// His position in the player's room (place())
static uint8_t placed_room = 0xFF;   // the room xa/ya are for (0xFF: none)
static uint8_t placed_in;            // ... while the player was in this room
static int16_t placed_d;             // ... at this distance
static int32_t xa, ya;               // position, 24.8 fixed point
static int16_t sx, sy;               // px moved per px of route, 8.8

static spr_sheet_t rush_sheet, ambush_sheet;

// He fires only on entering a room (entity_room_entered), so his sheet is known at the door: the one he is,
// while he's on his way and this room is on his route (he never goes past the room he fired in)
void entity_need_sprites(void) BANKED {
    if (entity_state == ENT_IDLE || end_room < run_room)
        return;
    if (entity_kind == ENTITY_AMBUSH)
        spr_need(&ambush_sheet, BANK(sprite_ambush), &sprite_ambush);
    else
        spr_need(&rush_sheet, BANK(sprite_rush), &sprite_rush);
}

void entity_reset(void) BANKED {
    entity_state = ENT_IDLE;
    entity_queued = 0;
    sfx_rumble(0, RUMBLE_RUSH);
}

static int16_t route_length(uint8_t room) {
    return (int16_t)room_template_length[run_template[room]][run_exit[room]];   // to this run's real exit
}

static void fire(void) {
    entity_kind = entity_queued;
    entity_queued = 0;
    // The Greenhouse takes Rush and never Ambush: three to five passes you can't see is guessing, not a
    // sequence to read (Minecraft docs 09). One drawn there comes as a Rush.
    unlit = run_kind_at(run_room) == ROOM_KIND_GREENHOUSE;
    if (unlit)
        entity_kind = ENTITY_RUSH;
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
    placed_room = 0xFF;
    entity_d = start_d;
    sub = 0;
    dir = 1;
    // Ambush: 3-5 legs, from the seed so a run replays the same (an odd count ends at the far door)
    legs = entity_kind == ENTITY_AMBUSH ? 3 + (uint8_t)((run_seed ^ run_room) % 3) : 1;
}

void entity_room_entered(uint8_t first_time) BANKED {
    uint8_t kind = run_kind_at(run_room);
    // The Courtyard promises nothing hunts you: whatever was on its way is sent off (the queued spawn stays)
    if (kind == ROOM_KIND_COURTYARD && entity_state != ENT_IDLE) {
        entity_state = ENT_IDLE;
        sfx_rumble(0, RUMBLE_RUSH);
    }
    if (!first_time || entity_state != ENT_IDLE)
        return;
    if (!entity_queued && (run_flags[run_room] & RF_SPAWN)) {
        entity_queued = (run_flags[run_room] & RF_AMBUSH) ? ENTITY_AMBUSH : ENTITY_RUSH;
        patience = QUEUE_PATIENCE;
    }
    if (!entity_queued || run_room == RUN_LIBRARY_ROOM || ROOM_KIND_IS_CHASE(kind) ||
        kind == ROOM_KIND_COURTYARD || run_room == RUN_GREENHOUSE_LAST || kind == ROOM_KIND_ELECTRICAL)
        return;   // nothing follows you into the Library, a chase, a safe room or Room 100: the spawn waits (docs 07-10)
    // Never into a dark room: the flicker is his whole warning (Minecraft docs 05). The spawn waits. The
    // Greenhouse is the exception: dark throughout, and Rush comes anyway, heard but not flickered (docs 09).
    if (hide_free_spots() && (!light_dark || kind == ROOM_KIND_GREENHOUSE))
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

// Where he is in the current room, on the line between the route's two door centres. Worked out exactly
// (two 32-bit divisions) only when he's first placed in the player's room; after that each frame adds his
// step times a per-room slope. The divisions every frame took ~100 of the DMG's 154 scanlines.

static void place(void) {
    if (placed_room != entity_room || placed_in != run_room) {
        uint8_t t = run_template[entity_room];
        const uint8_t *r = room_template_route[t][run_exit[entity_room]];
        int16_t ax = (int16_t)r[0] << 3, ay = (int16_t)r[1] << 3;
        int16_t bx = (int16_t)r[2] << 3, by = (int16_t)r[3] << 3;
        int16_t len = route_length(entity_room);
        xa = ((int32_t)ax << 8) + (((int32_t)(bx - ax) * entity_d << 8) / len);
        ya = ((int32_t)ay << 8) + (((int32_t)(by - ay) * entity_d << 8) / len);
        sx = (int16_t)(((int32_t)(bx - ax) << 8) / len);
        sy = (int16_t)(((int32_t)(by - ay) << 8) / len);
        placed_room = entity_room;
        placed_in = run_room;
    } else {
        int16_t moved = entity_d - placed_d;   // at most a few px a frame, either way
        xa += (int16_t)(sx * moved);
        ya += (int16_t)(sy * moved);
    }
    placed_d = entity_d;
    entity_x = (int16_t)(xa >> 8);
    entity_y = (int16_t)(ya >> 8);
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

// Does a straight line from him to the player cross no wall? A line of metatiles (Bresenham) from his cell
// to the player's feet, his own cell and theirs not counted (his route cuts through anything). Only walls
// (MTF_WALL) and void block it, never furniture: in a big room the far corner isn't safe (owner, 2026-10-18;
// it was a 96/128 px circle through walls, the Minecraft kill radius).
static uint8_t sees_player(void) {
    if (entity_x < 0 || entity_y < 0)
        return 0;
    uint8_t x = (uint8_t)(entity_x >> 4), y = (uint8_t)(entity_y >> 4);
    uint8_t tx = (uint8_t)((player_x + 8) >> 4), ty = (uint8_t)((player_y + 12) >> 4);
    uint8_t dx, dy;
    int8_t sx, sy;
    if (tx >= x) { dx = tx - x; sx = 1; } else { dx = x - tx; sx = -1; }
    if (ty >= y) { dy = ty - y; sy = 1; } else { dy = y - ty; sy = -1; }
    int16_t err = (int16_t)dx - dy;
    for (;;) {
        int16_t e2 = err << 1;
        if (e2 > -(int16_t)dy) { err -= dy; x += sx; }
        if (e2 < (int16_t)dx) { err += dx; y += sy; }
        if (x == tx && y == ty)
            return 1;
        uint8_t mt = room_map[room_row[y] + x];
        if (!mt || (mt_flags[mt] & MTF_WALL))
            return 0;
    }
}

static uint8_t caught(void) {
    if (entity_room != run_room || hide_hidden)
        return 0;
    // Every 4th frame: up to a room's width of lookups costs DMG scanlines, and he moves 21 px meanwhile
    if (anim & 3)
        return 0;
    return sees_player();
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
            if (unlit)
                sfx_rumble(UNLIT_FAR_VOLUME, RUMBLE_RUSH);   // no lights to flicker: his sound is the warning
            else
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
    else
        placed_room = 0xFF;   // he left: worked out afresh when he's back
    uint16_t d = route_distance();
    uint8_t vol = d >= HEAR_DISTANCE ? 0 : 15 - (uint8_t)(d / (HEAR_DISTANCE / 15));
    if (unlit && vol < UNLIT_FAR_VOLUME)
        vol = UNLIT_FAR_VOLUME;   // heard all the way (Minecraft docs 09: rush.far plays the instant he's released)
    sfx_rumble(vol, entity_kind == ENTITY_AMBUSH ? RUMBLE_AMBUSH : RUMBLE_RUSH);
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
    spr_draw_big(sh, frame, 2, 2, entity_x - 16 - cam_x, entity_y - 16 - cam_y, 0);
}

// The lights stutter while he's announced and while he's in the room: an irregular pattern, not a switch
static const uint8_t flicker[16] = { 0, 2, 0, 0, 1, 3, 0, 2, 0, 0, 3, 1, 0, 2, 2, 0 };

void entity_lights(uint8_t *dim, uint8_t *tint) BANKED {
    *dim = 0;
    *tint = TINT_NONE;
    if (!unlit && (entity_state == ENT_WARN || (entity_state == ENT_MOVE && entity_room == run_room))) {
        *dim = flicker[(anim >> 2) & 15];
        if (entity_kind == ENTITY_AMBUSH)
            *tint = TINT_GREEN;
    }
}

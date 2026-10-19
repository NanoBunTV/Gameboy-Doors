#pragma bank 255
#include <gb/gb.h>
#include "screech.h"
#include "run.h"
#include "light.h"
#include "hide.h"
#include "audio.h"
#include "player.h"
#include "sprites.h"
#include "bg.h"
#include "sprite_screech.h"

// Minecraft config `screech`, with the owner's 3 s. Rolls on a 4 Hz tick like the Minecraft version: about
// 45% of dark rooms for someone spending 8 s in one solves to 1 - 0.55^(1/32) = 1.85% per tick, 5/256 here.
#define TICK_FRAMES  15
#define ODDS         5        // of 256, per tick
#define BITE_FRAMES  180      // owner: about 3 s
#define LOOK_FRAMES  18       // observe-seconds 0.3, looking straight at him without a break
#define GOING_FRAMES 40       // the shudder before he's gone
#define COOLDOWN     1200     // 20 s, after either ending (or hiding)
#define DISTANCE     32       // px from the player's middle: just outside the glow

uint8_t screech_state;
uint8_t screech_dir;
static uint8_t tick, look, anim;
static uint16_t timer, cooldown;
static uint16_t rng;

static spr_sheet_t sheet;

// Direction steps (player.h DIR_*, clockwise from down), DISTANCE long (diagonals 32 / 1.41)
static const int8_t off_x[8] = { 0, -23, -32, -23, 0, 23, 32, 23 };
static const int8_t off_y[8] = { 32, 23, 0, -23, -32, -23, 0, 23 };

// Only the dark has him (screech_update), and a room's darkness is set at the door (light_room)
void screech_need_sprites(void) BANKED {
    if (light_dark)
        spr_need(&sheet, BANK(sprite_screech), &sprite_screech);
}

// xorshift16, seeded from the run so a recorded run replays the same
static uint8_t rnd8(void) {
    rng ^= rng << 7;
    rng ^= rng >> 9;
    rng ^= rng << 8;
    return (uint8_t)rng;
}

void screech_reset(void) BANKED {
    rng = run_seed ^ 0x5C2E;
    if (!rng)
        rng = 1;
    screech_state = SCREECH_IDLE;
    cooldown = 0;
    tick = 0;
}

void screech_room_entered(void) BANKED {
    screech_state = SCREECH_IDLE;
}

static void done(void) {
    screech_state = SCREECH_IDLE;
    cooldown = COOLDOWN;
}

uint8_t screech_update(void) BANKED {
    anim++;
    if (cooldown)
        cooldown--;
    switch (screech_state) {
    case SCREECH_IDLE:
        // Only in the dark, never to someone hidden (a closet is the one safe place)
        if (!light_dark || hide_hidden || cooldown || ++tick < TICK_FRAMES)
            return 0;
        tick = 0;
        if (rnd8() >= ODDS)
            return 0;
        // Anywhere but where you're looking or right beside it (90-270 degrees off), and on screen: the
        // camera stops at the room's edges, so the player isn't always in the middle
        {
            uint8_t first = rnd8() % 5;
            for (uint8_t i = 0; i < 5; i++) {
                screech_dir = (player_facing + 2 + (first + i) % 5) & 7;
                int16_t sx = player_x + off_x[screech_dir] - cam_x;
                int16_t sy = player_y + off_y[screech_dir] - cam_y;
                if (sx >= 0 && sx <= 160 - 16 && sy >= 0 && sy <= 144 - 16 - 16)   // clear of the HUD strip
                    break;
            }
        }
        screech_state = SCREECH_HERE;
        timer = 0;
        look = 0;
        sfx_play(SFX_PSST);
        return 0;
    case SCREECH_HERE:
        if (hide_hidden) {   // hiding sends him away without a sound (Minecraft docs 11)
            done();
            return 0;
        }
        if (player_facing == screech_dir) {
            if (++look >= LOOK_FRAMES) {
                screech_state = SCREECH_GOING;
                timer = GOING_FRAMES;
                sfx_play(SFX_SCREECH_GONE);
                return 0;
            }
        } else {
            look = 0;   // "straight": looking away starts the count again
        }
        if (++timer < BITE_FRAMES)
            return 0;
        done();
        return 1;
    case SCREECH_GOING:
        if (!--timer)
            done();
        return 0;
    }
    return 0;
}

void screech_draw(void) BANKED {
    if (screech_state == SCREECH_IDLE)
        return;
    int16_t x = player_x + off_x[screech_dir], y = player_y + off_y[screech_dir];
    if (screech_state == SCREECH_GOING) {
        if (timer & 2)
            return;                       // flickers ...
        x += (anim & 1) ? 1 : -1;         // ... and shakes
    }
    spr_draw(&sheet, (anim & 8) ? SPR_SCREECH_F1 : SPR_SCREECH_F0, x - cam_x, y - cam_y, 0);
}

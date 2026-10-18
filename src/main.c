// DOORS for Game Boy. Phase 1: walk around a test room (engine core).
#include <gb/gb.h>
#include <stdint.h>
#include "platform.h"
#include "input.h"
#include "room.h"
#include "bg.h"
#include "sprites.h"
#include "player.h"
#include "room_test.h"

#define SCREEN_W 160
#define SCREEN_H 144

// Camera centred on the player, stopping at the room's edges (owner, 2026-10-17); a room smaller than the
// screen is centred
static int16_t camera_axis(int16_t centre, uint16_t room_size, uint8_t screen) {
    if (room_size <= screen)
        return -(int16_t)((screen - room_size) >> 1);
    int16_t c = centre - (screen >> 1);
    if (c < 0)
        return 0;
    if (c > (int16_t)(room_size - screen))
        return room_size - screen;
    return c;
}

static int16_t camera_target_x(void) { return camera_axis(player_x + 8, room_px_w, SCREEN_W); }
static int16_t camera_target_y(void) { return camera_axis(player_y + 8, room_px_h, SCREEN_H); }

// Clamps a camera move to 4 px per axis per frame, the most bg_set_camera can stream (bg.h)
static int16_t step_toward(int16_t from, int16_t to) {
    int16_t d = to - from;
    if (d > 4) d = 4;
    if (d < -4) d = -4;
    return from + d;
}

#ifdef AUTOTEST
// Worst case of how far into VBlank bg_flush() got, in scanlines (VBlank is lines 144-153, i.e. 0-9 here;
// 10 or more means it ran into the next frame). Read by scripts/vblank-budget.py.
uint8_t autotest_flush_lines;
static void measure_flush(void) {
    uint8_t ly = LY_REG;
    uint8_t lines = ly >= 144 ? ly - 144 : ly + 10;
    if (lines > autotest_flush_lines)
        autotest_flush_lines = lines;
}
#else
#define measure_flush()
#endif

static void load_room(uint8_t bank, const room_def_t *def) {
    DISPLAY_OFF;  // waits for VBlank first
    room_load(bank, def);
    player_init(room_spawn_x, room_spawn_y);
    bg_draw_full(camera_target_x(), camera_target_y());
    spr_begin();
    player_draw();
    spr_end();
    DISPLAY_ON;
}

void main(void) {
    DISPLAY_OFF;
    platform_init();
    // BG tiles at 0x8800-0x97FF (signed), map at 0x9800, 8x16 sprites with tiles at 0x8000-0x87FF
    LCDC_REG = LCDCF_OFF | LCDCF_BG8800 | LCDCF_BG9800 | LCDCF_OBJ16 | LCDCF_OBJON | LCDCF_BGON;

    load_room(BANK(room_test), &room_test);

    while (1) {
        vsync();
        bg_flush();          // VRAM and scroll first, while VBlank lasts
        measure_flush();
        input_update();
        player_update();
        bg_set_camera(step_toward(cam_x, camera_target_x()), step_toward(cam_y, camera_target_y()));
        spr_begin();
        player_draw();
        spr_end();
    }
}

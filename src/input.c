#include <gb/gb.h>
#include "input.h"

uint8_t input_held;
uint8_t input_pressed;

#ifdef AUTOTEST
// AUTOTEST builds (make AUTOTEST=1) ignore the joypad and repeat this route, so headless emulator checks
// (scripts/bgb-check.ps1) exercise scrolling in every direction. It's laid out for assets/rooms/test.tmx,
// starting at the spawn on the carpet: { buttons, frames }.
static const uint8_t route[][2] = {
    { 0, 30 },
    { J_UP, 250 },                // up the carpet, through the doorway, to the north wall
    { J_LEFT, 200 },              // to the west wall
    { J_DOWN | J_RIGHT, 120 },    // diagonal into the inner wall
    { J_RIGHT, 250 },
    { J_RIGHT, 200 },             // to the east wall
    { J_UP | J_LEFT, 80 },
    { J_LEFT, 120 },              // back to the doorway
    { J_DOWN, 250 },              // down through the doorway to the south wall
    { J_LEFT | J_DOWN, 100 },
    { J_UP | J_RIGHT, 160 },
    { J_RIGHT, 100 },
    { J_LEFT, 110 },
};
static uint8_t route_step, route_timer;

static uint8_t read_buttons(void) {
    if (route_timer >= route[route_step][1]) {
        route_timer = 0;
        if (++route_step == sizeof(route) / sizeof(route[0]))
            route_step = 0;
    }
    route_timer++;
    return route[route_step][0];
}
#else
#define read_buttons joypad
#endif

void input_update(void) {
    uint8_t now = read_buttons();
    input_pressed = now & ~input_held;
    input_held = now;
}

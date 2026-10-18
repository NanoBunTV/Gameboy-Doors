#include <gb/gb.h>
#include "input.h"

uint8_t input_held;
uint8_t input_pressed;
uint16_t input_frame;   // input_update() calls so far; the replay and scripts/bot.py count in these

#ifdef AUTOTEST
// AUTOTEST builds (make AUTOTEST=1) replay a recorded run, so headless emulators (BGB, which can't press
// buttons) play the game too. tests/autotest_input.c is written by scripts/bot.py: pairs of
// { buttons, count } in input_update() calls. After the end, or if it's empty, the joypad is read.
#include "autotest_input.h"
static uint16_t replay_pos;
static uint8_t replay_left;

static uint8_t read_buttons(void) {
    uint8_t saved = CURRENT_BANK, buttons;
    SWITCH_ROM(BANK(autotest_input));
    while (!replay_left) {
        if (replay_pos >= autotest_input_len) {
            SWITCH_ROM(saved);
            return joypad();
        }
        replay_left = autotest_input[replay_pos + 1];
        replay_pos += 2;
    }
    replay_left--;
    buttons = autotest_input[replay_pos - 2];
    SWITCH_ROM(saved);
    return buttons;
}
#else
#define read_buttons joypad
#endif

void input_update(void) {
    uint8_t now = read_buttons();
    input_pressed = now & ~input_held;
    input_held = now;
    input_frame++;
}

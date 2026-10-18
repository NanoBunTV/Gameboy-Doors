// Joypad, read once per frame
#ifndef INPUT_H
#define INPUT_H

#include <stdint.h>

extern uint8_t input_held;      // J_* bits held this frame
extern uint8_t input_pressed;   // J_* bits that went down this frame
extern uint16_t input_frame;    // number of input_update() calls so far

void input_update(void);

#endif

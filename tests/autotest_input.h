// The recorded run AUTOTEST builds replay (src/input.c). The data is in autotest_input.c (banked ROM),
// written by scripts/bot.py.
#ifndef AUTOTEST_INPUT_H
#define AUTOTEST_INPUT_H

#include <gb/gb.h>
#include <stdint.h>

BANKREF_EXTERN(autotest_input)
extern const uint8_t autotest_input[];   // pairs: buttons (J_*), number of input frames (1-255)
extern const uint16_t autotest_input_len; // bytes

#endif

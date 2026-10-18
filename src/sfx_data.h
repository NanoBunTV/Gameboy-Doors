// The sound effects' register scripts (sfx_data.c, banked). Only audio.c reads them, with
// BANK(sfx_data) switched in.
#ifndef SFX_DATA_H
#define SFX_DATA_H

#include <gb/gb.h>
#include <stdint.h>
#include "audio.h"

// A step: how many frames it lasts, then the channel's five registers (CH1 NR10-NR14; CH2 and CH4 use
// the last four). The last register's bit 7 restarts the sound. A step of 0 frames ends the effect.
typedef struct { uint8_t frames, r0, r1, r2, r3, r4; } step_t;

#define ENV(vol, step) (((vol) << 4) | (step))   // volume, fading out by one every step/64 s (0: held)
#define REST(frames) { frames, 0, 0, 0, 0, 0x80 }  // silence (DAC off)

extern const step_t *const sfx_steps[SFX_COUNT];
extern const uint8_t sfx_chan[SFX_COUNT];   // 0 = CH1, 1 = CH2, 3 = CH4 (hUGE_channel_t numbering)
BANKREF_EXTERN(sfx_data)

#endif

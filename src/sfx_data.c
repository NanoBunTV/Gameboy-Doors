#pragma bank 255
#include "sfx_data.h"

// The effects' scripts, banked (moved out of bank 0, 2026-10-18). audio.c switches BANK(sfx_data) in to
// start an effect and to step the ones playing.
BANKREF(sfx_data)

#define FQ(hz) (2048 - 131072 / (hz))            // CH1/CH2 period for a frequency in Hz (64 Hz and up)
#define LO(hz) (FQ(hz) & 0xFF)
#define HI(hz) (((FQ(hz) >> 8) & 7) | 0x80)       // with the restart bit
#define DUTY(d) ((d) << 6)                       // 0 12.5%, 1 25%, 2 50%, 3 75%
#define END { 0, 0, 0, 0, 0, 0 }

static const step_t s_door[] = {
    { 8, 0, 0, ENV(7, 1), 0x45, 0x80 },          // creak
    { 16, 0, 0, ENV(11, 2), 0x73, 0x80 },        // thud
    END };
static const step_t s_drawer[] = {
    { 5, 0, 0, ENV(6, 1), 0x36, 0x80 },
    { 8, 0, 0, ENV(4, 1), 0x26, 0x80 },
    END };
static const step_t s_key[] = {
    { 5, 0, DUTY(2), ENV(10, 2), LO(1568), HI(1568) },
    { 12, 0, DUTY(2), ENV(10, 3), LO(2093), HI(2093) },
    END };
static const step_t s_locked[] = {
    { 4, 0, 0, ENV(9, 1), 0x2B, 0x80 },
    REST(5),
    { 8, 0, 0, ENV(9, 1), 0x2B, 0x80 },
    END };
static const step_t s_unlock[] = {
    { 3, 0, 0, ENV(10, 1), 0x1A, 0x80 },         // (CH4) the bolt
    { 10, 0, 0, ENV(7, 1), 0x3A, 0x80 },
    END };
static const step_t s_hide[] = {
    { 18, 0, 0, ENV(12, 2), 0x75, 0x80 },
    END };
static const step_t s_heartbeat[] = {
    { 8, 0, DUTY(2), ENV(12, 1), LO(70), HI(70) },
    { 12, 0, DUTY(2), ENV(8, 1), LO(64), HI(64) },
    END };
static const step_t s_get_out[] = {
    { 40, 0x16, DUTY(3), ENV(15, 4), LO(150), HI(150) },
    END };
static const step_t s_hurt[] = {
    { 24, 0x2E, DUTY(2), ENV(13, 2), LO(700), HI(700) },
    END };
// The lights: an irregular electrical buzz, about two seconds
#define BUZZ(frames) { frames, 0, DUTY(0), ENV(5, 0), LO(240), HI(240) }
static const step_t s_flicker[] = {
    BUZZ(6), REST(3), BUZZ(2), REST(8), BUZZ(10), REST(2), BUZZ(3), REST(12), BUZZ(5), REST(4),
    BUZZ(14), REST(2), BUZZ(2), REST(6), BUZZ(8), REST(10), BUZZ(4), REST(3), BUZZ(12), REST(4),
    END };
// Ambush: three rising howls
#define HOWL { 22, 0x15, DUTY(1), ENV(13, 0), LO(220), HI(220) }
static const step_t s_ambush[] = { HOWL, REST(6), HOWL, REST(6), HOWL, END };
static const step_t s_scream[] = {
    { 50, 0, 0, ENV(15, 0), 0x11, 0x80 },
    { 40, 0, 0, ENV(15, 3), 0x21, 0x80 },
    END };
static const step_t s_type[] = {
    { 2, 0, DUTY(2), ENV(3, 1), LO(2600), HI(2600) },
    END };
static const step_t s_select[] = {
    { 6, 0, DUTY(2), ENV(8, 1), LO(1046), HI(1046) },
    END };

// Lights out: a breaker's clunk, then the hum dying away
static const step_t s_dark[] = {
    { 3, 0, 0, ENV(13, 1), 0x34, 0x80 },
    { 40, 0, 0, ENV(7, 4), 0x87, 0x80 },
    END };
// Screech: "p", then a long hiss
static const step_t s_psst[] = {
    { 3, 0, 0, ENV(6, 1), 0x22, 0x80 },
    REST(3),
    { 24, 0, 0, ENV(10, 2), 0x10, 0x80 },
    END };
// Screech looked at: a squeak sliding down (CH1 sweep)
static const step_t s_screech_gone[] = {
    { 24, 0x2B, DUTY(2), ENV(10, 2), LO(1400), HI(1400) },
    END };
// Eyes: a low wavering drone
static const step_t s_eyes[] = {
    { 20, 0, DUTY(1), ENV(9, 0), LO(110), HI(110) },
    { 20, 0, DUTY(1), ENV(9, 0), LO(104), HI(104) },
    { 30, 0, DUTY(1), ENV(9, 3), LO(98), HI(98) },
    END };
static const step_t s_eyes_hit[] = {
    { 8, 0, DUTY(3), ENV(11, 1), LO(330), HI(330) },
    END };

// Halt's TURN AROUND: a bright stab sweeping down, twice
static const step_t s_halt[] = {
    { 12, 0x1C, DUTY(2), ENV(15, 2), LO(1760), HI(1760) },
    REST(3),
    { 20, 0x1C, DUTY(2), ENV(15, 3), LO(1320), HI(1320) },
    END };
// Timothy: a short, high hiss and a scrabble
static const step_t s_timothy[] = {
    { 10, 0, 0, ENV(12, 1), 0x02, 0x80 },
    REST(2),
    { 4, 0, 0, ENV(8, 1), 0x24, 0x80 },
    { 4, 0, 0, ENV(6, 1), 0x24, 0x80 },
    END };

const step_t *const sfx_steps[SFX_COUNT] = {
    0, s_door, s_drawer, s_key, s_locked, s_unlock, s_hide, s_heartbeat, s_get_out, s_hurt,
    s_flicker, s_ambush, s_scream, s_type, s_select, s_dark, s_psst, s_screech_gone, s_eyes, s_eyes_hit,
    s_halt, s_timothy,
};
const uint8_t sfx_chan[SFX_COUNT] = { 0, 3, 3, 1, 3, 3, 3, 0, 0, 0, 1, 0, 3, 1, 1, 3, 3, 0, 0, 1, 0, 3 };

#include <gb/gb.h>
#include <gb/hardware.h>
#include "hUGEDriver.h"
#include "audio.h"

// Songs: C written by hUGETracker's uge2source from assets/music/*.uge (Makefile)
extern const hUGESong_t music_title, music_hotel, music_guiding_light;
BANKREF_EXTERN(music_title)
BANKREF_EXTERN(music_hotel)
BANKREF_EXTERN(music_guiding_light)

static uint8_t music_id, music_bank;

// ------------------------------------------------------------------------------------------ effects
// A step: how many frames it lasts, then the channel's five registers (CH1 NR10-NR14; CH2 and CH4 use
// the last four). The last register's bit 7 restarts the sound. A step of 0 frames ends the effect.
typedef struct { uint8_t frames, r0, r1, r2, r3, r4; } step_t;

#define FQ(hz) (2048 - 131072 / (hz))            // CH1/CH2 period for a frequency in Hz (64 Hz and up)
#define LO(hz) (FQ(hz) & 0xFF)
#define HI(hz) (((FQ(hz) >> 8) & 7) | 0x80)       // with the restart bit
#define ENV(vol, step) (((vol) << 4) | (step))   // volume, fading out by one every step/64 s (0: held)
#define DUTY(d) ((d) << 6)                       // 0 12.5%, 1 25%, 2 50%, 3 75%
#define REST(frames) { frames, 0, 0, 0, 0, 0x80 }  // silence (DAC off)
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

static const step_t *const sfx_steps[SFX_COUNT] = {
    0, s_door, s_drawer, s_key, s_locked, s_unlock, s_hide, s_heartbeat, s_get_out, s_hurt,
    s_flicker, s_ambush, s_scream, s_type, s_select,
};
// Channel of each effect: 0 = CH1, 1 = CH2, 3 = CH4 (hUGE_channel_t numbering)
static const uint8_t sfx_chan[SFX_COUNT] = { 0, 3, 3, 1, 3, 3, 3, 0, 0, 0, 1, 0, 3, 1, 1 };

static const step_t *playing[4];
static uint8_t left[4];
static uint8_t rumble_vol, rumble_kind, rumble_hw;   // wanted, and what CH4 is doing now

static void write_step(uint8_t ch, const step_t *s) {
    switch (ch) {
    case 0: NR10_REG = s->r0; NR11_REG = s->r1; NR12_REG = s->r2; NR13_REG = s->r3; NR14_REG = s->r4; break;
    case 1: NR21_REG = s->r1; NR22_REG = s->r2; NR23_REG = s->r3; NR24_REG = s->r4; break;
    default: NR41_REG = s->r1; NR42_REG = s->r2; NR43_REG = s->r3; NR44_REG = s->r4; break;
    }
}

// The effect on a channel ended: silence it and hand it back to the music (CH4 stays ours while rumbling)
static void release(uint8_t ch) {
    static const step_t silent = REST(0);
    playing[ch] = 0;
    write_step(ch, &silent);
    if (ch == 3) {
        rumble_hw = 0;   // the rumble, if wanted, restarts in audio_frame
        if (rumble_vol)
            return;
    }
    hUGE_mute_channel(ch, HT_CH_PLAY);
}

void sfx_play(uint8_t id) {
    if (!id || id >= SFX_COUNT)
        return;
    uint8_t ch = sfx_chan[id];
    hUGE_mute_channel(ch, HT_CH_MUTE);
    playing[ch] = sfx_steps[id];
    left[ch] = playing[ch]->frames;
    write_step(ch, playing[ch]);
}

void sfx_rumble(uint8_t volume, uint8_t kind) {
    rumble_vol = volume > 15 ? 15 : volume;
    rumble_kind = kind;
}

static void rumble_update(void) {
    if (playing[3] || rumble_vol == rumble_hw)
        return;
    if (rumble_vol) {
        hUGE_mute_channel(HT_CH4, HT_CH_MUTE);
        NR41_REG = 0;
        NR42_REG = ENV(rumble_vol, 0);
        NR43_REG = rumble_kind == RUMBLE_RUSH ? 0x84 : 0x6D;   // Rush: low roar; Ambush: metallic (7-bit)
        NR44_REG = 0x80;
    } else {
        NR42_REG = 0;
        NR44_REG = 0x80;
        hUGE_mute_channel(HT_CH4, HT_CH_PLAY);
    }
    rumble_hw = rumble_vol;
}

// ------------------------------------------------------------------------------------------ music

void audio_init(void) {
    NR52_REG = 0x80;   // sound on
    NR50_REG = 0x77;   // full volume both sides
    NR51_REG = 0xFF;   // every channel on both sides
    music_id = MUSIC_NONE;
}

static void silence_music(void) {
    NR12_REG = 0; NR14_REG = 0x80;
    NR22_REG = 0; NR24_REG = 0x80;
    NR30_REG = 0;
    NR42_REG = 0; NR44_REG = 0x80;
}

void music_play(uint8_t id) {
    if (id == music_id)
        return;
    music_id = id;
    const hUGESong_t *song;
    switch (id) {
    case MUSIC_TITLE:   song = &music_title;         music_bank = BANK(music_title); break;
    case MUSIC_HOTEL:   song = &music_hotel;         music_bank = BANK(music_hotel); break;
    case MUSIC_GUIDING: song = &music_guiding_light; music_bank = BANK(music_guiding_light); break;
    default:
        silence_music();
        return;
    }
    uint8_t saved = CURRENT_BANK;
    SWITCH_ROM(music_bank);
    hUGE_init(song);
    SWITCH_ROM(saved);
    // hUGE_init forgets which channels were lent out
    for (uint8_t ch = 0; ch < 4; ch++)
        if (playing[ch] || (ch == 3 && rumble_hw))
            hUGE_mute_channel(ch, HT_CH_MUTE);
}

void audio_frame(void) {
    if (music_id) {
        uint8_t saved = CURRENT_BANK;
        SWITCH_ROM(music_bank);
        hUGE_dosound();
        SWITCH_ROM(saved);
    }
    for (uint8_t ch = 0; ch < 4; ch++) {
        if (!playing[ch] || --left[ch])
            continue;
        const step_t *s = playing[ch] + 1;
        if (!s->frames) {
            release(ch);
            continue;
        }
        playing[ch] = s;
        left[ch] = s->frames;
        write_step(ch, s);
    }
    rumble_update();
}

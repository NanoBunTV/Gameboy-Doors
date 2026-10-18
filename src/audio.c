#include <gb/gb.h>
#include <gb/hardware.h>
#include "hUGEDriver.h"
#include "audio.h"
#include "sfx_data.h"

// Songs: C written by hUGETracker's uge2source from assets/music/*.uge (Makefile)
extern const hUGESong_t music_title, music_guiding_light, music_library;
BANKREF_EXTERN(music_title)
BANKREF_EXTERN(music_guiding_light)
BANKREF_EXTERN(music_library)

static uint8_t music_id, music_bank;

// ------------------------------------------------------------------------------------------ effects
// The scripts are in sfx_data.c (banked): BANK(sfx_data) is switched in whenever they're read.

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

void sfx_play_vol(uint8_t id, uint8_t volume) {
    if (!id || id >= SFX_COUNT)
        return;
    uint8_t saved = CURRENT_BANK;
    SWITCH_ROM(BANK(sfx_data));
    uint8_t ch = sfx_chan[id];
    hUGE_mute_channel(ch, HT_CH_MUTE);
    playing[ch] = sfx_steps[id];
    left[ch] = playing[ch]->frames;
    step_t s = *playing[ch];
    if (volume)
        s.r2 = (uint8_t)((volume << 4) | (s.r2 & 0x0F));
    write_step(ch, &s);
    SWITCH_ROM(saved);
}

void sfx_play(uint8_t id) {
    sfx_play_vol(id, 0);
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

// Stops whatever the music left sounding (a held note keeps ringing once hUGE_dosound stops being called),
// on the channels no sound effect or rumble is using
static void silence_music(void) {
    if (!playing[0]) { NR12_REG = 0; NR14_REG = 0x80; }
    if (!playing[1]) { NR22_REG = 0; NR24_REG = 0x80; }
    NR30_REG = 0;
    if (!playing[3] && !rumble_hw) { NR42_REG = 0; NR44_REG = 0x80; }
}

void music_play(uint8_t id) {
    if (id == music_id)
        return;
    music_id = id;
    silence_music();
    const hUGESong_t *song;
    switch (id) {
    case MUSIC_TITLE:   song = &music_title;         music_bank = BANK(music_title); break;
    case MUSIC_GUIDING: song = &music_guiding_light; music_bank = BANK(music_guiding_light); break;
    case MUSIC_LIBRARY: song = &music_library;       music_bank = BANK(music_library); break;
    default:
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
    uint8_t saved = CURRENT_BANK;
    if (music_id) {
        SWITCH_ROM(music_bank);
        hUGE_dosound();
    }
    SWITCH_ROM(BANK(sfx_data));
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
    SWITCH_ROM(saved);
    rumble_update();
}

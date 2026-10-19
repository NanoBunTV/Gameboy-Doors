// Music (hUGEDriver, songs from assets/music/*.uge) and sound effects.
//
// Sound effects are short register scripts on CH1, CH2 or CH4. While one plays, hUGEDriver is told to
// leave that channel alone ("muted", hUGETracker manual: Sound effects) and gets it back afterwards. CH3
// (wave) belongs to the music only. Rush and Ambush's rumble is a continuous CH4 noise whose volume
// follows their distance (sfx_rumble).
#ifndef AUDIO_H
#define AUDIO_H

#include <stdint.h>

#define MUSIC_NONE    0
#define MUSIC_TITLE   1   // the owner's Lobby Theme
#define MUSIC_GUIDING 3   // the Guiding Light (death screen)
#define MUSIC_LIBRARY 4   // the owner's Unhinged (Library Theme), in room 50 until you walk out
#define MUSIC_SEEK    5   // the owner's Here I Come (Seek Chase), from his rise to the door slamming

#define SFX_DOOR       1
#define SFX_DRAWER     2
#define SFX_KEY        3
#define SFX_LOCKED     4
#define SFX_UNLOCK     5
#define SFX_HIDE       6   // into or out of a closet / under a bed
#define SFX_HEARTBEAT  7
#define SFX_GET_OUT    8   // Hide throws you out
#define SFX_HURT       9
#define SFX_FLICKER    10  // the lights before Rush
#define SFX_AMBUSH     11  // Ambush's own cue
#define SFX_SCREAM     12  // jumpscare
#define SFX_TYPE       13  // Guiding Light typewriter (unused since 2026-10-18: it clashed with the song)
#define SFX_SELECT     14  // menu
#define SFX_DARK       15  // the lights are out (entering a dark room after a lit one)
#define SFX_PSST       16  // Screech arrives
#define SFX_SCREECH_GONE 17 // ... and leaves, looked at
#define SFX_EYES       18  // Eyes is in the room
#define SFX_EYES_HIT   19  // Eyes hurts you (about once a second)
#define SFX_HALT       20  // TURN AROUND
#define SFX_TIMOTHY    21  // the spider's hiss
#define SFX_FIGURE_STEP 22 // Figure's footstep (sfx_play_vol: louder as he gets closer)
#define SFX_FIGURE_HEAR 23 // he heard you
#define SFX_WHEEL      24  // a padlock wheel turns
#define SFX_BOOK       25  // a book or the paper read
#define SFX_SEEK_STEP  26  // Seek's footstep (sfx_play_vol: louder as he gets closer)
#define SFX_SEEK_RISE  27  // the puddle, and him coming up out of it
#define SFX_SLAM       28  // the chase's last door slams behind you
#define SFX_FIRE       29  // you walked into the fire
#define SFX_SNARE      30  // a Snare snaps shut on you
#define SFX_COUNT      31

#define RUMBLE_RUSH   0
#define RUMBLE_AMBUSH 1

// Once at boot: sound on, nothing playing
void audio_init(void);

// Starts a song from the beginning (MUSIC_NONE: silence). Does nothing if it's already playing.
void music_play(uint8_t id);

// Once per frame, after the VBlank work: a music tick and sound-effect steps
void audio_frame(void);

void sfx_play(uint8_t id);

// The same with the first step's volume replaced (1-15)
void sfx_play_vol(uint8_t id, uint8_t volume);

// Continuous noise on CH4, volume 0 (off) - 15; retriggered only when the volume changes
void sfx_rumble(uint8_t volume, uint8_t kind);

// Rain (the Courtyard and the Greenhouse, owner 2026-10-18): soft noise drops on CH4 whenever nothing else
// (an effect, a rumble) has it. Starting a song stops it.
void audio_rain(uint8_t on);

#endif

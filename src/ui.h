#pragma once

#include <raylib.h>
#include <rmidi.h>
#include <rmidi_ext.h>
#include "types.h"

#ifndef PIANO_ROLL_BLOOM
#define PIANO_ROLL_BLOOM            1
#endif

#ifndef PIANO_ROLL_PARTICLE_TAIL
#define PIANO_ROLL_PARTICLE_TAIL    1
#endif

#ifndef PIANO_ROLL_KICK_FLASH
#define PIANO_ROLL_KICK_FLASH       1
#endif

#ifndef PIANO_ROLL_TAIL_LIFE
#define PIANO_ROLL_TAIL_LIFE        4.0
#endif

#ifndef PIANO_ROLL_TAIL_DELAY
#define PIANO_ROLL_TAIL_DELAY       1.40
#endif

#ifndef PIANO_ROLL_TAIL_SPREAD
#define PIANO_ROLL_TAIL_SPREAD      1.0
#endif

enum {
    PIANO_ROLL_FX_BLOOM = 1 << 0,
    PIANO_ROLL_FX_TAIL  = 1 << 1,
    PIANO_ROLL_FX_KICK  = 1 << 2,
    PIANO_ROLL_FX_TRAIL = 1 << 3,
    PIANO_ROLL_FX_DRUMS = 1 << 4,
};

#define PIANO_ROLL_FX_DEFAULT ( \
    (PIANO_ROLL_BLOOM         ? PIANO_ROLL_FX_BLOOM : 0) | \
    (PIANO_ROLL_PARTICLE_TAIL ? PIANO_ROLL_FX_TAIL  : 0) | \
    (PIANO_ROLL_KICK_FLASH    ? PIANO_ROLL_FX_KICK  : 0) | \
    (PIANO_ROLL_FX_TRAIL      ? PIANO_ROLL_FX_TRAIL : 0) | \
    (PIANO_ROLL_FX_DRUMS      ? PIANO_ROLL_FX_DRUMS : 0)) 

void ui_music_profile(
    u32   w,
    u32   h,
    const Texture* album_art,
    const char*    title,
    const char*    album,
    const char*    author
);

void ui_background_init(f32 blur);
void ui_background_shutdown(void);
void ui_background_target_pan(u32 x, u32 y);
void ui_background(
    Texture* thumbptr, 
    u32 w, u32 h, 
    f32 dt, 
    f32 music_elapsed, 
    f32 music_duration
);

void ui_music_info(
    u32 w, 
    u32 h, 
    const char* key_signature, 
    u8 bpm,
    float time_played,
    u8 nominator,
    u8 denominator
);

void ui_chord_info(
    u32 w, 
    u32 h, 
    MidiChord* chord,
    MidiKey*   key
);

void ui_beat_info(
    u32 w, 
    u32 h, 
    MidiPosition* pos
);

#define PIANO_ROLL_MAX_BARS 8

typedef struct PianoRoll {
    const Midi*           midi;
    const MidiTempoMap*   tempo;
    const MidiTimeSigMap* sig;
    const MidiNoteList*   notes;
    int                   bars_per_page;
    int                   low_key;
    int                   high_key;
    double                max_len;
    double                clock;
} PianoRoll;

void   piano_roll_init(PianoRoll* r, const Midi* midi, const MidiTempoMap* tempo, const MidiTimeSigMap* sig, const MidiNoteList* notes);
double piano_roll_time(PianoRoll* r, double audio, double dt, bool playing);
void   piano_roll_draw(const PianoRoll* r, Rectangle roll, double now);

#include "lyla.h"
#include "raylib.h"
#include "rmidi.h"
#include "rmidi_ext.h"
#include "rstb_common.h"

#include "assets/rori.c"
#include "text.h"
#include "ui.h"
#include "utils.h"
#include <stdlib.h>

static bool s_should_exit = false;
Texture rori;

static void handle_input(void)
{
    if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
        s_should_exit = true;
    }
    if (IsKeyPressed(KEY_F)) {
        ToggleBorderlessWindowed();
    }
}

Midi           midi = {0};
MidiTempoMap   tempo;
MidiTimeSigMap sig;
MidiNoteList   notes;
Music          music;
char   key_name[16];
i32    min_key, max_key, rows;
double max_len;

void lyla_init(void)
{
    Image img = LoadImageFromMemory(".png", __resources_rori_png, __resources_rori_png_len);
    rori = LoadTextureFromImage(img);
    UnloadImage(img);

    // TODO : Refactor this
    i32 buf_size;
    u8* buf = LoadFileData("test2.mid", &buf_size);

    if (!midi_parse(&midi, buf, buf_size)) {
        exit(69);
    }
    midi_build_tempo_map(&midi, &tempo);
    midi_build_timesig_map(&midi, &sig);
    midi_collect_notes(&midi, &tempo, &notes);

    min_key = 127, max_key = 0;
    max_len = 0.0;
    da_foreach(MidiNote, n, &notes) {
        if (n->key < min_key) min_key = n->key;
        if (n->key > max_key) max_key = n->key;
        if (n->end - n->start > max_len) max_len = n->end - n->start;
    }
    if (min_key > 0)   min_key--;
    if (max_key < 127) max_key++;
    rows = max_key - min_key + 1;

    MidiKey song_key;
    bool    has_song_key = midi_detect_key(&notes, &song_key, -1, 0.0, MIDI_TIME_END);
    if (has_song_key) midi_key_name(&song_key, key_name, sizeof( key_name));

    music = LoadMusicStream("test2.mp3");
    music.looping = false;
    PlayMusicStream(music);

    text_init();
    ui_background_init();
}

void lyla_update(void)
{
    u32 w  = GetScreenWidth();
    u32 h  = GetScreenHeight();
    u32 dt = GetFrameTime();
    UNUSED(dt);

    handle_input();
    UpdateMusicStream(music);

    // TODO : Refactor this
    f32 now = GetMusicTimePlayed(music);
    double       ticks = midi_seconds_to_ticks(&midi, &tempo, now);
    MidiPosition pos;
    midi_ticks_to_position(&midi, &sig, &pos, ticks);

    BeginDrawing(); {
        ClearBackground(BLANK);
        ui_background(w, h, &rori, false);
        ui_music_profile(w, h, &rori, "Yooo mom", "Mommy Remy", "UnknownRori");
        ui_music_info(w, h, "F#", midi_bpm_at(&tempo, ticks) , now, pos.numerator, pos.denominator);

    }; EndDrawing();
}

void lyla_shutdown(void)
{

}


bool lyla_should_close() { return s_should_exit; }

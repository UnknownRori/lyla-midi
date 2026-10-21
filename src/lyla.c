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

Midi           midi = {0};
MidiTempoMap   tempo;
MidiKeySigMap  key_sig;
MidiTimeSigMap sig;
MidiNoteList   notes;
Music          music;
PianoRoll      s_roll;


static void handle_input(void)
{
    if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
        s_should_exit = true;
    }
    if (IsKeyPressed(KEY_F)) {
        ToggleBorderlessWindowed();
    }

    if (IsKeyPressed(KEY_R)) {
        SeekMusicStream(music, 0.f);
        PlayMusicStream(music);
    }

    if (IsKeyPressed(KEY_RIGHT)) {
        SeekMusicStream(music, GetMusicTimePlayed(music) + 5.);
    }
    if (IsKeyPressed(KEY_LEFT)) {
        SeekMusicStream(music, GetMusicTimePlayed(music) - 5.);
    }

    if (IsKeyPressed(KEY_SPACE)) {
        if (IsMusicStreamPlaying(music)) {
            PauseMusicStream(music);
        } else {
            ResumeMusicStream(music);
        }
    }
}

static float time_to_x(double t, double win_start, double win_end, Rectangle roll)
{
    double f = (t - win_start) / (win_end - win_start);
    if (f < 0.0) f = 0.0;
    if (f > 1.0) f = 1.0;
    return roll.x + (float)(f * roll.width);
}

static size_t first_note_from(const MidiNoteList* notes, double t)
{
    size_t lo = 0, hi = notes->count;
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        if (notes->items[mid].start < t) 
            lo = mid + 1;
        else
            hi = mid;
    }
    return lo;
}


#include "assets/lythian.c"

Font lythian;

void lyla_init(void)
{
    // Image img = LoadImageFromMemory(".png", __resources_rori_png, __resources_rori_png_len);
    // rori = LoadTextureFromImage(img);
    // UnloadImage(img);
    rori = LoadTexture("cover.png");

    lythian = LoadFontFromMemory(".ttf", __resources_Lythian_ttf, __resources_Lythian_ttf_len, 32, NULL, 0);

    // TODO : Refactor this
    i32 buf_size;
    u8* buf = LoadFileData("lurie.mid", &buf_size);

    if (!midi_parse(&midi, buf, buf_size)) {
        exit(69);
    }
    midi_build_tempo_map(&midi, &tempo);
    midi_build_timesig_map(&midi, &sig);
    midi_collect_notes(&midi, &tempo, &notes);
    midi_build_keysig_map(&midi, &key_sig);

    piano_roll_init(&s_roll, &midi, &tempo, &sig, &notes);

    music = LoadMusicStream("lurie-ori.mp3");
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
    MidiBar bar[2];
    midi_bar_at(&midi, &sig, &bar[0], ticks);
    midi_bar_at(&midi, &sig, &bar[1], bar[0].start_tick + bar[0].length_ticks);
    MidiKey   key   = {0};
    Rectangle roll  = { 32, 128 + 32, w - 64 , h - 304 };
    MidiChord chord = {0};
    char key_text[32];
    if (!midi_keysig_at(&key_sig, &key, ticks))  {
        midi_detect_key(&notes, &key, -1, now, now);
    }
    key.is_minor = true; // fak off
    midi_key_name(&key, key_text, sizeof key_text);

    midi_detect_chord(&notes, &chord, -1, now, now);

    BeginDrawing(); {
        ClearBackground(BLANK);
        ui_background(w, h, &rori, false);
        // ui_music_profile(w, h, &rori, "Lilith Introduction", "Lilith", "UnknownRori");
        ui_music_profile(w, h, &rori, "Lurie", "Solas na Gréine", "Rigël Theatre");
        ui_music_info(w, h, key_text, midi_bpm_at(&tempo, ticks) , now, pos.numerator, pos.denominator);
        ui_chord_info(w, h, &chord, &key);
        ui_beat_info(w, h, &pos);
        piano_roll_draw(&s_roll, roll, now);

        text_draw("UnknownRori's Softwork", 0, h - 16, 12, GRAY);

    }; EndDrawing();
}

void lyla_shutdown(void)
{

}


bool lyla_should_close() { return s_should_exit; }

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

Midi           midi = {0};
MidiTempoMap   tempo;
MidiTimeSigMap sig;
MidiNoteList   notes;
Music          music;
char   key_name[16] = "-";
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
    MidiBar bar[2];
    midi_bar_at(&midi, &sig, &bar[0], ticks);
    midi_bar_at(&midi, &sig, &bar[1], bar[0].start_tick + bar[0].length_ticks);
    double win_start = midi_tick_to_seconds(&midi, &tempo, bar[0].start_tick);
    double win_end   = midi_tick_to_seconds(&midi, &tempo, bar[1].start_tick + bar[1].length_ticks);
    MidiChord chord  = {0};
    MidiKey   key    = {0};
    Rectangle roll   = { 32, 128 + 32, w - 64 , h - 128 - 64 };
    midi_detect_key(&notes, &key, -1, now, now);

    MidiChord found;
    if (midi_detect_chord(&notes, &found, -1, now, now)) chord = found;

    BeginDrawing(); {
        ClearBackground(BLANK);
        ui_background(w, h, &rori, false);
        ui_music_profile(w, h, &rori, "Yooo mom", "Mommy Remy", "UnknownRori");
        ui_music_info(w, h, key_name, midi_bpm_at(&tempo, ticks) , now, pos.numerator, pos.denominator);

        // TODO : Refactor this
        DrawRectangleRec(roll, (Color){28, 28, 36, 255});
        float row_h  = roll.height / rows;
        for (int b = 0; b < 2; b++) {
            float xb = time_to_x(midi_tick_to_seconds(&midi, &tempo, bar[b].start_tick), win_start, win_end, roll);
            DrawLineEx(VEC2(xb, roll.y), VEC2(xb, roll.y + roll.height), 2.0f, GRAY);
            DrawText(TextFormat("%u", pos.bar + b), (int)xb + 6, (int)roll.y - 24, 20, LIGHTGRAY);
            for (int k = 1; k < bar[b].beats; k++) {
                float xk = time_to_x(midi_tick_to_seconds(&midi, &tempo, bar[b].start_tick + k * bar[b].beat_ticks),
                                     win_start, win_end, roll);
                DrawLine(
                    (int)xk, 
                    (int)roll.y, 
                    (int)xk, 
                    (int)(roll.y + roll.height), 
                    (Color){60, 60, 72, 255});
            }
        }

        for (size_t i = first_note_from(&notes, win_start - max_len); i < notes.count; i++) {
            const MidiNote* n = &notes.items[i];
            if (n->start >= win_end) break;
            if (n->end <= win_start) continue;
 
            float x0 = time_to_x(n->start, win_start, win_end, roll);
            float x1 = time_to_x(n->end,   win_start, win_end, roll);
            if (x1 - x0 < 2.0f) x1 = x0 + 2.0f;
            float y = roll.y + (max_key - n->key) * row_h;
 
            Color c = (n->channel == 9) ? 
                (Color){160, 160, 170, 255} :
                ColorFromHSV(n->channel * 22.5f, 0.65f, 0.95f);

            bool  analysed = true;
            c = Fade(c, analysed ? 0.4f + 0.6f * (n->velocity / 127.0f) : 0.15f);
            Rectangle r = { x0, y + 1, x1 - x0, row_h - 2 };
            DrawRectangleRec(r, c);
            if (n->start <= now && now < n->end) DrawRectangleLinesEx(r, 2.0f, WHITE);
        }

        float px = time_to_x(now, win_start, win_end, roll);
        DrawLineEx(VEC2(px, roll.y), VEC2(px, roll.y + roll.height), 2.0f, ORANGE);
 
 

    }; EndDrawing();
}

void lyla_shutdown(void)
{

}


bool lyla_should_close() { return s_should_exit; }

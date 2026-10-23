#include <stdlib.h>

#include "lyla.h"
#include "config.h"
#include "raylib.h"
#include "rmidi.h"
#include "rmidi_ext.h"
#include "record.h"
#include "rstb_common.h"
#include "visualizer/fft.h"
#include "visualizer/visualizer.h"

#include "assets/rori.c"
#include "text.h"
#include "ui.h"
#include "utils.h"
#include "assets/lythian.c"

Font lythian;


static bool s_should_exit = false;
static FFT_Analyzer* analyzer = NULL;
static RenderTexture visualizer;
static RenderTexture render;
static Texture rori;
static Texture thumbnail;
static Texture bg;

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
        PauseMusicStream(music);
        fft_analyzer_reset(analyzer);
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
static f32 fast_energy = 0.0f;
static f32 slow_energy = 0.0f;

f32 player_get_fast_energy(void) { return fast_energy; }
f32 player_get_slow_energy(void) { return slow_energy; }

static void audio_callback(void *buffer, unsigned int frames)
{
    f32 (*fs)[2] = buffer;
    f32 sum_squares = 0.0f;

    for (unsigned int i = 0; i < frames; ++i) {
        f32 sample = fs[i][0];
        fft_analyzer_push(analyzer, sample);
        sum_squares += sample * sample;
    }

    if (frames > 0) {
        f32 current_energy = sqrtf(sum_squares / frames);
        
        f32 alpha_fast = 0.05f;
        fast_energy = fast_energy * (1.0f - alpha_fast) + current_energy * alpha_fast;

        f32 alpha_slow = 0.0008f; 
        slow_energy = slow_energy * (1.0f - alpha_slow) + current_energy * alpha_slow;
    }
}


void lyla_init(void)
{
    ChangeDirectory("./");
    analyzer = fft_analyzer_create();
    if (analyzer == NULL) {
        TraceLog(LOG_ERROR, "Failed to allocate FFT Analyzer");
        exit(69);
    }

    Image img = LoadImageFromMemory(".png", __resources_rori_png, __resources_rori_png_len);
    rori = LoadTextureFromImage(img);
    UnloadImage(img);
    if (config_music_thumbnail() == NULL) {
        thumbnail = rori;
    } else {
        thumbnail = LoadTexture(config_music_thumbnail());
    }
    if (config_music_bg() == NULL) {
        bg = rori;
    } else {
        bg = LoadTexture(config_music_bg());
    }

    lythian = LoadFontFromMemory(".ttf", __resources_Lythian_ttf, __resources_Lythian_ttf_len, 32, NULL, 0);

    // TODO : Refactor this
    i32 buf_size;
    u8* buf = LoadFileData(config_music_midi(), &buf_size);

    if (!midi_parse(&midi, buf, buf_size)) {
        exit(69);
    }
    midi_build_tempo_map(&midi, &tempo);
    midi_build_timesig_map(&midi, &sig);
    midi_collect_notes(&midi, &tempo, &notes);
    midi_build_keysig_map(&midi, &key_sig);

    piano_roll_init(&s_roll, &midi, &tempo, &sig, &notes);

    music = LoadMusicStream(config_music_audio());
    music.looping = false;
    PlayMusicStream(music);
    PauseMusicStream(music);


    AttachAudioStreamProcessor(music.stream, audio_callback);

    text_init();
    visualizer_init();
    ui_background_init(config_fx_bg_blur());

    visualizer = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
    if (config_app_render()) {
        PlayMusicStream(music);
        render = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
        record_start(config_video_output(), TextFormat("%dK", config_video_video_bitrate()), GetScreenWidth(), GetScreenHeight(), config_video_fps(), config_music_audio());
    }
}

void lyla_update(void)
{
    u32 w  = GetScreenWidth();
    u32 h  = GetScreenHeight();
    f32 dt = GetFrameTime();

    if (IsRenderTextureValid(visualizer)&& ((u32)visualizer.texture.width != w || (u32)visualizer.texture.height != h)) {
        UnloadRenderTexture(visualizer);
        visualizer = LoadRenderTexture(w, h);
    }

    handle_input();
    UpdateMusicStream(music);

    f32    now   = GetMusicTimePlayed(music);
    f32    du    = GetMusicTimeLength(music);
    double ticks = midi_seconds_to_ticks(&midi, &tempo, now);
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
    size_t m = fft_analyzer_analyze(analyzer, dt);
    f32 beat = fft_analyzer_beat(analyzer);

    bool recording = config_app_render();
    BeginDrawing(); {
        if (IsRenderTextureValid(visualizer) && config_fx_audio_visualizer())
        {
            BeginTextureMode(visualizer);
                ClearBackground(BLANK);
                static f32 quiet_time = 0.0f;
                f32 fe   = player_get_fast_energy(), se = player_get_slow_energy();
                f32 t    = now;
                f32 prog = t/du;
                bool quiet;
                if (t < 15.0f || prog < 0.05f) {
                    quiet = fe < 0.095f;
                } else if (se < 0.005f) {
                    quiet = fe < 0.01f;
                } else if (prog > 0.98f) {
                    quiet = fe < se*0.7f;
                } else {
                    quiet = fe < se*0.15f;
                }

                quiet_time = quiet ? quiet_time + dt : 0.0f;
                bool detach = quiet_time > 0.4f;
                visualizer_render(
                    (Rectangle) {0, 0, w, h},
                    fft_analyzer_smooth(analyzer),
                    fft_analyzer_smear(analyzer),
                    m, 
                    detach, 
                    beat, 
                    dt
                );
            EndTextureMode();
        }


        if (recording) {
            BeginTextureMode(render);
        }
        ClearBackground(BLANK);
        ui_background(&bg, w, h, dt, now, du);
        if (IsRenderTextureValid(visualizer)) {
            DrawTexturePro(
                visualizer.texture, 
                RECT(0, 0, w, -((float)h)), 
                RECT(0, 0, w, h), 
                VEC2_ZERO, 
                0.f, 
                ColorAlpha(WHITE, 0.10)
            );
        }
        // ui_music_profile(w, h, &rori, "Lilith Introduction", "Lilith", "UnknownRori");
        ui_music_profile(w, h, &thumbnail, "Lurie", "Solas na Gréine", "Rigël Theatre");
        ui_music_info(w, h, dt, key_text, midi_bpm_at(&tempo, ticks) , now, pos.numerator, pos.denominator);
        ui_chord_info(w, h, dt, &chord, &key);
        ui_beat_info(w, h, dt, &pos);
        piano_roll_draw(&s_roll, roll, now);

        if (recording) {
            EndTextureMode();
            record_capture(render, GetTime());

            DrawTexturePro(
                render.texture, 
                RECT_FROM_TEXTURE(0, 0, render.texture), 
                RECT(0, 0, w, h), 
                VEC2_ZERO, 
                0.f, 
                WHITE
            );
        }


        text_draw("UnknownRori's Softwork - v0.1", 0, 0, 6, GRAY);

        DrawTexturePro(
            rori, 
            RECT_FROM_TEXTURE(0, 0, rori), 
            RECT(w - 64 - 16, h - 64 - 16, 64, 64), 
            VEC2_ZERO, 
            0.0, 
            WHITE
        );

    }; EndDrawing();

    if (!IsMusicStreamPlaying(music)) {
        record_stop();
    }
}

void lyla_shutdown(void)
{
    record_stop();
    visualizer_shutdown();
    ui_background_shutdown();
    fft_analyzer_destroy(analyzer);
}


bool lyla_should_close() { return s_should_exit; }

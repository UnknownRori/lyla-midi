#include <raylib.h>
#include <stdlib.h>
#include <time.h>
#include "config.h"
#include "lyla.h"
#include "./assets/lyla.c"

#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>

static void UpdateGame()
{
    lyla_update();
}
#endif

#if defined(_WIN32)
#   define CP_UTF8 65001
#   ifdef __cplusplus
extern "C" {
#   endif

__declspec(dllimport) int __stdcall SetConsoleOutputCP(unsigned int wCodePageID);

#   ifdef __cplusplus
}
#   endif
#endif

int main()
{
#if defined(_WIN32)
    SetConsoleOutputCP(CP_UTF8);
#endif
    config_load();
    // int raylib_cfg_flag = 
    //         FLAG_WINDOW_UNDECORATED  | 
    //         FLAG_WINDOW_TRANSPARENT  | FLAG_VSYNC_HINT;
    int raylib_cfg_flag = 0;
    if (config_app_transparent()) {
        raylib_cfg_flag |= FLAG_WINDOW_TRANSPARENT;
    }
    if (config_app_vsync()) {
        raylib_cfg_flag |= FLAG_VSYNC_HINT;
    }
    if (config_app_fullscreen()) {
        raylib_cfg_flag |= FLAG_FULLSCREEN_MODE;
    }
    SetConfigFlags(raylib_cfg_flag);
    InitWindow(1280, 720, "UnknownRori's Lyla Midi Visualizer");
    InitAudioDevice();
#ifdef WITH_MICROPHONE
    InitMicrophoneDevice(44100, 2);
#endif
    SetExitKey(KEY_NULL);
    SetRandomSeed((unsigned int)time(NULL));

    if (config_app_fps() != 0 && !config_app_vsync()) {
        SetTargetFPS(config_app_fps());
    }

    if (config_music_audio() == NULL) {
        TraceLog(LOG_ERROR, "Need audio file to play");
        exit(69);
    }

    if (config_music_midi() == NULL) {
        TraceLog(LOG_ERROR, "Need midi file to visualize");
        exit(69);
    }

    lyla_init();
    Image img = LoadImageFromMemory(".png", icon_png, icon_png_len);
    SetWindowIcon(img);

#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(UpdateGame, 0, 1);
#else
    while (!WindowShouldClose()) {
        if (lyla_should_close()) {
            break;
        }

        lyla_update();
    }
#endif

    lyla_shutdown();

    CloseAudioDevice();
    CloseWindow();
}

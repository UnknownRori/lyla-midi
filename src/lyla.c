#include "lyla.h"
#include "raylib.h"
#include "rstb_common.h"

#include "assets/rori.c"
#include "text.h"
#include "ui.h"

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

void lyla_init(void)
{
    Image img = LoadImageFromMemory(".png", __resources_rori_png, __resources_rori_png_len);
    rori = LoadTextureFromImage(img);
    UnloadImage(img);

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

    BeginDrawing(); {
        ClearBackground(BLANK);
        ui_background(w, h, &rori, false);
        ui_music_profile(w, h, &rori, "Yooo mom", "Mommy Remy", "UnknownRori");
        ui_music_info(w, h, "F#", 169, 69, 3, 4);
    }; EndDrawing();
}

void lyla_shutdown(void)
{

}


bool lyla_should_close() { return s_should_exit; }

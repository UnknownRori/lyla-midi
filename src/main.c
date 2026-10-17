#include <raylib.h>

int main()
{
    InitWindow(1280, 720, "Lyla Midi");
    InitAudioDevice();

    while (!WindowShouldClose()) {
        BeginDrawing(); {
            ClearBackground(GetColor(0x181818FF));
        } EndDrawing();
    }

    CloseAudioDevice();
    CloseWindow();
    return 0;
}

#include "lyla.h"
#include "raylib.h"

static bool s_should_exit = false;

static void handle_input(void)
{
    if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
        s_should_exit = true;
    }
}

void lyla_init(void)
{

}

void lyla_update(void)
{
    handle_input();

    BeginDrawing(); {

    }; EndDrawing();
}

void lyla_shutdown(void)
{

}


bool lyla_should_close() { return s_should_exit; }

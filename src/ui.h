#pragma once

#include <raylib.h>
#include "types.h"

void ui_music_profile(
    u32   w,
    u32   h,
    const Texture* album_art,
    const char*    title,
    const char*    album,
    const char*    author
);

bool ui_background_init();
void ui_background_target_pan(u32 x, u32 y);
void ui_background(
    u32   w,
    u32   h,
    const Texture* texture,
    bool  auto_pan
);

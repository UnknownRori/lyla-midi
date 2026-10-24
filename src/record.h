#pragma once

#include <raylib.h>
#include "types.h"

bool record_start(const char *filename, const char *bitrate, u32 width, u32 height, u32 fps, const char *audio_path);
bool record_capture(RenderTexture2D target, double time_sec);
bool record_stop(void);

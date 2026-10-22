#pragma once

#include "types.h"

bool config_load();

u8   config_app_fps();
bool config_app_vsync();
bool config_app_transparent();
bool config_app_fullscreen();
bool config_app_render();

bool config_fx_piano_roll_bloom();
bool config_fx_piano_roll_trail();
bool config_fx_piano_roll_flash();
bool config_fx_audio_visualizer();
f32  config_fx_bg_blur();

const char* config_music_title();
const char* config_music_album();
const char* config_music_author();
const char* config_music_audio();
const char* config_music_midi();
const char* config_music_thumbnail();
const char* config_music_bg();

const char* config_video_output();
u32   config_video_video_bitrate();
u32   config_video_fps();

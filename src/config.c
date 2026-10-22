#include "config.h"
#include "raylib.h"
#include "raymath.h"
#include "rconfig.h"
#include "types.h"
#include <string.h>

typedef struct LylaMidiConfig {
    u8   fps;
    bool vsync;
    bool transparent;
    bool fullscreen;
    bool render;
    bool piano_roll_bloom;
    bool piano_roll_trail;
    bool piano_roll_flash;
    bool audio_visualizer;
    f32  bg_blur;
    char* music_title;
    char* music_album;
    char* music_author;
    char* music_audio;
    char* music_midi;
    char* music_thumbnail;
    char* music_bg;

    char* video_output;
    u32   video_bitrate;
    u8   video_fps;
} LylaMidiConfig;

static rori_config_t  s_config;
static LylaMidiConfig s_midi_config;

bool config_load()
{
    memset(&s_config, 0, sizeof(s_config));
    char* config_file = LoadFileText("config.cfg");
    if (!rconfig_parse_buffer(&s_config, config_file)) {
        return false;
    }

    s_midi_config = (LylaMidiConfig){
        .fps              = 0,
        .vsync            = true,
        .transparent      = false,
        .fullscreen       = true,
        .render           = false,
        .piano_roll_bloom = true,
        .piano_roll_trail = true,
        .piano_roll_flash = true,
        .audio_visualizer = true,
        .bg_blur          = 1.0,
        .music_title      = "No Title",
        .music_album      = NULL,
        .music_author     = "No Author",
        .music_audio      = NULL,
        .music_midi       = NULL,
        .music_thumbnail  = NULL,
        .music_bg         = NULL,
        .video_output     = NULL,
        .video_bitrate    = 10000,
    };

    i32   i32val;
    bool  boolval;
    char strval[256];
    if (rconfig_get_properties_i32(&s_config, "APP", "fps", &i32val)) {
        s_midi_config.fps = i32val > 0 ? (u8)i32val > 255 ? 255 : (u8)i32val : 60;
    }

    if (rconfig_get_properties_bool(&s_config, "APP", "vsync", &boolval)) {
        s_midi_config.vsync = boolval;
    }

    if (rconfig_get_properties_bool(&s_config, "APP", "transparent", &boolval)) {
        s_midi_config.transparent = boolval;
    }

    if (rconfig_get_properties_bool(&s_config, "APP", "fullscreen", &boolval)) {
        s_midi_config.fullscreen = boolval;
    }

    if (rconfig_get_properties_bool(&s_config, "APP", "render", &boolval)) {
        s_midi_config.render = boolval;
    }

    if (rconfig_get_properties_bool(&s_config, "FX", "piano_roll_bloom", &boolval)) {
        s_midi_config.piano_roll_bloom = boolval;
    }

    if (rconfig_get_properties_bool(&s_config, "FX", "piano_roll_trail", &boolval)) {
        s_midi_config.piano_roll_trail = boolval;
    }

    if (rconfig_get_properties_bool(&s_config, "FX", "piano_roll_flash", &boolval)) {
        s_midi_config.piano_roll_flash = boolval;
    }

    if (rconfig_get_properties_bool(&s_config, "FX", "audio_visualizer", &boolval)) {
        s_midi_config.audio_visualizer = boolval;
    }

    if (rconfig_get_properties_i32(&s_config, "FX", "bg_blur", &i32val)) {
        s_midi_config.bg_blur = Clamp((f32)i32val/100., 0., 1.);
    }

    if (rconfig_get_properties_cstr(&s_config, "MUSIC", "title", strval, sizeof(strval))) {
        s_midi_config.music_title = strdup(strval);
    }

    if (rconfig_get_properties_cstr(&s_config, "MUSIC", "title", strval, sizeof(strval))) {
        s_midi_config.music_album = strdup(strval);
    }

    if (rconfig_get_properties_cstr(&s_config, "MUSIC", "author", strval, sizeof(strval))) {
        s_midi_config.music_author = strdup(strval);
    }

    if (rconfig_get_properties_cstr(&s_config, "MUSIC", "audio", strval, sizeof(strval))) {
        s_midi_config.music_audio = strdup(strval);
    }

    if (rconfig_get_properties_cstr(&s_config, "MUSIC", "midi", strval, sizeof(strval))) {
        s_midi_config.music_midi = strdup(strval);
    }

    if (rconfig_get_properties_cstr(&s_config, "MUSIC", "thumbnail", strval, sizeof(strval))) {
        s_midi_config.music_thumbnail = strdup(strval);
    }

    if (rconfig_get_properties_cstr(&s_config, "MUSIC", "bg", strval, sizeof(strval))) {
        s_midi_config.music_bg = strdup(strval);
    }

    if (rconfig_get_properties_cstr(&s_config, "RENDER", "output", strval, sizeof(strval))) {
        s_midi_config.video_output = strdup(strval);
    }

    if (rconfig_get_properties_i32(&s_config, "RENDER", "video_bitrate", &i32val)) {
        s_midi_config.video_bitrate = i32val > 0 ? i32val : 10000;
    }

    if (rconfig_get_properties_i32(&s_config, "RENDER", "fps", &i32val)) {
        s_midi_config.video_fps = i32val > 0 ? (u8)i32val > 255 ? 255 : (u8)i32val : 60;
    }

    return true;
}

u8   config_app_fps()         { return s_midi_config.fps; }
bool config_app_vsync()       { return s_midi_config.vsync; }
bool config_app_transparent() { return s_midi_config.transparent; }
bool config_app_fullscreen()  { return s_midi_config.fullscreen; }
bool config_app_render()      { return s_midi_config.render; }

bool config_fx_piano_roll_bloom() { return s_midi_config.piano_roll_bloom; }
bool config_fx_piano_roll_trail() { return s_midi_config.piano_roll_trail; }
bool config_fx_piano_roll_flash() { return s_midi_config.piano_roll_flash; }
bool config_fx_audio_visualizer() { return s_midi_config.audio_visualizer; }
f32  config_fx_bg_blur()          { return s_midi_config.bg_blur; }

const char* config_music_title()     { return s_midi_config.music_title; }
const char* config_music_album()     { return s_midi_config.music_album; }
const char* config_music_author()    { return s_midi_config.music_author; }
const char* config_music_audio()     { return s_midi_config.music_audio; }
const char* config_music_midi()      { return s_midi_config.music_midi; }
const char* config_music_thumbnail() { return s_midi_config.music_thumbnail; }
const char* config_music_bg()        { return s_midi_config.music_bg; }

const char* config_video_output()    { return s_midi_config.video_output; }
u32   config_video_video_bitrate()   { return s_midi_config.video_bitrate; }
u32   config_video_fps()             { return s_midi_config.video_fps; }

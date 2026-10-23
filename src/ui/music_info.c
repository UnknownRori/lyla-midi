#include <stdio.h>
#include <string.h>
#include "raylib.h"
#include "rstb_common.h"
#include "text.h"
#include "ui.h"
#include "utils.h"
#include "anim.h"

#define PADDING             16
#define PADDING_BETWEEN     (PADDING * 3)
#define TEXT_SIZE           28
#define TEXT_SIZE_SECONDARY 16

static AnimSwap s_key;
static AnimSwap s_time;
static AnimSwap s_bpm;

static char s_elapsed[32];
static f32  s_elapsed_alpha;
static f32  s_elapsed_offset;

void ui_music_info(
    u32 w, 
    u32 h, 
    f32 dt,
    const char* key_signature, 
    u8 bpm,
    float time_played,
    u8 nominator,
    u8 denominator
) {
    UNUSED(h);

    f32 offset_y = PADDING * 3;
    f32 offset_x = w - PADDING * 2;
    const char* elapsed_text = "ELAPSED";
    Vector2 elapsed_text_measure = text_measure(elapsed_text, TEXT_SIZE_SECONDARY);
    elapsed_text_measure.x += PADDING_BETWEEN;
    text_draw(
        elapsed_text, 
        offset_x -  elapsed_text_measure.x, 
        offset_y, 
        TEXT_SIZE_SECONDARY, 
        GRAY
    );

    const char* elapsed = TextFormat("%02d:%02d", (int)(time_played / 60), (int)time_played % 60);
    text_draw(
        elapsed, 
        offset_x -  elapsed_text_measure.x, 
        offset_y + PADDING + elapsed_text_measure.y, 
        TEXT_SIZE, 
        WHITE
    );


    const char* meter = "METER";
    Vector2 meter_measure = text_measure(meter, TEXT_SIZE_SECONDARY);
    meter_measure.x += PADDING_BETWEEN;
    text_draw(
        meter, 
        offset_x - meter_measure.x - elapsed_text_measure.x, 
        offset_y, 
        TEXT_SIZE_SECONDARY, 
        GRAY
    );

    const char* bar = TextFormat("%u/%u", nominator, denominator);
    AnimFrame time_anim = anim_swap_update(&s_time, bar, ANIM_NO_DELAY, dt);
    text_draw(
        s_time.shown, 
        offset_x - meter_measure.x - elapsed_text_measure.x, 
        offset_y + PADDING + meter_measure.y + time_anim.offset, 
        TEXT_SIZE, 
        Fade(WHITE, time_anim.alpha)
    );

    const char* bpm_text = "BPM";
    Vector2 bpm_text_measure = text_measure(bpm_text, TEXT_SIZE_SECONDARY);
    bpm_text_measure.x += PADDING_BETWEEN;
    text_draw(
        bpm_text, 
        offset_x - meter_measure.x - elapsed_text_measure.x - bpm_text_measure.x, 
        offset_y, 
        TEXT_SIZE_SECONDARY, 
        GRAY
    );

    AnimFrame bpm_anim = anim_swap_update(&s_bpm, TextFormat("%d", bpm), ANIM_NO_DELAY, dt);
    text_draw(
        s_bpm.shown, 
        offset_x - meter_measure.x - elapsed_text_measure.x - bpm_text_measure.x, 
        offset_y + PADDING + elapsed_text_measure.y + bpm_anim.offset, 
        TEXT_SIZE, 
        Fade(WHITE, bpm_anim.alpha)
    );

    const char* key_sig_text = "Key";
    Vector2 key_sig_text_measure = text_measure(key_sig_text, TEXT_SIZE_SECONDARY);
    key_sig_text_measure.x += PADDING_BETWEEN * 2;
    const char* key_text = key_signature ? key_signature : "";
    AnimFrame key_anim = anim_swap_update(&s_key, key_text, ANIM_NO_DELAY, dt);

    Vector2 key_sig_measure = text_measure(s_key.shown, TEXT_SIZE);
    text_draw(
        s_key.shown, 
        offset_x - meter_measure.x - elapsed_text_measure.x - bpm_text_measure.x - key_sig_text_measure.x - key_sig_measure.x / 2. ,
        offset_y + PADDING + elapsed_text_measure.y + key_anim.offset, 
        TEXT_SIZE, 
        Fade(WHITE, key_anim.alpha)
    );

    text_draw(
        key_sig_text, 
        offset_x - meter_measure.x - elapsed_text_measure.x - bpm_text_measure.x - key_sig_text_measure.x - key_sig_measure.x /8., 
        offset_y, 
        TEXT_SIZE_SECONDARY, 
        GRAY
    );
}

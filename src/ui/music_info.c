#include "raylib.h"
#include "rstb_common.h"
#include "text.h"
#include "ui.h"
#include "utils.h"

#define PADDING             16
#define PADDING_BETWEEN     (PADDING * 3)
#define TEXT_SIZE           28
#define TEXT_SIZE_SECONDARY 16

void ui_music_info(
    u32 w, 
    u32 h, 
    const char* key_signature, 
    u8 bpm,
    float time_played,
    u8 nominator,
    u8 denominator
) {
    UNUSED(h);

    f32 offset_y = PADDING * 3;
    f32 offset_x = w - PADDING * 2;
    const char* meter = "METER";
    Vector2 meter_measure = text_measure(meter, TEXT_SIZE_SECONDARY);
    text_draw(
        meter, 
        offset_x - meter_measure.x, 
        offset_y, 
        TEXT_SIZE_SECONDARY, 
        GRAY
    );

    const char* bar = TextFormat("%u/%u", nominator, denominator);
    text_draw(
        bar, 
        offset_x - meter_measure.x, 
        offset_y + PADDING + meter_measure.y, 
        TEXT_SIZE, 
        WHITE
    );

    const char* elapsed_text = "ELAPSED";
    Vector2 elapsed_text_measure = text_measure(elapsed_text, TEXT_SIZE_SECONDARY);
    elapsed_text_measure.x += PADDING_BETWEEN;
    text_draw(
        elapsed_text, 
        offset_x - meter_measure.x - elapsed_text_measure.x, 
        offset_y, 
        TEXT_SIZE_SECONDARY, 
        GRAY
    );

    const char* elapsed = TextFormat("%02d:%02d", (int)(time_played / 60), (int)time_played % 60);
    text_draw(
        elapsed, 
        offset_x - meter_measure.x - elapsed_text_measure.x, 
        offset_y + PADDING + elapsed_text_measure.y, 
        TEXT_SIZE, 
        WHITE
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

    text_draw(
        TextFormat("%d", bpm), 
        offset_x - meter_measure.x - elapsed_text_measure.x - bpm_text_measure.x, 
        offset_y + PADDING + elapsed_text_measure.y, 
        TEXT_SIZE, 
        WHITE
    );

    const char* key_sig_text = "Key";
    Vector2 key_sig_text_measure = text_measure(key_sig_text, TEXT_SIZE_SECONDARY);
    key_sig_text_measure.x += PADDING_BETWEEN;
    text_draw(
        key_sig_text, 
        offset_x - meter_measure.x - elapsed_text_measure.x - bpm_text_measure.x - key_sig_text_measure.x, 
        offset_y, 
        TEXT_SIZE_SECONDARY, 
        GRAY
    );

    text_draw(
        key_signature, 
        offset_x - meter_measure.x - elapsed_text_measure.x - bpm_text_measure.x - key_sig_text_measure.x, 
        offset_y + PADDING + elapsed_text_measure.y, 
        TEXT_SIZE, 
        WHITE
    );
}

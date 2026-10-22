#include "raylib.h"
#include "rmidi_ext.h"
#include "text.h"
#include "ui.h"
#include "utils.h"

#define PADDING             16
#define TEXT_SIZE           32
#define TEXT_SIZE_SECONDARY 16
#define ALBUM_ART_SIZE      96
#define TEXT_SIZE_TERTIARY  11

#ifndef CHORD_INFO_TONES
#define CHORD_INFO_TONES    1
#endif

void ui_chord_info_ex(
    u32 w, 
    u32 h, 
    MidiChord* chord,
    MidiKey*   key,
    bool       show_tones
)
{
    char name[32];
    char roman[32];
    midi_chord_name(chord, false, name, sizeof(name));
    midi_chord_roman(chord, key, roman, sizeof(roman));

    DrawRectangleGradientH(w / 2., h - 96, 256, 2, WHITE, BLANK);
    DrawRectangleGradientH(w / 2. - 256, h - 96, 256, 2, BLANK, WHITE);

    Vector2 name_measure = text_measure(name, TEXT_SIZE);
    Vector2 name_pos = {w / 2. - name_measure.x / 2., h - 128 + name_measure.y + PADDING};
    text_draw(name, name_pos.x, name_pos.y, TEXT_SIZE, WHITE);

    Vector2 roman_measure = text_measure(roman, TEXT_SIZE_SECONDARY);
    text_draw(roman, name_pos.x - roman_measure.x - PADDING, name_pos.y + roman_measure.y / 2., TEXT_SIZE_SECONDARY, GRAY);

    if (show_tones) {
        MidiChordTone tones[8];
        int           count = midi_chord_tones(chord, false, tones, 8);
        float         x     = name_pos.x + name_measure.x + PADDING * 2;
        Vector2 tones_measure = text_measure("TONES", 8);
        text_draw("TONES", x, name_pos.y - tones_measure.y + 8, 8, GRAY);
        for (int i = 0; i < count; i++) {
            Vector2 tone_measure   = text_measure(tones[i].name, TEXT_SIZE_SECONDARY);
            float   top = name_pos.y + tones_measure.y + tone_measure.y / 2.;
            text_draw(tones[i].name, x, top, TEXT_SIZE_SECONDARY, i == 0 ? WHITE : LIGHTGRAY);
            x += (tone_measure.x) + PADDING;
        }

    }
}

void ui_chord_info(
    u32 w, 
    u32 h, 
    MidiChord* chord,
    MidiKey*   key
)
{
    ui_chord_info_ex(w, h, chord, key, CHORD_INFO_TONES);
}

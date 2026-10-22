#include "raylib.h"
#include "raylib.h"
#include "rmidi_ext.h"
#include "text.h"
#include "ui.h"
#include "utils.h"
#include "anim.h"

#define PADDING             16
#define TEXT_SIZE           32
#define TEXT_SIZE_SECONDARY 16
#define ALBUM_ART_SIZE      96
#define TEXT_SIZE_TERTIARY  11

#ifndef CHORD_INFO_TONES
#define CHORD_INFO_TONES    1
#endif

static AnimSwap  s_swap;
static char      s_name[32];
static char      s_roman[32];
static MidiChord s_chord;

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

    char signature[64];
    snprintf(signature, sizeof(signature), "%s|%s", name, roman);
    AnimFrame frame = anim_swap_update(&s_swap, signature, ANIM_CHORD_DELAY, GetFrameTime());
    if (frame.replaced) {
        snprintf(s_name, sizeof(s_name), "%s", name);
        snprintf(s_roman, sizeof(s_roman), "%s", roman);
        s_chord = *chord;
    }

    DrawRectangleGradientH(w / 2., h - 96, 256, 2, WHITE, BLANK);
    DrawRectangleGradientH(w / 2. - 256, h - 96, 256, 2, BLANK, WHITE);

    Vector2 name_measure = text_measure(s_name, TEXT_SIZE);
    Vector2 name_pos = {w / 2. - name_measure.x / 2., h - 128 + name_measure.y + PADDING};
    text_draw(s_name, name_pos.x, name_pos.y + frame.offset, TEXT_SIZE, Fade(WHITE, frame.alpha));

    Vector2 roman_measure = text_measure(s_roman, TEXT_SIZE_SECONDARY);
    text_draw(s_roman, name_pos.x - roman_measure.x - PADDING, name_pos.y + roman_measure.y / 2. + frame.offset, TEXT_SIZE_SECONDARY, Fade(GRAY, frame.alpha));

    if (show_tones) {
        MidiChordTone tones[8];
        int           count = midi_chord_tones(&s_chord, false, tones, 8);
        float         x     = name_pos.x + name_measure.x + PADDING * 2;
        Vector2 tones_measure = text_measure("TONES", 8);
        text_draw("TONES", x, name_pos.y - tones_measure.y + 8, 8, Fade(GRAY, frame.alpha));
        for (int i = 0; i < count; i++) {
            Vector2 tone_measure   = text_measure(tones[i].name, TEXT_SIZE_SECONDARY);
            float   top = name_pos.y + tones_measure.y + tone_measure.y / 2.;
            text_draw(tones[i].name, x, top + frame.offset, TEXT_SIZE_SECONDARY, Fade(i == 0 ? WHITE : LIGHTGRAY, frame.alpha));
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

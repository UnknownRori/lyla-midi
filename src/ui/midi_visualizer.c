#include "raylib.h"
#include "ui.h"
#include "text.h"
#include <math.h>

#define PIANO_ROLL_MAX_GLOW  256
#define PIANO_ROLL_DRUM_ROWS 4
#define PIANO_ROLL_DRUM_LIFE 0.45f
#define PIANO_ROLL_KICK_LIFE 0.70f

typedef enum PianoRollDrumRow {
    PIANO_ROLL_DRUM_KICK,
    PIANO_ROLL_DRUM_SNARE,
    PIANO_ROLL_DRUM_TOM,
    PIANO_ROLL_DRUM_HAT,
} PianoRollDrumRow;

typedef struct PianoRollGlow {
    float x, y, height;
    Color color;
    float intensity;
} PianoRollGlow;

typedef struct PianoRollPage {
    double start_tick;
    double end_tick;
    double start_sec;
    double end_sec;
    bool   is_trail;
} PianoRollPage;

typedef struct PianoRollView {
    const PianoRoll* piano_roll;
    unsigned         fx;
    double           now;
    double           playhead_tick;
    float            scale;
    float            left_x, right_x, playhead_x;
    float            area_top, area_bottom;
    float            ruler_y, playhead_top;
    float            pitch_bottom;
    float            drum_lane_height, drum_row_height;
    float            key_slot, note_thickness;
    float            head_width, tail_spread;
    float            kick_flash, kick_spread;
    double           ticks_per_pixel;
    double           head_ticks;
    double           tail_life_ticks, tail_delay_ticks, tail_decay_ticks;
    double           oldest_visible_tick;
    double           cell_width_ticks, cell_pitch_ticks;
    PianoRollGlow    glows[PIANO_ROLL_MAX_GLOW];
    int              glow_count;
} PianoRollView;

static float piano_roll_x(double tick, double from_tick, double to_tick, float left_x, float right_x)
{
    return left_x + (float)((tick - from_tick) / (to_tick - from_tick)) * (right_x - left_x);
}

static float piano_roll_page_x(const PianoRollView* view, const PianoRollPage* page, double tick)
{
    return piano_roll_x(tick, page->start_tick, page->end_tick, view->left_x, view->right_x);
}

static size_t piano_roll_first_note(const MidiNoteList* notes, double start_sec)
{
    size_t low = 0, high = notes->count;
    while (low < high) {
        size_t middle = (low + high) / 2;
        if (notes->items[middle].start < start_sec) low = middle + 1;
        else                                        high = middle;
    }
    return low;
}

static float piano_roll_ease_out(float progress)
{
    return 1.0f - (1.0f - progress) * (1.0f - progress);
}

static Color piano_roll_color(int channel)
{
    static const Color BASE = { 226, 219, 210, 255 };
    if (channel == 9) return BASE;
    Color hue = ColorFromHSV(fmodf(channel * 137.5f, 360.0f), (channel % 2) ? 0.45f : 0.6f, 1.0f);
    return (Color){
        (unsigned char)(BASE.r + (hue.r - BASE.r) * 0.35f),
        (unsigned char)(BASE.g + (hue.g - BASE.g) * 0.35f),
        (unsigned char)(BASE.b + (hue.b - BASE.b) * 0.35f),
        255
    };
}

static Color piano_roll_glow_color(int channel)
{
    if (channel == 9) return WHITE;
    return ColorFromHSV(fmodf(channel * 137.5f, 360.0f), 0.7f, 1.0f);
}

static Color piano_roll_lerp(Color from, Color to, float amount)
{
    return (Color){
        (unsigned char)(from.r + (to.r - from.r) * amount),
        (unsigned char)(from.g + (to.g - from.g) * amount),
        (unsigned char)(from.b + (to.b - from.b) * amount),
        255
    };
}

static float piano_roll_hash(uint32_t seed_a, uint32_t seed_b)
{
    uint32_t hash = (seed_a * 0x9E3779B1u) ^ ((seed_b + 0x7F4A7C15u) * 0x85EBCA6Bu);
    hash ^= hash >> 15;
    hash *= 0x2C1B3C6Du;
    hash ^= hash >> 12;
    return (float)(hash & 0xFFFF) / 32767.5f - 1.0f;
}

static PianoRollDrumRow piano_roll_drum_row(int key)
{
    switch (key) {
        case 35: case 36:
            return PIANO_ROLL_DRUM_KICK;
        case 37: case 38: case 39: case 40:
            return PIANO_ROLL_DRUM_SNARE;
        case 41: case 43: case 45: case 47: case 48: case 50:
            return PIANO_ROLL_DRUM_TOM;
        default:
            return PIANO_ROLL_DRUM_HAT;
    }
}

static Color piano_roll_drum_color(PianoRollDrumRow row)
{
    switch (row) {
        case PIANO_ROLL_DRUM_KICK:  return (Color){ 255, 120,  80, 255 };
        case PIANO_ROLL_DRUM_SNARE: return (Color){ 255, 214, 120, 255 };
        case PIANO_ROLL_DRUM_TOM:   return (Color){ 120, 200, 255, 255 };
        default:                    return (Color){ 190, 255, 225, 255 };
    }
}

static bool piano_roll_last_kick(const MidiNoteList* notes, double now, double* age, float* velocity)
{
    for (size_t note_index = piano_roll_first_note(notes, now + 1e-9); note_index > 0; note_index--) {
        const MidiNote* note = &notes->items[note_index - 1];
        if (now - note->start > 0.5) break;
        if (note->channel == 9 && (note->key == 35 || note->key == 36)) {
            *age      = now - note->start;
            *velocity = note->velocity;
            return true;
        }
    }
    return false;
}

void piano_roll_init(PianoRoll* piano_roll, const Midi* midi, const MidiTempoMap* tempo, const MidiTimeSigMap* sig, const MidiNoteList* notes)
{
    piano_roll->midi          = midi;
    piano_roll->tempo         = tempo;
    piano_roll->sig           = sig;
    piano_roll->notes         = notes;
    piano_roll->bars_per_page = 2;
    piano_roll->max_len       = 0.0;
    piano_roll->clock         = 0.0;

    int lowest_key = 127, highest_key = 0;
    da_foreach(MidiNote, note, notes) {
        if (note->end - note->start > piano_roll->max_len) piano_roll->max_len = note->end - note->start;
        if (note->channel == 9) continue;
        if (note->key < lowest_key)  lowest_key  = note->key;
        if (note->key > highest_key) highest_key = note->key;
    }
    if (lowest_key > highest_key) { lowest_key = 48; highest_key = 84; }
    lowest_key  -= 1;
    highest_key += 1;
    while (highest_key - lowest_key < 24) { lowest_key--; highest_key++; }
    piano_roll->low_key  = lowest_key < 0 ? 0 : lowest_key;
    piano_roll->high_key = highest_key > 127 ? 127 : highest_key;
}

double piano_roll_time(PianoRoll* piano_roll, double audio_sec, double delta_sec, bool playing)
{
    if (!playing) {
        piano_roll->clock = audio_sec;
        return piano_roll->clock;
    }
    piano_roll->clock += delta_sec;
    double error = audio_sec - piano_roll->clock;
    if (fabs(error) > 0.05) piano_roll->clock = audio_sec;
    else                    piano_roll->clock += error * 0.1;
    return piano_roll->clock;
}

static void piano_roll_draw_page_notes(PianoRollView* view, const PianoRollPage* page)
{
    const PianoRoll* piano_roll = view->piano_roll;
    const float      scale      = view->scale;
    const float      clip_x     = page->is_trail ? view->playhead_x : -1e9f;
    const double     first_sec  = page->is_trail ? fmax(page->start_sec, view->now - PIANO_ROLL_TAIL_LIFE) : page->start_sec;
    const double     last_sec   = page->is_trail ? page->end_sec : view->now;

    for (size_t note_index = piano_roll_first_note(piano_roll->notes, first_sec - piano_roll->max_len);
         note_index < piano_roll->notes->count; note_index++) {
        const MidiNote* note = &piano_roll->notes->items[note_index];
        if (note->start > last_sec) break;
        if (note->channel == 9) continue;
        if (note->key < piano_roll->low_key || note->key > piano_roll->high_key) continue;

        double note_start_tick = midi_seconds_to_ticks(piano_roll->midi, piano_roll->tempo, note->start);
        double note_end_tick   = midi_seconds_to_ticks(piano_roll->midi, piano_roll->tempo, note->end);
        if (note_start_tick < page->start_tick && note_end_tick <= page->start_tick) continue;
        if (note_start_tick >= page->end_tick) continue;

        double visible_start_tick = note_start_tick < page->start_tick ? page->start_tick : note_start_tick;
        double visible_end_tick   = note_end_tick < view->playhead_tick ? note_end_tick : view->playhead_tick;
        if (page->is_trail && visible_end_tick > page->end_tick) visible_end_tick = page->end_tick;
        if (page->is_trail && visible_end_tick <= view->oldest_visible_tick) continue;
        bool is_active = !page->is_trail && view->playhead_tick < note_end_tick;

        float       note_left_x  = piano_roll_page_x(view, page, visible_start_tick);
        float       note_right_x = piano_roll_page_x(view, page, visible_end_tick);
        if (note_right_x <= clip_x) continue;
        float       head_x       = note_right_x;
        float       center_y     = view->pitch_bottom - ((float)(note->key - piano_roll->low_key) + 0.5f) * view->key_slot;
        Color       note_color   = piano_roll_color(note->channel);
        Color       bright_color = piano_roll_glow_color(note->channel);
        const float thickness    = view->note_thickness;

        if (view->fx & PIANO_ROLL_FX_TAIL) {
            const double solid_from_tick  = view->playhead_tick - view->tail_delay_ticks;
            const double solid_start_tick = visible_start_tick > solid_from_tick ? visible_start_tick : solid_from_tick;
            if (visible_end_tick > solid_start_tick) {
                float age_left    = (float)((view->playhead_tick - solid_start_tick) / view->tail_life_ticks);
                float age_right   = (float)((view->playhead_tick - visible_end_tick) / view->tail_life_ticks);
                int   solid_x     = (int)roundf(fmaxf(clip_x, piano_roll_page_x(view, page, solid_start_tick)));
                int   solid_width = (int)roundf(note_right_x) - solid_x;
                if (solid_width < 1 && !page->is_trail) solid_width = 1;
                if (solid_width >= 1) {
                    DrawRectangleGradientH(solid_x, (int)roundf(center_y - thickness / 2), solid_width, (int)fmaxf(1.0f, roundf(thickness)),
                                           Fade(piano_roll_lerp(bright_color, note_color, fminf(1.0f, age_left * 2.0f)), 0.9f),
                                           Fade(piano_roll_lerp(bright_color, note_color, fminf(1.0f, age_right * 2.0f)), 0.9f));
                }
            }

            const double cell_start_limit = visible_start_tick > view->oldest_visible_tick ? visible_start_tick : view->oldest_visible_tick;
            const double cell_end_limit   = visible_end_tick < solid_from_tick ? visible_end_tick : solid_from_tick;
            if (cell_end_limit > cell_start_limit && view->cell_pitch_ticks > 1e-9 && view->tail_decay_ticks > 1e-9) {
                long cell_index = (long)floor((cell_start_limit - note_start_tick) / view->cell_pitch_ticks);
                if (cell_index < 0) cell_index = 0;
                for (int count = 0; count < 512; count++, cell_index++) {
                    double cell_start_tick = note_start_tick + cell_index * view->cell_pitch_ticks;
                    if (cell_start_tick >= cell_end_limit) break;
                    double from_tick = cell_start_tick < cell_start_limit ? cell_start_limit : cell_start_tick;
                    double to_tick   = cell_start_tick + view->cell_width_ticks > cell_end_limit ? cell_end_limit : cell_start_tick + view->cell_width_ticks;
                    if (to_tick <= from_tick) continue;
                    double middle_tick = (from_tick + to_tick) / 2;
                    float  decay       = (float)((view->playhead_tick - middle_tick - view->tail_delay_ticks) / view->tail_decay_ticks);
                    if (decay >= 1.0f) continue;
                    if (decay < 0.0f) decay = 0.0f;

                    uint32_t note_seed = (uint32_t)note_index, cell_seed = (uint32_t)cell_index * 4u;
                    float random_y = piano_roll_hash(note_seed, cell_seed + 0u);
                    float random_x = piano_roll_hash(note_seed, cell_seed + 1u);
                    float random_w = piano_roll_hash(note_seed, cell_seed + 2u);
                    float random_h = piano_roll_hash(note_seed, cell_seed + 3u);

                    float variation   = fminf(1.0f, decay * 3.0f);
                    float cell_width  = (float)((to_tick - from_tick) / view->ticks_per_pixel) * (1.0f - 0.45f * decay) * (1.0f + 0.30f * view->tail_spread * variation * random_w);
                    float cell_height = thickness * (1.0f - 0.75f * decay) * (1.0f + 0.25f * view->tail_spread * variation * random_h);
                    if (cell_width < scale)  cell_width  = scale;
                    if (cell_height < scale) cell_height = scale;
                    float cell_center_x = piano_roll_page_x(view, page, middle_tick)
                                        + (-0.55f + 0.45f * random_x) * 16 * scale * view->tail_spread * decay;
                    if (cell_center_x <= clip_x) continue;
                    float cell_center_y = center_y + random_y * thickness * view->tail_spread * decay;
                    if (cell_center_y - cell_height / 2 < view->area_top)      cell_center_y = view->area_top + cell_height / 2;
                    if (cell_center_y + cell_height / 2 > view->pitch_bottom)  cell_center_y = view->pitch_bottom - cell_height / 2;
                    float age_ratio  = (float)((view->playhead_tick - middle_tick) / view->tail_life_ticks);
                    Color cell_color = piano_roll_lerp(bright_color, note_color, fminf(1.0f, age_ratio * 2.0f));
                    DrawRectangleRec((Rectangle){ cell_center_x - cell_width / 2, cell_center_y - cell_height / 2, cell_width, cell_height },
                                     Fade(cell_color, powf(1.0f - decay, 1.4f) * 0.9f * (1.0f - 0.15f * view->tail_spread * variation * (random_w + 1.0f) / 2.0f)));
                }
            }
        } else {
            if (note_right_x - note_left_x < 2 * scale) note_right_x = note_left_x + 2 * scale;
            float attack_x = piano_roll_x(fmin(visible_start_tick + view->head_ticks, visible_end_tick), page->start_tick, page->end_tick, view->left_x, view->right_x);
            if (attack_x < note_left_x + 2 * scale) attack_x = fminf(note_left_x + 2 * scale, note_right_x);
            float top_y = center_y - thickness / 2;
            if (note_right_x > attack_x) DrawRectangleRec((Rectangle){ attack_x, top_y, note_right_x - attack_x, thickness }, Fade(note_color, is_active ? 0.78f : 0.5f));
            DrawRectangleRec((Rectangle){ note_left_x, top_y, attack_x - note_left_x, thickness }, Fade(note_color, 0.96f));
        }

        if ((view->fx & PIANO_ROLL_FX_BLOOM) && is_active && view->glow_count < PIANO_ROLL_MAX_GLOW) {
            float head_left   = fminf(fmaxf(note_left_x, head_x - view->head_width), head_x - 2 * scale);
            float velocity    = 0.35f + 0.65f * (note->velocity / 127.0f);
            float onset_boost = 1.0f + 1.6f * expf(-(float)(view->now - note->start) / 0.10f);
            DrawRectangleRec((Rectangle){ head_left, center_y - thickness * 0.65f, head_x - head_left, thickness * 1.3f }, bright_color);
            view->glows[view->glow_count++] = (PianoRollGlow){ head_x, center_y, thickness, bright_color, velocity * onset_boost };
        }
    }
}

static void piano_roll_draw_page_drums(PianoRollView* view, const PianoRollPage* page)
{
    const PianoRoll* piano_roll = view->piano_roll;
    const float      scale      = view->scale;
    const bool       has_lanes  = (view->fx & PIANO_ROLL_FX_DRUMS) != 0;
    const double     first_sec  = has_lanes ? fmax(page->start_sec, view->now - PIANO_ROLL_KICK_LIFE) : page->start_sec;
    const double     last_sec   = page->is_trail ? page->end_sec : view->now;

    for (size_t note_index = piano_roll_first_note(piano_roll->notes, first_sec); note_index < piano_roll->notes->count; note_index++) {
        const MidiNote* note = &piano_roll->notes->items[note_index];
        if (note->start > last_sec) break;
        if (note->channel != 9) continue;

        double hit_tick = midi_seconds_to_ticks(piano_roll->midi, piano_roll->tempo, note->start);
        if (hit_tick < page->start_tick || hit_tick >= page->end_tick) continue;
        float hit_x = piano_roll_page_x(view, page, hit_tick);
        if (page->is_trail && hit_x <= view->playhead_x) continue;
        float age = (float)(view->now - note->start);

        if (!has_lanes) {
            float marker_size = 5 * scale;
            float marker_y    = view->area_bottom - view->drum_lane_height / 2;
            DrawRectanglePro((Rectangle){ hit_x, marker_y, marker_size, marker_size }, (Vector2){ marker_size / 2, marker_size / 2 }, 45,
                             age < 0.08f ? WHITE : Fade(piano_roll_color(9), 0.7f));
            continue;
        }

        PianoRollDrumRow row  = piano_roll_drum_row(note->key);
        float            life = row == PIANO_ROLL_DRUM_KICK ? PIANO_ROLL_KICK_LIFE : PIANO_ROLL_DRUM_LIFE;
        if (age >= life) continue;

        float progress      = age / life;
        float base_y        = view->area_bottom - ((float)row + 0.5f) * view->drum_row_height;
        float rise_distance = row == PIANO_ROLL_DRUM_KICK ? view->drum_lane_height : view->drum_row_height * 0.9f;
        float marker_y      = base_y - piano_roll_ease_out(progress) * rise_distance;
        float marker_size   = (row == PIANO_ROLL_DRUM_KICK ? 9.0f : 5.0f) * scale * (1.0f - 0.35f * progress);
        float alpha         = powf(1.0f - progress, 1.3f);
        Color drum_color    = piano_roll_drum_color(row);

        if (row == PIANO_ROLL_DRUM_KICK) {
            float streak_height = base_y - marker_y + marker_size;
            DrawRectangleGradientV((int)(hit_x - 1.5f * scale), (int)marker_y, (int)fmaxf(1.0f, 3.0f * scale), (int)fmaxf(1.0f, streak_height),
                                   Fade(drum_color, alpha * 0.7f), Fade(drum_color, 0.0f));
        }
        DrawRectanglePro((Rectangle){ hit_x, marker_y, marker_size, marker_size }, (Vector2){ marker_size / 2, marker_size / 2 }, 45,
                         age < 0.08f ? WHITE : Fade(drum_color, alpha));
    }
}

static void piano_roll_draw_neon_edge(const PianoRollView* view, float edge_x, bool is_left)
{
    const float scale    = view->scale;
    const float center_y = (view->area_top + view->area_bottom) / 2;
    const float half_len = view->kick_spread * (view->area_bottom - view->area_top) / 2;
    const float flash    = view->kick_flash;

    BeginBlendMode(BLEND_ADDITIVE);

    const Color base_neon_color = { 190, 220, 255, 255 };

    const float line_width = (1.5f + 3.0f * flash) * scale;
    const int   glow_width = (int)((8 + 26 * flash) * scale);

    const float top_y  = center_y - half_len;
    const float height = half_len * 2.0f;

    Color line_color = base_neon_color;
    line_color.a = (unsigned char)(40 + 215 * flash);

    DrawLineEx(
        (Vector2){ edge_x, top_y }, 
        (Vector2){ edge_x, top_y + height }, 
        line_width, 
        line_color
    );

    Color glow_color = base_neon_color;
    glow_color.a = (unsigned char)(140 * flash);

    Color transparent_color = base_neon_color;
    transparent_color.a = 0;

    if (is_left) {
        DrawRectangleGradientH(
            (int)edge_x, (int)top_y, glow_width, (int)height,
            glow_color, 
            transparent_color
        );
    } else {
        DrawRectangleGradientH(
            (int)edge_x - glow_width, (int)top_y, glow_width, (int)height,
            transparent_color, 
            glow_color
        );
    }

    EndBlendMode();
}

static void piano_roll_draw_playhead_flash(const PianoRollView* view)
{
    const float flash = view->kick_flash;
    if (flash <= 0.002f) return;

    const float scale    = view->scale;
    const float height   = view->area_bottom - view->playhead_top;
    const float glow_w   = (8.0f + 26.0f * flash) * scale;
    const Color neon     = { 190, 220, 255, 255 };
    const Color clear    = { 190, 220, 255, 0 };
    DrawRectangleGradientH((int)view->playhead_x, (int)view->playhead_top, (int)glow_w, (int)height, Fade(neon, 0.55f * flash), clear);
    DrawRectangleGradientH((int)(view->playhead_x - glow_w), (int)view->playhead_top, (int)glow_w, (int)height, clear, Fade(neon, 0.55f * flash));
    DrawLineEx((Vector2){ view->playhead_x, view->playhead_top }, (Vector2){ view->playhead_x, view->area_bottom }, (3.0f + 6.0f * flash) * scale, Fade(neon, flash));
}

void piano_roll_draw_fx(const PianoRoll* piano_roll, Rectangle area, double now, unsigned fx)
{
    PianoRollView view = {0};
    const float   scale          = GetScreenHeight() / 1080.0f;
    const bool    has_drum_lanes = (fx & PIANO_ROLL_FX_DRUMS) != 0;

    view.piano_roll       = piano_roll;
    view.fx               = fx;
    view.now              = now;
    view.scale            = scale;
    view.left_x           = area.x;
    view.right_x          = area.x + area.width;
    view.area_top         = area.y;
    view.area_bottom      = area.y + area.height;
    view.ruler_y          = area.y - 8 * scale;
    view.playhead_top     = view.ruler_y - 8 * scale;
    view.drum_lane_height = (has_drum_lanes ? 64.0f : 26.0f) * scale;
    view.drum_row_height  = has_drum_lanes ? view.drum_lane_height / PIANO_ROLL_DRUM_ROWS : view.drum_lane_height;
    view.pitch_bottom     = view.area_bottom - view.drum_lane_height;

    int bars_per_page = piano_roll->bars_per_page;
    if (bars_per_page < 1) bars_per_page = 1;
    if (bars_per_page > PIANO_ROLL_MAX_BARS) bars_per_page = PIANO_ROLL_MAX_BARS;

    view.playhead_tick = midi_seconds_to_ticks(piano_roll->midi, piano_roll->tempo, now);
    MidiPosition position;
    midi_ticks_to_position(piano_roll->midi, piano_roll->sig, &position, view.playhead_tick);

    MidiBar page_bars[PIANO_ROLL_MAX_BARS];
    midi_bar_at(piano_roll->midi, piano_roll->sig, &page_bars[0], view.playhead_tick);
    const MidiBar  current_bar = page_bars[0];
    const uint32_t bar_in_page = (position.bar - 1) % (uint32_t)bars_per_page;
    for (uint32_t step = bar_in_page; step > 0 && page_bars[0].start_tick > 0; step--)
        midi_bar_at(piano_roll->midi, piano_roll->sig, &page_bars[0], page_bars[0].start_tick - 1);
    for (int bar_index = 1; bar_index < bars_per_page; bar_index++)
        midi_bar_at(piano_roll->midi, piano_roll->sig, &page_bars[bar_index],
                    page_bars[bar_index - 1].start_tick + page_bars[bar_index - 1].length_ticks);

    PianoRollPage page = {0};
    page.start_tick = page_bars[0].start_tick;
    page.end_tick   = (double)page_bars[bars_per_page - 1].start_tick + page_bars[bars_per_page - 1].length_ticks;
    page.start_sec  = midi_tick_to_seconds(piano_roll->midi, piano_roll->tempo, page_bars[0].start_tick);
    page.end_sec    = midi_tick_to_seconds(piano_roll->midi, piano_roll->tempo, (uint32_t)page.end_tick);
    view.playhead_x = piano_roll_page_x(&view, &page, view.playhead_tick);

    PianoRollPage trail_page = {0};
    bool          has_trail  = false;
    if ((fx & PIANO_ROLL_FX_TRAIL) && page.start_tick > 0 && now - page.start_sec < fmax(PIANO_ROLL_TAIL_LIFE, PIANO_ROLL_KICK_LIFE)) {
        MidiBar previous_bar = page_bars[0];
        for (int step = 0; step < bars_per_page && previous_bar.start_tick > 0; step++)
            midi_bar_at(piano_roll->midi, piano_roll->sig, &previous_bar, previous_bar.start_tick - 1);
        trail_page.start_tick = previous_bar.start_tick;
        trail_page.end_tick   = page.start_tick;
        trail_page.start_sec  = midi_tick_to_seconds(piano_roll->midi, piano_roll->tempo, previous_bar.start_tick);
        trail_page.end_sec    = page.start_sec;
        trail_page.is_trail   = true;
        has_trail             = trail_page.end_tick > trail_page.start_tick;
    }

    if (fx & PIANO_ROLL_FX_KICK) {
        double kick_age = 0.0;
        float  kick_velocity = 0.0f;
        if (piano_roll_last_kick(piano_roll->notes, now, &kick_age, &kick_velocity)) {
            view.kick_flash  = expf(-(float)kick_age / 0.14f) * (0.4f + 0.6f * kick_velocity / 127.0f);
            view.kick_spread = 0.08f + 0.92f * piano_roll_ease_out(fminf(1.0f, (float)kick_age / 0.07f));
        }
    }

    DrawRectangleRec(area, ColorAlpha(BLACK, 0.25));

    const Color faint_grid_color = { 255, 255, 255, 20 };
    DrawLineEx((Vector2){ view.left_x, view.ruler_y }, (Vector2){ view.right_x, view.ruler_y }, 1.0f, (Color){ 255, 255, 255, 45 });
    for (int bar_index = 0; bar_index < bars_per_page; bar_index++) {
        const MidiBar* bar          = &page_bars[bar_index];
        const uint32_t bar_end_tick = bar->start_tick + bar->length_ticks;
        for (uint32_t beat_tick = bar->start_tick; beat_tick < bar_end_tick; beat_tick += bar->beat_ticks) {
            float beat_x       = piano_roll_page_x(&view, &page, beat_tick);
            bool  is_bar_start = (beat_tick == bar->start_tick);
            DrawLineEx((Vector2){ beat_x, view.ruler_y - (is_bar_start ? 8 : 4) * scale }, (Vector2){ beat_x, view.ruler_y }, 1.0f,
                       (Color){ 255, 255, 255, is_bar_start ? 120 : 70 });
            if (is_bar_start && bar_index > 0) DrawLineEx((Vector2){ beat_x, view.area_top }, (Vector2){ beat_x, view.area_bottom }, 1.0f, (Color){ 255, 255, 255, 48 });
            else if (!is_bar_start)            DrawLineEx((Vector2){ beat_x, view.area_top }, (Vector2){ beat_x, view.area_bottom }, 1.0f, faint_grid_color);
            for (int quarter = 1; quarter < 4; quarter++) {
                double sub_tick = (double)beat_tick + (double)bar->beat_ticks * quarter / 4.0;
                if (sub_tick < (double)bar_end_tick) {
                    float sub_x = piano_roll_page_x(&view, &page, sub_tick);
                    DrawLineEx((Vector2){ sub_x, view.ruler_y - 2 * scale }, (Vector2){ sub_x, view.ruler_y }, 1.0f, (Color){ 255, 255, 255, 35 });
                }
            }
        }
        text_draw(TextFormat("%u", position.bar - bar_in_page + (uint32_t)bar_index),
                 (int)(piano_roll_page_x(&view, &page, bar->start_tick) + 6 * scale),
                 (int)(area.y + 4 * scale), (int)fmaxf(10.0f, 14 * scale), (Color){ 128, 148, 165, 200 });
    }
    if (has_drum_lanes) {
        DrawLineEx((Vector2){ view.left_x, view.pitch_bottom }, (Vector2){ view.right_x, view.pitch_bottom }, 1.0f, (Color){ 255, 255, 255, 30 });
        for (int row = 1; row < PIANO_ROLL_DRUM_ROWS; row++) {
            float row_y = view.area_bottom - row * view.drum_row_height;
            DrawLineEx((Vector2){ view.left_x, row_y }, (Vector2){ view.right_x, row_y }, 1.0f, (Color){ 255, 255, 255, 14 });
        }
    }
    DrawRectangleGradientH((int)view.left_x, (int)view.ruler_y, (int)(view.playhead_x - view.left_x), 2, BLANK, WHITE);

    const int   key_count = piano_roll->high_key - piano_roll->low_key + 1;
    view.key_slot         = (view.pitch_bottom - view.area_top) / (float)key_count;
    view.note_thickness   = view.key_slot * 0.6f;
    if (view.note_thickness > 9 * scale) view.note_thickness = 9 * scale;
    if (view.note_thickness < 2 * scale) view.note_thickness = 2 * scale;

    const MidiTempo* current_tempo = midi_tempo_at(piano_roll->tempo, view.playhead_tick);
    const double     us_per_quarter = current_tempo->us_per_quarter ? current_tempo->us_per_quarter : 500000.0;
    const double     ticks_per_sec  = piano_roll->midi->division * 1e6 / us_per_quarter;
    view.head_ticks          = current_bar.beat_ticks / 2.0;
    view.ticks_per_pixel     = (page.end_tick - page.start_tick) / (double)(view.right_x - view.left_x);
    view.tail_life_ticks     = PIANO_ROLL_TAIL_LIFE * ticks_per_sec;
    view.tail_delay_ticks    = fmin(PIANO_ROLL_TAIL_DELAY * ticks_per_sec, view.tail_life_ticks * 0.9);
    view.tail_decay_ticks    = view.tail_life_ticks - view.tail_delay_ticks;
    view.oldest_visible_tick = view.playhead_tick - view.tail_life_ticks;
    view.tail_spread         = PIANO_ROLL_TAIL_SPREAD;
    view.cell_width_ticks    = 6 * scale * view.ticks_per_pixel;
    view.cell_pitch_ticks    = 8 * scale * view.ticks_per_pixel;
    view.head_width          = 7 * scale;

    if (has_trail && (fx & PIANO_ROLL_FX_TAIL)) piano_roll_draw_page_notes(&view, &trail_page);
    if (has_trail && has_drum_lanes)            piano_roll_draw_page_drums(&view, &trail_page);
    piano_roll_draw_page_notes(&view, &page);
    piano_roll_draw_page_drums(&view, &page);

    if (view.glow_count > 0 || (fx & PIANO_ROLL_FX_KICK)) {
        BeginBlendMode(BLEND_ADDITIVE);
        for (int glow_index = 0; glow_index < view.glow_count; glow_index++) {
            const PianoRollGlow* glow = &view.glows[glow_index];
            for (int layer = 0; layer < 3; layer++) {
                float grow  = (3 + layer * 5) * scale;
                float alpha = fminf(0.5f, 0.22f * glow->intensity / (1.0f + layer));
                DrawRectangleRec((Rectangle){ glow->x - view.head_width - grow, glow->y - glow->height * 0.65f - grow,
                                              view.head_width + grow * 2, glow->height * 1.3f + grow * 2 }, Fade(glow->color, alpha));
            }
            float streak = (24 + 24 * glow->intensity) * scale;
            DrawRectangleGradientH((int)(glow->x - streak), (int)(glow->y - glow->height * 0.3f), (int)streak, (int)fmaxf(1.0f, glow->height * 0.6f),
                                   (Color){ glow->color.r, glow->color.g, glow->color.b, 0 }, Fade(glow->color, fminf(0.6f, 0.3f * glow->intensity)));
        }
        if (fx & PIANO_ROLL_FX_KICK) {
            piano_roll_draw_neon_edge(&view, view.left_x, true);
            piano_roll_draw_neon_edge(&view, view.right_x, false);
            piano_roll_draw_playhead_flash(&view);
        }
        EndBlendMode();
    }

    DrawLineEx((Vector2){ view.playhead_x, view.playhead_top }, (Vector2){ view.playhead_x, view.area_bottom }, 7 * scale, (Color){ 255, 255, 255, 28 });
    DrawLineEx((Vector2){ view.playhead_x, view.playhead_top }, (Vector2){ view.playhead_x, view.area_bottom },
               fmaxf(1.5f, 1.8f * scale) * (1.0f + view.kick_flash), WHITE);
}

void piano_roll_draw(const PianoRoll* piano_roll, Rectangle area, double now)
{
    piano_roll_draw_fx(piano_roll, area, now, PIANO_ROLL_FX_DEFAULT);
}

#include "raylib.h"
#include "ui.h"
#include "text.h"
#include <math.h>

static float piano_roll_x(double tick, double from, double to, float left, float right)
{
    return left + (float)((tick - from) / (to - from)) * (right - left);
}

static size_t piano_roll_first_note(const MidiNoteList* notes, double t)
{
    size_t lo = 0, hi = notes->count;
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        if (notes->items[mid].start < t) lo = mid + 1;
        else                             hi = mid;
    }
    return lo;
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

#define PIANO_ROLL_MAX_GLOW 256

typedef struct PianoRollGlow {
    float x, y, h;
    Color c;
    float intensity;
} PianoRollGlow;

static Color piano_roll_glow_color(int channel)
{
    if (channel == 9) return WHITE;
    return ColorFromHSV(fmodf(channel * 137.5f, 360.0f), 0.7f, 1.0f);
}

static Color piano_roll_lerp(Color a, Color b, float t)
{
    return (Color){
        (unsigned char)(a.r + (b.r - a.r) * t),
        (unsigned char)(a.g + (b.g - a.g) * t),
        (unsigned char)(a.b + (b.b - a.b) * t),
        255
    };
}

static float piano_roll_hash(uint32_t a, uint32_t b)
{
    uint32_t h = (a * 0x9E3779B1u) ^ ((b + 0x7F4A7C15u) * 0x85EBCA6Bu);
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    h ^= h >> 12;
    return (float)(h & 0xFFFF) / 32767.5f - 1.0f;
}

static bool piano_roll_last_kick(const MidiNoteList* notes, double now, double* age, float* velocity)
{
    for (size_t i = piano_roll_first_note(notes, now + 1e-9); i > 0; i--) {
        const MidiNote* n = &notes->items[i - 1];
        if (now - n->start > 0.5) break;
        if (n->channel == 9 && (n->key == 35 || n->key == 36)) {
            *age      = now - n->start;
            *velocity = n->velocity;
            return true;
        }
    }
    return false;
}

void piano_roll_init(PianoRoll* r, const Midi* midi, const MidiTempoMap* tempo, const MidiTimeSigMap* sig, const MidiNoteList* notes)
{
    r->midi          = midi;
    r->tempo         = tempo;
    r->sig           = sig;
    r->notes         = notes;
    r->bars_per_page = 2;
    r->max_len       = 0.0;
    r->clock         = 0.0;

    int lo = 127, hi = 0;
    da_foreach(MidiNote, n, notes) {
        if (n->end - n->start > r->max_len) r->max_len = n->end - n->start;
        if (n->channel == 9) continue;
        if (n->key < lo) lo = n->key;
        if (n->key > hi) hi = n->key;
    }
    if (lo > hi) { lo = 48; hi = 84; }
    lo -= 1;
    hi += 1;
    while (hi - lo < 24) { lo--; hi++; }
    r->low_key  = lo < 0 ? 0 : lo;
    r->high_key = hi > 127 ? 127 : hi;
}

double piano_roll_time(PianoRoll* r, double audio, double dt, bool playing)
{
    if (!playing) {
        r->clock = audio;
        return r->clock;
    }
    r->clock += dt;
    double err = audio - r->clock;
    if (fabs(err) > 0.05) r->clock = audio;
    else                  r->clock += err * 0.1;
    return r->clock;
}

void piano_roll_draw_fx(const PianoRoll* r, Rectangle roll, double now, unsigned fx)
{
    const float sc          = GetScreenHeight() / 1080.0f;
    const float left        = roll.x, right = roll.x + roll.width;
    const float lane_h      = 26 * sc;
    const float ruler_y     = roll.y - 8 * sc;
    const float notes_top   = roll.y;
    const float notes_bot   = roll.y + roll.height - lane_h;
    const float drum_y      = roll.y + roll.height - lane_h / 2;
    const float roll_bottom = roll.y + roll.height;

    int bars = r->bars_per_page;
    if (bars < 1) bars = 1;
    if (bars > PIANO_ROLL_MAX_BARS) bars = PIANO_ROLL_MAX_BARS;

    double       ticks = midi_seconds_to_ticks(r->midi, r->tempo, now);
    MidiPosition pos;
    midi_ticks_to_position(r->midi, r->sig, &pos, ticks);

    MidiBar page[PIANO_ROLL_MAX_BARS];
    midi_bar_at(r->midi, r->sig, &page[0], ticks);
    const MidiBar current   = page[0];
    const uint32_t into_page = (pos.bar - 1) % (uint32_t)bars;
    for (uint32_t k = into_page; k > 0 && page[0].start_tick > 0; k--)
        midi_bar_at(r->midi, r->sig, &page[0], page[0].start_tick - 1);
    for (int i = 1; i < bars; i++)
        midi_bar_at(r->midi, r->sig, &page[i], page[i - 1].start_tick + page[i - 1].length_ticks);
    const double span_start = page[0].start_tick;
    const double span_end   = (double)page[bars - 1].start_tick + page[bars - 1].length_ticks;
    const double span_start_sec = midi_tick_to_seconds(r->midi, r->tempo, page[0].start_tick);

    DrawRectangleRec(roll, ColorAlpha(BLACK, 0.25));

    const Color faint = { 255, 255, 255, 20 };
    DrawLineEx((Vector2){ left, ruler_y }, (Vector2){ right, ruler_y }, 1.0f, (Color){ 255, 255, 255, 45 });
    for (int bi = 0; bi < bars; bi++) {
        const MidiBar* pb     = &page[bi];
        const uint32_t pb_end = pb->start_tick + pb->length_ticks;
        for (uint32_t tk = pb->start_tick; tk < pb_end; tk += pb->beat_ticks) {
            float x         = piano_roll_x(tk, span_start, span_end, left, right);
            bool  bar_start = (tk == pb->start_tick);
            DrawLineEx((Vector2){ x, ruler_y - (bar_start ? 8 : 4) * sc }, (Vector2){ x, ruler_y }, 1.0f,
                       (Color){ 255, 255, 255, bar_start ? 120 : 70 });
            if (bar_start && bi > 0) DrawLineEx((Vector2){ x, notes_top }, (Vector2){ x, roll_bottom }, 1.0f, (Color){ 255, 255, 255, 48 });
            else if (!bar_start)     DrawLineEx((Vector2){ x, notes_top }, (Vector2){ x, roll_bottom }, 1.0f, faint);
            for (int q = 1; q < 4; q++) {
                double qt = (double)tk + (double)pb->beat_ticks * q / 4.0;
                if (qt < (double)pb_end) {
                    float qx = piano_roll_x(qt, span_start, span_end, left, right);
                    DrawLineEx((Vector2){ qx, ruler_y - 2 * sc }, (Vector2){ qx, ruler_y }, 1.0f, (Color){ 255, 255, 255, 35 });
                }
            }
        }
        text_draw(TextFormat("%u", pos.bar - into_page + (uint32_t)bi),
                 (int)(piano_roll_x(pb->start_tick, span_start, span_end, left, right) + 6 * sc),
                 (int)(roll.y + 4 * sc), (int)fmaxf(10.0f, 14 * sc), (Color){ 128, 148, 165, 200 });
    }
    const float px = piano_roll_x(ticks, span_start, span_end, left, right);
    DrawRectangleGradientH(left, ruler_y, px - left, 2., BLANK, WHITE);

    const int   n_keys = r->high_key - r->low_key + 1;
    const float slot   = (notes_bot - notes_top) / (float)n_keys;
    float thick = slot * 0.6f;
    if (thick > 9 * sc) thick = 9 * sc;
    if (thick < 2 * sc) thick = 2 * sc;
    const double head_ticks = current.beat_ticks / 2.0;

    const double           tpp          = (span_end - span_start) / (double)(right - left);
    const MidiTempo*       tempo_now    = midi_tempo_at(r->tempo, ticks);
    const double           us_quarter   = tempo_now->us_per_quarter ? tempo_now->us_per_quarter : 500000.0;
    const double           window_ticks = PIANO_ROLL_TAIL_LIFE * r->midi->division * 1e6 / us_quarter;
    const double           delay_ticks  = fmin(PIANO_ROLL_TAIL_DELAY * r->midi->division * 1e6 / us_quarter, window_ticks * 0.9);
    const double           decay_ticks  = window_ticks - delay_ticks;
    const double           oldest       = ticks - window_ticks;
    const float            spread       = PIANO_ROLL_TAIL_SPREAD;
    const double           cell_ticks   = 6 * sc * tpp;
    const double           pitch_ticks  = 8 * sc * tpp;
    const float            head_w       = 7 * sc;
    PianoRollGlow          glow[PIANO_ROLL_MAX_GLOW];
    int                    n_glow       = 0;

    for (size_t i = piano_roll_first_note(r->notes, span_start_sec - r->max_len); i < r->notes->count; i++) {
        const MidiNote* n = &r->notes->items[i];
        if (n->start > now) break;

        double n_start = midi_seconds_to_ticks(r->midi, r->tempo, n->start);
        double n_end   = midi_seconds_to_ticks(r->midi, r->tempo, n->end);
        if (n_start < span_start && n_end <= span_start) continue;

        double vis_start = n_start < span_start ? span_start : n_start;
        double vis_end   = n_end < ticks ? n_end : ticks;
        bool   active    = ticks < n_end;
        Color  c         = piano_roll_color(n->channel);

        if (n->channel == 9) {
            if (n_start < span_start) continue;
            bool  flash = (now - n->start) < 0.08;
            float x = piano_roll_x(vis_start, span_start, span_end, left, right), s = 5 * sc;
            DrawRectanglePro((Rectangle){ x, drum_y, s, s }, (Vector2){ s / 2, s / 2 }, 45, flash ? WHITE : Fade(c, 0.7f));
            continue;
        }
        if (n->key < r->low_key || n->key > r->high_key) continue;

        float x1     = piano_roll_x(vis_start, span_start, span_end, left, right);
        float x2     = piano_roll_x(vis_end,   span_start, span_end, left, right);
        float hx     = x2;
        float yc     = notes_bot - ((float)(n->key - r->low_key) + 0.5f) * slot;
        Color bright = piano_roll_glow_color(n->channel);

        if (fx & PIANO_ROLL_FX_TAIL) {
            const double hold_from = ticks - delay_ticks;
            const double solid_lo  = vis_start > hold_from ? vis_start : hold_from;
            if (vis_end > solid_lo) {
                float age_l = (float)((ticks - solid_lo) / window_ticks);
                float age_r = (float)((ticks - vis_end) / window_ticks);
                int   gx    = (int)roundf(piano_roll_x(solid_lo, span_start, span_end, left, right));
                int   gw    = (int)roundf(x2) - gx;
                if (gw < 1) gw = 1;
                DrawRectangleGradientH(gx, (int)roundf(yc - thick / 2), gw, (int)fmaxf(1.0f, roundf(thick)),
                                       Fade(piano_roll_lerp(bright, c, fminf(1.0f, age_l * 2.0f)), 0.9f),
                                       Fade(piano_roll_lerp(bright, c, fminf(1.0f, age_r * 2.0f)), 0.9f));
            }

            const double lo      = vis_start > oldest ? vis_start : oldest;
            const double cell_hi = vis_end < hold_from ? vis_end : hold_from;
            if (cell_hi > lo && pitch_ticks > 1e-9 && decay_ticks > 1e-9) {
                long k = (long)floor((lo - n_start) / pitch_ticks);
                if (k < 0) k = 0;
                for (int count = 0; count < 512; count++, k++) {
                    double c0 = n_start + k * pitch_ticks;
                    if (c0 >= cell_hi) break;
                    double a = c0 < lo ? lo : c0;
                    double b = c0 + cell_ticks > cell_hi ? cell_hi : c0 + cell_ticks;
                    if (b <= a) continue;
                    float f = (float)((ticks - (a + b) / 2 - delay_ticks) / decay_ticks);
                    if (f >= 1.0f) continue;
                    if (f < 0.0f) f = 0.0f;

                    uint32_t ci = (uint32_t)i, ck = (uint32_t)k * 4u;
                    float h_y = piano_roll_hash(ci, ck + 0u);
                    float h_x = piano_roll_hash(ci, ck + 1u);
                    float h_w = piano_roll_hash(ci, ck + 2u);
                    float h_h = piano_roll_hash(ci, ck + 3u);

                    float vary = fminf(1.0f, f * 3.0f);
                    float cw = (float)((b - a) / tpp) * (1.0f - 0.45f * f) * (1.0f + 0.30f * spread * vary * h_w);
                    float ch = thick * (1.0f - 0.75f * f) * (1.0f + 0.25f * spread * vary * h_h);
                    if (cw < sc) cw = sc;
                    if (ch < sc) ch = sc;
                    float cx = piano_roll_x((a + b) / 2, span_start, span_end, left, right)
                             + (-0.55f + 0.45f * h_x) * 16 * sc * spread * f;
                    float cy = yc + h_y * thick * spread * f;
                    if (cy - ch / 2 < notes_top) cy = notes_top + ch / 2;
                    if (cy + ch / 2 > notes_bot) cy = notes_bot - ch / 2;
                    float u  = (float)((ticks - (a + b) / 2) / window_ticks);
                    Color cc = piano_roll_lerp(bright, c, fminf(1.0f, u * 2.0f));
                    DrawRectangleRec((Rectangle){ cx - cw / 2, cy - ch / 2, cw, ch },
                                     Fade(cc, powf(1.0f - f, 1.4f) * 0.9f * (1.0f - 0.15f * spread * vary * (h_w + 1.0f) / 2.0f)));
                }
            }
        } else {
            if (x2 - x1 < 2 * sc) x2 = x1 + 2 * sc;
            float xh = piano_roll_x(fmin(vis_start + head_ticks, vis_end), span_start, span_end, left, right);
            if (xh < x1 + 2 * sc) xh = fminf(x1 + 2 * sc, x2);
            float y0 = yc - thick / 2;
            if (x2 > xh) DrawRectangleRec((Rectangle){ xh, y0, x2 - xh, thick }, Fade(c, active ? 0.78f : 0.5f));
            DrawRectangleRec((Rectangle){ x1, y0, xh - x1, thick }, Fade(c, 0.96f));
        }

        if ((fx & PIANO_ROLL_FX_BLOOM) && active && n_glow < PIANO_ROLL_MAX_GLOW) {
            float hl    = fminf(fmaxf(x1, hx - head_w), hx - 2 * sc);
            float vel   = 0.35f + 0.65f * (n->velocity / 127.0f);
            float onset = 1.0f + 1.6f * expf(-(float)(now - n->start) / 0.10f);
            DrawRectangleRec((Rectangle){ hl, yc - thick * 0.65f, hx - hl, thick * 1.3f }, bright);
            glow[n_glow++] = (PianoRollGlow){ hx, yc, thick, bright, vel * onset };
        }
    }

    if (n_glow > 0 || (fx & PIANO_ROLL_FX_KICK)) {
        BeginBlendMode(BLEND_ADDITIVE);
        for (int g = 0; g < n_glow; g++) {
            const PianoRollGlow* gl = &glow[g];
            for (int layer = 0; layer < 3; layer++) {
                float grow  = (3 + layer * 5) * sc;
                float alpha = fminf(0.5f, 0.22f * gl->intensity / (1.0f + layer));
                DrawRectangleRec((Rectangle){ gl->x - head_w - grow, gl->y - gl->h * 0.65f - grow,
                                              head_w + grow * 2, gl->h * 1.3f + grow * 2 }, Fade(gl->c, alpha));
            }
            float streak = (24 + 24 * gl->intensity) * sc;
            DrawRectangleGradientH((int)(gl->x - streak), (int)(gl->y - gl->h * 0.3f), (int)streak, (int)fmaxf(1.0f, gl->h * 0.6f),
                                   (Color){ gl->c.r, gl->c.g, gl->c.b, 0 }, Fade(gl->c, fminf(0.6f, 0.3f * gl->intensity)));
        }
        if (fx & PIANO_ROLL_FX_KICK) {
            double age = 0.0;
            float  vel = 0.0f, k = 0.0f;
            if (piano_roll_last_kick(r->notes, now, &age, &vel))
                k = expf(-(float)age / 0.14f) * (0.4f + 0.6f * vel / 127.0f);
            const unsigned char er = 190, eg = 220, eb = 255;
            const float         lw = (1.5f + 3.0f * k) * sc;
            const unsigned char la = (unsigned char)(40 + 215 * k);
            const int           gw = (int)((8 + 26 * k) * sc);
            DrawLineEx((Vector2){ left,  roll.y }, (Vector2){ left,  roll_bottom }, lw, (Color){ er, eg, eb, la });
            DrawLineEx((Vector2){ right, roll.y }, (Vector2){ right, roll_bottom }, lw, (Color){ er, eg, eb, la });
            DrawRectangleGradientH((int)left, (int)roll.y, gw, (int)roll.height,
                                   (Color){ er, eg, eb, (unsigned char)(140 * k) }, (Color){ er, eg, eb, 0 });
            DrawRectangleGradientH((int)right - gw, (int)roll.y, gw, (int)roll.height,
                                   (Color){ er, eg, eb, 0 }, (Color){ er, eg, eb, (unsigned char)(140 * k) });

            DrawRectangleGradientH(px, ruler_y - 8 * sc, gw, (int)roll.height,
                                   (Color){ er, eg, eb, (unsigned char)(140 * k) }, (Color){ er, eg, eb, 0 });
            DrawRectangleGradientH(px - gw, ruler_y - 8 * sc, gw, (int)roll.height,
                                   (Color){ er, eg, eb, 0 }, (Color){ er, eg, eb, (unsigned char)(140 * k) });
        }
        EndBlendMode();
    }

    DrawLineEx((Vector2){ px, ruler_y - 8 * sc }, (Vector2){ px, roll_bottom }, 7 * sc, (Color){ 255, 255, 255, 28 });
    DrawLineEx((Vector2){ px, ruler_y - 8 * sc }, (Vector2){ px, roll_bottom }, fmaxf(1.5f, 1.8f * sc), WHITE);
}

void piano_roll_draw(const PianoRoll* r, Rectangle roll, double now)
{
    piano_roll_draw_fx(r, roll, now, PIANO_ROLL_FX_DEFAULT);
}

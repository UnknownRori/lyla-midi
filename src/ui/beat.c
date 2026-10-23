#include <raylib.h>
#include "rstb_common.h"
#include "ui.h"
#include "utils.h"
#include "anim.h"

#define PADDING   32
#define MAX_BEATS 255

static f32 s_show[MAX_BEATS];
static f32 s_fill[MAX_BEATS];

void ui_beat_info(
    u32 w, 
    u32 h, 
    f32 dt,
    MidiPosition* pos
)
{
    UNUSED(dt);
    Vector2 block = {12, 12};
    for (u8 i = 0; i < MAX_BEATS; i++) {
        s_show[i] = anim_move_toward(s_show[i], i < pos->numerator ? 1.f : 0.f, ANIM_BEAT_SPEED, dt);
        s_fill[i] = anim_move_toward(s_fill[i], pos->beat > i ? 1.f : 0.f, ANIM_BEAT_SPEED, dt);
        if (s_show[i] <= 0.f) continue;

        Rectangle rect   = RECT(w - PADDING - block.x - (16 * i) - (4 * i), 128, block.x, block.y);
        f32       fill_h = rect.height * s_fill[i];
        f32       fill_y = pos->beat > i ? rect.y + rect.height - fill_h : rect.y;
        DrawRectangleRec(
            RECT(rect.x, fill_y, rect.width, fill_h), 
            Fade(WHITE, s_fill[i] * s_show[i])
        );
        DrawRectangleLinesEx(
            rect, 
            2., 
            Fade(WHITE, s_show[i])
        );
    }
}

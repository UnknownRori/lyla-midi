#include <raylib.h>
#include "ui.h"
#include "utils.h"

#define PADDING 32

void ui_beat_info(
    u32 w, 
    u32 h, 
    MidiPosition* pos
)
{
    Vector2 block = {12, 12};
    for (u8 i = 0; i < pos->numerator; i++) {
        Rectangle rect = RECT(w - PADDING - block.x - (16 * i) - (4 * i), 128, block.x, block.y);
        if (pos->beat > i) {
            DrawRectangleRec(
                rect, 
                WHITE
            );
        } else {
            DrawRectangleLinesEx(
                rect, 
                2., 
                WHITE
            );
        }
    }
}

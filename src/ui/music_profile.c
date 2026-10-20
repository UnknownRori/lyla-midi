#include "raylib.h"
#include "rstb_common.h"
#include "utils.h"
#include "ui.h"
#include "text.h"

#define PADDING             16
#define TEXT_SIZE           28
#define TEXT_SIZE_SECONDARY 16
#define ALBUM_ART_SIZE      96

void ui_music_profile(
    u32   w,
    u32   h,
    const Texture* album_art,
    const char*    title,
    const char*    album,
    const char*    author
) {
    UNUSED(w); UNUSED(h);

    Rectangle album_dst = RECT(PADDING * 2, PADDING * 2, ALBUM_ART_SIZE, ALBUM_ART_SIZE);
    DrawTexturePro(
        *album_art,
        RECT_FROM_TEXTURE(0, 0, *album_art), 
        album_dst, 
        VEC2(0, 0),
        0.f, 
        WHITE
    );

    f32     text_offset   = album_dst.x + album_dst.width + PADDING;
    Vector2 title_measure = text_measure(title, TEXT_SIZE);
    f32     title_y       = album_dst.y + album_dst.height / 2. - title_measure.y;
    text_draw(
        title, 
        text_offset, 
        title_y, 
        TEXT_SIZE, 
        WHITE
    );


    if (album != NULL && author != NULL) {
        text_draw(
            TextFormat("%s by %s", album, author), 
            text_offset, 
            title_y + PADDING + TEXT_SIZE, 
            TEXT_SIZE_SECONDARY, 
            WHITE
        );
    } else if (author != NULL) {
        text_draw(
            TextFormat("%s", author), 
            text_offset, 
            title_y + PADDING + TEXT_SIZE, 
            TEXT_SIZE_SECONDARY, 
            WHITE
        );
    } else {
        PANIC("WTF are you doing!!!");
    }
}

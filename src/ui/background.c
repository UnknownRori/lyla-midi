#include <raylib.h>
#include <math.h>

#include "rstb_common.h"
#include "ui.h"
#include "utils.h"
#if defined(PLATFORM_WEB)
#   include "assets/blur100_frag.c"
#else
#   include "assets/blur330_frag.c"
#endif

#define BG_BLACK_BLEND 0.5f

static RenderTexture2D pass1;
static RenderTexture2D cache;
static Shader          blurShader;
static i32             resolutionLoc;
static i32             directionLoc;
static i32             blurRadiusLoc;
static bool            dirty;

static const Texture*  bgptr;

bool ui_background_init()
{
    pass1 = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
    cache = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
#if defined(PLATFORM_WEB)
    blurShader = LoadShaderFromMemory(NULL, (char*) __resources_blur100_frag_glsl);
#else
    blurShader = LoadShaderFromMemory(NULL, (char*) __resources_blur330_frag_glsl);
#endif
    resolutionLoc = GetShaderLocation(blurShader, "resolution");
    directionLoc  = GetShaderLocation(blurShader, "direction");
    blurRadiusLoc = GetShaderLocation(blurShader, "blurRadius");
}

void ui_background_target_pan(u32 x, u32 y)
{
    UNUSED(x); UNUSED(y);
    UNIMPLEMENTED;
    // TODO : Stinky better auto pan:p
}

void ui_background(
    u32   w,
    u32   h,
    const Texture* texture,
    bool  auto_pan
)
{
    UNUSED(auto_pan);
    if (texture != bgptr) {
        bgptr = texture;
        dirty = true;
    }

    Texture bg = *texture;
    f32 scaleX = (f32)w / texture->width;
    f32 scaleY = (f32)h / texture->height;
    f32 scale  = scaleX > scaleY ? scaleX : scaleY;
    i32 iw = (i32)(texture->width  * scale + 0.5f);
    i32 ih = (i32)(texture->height * scale + 0.5f);
    if ((pass1.texture.width != iw || pass1.texture.height != ih)) {
        UnloadRenderTexture(pass1);
        UnloadRenderTexture(cache);
        pass1 = LoadRenderTexture(iw, ih);
        cache = LoadRenderTexture(iw, ih);
        dirty = true;
    }

    i32 cw = cache.texture.width;
    i32 ch = cache.texture.height;
    if (dirty && IsRenderTextureValid(pass1) && IsRenderTextureValid(cache)) {
        dirty = false;

        Rectangle source = RECT_FROM_TEXTURE(0, 0, *texture);
        Rectangle full   = { 0, 0, (f32)cw, (f32)ch };

        BeginTextureMode(pass1);
            ClearBackground(BLACK);
            DrawTexturePro(bg, source, full, (Vector2){0, 0}, 0.0f, WHITE);
        EndTextureMode();

        f32 res[2] = { (f32)cw, (f32)ch };
        f32 radius = 8.0f;
        SetShaderValue(blurShader, resolutionLoc, res, SHADER_UNIFORM_VEC2);
        SetShaderValue(blurShader, blurRadiusLoc, &radius, SHADER_UNIFORM_FLOAT);

        BeginTextureMode(pass1);
            ClearBackground(BLACK);
            BeginShaderMode(blurShader);
                f32 dirX[2] = { 1.0f, 0.0f };
                SetShaderValue(blurShader, directionLoc, dirX, SHADER_UNIFORM_VEC2);
                DrawTexturePro(bg, source, full, (Vector2){0, 0}, 0.0f, WHITE);
            EndShaderMode();
        EndTextureMode();

        BeginTextureMode(cache);
            ClearBackground(BLACK);
            BeginShaderMode(blurShader);
                f32 dirY[2] = { 0.0f, 1.0f };
                SetShaderValue(blurShader, directionLoc, dirY, SHADER_UNIFORM_VEC2);
                DrawTexturePro(
                    pass1.texture,
                    (Rectangle){ 0, 0, (f32)cw, -(f32)ch },
                    full,
                    (Vector2){0, 0},
                    0.0f,
                    WHITE
                );
            EndShaderMode();
        EndTextureMode();
    }

    DrawTexturePro(
        cache.texture,
        RECT(0, fminf(h, bg.height) / 2., fminf(w, bg.width), -fminf(h, bg.height)),
        RECT(0, 0, w, h),
        VEC2_ZERO,
        0.0f,
        WHITE
    );
    DrawRectangleRec(RECT(0, 0, w, h), ColorAlpha(BLACK, BG_BLACK_BLEND));
}

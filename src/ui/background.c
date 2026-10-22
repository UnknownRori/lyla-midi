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

#define BG_BLACK_BLEND  0.7f

#define BG_BLUR_SIZE    512
#define BG_BLUR_RADIUS  8.0f
#define BG_PAN_ZOOM     1.08f
#define BG_PAN_DRIFT    0.8f

typedef struct BgSource {
    u32 id;
    i32 width;
    i32 height;
} BgSource;

static RenderTexture2D pass1;
static RenderTexture2D cache;
static Shader          blurShader;
static i32             resolutionLoc;
static i32             directionLoc;
static i32             blurRadiusLoc;
static bool            initialized;
static bool            dirty;
static bool            blurred;
static BgSource        source;

static f32             panX, panY, panStrength;
static f32             panTargetX, panTargetY;
static bool            panHasTarget;
static f32             viewW, viewH;

static f32 bg_clamp(f32 v, f32 lo, f32 hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

bool ui_background_init(void)
{
    if (initialized) return IsShaderValid(blurShader);
    initialized = true;
#if defined(PLATFORM_WEB)
    blurShader = LoadShaderFromMemory(NULL, (char*) __resources_blur100_frag_glsl);
#else
    blurShader = LoadShaderFromMemory(NULL, (char*) __resources_blur330_frag_glsl);
#endif
    resolutionLoc = GetShaderLocation(blurShader, "resolution");
    directionLoc  = GetShaderLocation(blurShader, "direction");
    blurRadiusLoc = GetShaderLocation(blurShader, "blurRadius");
    return IsShaderValid(blurShader);
}

void ui_background_shutdown(void)
{
    if (IsRenderTextureValid(pass1)) UnloadRenderTexture(pass1);
    if (IsRenderTextureValid(cache)) UnloadRenderTexture(cache);
    if (IsShaderValid(blurShader))   UnloadShader(blurShader);
    pass1         = (RenderTexture2D){0};
    cache         = (RenderTexture2D){0};
    blurShader    = (Shader){0};
    source        = (BgSource){0};
    initialized   = false;
    dirty         = false;
    blurred       = false;
    panX = panY = panStrength = 0.0f;
    panHasTarget  = false;
}

void ui_background_target_pan(u32 x, u32 y)
{
    f32 w = viewW > 1.0f ? viewW : (f32)GetScreenWidth();
    f32 h = viewH > 1.0f ? viewH : (f32)GetScreenHeight();
    if (w < 1.0f) w = 1.0f;
    if (h < 1.0f) h = 1.0f;
    panTargetX   = bg_clamp((f32)x / w * 2.0f - 1.0f, -1.0f, 1.0f);
    panTargetY   = bg_clamp((f32)y / h * 2.0f - 1.0f, -1.0f, 1.0f);
    panHasTarget = true;
}

static void bg_update_pan(bool auto_pan)
{
    f32 dt      = GetFrameTime();
    f32 targetX = 0.0f, targetY = 0.0f, strength = 0.0f;
    if (panHasTarget) {
        targetX  = panTargetX;
        targetY  = panTargetY;
        strength = 1.0f;
    } else if (auto_pan) {
        double t = GetTime();
        targetX  = (f32)sin(t * 0.11)       * BG_PAN_DRIFT;
        targetY  = (f32)sin(t * 0.07 + 1.7) * BG_PAN_DRIFT;
        strength = 1.0f;
    }
    f32 kPan  = 1.0f - expf(-dt * 1.5f);
    f32 kZoom = 1.0f - expf(-dt * 2.0f);
    panX        += (targetX  - panX)        * kPan;
    panY        += (targetY  - panY)        * kPan;
    panStrength += (strength - panStrength) * kZoom;
    panHasTarget = false;
}

static Rectangle bg_view(i32 iw, i32 ih, f32 w, f32 h, bool flipped)
{
    f32 cover = fmaxf(w / (f32)iw, h / (f32)ih);
    f32 zoom  = 1.0f + (BG_PAN_ZOOM - 1.0f) * panStrength;
    f32 vw    = w / (cover * zoom);
    f32 vh    = h / (cover * zoom);
    f32 x0    = fmaxf(0.0f, (f32)iw - vw) * 0.5f * (1.0f + panX);
    f32 y0    = fmaxf(0.0f, (f32)ih - vh) * 0.5f * (1.0f + panY);
    return flipped ? (Rectangle){ x0, (f32)ih - y0 - vh, vw, -vh }
                   : (Rectangle){ x0, y0, vw, vh };
}

static bool bg_rebuild(const Texture* texture)
{
    f32 k = (f32)BG_BLUR_SIZE / (f32)(texture->width > texture->height ? texture->width : texture->height);
    if (k > 1.0f) k = 1.0f;
    i32 cw = (i32)((f32)texture->width  * k + 0.5f);
    i32 ch = (i32)((f32)texture->height * k + 0.5f);
    if (cw < 1) cw = 1;
    if (ch < 1) ch = 1;

    if (cache.texture.width != cw || cache.texture.height != ch) {
        if (IsRenderTextureValid(pass1)) UnloadRenderTexture(pass1);
        if (IsRenderTextureValid(cache)) UnloadRenderTexture(cache);
        pass1 = LoadRenderTexture(cw, ch);
        cache = LoadRenderTexture(cw, ch);
        SetTextureFilter(pass1.texture, TEXTURE_FILTER_BILINEAR);
        SetTextureFilter(cache.texture, TEXTURE_FILTER_BILINEAR);
        SetTextureWrap(pass1.texture, TEXTURE_WRAP_CLAMP);
        SetTextureWrap(cache.texture, TEXTURE_WRAP_CLAMP);
    }
    if (!IsRenderTextureValid(pass1) || !IsRenderTextureValid(cache)) return false;

    Rectangle src  = RECT_FROM_TEXTURE(0, 0, *texture);
    Rectangle full = RECT(0, 0, cw, ch);
    Rectangle flip = RECT(0, 0, cw, -ch);
    f32 res[2]     = { (f32)cw, (f32)ch };
    f32 radius     = BG_BLUR_RADIUS * (f32)cw / 1920.0f;
    f32 dirX[2]    = { 1.0f, 0.0f };
    f32 dirY[2]    = { 0.0f, 1.0f };

    BeginTextureMode(cache);
        ClearBackground(BLACK);
        DrawTexturePro(*texture, src, full, VEC2_ZERO, 0.0f, WHITE);
    EndTextureMode();

    BeginTextureMode(pass1);
        ClearBackground(BLACK);
        BeginShaderMode(blurShader);
            SetShaderValue(blurShader, resolutionLoc, res,     SHADER_UNIFORM_VEC2);
            SetShaderValue(blurShader, blurRadiusLoc, &radius, SHADER_UNIFORM_FLOAT);
            SetShaderValue(blurShader, directionLoc,  dirX,    SHADER_UNIFORM_VEC2);
            DrawTexturePro(cache.texture, flip, full, VEC2_ZERO, 0.0f, WHITE);
        EndShaderMode();
    EndTextureMode();

    BeginTextureMode(cache);
        ClearBackground(BLACK);
        BeginShaderMode(blurShader);
            SetShaderValue(blurShader, resolutionLoc, res,     SHADER_UNIFORM_VEC2);
            SetShaderValue(blurShader, blurRadiusLoc, &radius, SHADER_UNIFORM_FLOAT);
            SetShaderValue(blurShader, directionLoc,  dirY,    SHADER_UNIFORM_VEC2);
            DrawTexturePro(pass1.texture, flip, full, VEC2_ZERO, 0.0f, WHITE);
        EndShaderMode();
    EndTextureMode();
    return true;
}

void ui_background(
    u32   w,
    u32   h,
    const Texture* texture,
    bool  auto_pan
)
{
    if (!initialized) ui_background_init();
    if (w == 0 || h == 0) return;
    viewW = (f32)w;
    viewH = (f32)h;

    if (!texture || texture->id == 0 || texture->width <= 0 || texture->height <= 0) {
        DrawRectangleRec(RECT(0, 0, w, h), BLACK);
        return;
    }

    if (texture->id != source.id || texture->width != source.width || texture->height != source.height) {
        source = (BgSource){ texture->id, texture->width, texture->height };
        dirty  = true;
    }
    if (dirty) {
        dirty   = false;
        blurred = IsShaderValid(blurShader) && bg_rebuild(texture);
    }

    bg_update_pan(auto_pan);

    if (blurred) {
        DrawTexturePro(
            cache.texture,
            bg_view(cache.texture.width, cache.texture.height, (f32)w, (f32)h, true),
            RECT(0, 0, w, h),
            VEC2_ZERO,
            0.0f,
            WHITE
        );
    } else {
        DrawTexturePro(
            *texture,
            bg_view(texture->width, texture->height, (f32)w, (f32)h, false),
            RECT(0, 0, w, h),
            VEC2_ZERO,
            0.0f,
            WHITE
        );
    }
    DrawRectangleRec(RECT(0, 0, w, h), ColorAlpha(BLACK, BG_BLACK_BLEND));
}

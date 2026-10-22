#pragma once

#include <stdio.h>
#include <string.h>
#include "types.h"

#ifndef ANIM_FADE_SPEED
#define ANIM_FADE_SPEED 16.f
#endif

#ifndef ANIM_SLIDE_DISTANCE
#define ANIM_SLIDE_DISTANCE 14.f
#endif

#ifndef ANIM_BEAT_SPEED
#define ANIM_BEAT_SPEED 12.f
#endif

#ifndef ANIM_CHORD_DELAY
#define ANIM_CHORD_DELAY 0.15f
#endif

#define ANIM_NO_DELAY 0.f
#define ANIM_TEXT_MAX 64

typedef struct AnimSwap {
    char shown[ANIM_TEXT_MAX];
    f32  alpha;
    f32  offset;
    f32  wait_left;
    bool waiting;
    bool leaving;
} AnimSwap;

typedef struct AnimFrame {
    f32  alpha;
    f32  offset;
    bool replaced;
} AnimFrame;

static inline f32 anim_move_toward(f32 value, f32 target, f32 speed, f32 dt)
{
    f32 step = speed * dt;
    if (value < target) return value + step > target ? target : value + step;
    return value - step < target ? target : value - step;
}

static inline AnimFrame anim_swap_update(AnimSwap* swap, const char* target, f32 delay, f32 dt)
{
    bool changed       = strcmp(target, swap->shown) != 0;
    bool nothing_shown = swap->alpha <= 0.f;

    if (swap->leaving) {
        if (!changed) swap->leaving = false;
    } else if (!changed) {
        swap->waiting = false;
    } else {
        if (!swap->waiting) {
            swap->waiting   = true;
            swap->wait_left = nothing_shown ? 0.f : delay;
        } else {
            swap->wait_left -= dt;
        }
        if (swap->wait_left <= 0.f) {
            swap->waiting = false;
            swap->leaving = true;
        }
    }

    f32 target_alpha  = swap->leaving ? 0.f : 1.f;
    f32 target_offset = swap->leaving ? -ANIM_SLIDE_DISTANCE : 0.f;
    swap->alpha  = anim_move_toward(swap->alpha,  target_alpha,  ANIM_FADE_SPEED, dt);
    swap->offset = anim_move_toward(swap->offset, target_offset, ANIM_SLIDE_DISTANCE * ANIM_FADE_SPEED, dt);

    AnimFrame frame = { 0.f, 0.f, false };
    if (swap->leaving && swap->alpha <= 0.f) {
        snprintf(swap->shown, sizeof(swap->shown), "%s", target);
        swap->offset  = ANIM_SLIDE_DISTANCE;
        swap->leaving = false;
        frame.replaced = true;
    }
    frame.alpha  = swap->alpha;
    frame.offset = swap->offset;
    return frame;
}

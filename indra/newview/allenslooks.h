/**
 * @file allenslooks.h
 * @brief Sanitised per-frame parameters and action presets for Lens Looks.
 */
#pragma once

#ifndef AL_LENSLOOKS_H
#define AL_LENSLOOKS_H

#include "v2math.h"
#include "v3color.h"

#include <string>
#include <vector>

namespace ALLensLooks
{
    struct Frame
    {
        F32 mBloomScale = 1.f;
        LLVector2 mCenter = LLVector2(0.5f, 0.5f);
        bool mInfraredActive = false;
        bool mSoftEdgeActive = false;
        bool mTagUsable = false;
    };

    struct InfraredParams
    {
        bool enabled = false;
        S32 mode = 0;
        F32 strength = 1.f;
        F32 exposure_gain = 1.f;
        F32 foliage_gain = 1.f;
        F32 sensitivity = 0.5f;
        F32 dry = 0.4f;
        F32 sky_darken = 2.f;
        F32 water_darken = 2.f;
        F32 skin_gain = 1.41421356f;
        F32 shadow_depth = 0.4f;
        F32 red_leak = 0.1f;
        F32 halation = 0.25f;
        F32 aero_b = 0.03f;
        F32 aero_g = 0.f;
        F32 aero_saturation = 1.15f;
        S32 debug_view = 0;
    };

    struct SoftEdgeParams
    {
        bool enabled = false;
        S32 blur_type = 0;
        S32 shape = 1;
        S32 anchor = 0;
        LLVector2 center = LLVector2(0.5f, 0.5f);
        F32 size = 0.35f;
        F32 aspect = 1.4f;
        F32 angle_rad = 0.f;
        F32 feather = 0.4f;
        F32 blur = 0.5f;
        F32 glow = 0.2f;
        F32 glow_threshold = 0.8f;
        F32 desaturate = 0.f;
        LLColor3 tint = LLColor3(1.f, 0.92f, 0.8f);
        F32 tint_amount = 0.f;
        bool show_mask = false;
    };

    void resolveFrame();
    const Frame& frame();
    InfraredParams infrared();
    SoftEdgeParams softEdge();
    LLColor3 gradTintLinear();
    LLColor3 frameFilterGain();
    void applyInfraredPreset(S32 index);
    void applySoftEdgePreset(S32 index);
    const std::vector<std::string>& settings();
}

#endif

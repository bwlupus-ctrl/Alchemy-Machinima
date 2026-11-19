/**
 * @file alavatarlightresponsemodel.h
 * @brief [AvatarLightResponse] Pure deterministic model for the per-avatar,
 *        light-independent response trim (Diffuse / Tame reflections /
 *        Brightness / Glow).
 *
 * No GL, no viewer globals: LLSD + math only, so every rule the shaders
 * mirror can be unit tested.  See doc/AVATAR_LIGHT_RESPONSE_DESIGN.md.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#ifndef AL_AVATAR_LIGHT_RESPONSE_MODEL_H
#define AL_AVATAR_LIGHT_RESPONSE_MODEL_H

#include "stdtypes.h"

#include "llsd.h"
#include "v3math.h"
#include "v4math.h"

namespace ALLightResponse
{
constexpr F32 DIFFUSE_MIN = 0.f;
constexpr F32 DIFFUSE_MAX = 1.f;
constexpr F32 TAME_MIN = 0.f;
constexpr F32 TAME_MAX = 1.f;
constexpr F32 EV_MIN = -2.f;
constexpr F32 EV_MAX = 1.f;
constexpr F32 GLOW_MIN = 0.f;
constexpr F32 GLOW_MAX = 2.f;
constexpr F32 GLOW_DEFAULT = 1.f;
constexpr F32 IDENTITY_EPSILON = 1.0e-4f;

struct Params
{
    F32  mDiffuse    = 0.f;              // d, 0..1  (UI 0-100 %)
    F32  mTame       = 0.f;              // t, 0..1  (UI 0-100 %)
    F32  mExposureEV = 0.f;              // e, -2..+1 EV
    F32  mGlow       = GLOW_DEFAULT;     // g, 0..2  (UI 0-200 %)
    bool mBypass     = false;            // explicit identity (A/B)
};

// Clamp every field; non-finite values fall back to the field default.
Params    sanitize(const Params& p);
// bypass || every (sanitized) field within 1e-4 of its default.
bool      isIdentity(const Params& p);
// (d, 1 - t, exp2(e), g); identity vector (0,1,1,1) when bypassed.
LLVector4 pack(const Params& p);
// CPU mirror of GLSL alr_gain: effective albedo gain (ceiling when k > 1).
F32       albedoGain(F32 k, F32 max_rgb);
// CPU mirror of the legacy-gloss rule (GLSL alrLegacySpec).
F32       legacyGloss(F32 gloss, F32 d);
// CPU mirror of GLSL alrGraphicCover: c + (k-1)*(c - (1-sigma)*lit_pre).
LLVector3 graphicCover(const LLVector3& c, const LLVector3& lit_pre, F32 sigma, F32 k);

enum EPreset
{
    PRESET_RESET = 0,
    PRESET_POWDERED,
    PRESET_MATTE_SKIN,
    PRESET_TAME_GLOSS,
    PRESET_KILL_SHEEN,
    PRESET_MINUS_HALF_STOP,
    PRESET_MINUS_ONE_STOP,
    PRESET_GLOW_OFF,
    PRESET_COUNT
};

Params    applyPreset(const Params& cur, EPreset preset);

LLSD      toLLSD(const Params& p);     // "diffuse","tame","ev","glow","bypass"
Params    fromLLSD(const LLSD& s);     // missing keys => defaults; result sanitized
}

#endif // AL_AVATAR_LIGHT_RESPONSE_MODEL_H

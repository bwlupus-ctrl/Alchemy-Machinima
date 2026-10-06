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
constexpr F32 SILHOUETTE_EPSILON = 1.0e-4f;

struct Params
{
    F32  mDiffuse    = 0.f;              // d, 0..1  (UI 0-100 %)
    F32  mTame       = 0.f;              // t, 0..1  (UI 0-100 %)
    F32  mExposureEV = 0.f;              // e, -2..+1 EV
    F32  mGlow       = GLOW_DEFAULT;     // g, 0..2  (UI 0-200 %)
    F32  mSilAmount  = 0.f;              // a, 0..1  (UI 0-100 %)
    LLVector3 mSilFillSrgb = LLVector3::zero; // display-sRGB fill colour
    bool mSilHaze    = true;             // atmosphere lies over the fill
    bool mSilKeepRim = false;            // keep evaluated rig/projector rim
    bool mBypass     = false;            // explicit identity (A/B)
};

struct Packed
{
    LLVector4 mParams = LLVector4(0.f, 1.f, 1.f, 1.f);
    LLVector4 mSil = LLVector4(0.f, 0.f, 0.f, 0.f);
    LLVector4 mSilAux = LLVector4(0.f, 0.f, 0.f, 0.f);
};

enum EUploadSlot
{
    UPLOAD_BEAUTY = 0,
    UPLOAD_GLOW,
    UPLOAD_SIL_ONLY
};

struct UploadPlan
{
    S32       enabled = 0;
    LLVector4 params = LLVector4(0.f, 1.f, 1.f, 1.f);
    LLVector4 sil = LLVector4(0.f, 0.f, 0.f, 0.f);
    LLVector4 aux = LLVector4(0.f, 0.f, 0.f, 0.f);
};

// Clamp every field; non-finite values fall back to the field default.
Params    sanitize(const Params& p);
// bypass || every (sanitized) field within 1e-4 of its default.
bool      isIdentity(const Params& p);
// (d, 1 - t, exp2(e), g); identity vector (0,1,1,1) when bypassed.
LLVector4 pack(const Params& p);
// All per-draw values. hdr_encoding selects the exact-integer fp16 carrier.
Packed    packAll(const Params& p, bool hdr_encoding);
S32       silQuant(F32 a, bool hdr);
F32       silAmountQ(S32 q, bool hdr);
F32       silCarrier(S32 q, bool haze_over, bool keep_rim, bool hdr);
bool      silDecode(F32 carrier, bool hdr, F32& a_q, bool& haze_skip, bool& rim_cut);
F32       srgbToLinear(F32 c);
LLVector3 srgbToLinear(const LLVector3& c);

enum EFillPreset
{
    FILL_BLACK = 0,
    FILL_CHARCOAL,
    FILL_WHITE,
    FILL_CHROMA_GREEN,
    FILL_CHROMA_BLUE,
    FILL_CUSTOM,
    FILL_PRESET_COUNT
};

LLVector3   fillPresetSrgb(EFillPreset preset);
EFillPreset matchFillPreset(const LLVector3& fill_srgb);

LLVector3 silForward(const LLVector3& lit, const LLVector3& fill_lin,
                     F32 a_q, F32 fill_scale,
                     const LLVector3& fog_A, const LLVector3& fog_B,
                     bool haze_over);
void      silHazeBlend(LLVector3& src_rgb, F32& src_a, F32 h);
UploadPlan uploadPlan(const Packed* slot, EUploadSlot which, bool debug_tint);
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
    PRESET_SILHOUETTE,
    PRESET_COUNT
};

Params    applyPreset(const Params& cur, EPreset preset);

LLSD      toLLSD(const Params& p);     // v1 keys, including sil_* fields
Params    fromLLSD(const LLSD& s);     // missing keys => defaults; result sanitized
}

#endif // AL_AVATAR_LIGHT_RESPONSE_MODEL_H

/**
 * @file alavatarlightresponsemodel.cpp
 * @brief [AvatarLightResponse] Pure deterministic model for the per-avatar,
 *        light-independent response trim.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include "alavatarlightresponsemodel.h"

#include <algorithm>
#include <cmath>

namespace ALLightResponse
{
namespace
{
F32 clampField(F32 v, F32 lo, F32 hi, F32 fallback)
{
    if (!std::isfinite(v))
    {
        return fallback;
    }
    return std::min(std::max(v, lo), hi);
}

F32 realField(const LLSD& s, const char* key, F32 fallback)
{
    if (!s.isMap() || !s.has(key))
    {
        return fallback;
    }
    const LLSD& v = s[key];
    if (!(v.isReal() || v.isInteger()))
    {
        return fallback;
    }
    return static_cast<F32>(v.asReal());
}
}

Params sanitize(const Params& p)
{
    Params r;
    r.mDiffuse    = clampField(p.mDiffuse, DIFFUSE_MIN, DIFFUSE_MAX, 0.f);
    r.mTame       = clampField(p.mTame, TAME_MIN, TAME_MAX, 0.f);
    r.mExposureEV = clampField(p.mExposureEV, EV_MIN, EV_MAX, 0.f);
    r.mGlow       = clampField(p.mGlow, GLOW_MIN, GLOW_MAX, GLOW_DEFAULT);
    r.mBypass     = p.mBypass;
    return r;
}

bool isIdentity(const Params& p)
{
    if (p.mBypass)
    {
        return true;
    }
    const Params s = sanitize(p);
    return std::fabs(s.mDiffuse) <= IDENTITY_EPSILON
        && std::fabs(s.mTame) <= IDENTITY_EPSILON
        && std::fabs(s.mExposureEV) <= IDENTITY_EPSILON
        && std::fabs(s.mGlow - GLOW_DEFAULT) <= IDENTITY_EPSILON;
}

LLVector4 pack(const Params& p)
{
    if (p.mBypass)
    {
        return LLVector4(0.f, 1.f, 1.f, 1.f);
    }
    const Params s = sanitize(p);
    return LLVector4(s.mDiffuse, 1.f - s.mTame, std::exp2(s.mExposureEV), s.mGlow);
}

F32 albedoGain(F32 k, F32 max_rgb)
{
    if (k > 1.f)
    {
        const F32 ceiling = 1.f / std::max(max_rgb, 1.0e-4f);
        return std::max(1.f, std::min(k, ceiling));
    }
    return k;
}

F32 legacyGloss(F32 gloss, F32 d)
{
    const F32 dd = std::min(std::max(d, 0.f), 1.f);
    if (dd > 0.f && gloss >= 0.5f / 255.f)
    {
        return std::max(gloss * (1.f - 0.95f * dd), 1.f / 255.f);
    }
    return gloss;
}

LLVector3 graphicCover(const LLVector3& c, const LLVector3& lit_pre, F32 sigma, F32 k)
{
    if (sigma <= 0.f)
    {
        return c;
    }
    if (k == 1.f)
    {
        return c;
    }
    LLVector3 out;
    for (S32 i = 0; i < 3; ++i)
    {
        out.mV[i] = c.mV[i] + (k - 1.f) * (c.mV[i] - (1.f - sigma) * lit_pre.mV[i]);
    }
    return out;
}

Params applyPreset(const Params& cur, EPreset preset)
{
    Params r = sanitize(cur);
    switch (preset)
    {
    case PRESET_RESET:
        r = Params();
        break;
    case PRESET_POWDERED:
        r.mDiffuse = 0.30f;
        r.mTame    = 0.25f;
        break;
    case PRESET_MATTE_SKIN:
        r.mDiffuse = 0.50f;
        r.mTame    = 0.50f;
        break;
    case PRESET_TAME_GLOSS:
        r.mDiffuse = 0.15f;
        r.mTame    = 0.65f;
        break;
    case PRESET_KILL_SHEEN:
        r.mDiffuse = 0.70f;
        r.mTame    = 0.90f;
        break;
    case PRESET_MINUS_HALF_STOP:
        r.mExposureEV = -0.5f;
        break;
    case PRESET_MINUS_ONE_STOP:
        r.mExposureEV = -1.0f;
        break;
    case PRESET_GLOW_OFF:
        r.mGlow = 0.f;
        break;
    default:
        break;
    }
    return r;
}

LLSD toLLSD(const Params& p)
{
    const Params s = sanitize(p);
    LLSD out = LLSD::emptyMap();
    out["diffuse"] = static_cast<LLSD::Real>(s.mDiffuse);
    out["tame"]    = static_cast<LLSD::Real>(s.mTame);
    out["ev"]      = static_cast<LLSD::Real>(s.mExposureEV);
    out["glow"]    = static_cast<LLSD::Real>(s.mGlow);
    out["bypass"]  = static_cast<LLSD::Boolean>(s.mBypass);
    return out;
}

Params fromLLSD(const LLSD& s)
{
    Params p;
    p.mDiffuse    = realField(s, "diffuse", 0.f);
    p.mTame       = realField(s, "tame", 0.f);
    p.mExposureEV = realField(s, "ev", 0.f);
    p.mGlow       = realField(s, "glow", GLOW_DEFAULT);
    p.mBypass     = s.isMap() && s.has("bypass") && s["bypass"].asBoolean();
    return sanitize(p);
}
}

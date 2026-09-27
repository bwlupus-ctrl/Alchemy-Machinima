/**
 * @file alenvintensity.cpp
 * @brief Personal Lighting exposure strip: settings -> effective light factors.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * [EnvIntensity v2] See alenvintensity.h. Preservation contract: with every
 * v2 key at its default the factors equal the original Env Light Intensity
 * feature (7f15bc28b18) for every value inside its old +-4 EV clamp:
 *   sun_mul  = exp2f(clamp(AlchemyEnvSunEV))        (moon linked -> same)
 *   amb_mul  = diff_mul = exp2f(clamp(AlchemyEnvSkyGIEV))
 *   spec_mul = local_mul = 1, tints = 1, lift_gain = 0
 * The clamp is now +-6 (documented change for Debug-Settings-only values).
 */

#include "llviewerprecompiledheaders.h"

#include "alenvintensity.h"

#include "llheroprobemanager.h"
#include "llmath.h"
#include "llrender.h"
#include "llviewercontrol.h"
#include "llviewerobject.h"
#include "pipeline.h"

#include <cmath>

extern bool gCubeSnapshot;

namespace
{
    // ScopedNeutral nesting depth (main/render thread only).
    S32 sNeutralDepth = 0;

    const F32 EV_CLAMP        = 6.f;
    const F32 LIFT_EV_CLAMP   = 2.f;
    const F32 TINT_MAX_CHAN   = 4.f;   // saturation limit on the combined tint (headroom, design 3.3)
    const F32 IDENTITY_EPS    = 1e-6f;
    const F32 KELVIN_NEUTRAL  = 6500.f;

    inline F32 clampEV(F32 ev)
    {
        return llclamp(ev, -EV_CLAMP, EV_CLAMP);
    }

    inline F32 srgbChannelToLinear(F32 c)
    {
        c = llclamp(c, 0.f, 1.f);
        return (c <= 0.04045f) ? c / 12.92f : powf((c + 0.055f) / 1.055f, 2.4f);
    }

    inline F32 maxChannel(const LLColor3& c)
    {
        return llmax(c.mV[0], llmax(c.mV[1], c.mV[2]));
    }

    inline bool isIdentity(const LLColor3& c)
    {
        return fabsf(c.mV[0] - 1.f) < IDENTITY_EPS
            && fabsf(c.mV[1] - 1.f) < IDENTITY_EPS
            && fabsf(c.mV[2] - 1.f) < IDENTITY_EPS;
    }

    // c / Y(c); grey (1,1,1) when the luminance is not positive.
    LLColor3 normLum(const LLColor3& c)
    {
        const F32 lum = ALEnvIntensity::linearLuminance(c);
        if (lum <= 0.f)
        {
            return LLColor3(1.f, 1.f, 1.f);
        }
        return LLColor3(c.mV[0] / lum, c.mV[1] / lum, c.mV[2] / lum);
    }

    // Krystek (1985) Planckian locus in CIE 1960 uv, 1000..15000 K, converted
    // to XYZ (Y = 1) and then to linear sRGB (D65). Clamped at 0.
    LLColor3 planckianLinearRGB(F32 kelvin)
    {
        const F64 t  = llclamp((F64)kelvin, 1000.0, 15000.0);
        const F64 t2 = t * t;
        const F64 u  = (0.860117757 + 1.54118254e-4 * t + 1.28641212e-7 * t2)
                     / (1.0 + 8.42420235e-4 * t + 7.08145163e-7 * t2);
        const F64 v  = (0.317398726 + 4.22806245e-5 * t + 4.20481691e-8 * t2)
                     / (1.0 - 2.89741816e-5 * t + 1.61456053e-7 * t2);
        const F64 denom = 2.0 * u - 8.0 * v + 4.0;
        const F64 x = 3.0 * u / denom;
        const F64 y = 2.0 * v / denom;
        // y is ~0.3..0.45 on the locus; guard anyway.
        if (y <= 1e-9)
        {
            return LLColor3(1.f, 1.f, 1.f);
        }
        const F64 big_x = x / y;
        const F64 big_y = 1.0;
        const F64 big_z = (1.0 - x - y) / y;
        const F64 r =  3.2404542 * big_x - 1.5371385 * big_y - 0.4985314 * big_z;
        const F64 g = -0.9692660 * big_x + 1.8760108 * big_y + 0.0415560 * big_z;
        const F64 b =  0.0556434 * big_x - 0.2040259 * big_y + 1.0572252 * big_z;
        return LLColor3((F32)llmax(r, 0.0), (F32)llmax(g, 0.0), (F32)llmax(b, 0.0));
    }

    // Tiny per-slot cache: the combined tint only changes when a setting
    // changes, so recompute only when the inputs differ.
    struct TintCache
    {
        bool     mValid = false;
        F32      mKelvin = 0.f;
        LLColor4 mSwatch;
        F32      mStrength = 0.f;
        LLColor3 mResult = LLColor3(1.f, 1.f, 1.f);
    };

    enum ETintSlot
    {
        TINT_SUN = 0,
        TINT_MOON,
        TINT_AMBIENT,
        TINT_SLOT_COUNT
    };

    TintCache sTintCache[TINT_SLOT_COUNT];

    LLColor3 cachedTint(ETintSlot slot, F32 kelvin, const LLColor4& swatch, F32 strength)
    {
        TintCache& tc = sTintCache[slot];
        if (!tc.mValid || tc.mKelvin != kelvin || tc.mStrength != strength || tc.mSwatch != swatch)
        {
            tc.mValid    = true;
            tc.mKelvin   = kelvin;
            tc.mSwatch   = swatch;
            tc.mStrength = strength;
            tc.mResult   = ALEnvIntensity::combinedTint(ALEnvIntensity::kelvinRatio(kelvin), swatch, strength);
        }
        return tc.mResult;
    }

    // Legacy-gamma skies skip the tonemapper: a tint must never raise any
    // channel above the untinted value, so scale it to a max channel of 1.
    LLColor3 normMaxChannel(const LLColor3& c)
    {
        const F32 hi = maxChannel(c);
        if (hi <= 0.f || hi == 1.f)
        {
            return c;
        }
        return LLColor3(c.mV[0] / hi, c.mV[1] / hi, c.mV[2] / hi);
    }
}

namespace ALEnvIntensity
{
    F32 linearLuminance(const LLColor3& c)
    {
        return 0.2126f * c.mV[0] + 0.7152f * c.mV[1] + 0.0722f * c.mV[2];
    }

    LLColor3 kelvinRatio(F32 kelvin)
    {
        if (kelvin == KELVIN_NEUTRAL)
        {
            return LLColor3(1.f, 1.f, 1.f);
        }
        const LLColor3 at_t   = planckianLinearRGB(kelvin);
        const LLColor3 at_ref = planckianLinearRGB(KELVIN_NEUTRAL);
        LLColor3 ratio(1.f, 1.f, 1.f);
        for (S32 i = 0; i < 3; ++i)
        {
            ratio.mV[i] = (at_ref.mV[i] > 1e-6f) ? at_t.mV[i] / at_ref.mV[i] : 1.f;
        }
        return ratio;
    }

    LLColor3 combinedTint(const LLColor3& kelvin_ratio, const LLColor4& swatch, F32 strength)
    {
        // Swatch term: bypassed (exactly 1) for strength <= 0 or a white swatch.
        LLColor3 sw(1.f, 1.f, 1.f);
        const bool white = swatch.mV[0] >= 1.f - IDENTITY_EPS
                        && swatch.mV[1] >= 1.f - IDENTITY_EPS
                        && swatch.mV[2] >= 1.f - IDENTITY_EPS;
        if (strength > 0.f && !white)
        {
            const F32 s = llclamp(strength, 0.f, 1.f);
            LLColor3 lin(srgbChannelToLinear(swatch.mV[0]),
                         srgbChannelToLinear(swatch.mV[1]),
                         srgbChannelToLinear(swatch.mV[2]));
            lin = normLum(lin);
            sw = LLColor3(1.f + (lin.mV[0] - 1.f) * s,
                          1.f + (lin.mV[1] - 1.f) * s,
                          1.f + (lin.mV[2] - 1.f) * s);
        }

        // Product, then luminance-normalise the COMBINED result (Y == 1).
        LLColor3 m = kelvin_ratio * sw;
        m = normLum(m);

        // Saturation limit by lerp toward grey: both ends have Y == 1, so the
        // luminance stays 1 and the max channel becomes exactly TINT_MAX_CHAN.
        const F32 hi = maxChannel(m);
        if (hi > TINT_MAX_CHAN)
        {
            const F32 k = (TINT_MAX_CHAN - 1.f) / (hi - 1.f);
            m = LLColor3(1.f + (m.mV[0] - 1.f) * k,
                         1.f + (m.mV[1] - 1.f) * k,
                         1.f + (m.mV[2] - 1.f) * k);
        }

        if (isIdentity(m))
        {
            return LLColor3(1.f, 1.f, 1.f);
        }
        return m;
    }

    bool isSamplingCapture()
    {
        return gCubeSnapshot && !gPipeline.mHeroProbeManager.isMirrorPass();
    }

    ScopedNeutral::ScopedNeutral()  { ++sNeutralDepth; }
    ScopedNeutral::~ScopedNeutral() { --sNeutralDepth; }

    bool isNeutral()
    {
        return sNeutralDepth > 0;
    }

    Factors compute(bool classic, bool legacy_gamma, bool capture, bool sun_up)
    {
        static LLCachedControl<F32>      sun_ev(gSavedSettings, "AlchemyEnvSunEV", 0.f);
        static LLCachedControl<bool>     moon_linked(gSavedSettings, "AlchemyEnvMoonLinked", true);
        static LLCachedControl<F32>      moon_ev(gSavedSettings, "AlchemyEnvMoonEV", 0.f);
        static LLCachedControl<F32>      gi_ev(gSavedSettings, "AlchemyEnvSkyGIEV", 0.f);
        static LLCachedControl<F32>      gi_ambient_ev(gSavedSettings, "AlchemyEnvGIAmbientEV", 0.f);
        static LLCachedControl<F32>      gi_diffuse_ev(gSavedSettings, "AlchemyEnvGIProbeDiffuseEV", 0.f);
        static LLCachedControl<F32>      gi_spec_ev(gSavedSettings, "AlchemyEnvGIProbeSpecEV", 0.f);
        static LLCachedControl<F32>      local_ev(gSavedSettings, "AlchemyEnvLocalLightEV", 0.f);
        static LLCachedControl<F32>      sun_kelvin(gSavedSettings, "AlchemyEnvSunKelvin", KELVIN_NEUTRAL);
        static LLCachedControl<LLColor4> sun_tint_color(gSavedSettings, "AlchemyEnvSunTintColor", LLColor4::white);
        static LLCachedControl<F32>      sun_tint_strength(gSavedSettings, "AlchemyEnvSunTintStrength", 1.f);
        static LLCachedControl<LLColor4> moon_tint_color(gSavedSettings, "AlchemyEnvMoonTintColor", LLColor4::white);
        static LLCachedControl<F32>      moon_tint_strength(gSavedSettings, "AlchemyEnvMoonTintStrength", 1.f);
        static LLCachedControl<LLColor4> amb_tint_color(gSavedSettings, "AlchemyEnvAmbientTintColor", LLColor4::white);
        static LLCachedControl<F32>      amb_tint_strength(gSavedSettings, "AlchemyEnvAmbientTintStrength", 1.f);
        static LLCachedControl<F32>      lift_ev(gSavedSettings, "AlchemyEnvShadowLiftEV", 0.f);

        Factors f;
        if (isNeutral())
        {
            return f;
        }

        const F32 body_ev = sun_up ? (F32)sun_ev
                                   : ((bool)moon_linked ? (F32)sun_ev : (F32)moon_ev);
        f.mSunMul   = exp2f(clampEV(body_ev));
        f.mAmbMul   = exp2f(clampEV((F32)gi_ev + (F32)gi_ambient_ev));
        f.mDiffMul  = exp2f(clampEV((F32)gi_ev + (F32)gi_diffuse_ev));
        f.mSpecMul  = exp2f(clampEV((F32)gi_spec_ev));
        f.mLocalMul = exp2f(clampEV((F32)local_ev));
        f.mSunTint  = sun_up ? cachedTint(TINT_SUN, (F32)sun_kelvin, (LLColor4)sun_tint_color, (F32)sun_tint_strength)
                             : cachedTint(TINT_MOON, KELVIN_NEUTRAL, (LLColor4)moon_tint_color, (F32)moon_tint_strength);
        f.mAmbTint  = cachedTint(TINT_AMBIENT, KELVIN_NEUTRAL, (LLColor4)amb_tint_color, (F32)amb_tint_strength);
        f.mLiftGain = exp2f(llclamp((F32)lift_ev, 0.f, LIFT_EV_CLAMP)) - 1.f;

        // Classic skies (pre-PBR shading) sum sun and ambient in encoded space
        // before decoding: no linear factor can be applied correctly, so every
        // sky-side term is identity. Local lights are added post-combine in
        // linear light and keep their factor.
        if (classic)
        {
            f.mSunMul  = 1.f;
            f.mAmbMul  = 1.f;
            f.mDiffMul = 1.f;
            f.mSpecMul = 1.f;
            f.mSunTint = LLColor3(1.f, 1.f, 1.f);
            f.mAmbTint = LLColor3(1.f, 1.f, 1.f);
            f.mLiftGain = 0.f;
        }
        // Legacy-gamma skies (probe ambiance == 0) skip the tonemapper: boosts
        // above stock would hard-clip in legacyGamma(). Darkening only, and
        // tints may only dim channels (max channel normalised to 1).
        if (legacy_gamma)
        {
            f.mSunMul  = llmin(f.mSunMul, 1.f);
            f.mSunTint = normMaxChannel(f.mSunTint);
            f.mAmbTint = normMaxChannel(f.mAmbTint);
        }
        // Probe captures bake amblit / GI-lit geometry into the irradiance and
        // radiance maps that display-time sampling multiplies again: the
        // sampling-time terms must be identity inside every capture so they are
        // applied exactly once, at display. Source-light terms (sun EV + tint,
        // local lights) stay active: they scale the light once and the bounce
        // they produce is what the probes hold. Hero mirror passes are NOT
        // captures (they show the display look).
        if (capture)
        {
            f.mAmbMul  = 1.f;
            f.mDiffMul = 1.f;
            f.mSpecMul = 1.f;
            f.mAmbTint = LLColor3(1.f, 1.f, 1.f);
            f.mLiftGain = 0.f;
        }
        return f;
    }

    F32 localLightEVScale(const LLViewerObject* obj)
    {
        static LLCachedControl<F32>  local_ev(gSavedSettings, "AlchemyEnvLocalLightEV", 0.f);
        static LLCachedControl<bool> include_rig(gSavedSettings, "AlchemyEnvLocalLightIncludeRig", false);

        if (isNeutral())
        {
            return 1.f;
        }
        if (obj && obj->isCineRigEmitter() && !(bool)include_rig)
        {
            return 1.f;
        }
        return exp2f(clampEV((F32)local_ev));
    }

    F32 ssaoCeilingScale()
    {
        static LLCachedControl<F32> gi_ev(gSavedSettings, "AlchemyEnvSkyGIEV", 0.f);
        static LLCachedControl<F32> gi_diffuse_ev(gSavedSettings, "AlchemyEnvGIProbeDiffuseEV", 0.f);

        if (LLRender::sClassicMode || isNeutral())
        {
            return 1.f;
        }
        return exp2f(clampEV((F32)gi_ev + (F32)gi_diffuse_ev));
    }
}

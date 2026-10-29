/**
 * @file alavatarlightresponsemodel_test.cpp
 * @brief [AvatarLightResponse] Tests for the pure light-response model.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include "../test/lltut.h"
#include "../alavatarlightresponsemodel.h"

#include <cmath>
#include <limits>

namespace tut
{
    using namespace ALLightResponse;

    struct avatar_light_response_data {};
    typedef test_group<avatar_light_response_data> avatar_light_response_group;
    typedef avatar_light_response_group::object avatar_light_response_object;
    avatar_light_response_group avatar_light_response_tests("ALAvatarLightResponseModel");

    // Defaults are exact identity and pack to (0, 1, 1, 1).
    template<> template<>
    void avatar_light_response_object::test<1>()
    {
        const Params p;
        ensure("default is identity", isIdentity(p));
        const LLVector4 v = pack(p);
        ensure_equals("pack d", v.mV[VX], 0.f);
        ensure_equals("pack s", v.mV[VY], 1.f);
        ensure_equals("pack k", v.mV[VZ], 1.f);
        ensure_equals("pack g", v.mV[VW], 1.f);
    }

    // Clamps and non-finite fallbacks.
    template<> template<>
    void avatar_light_response_object::test<2>()
    {
        Params p;
        p.mDiffuse    = 5.f;
        p.mTame       = -3.f;
        p.mExposureEV = -9.f;
        p.mGlow       = 7.f;
        const Params s = sanitize(p);
        ensure_equals("d clamp", s.mDiffuse, 1.f);
        ensure_equals("t clamp", s.mTame, 0.f);
        ensure_equals("e clamp", s.mExposureEV, -2.f);
        ensure_equals("g clamp", s.mGlow, 2.f);

        Params n;
        n.mDiffuse    = std::numeric_limits<F32>::quiet_NaN();
        n.mTame       = std::numeric_limits<F32>::infinity();
        n.mExposureEV = -std::numeric_limits<F32>::infinity();
        n.mGlow       = std::numeric_limits<F32>::quiet_NaN();
        const Params ns = sanitize(n);
        ensure_equals("nan d default", ns.mDiffuse, 0.f);
        ensure_equals("inf t default", ns.mTame, 0.f);
        ensure_equals("-inf e default", ns.mExposureEV, 0.f);
        ensure_equals("nan g default", ns.mGlow, 1.f);
        ensure("all-nonfinite is identity", isIdentity(n));
        ensure_equals("pack of clamped e", pack(p).mV[VZ], 0.25f);
    }

    // Bypass forces identity; near-default values are identity.
    template<> template<>
    void avatar_light_response_object::test<3>()
    {
        Params p;
        p.mDiffuse = 0.5f;
        ensure("adjusted is not identity", !isIdentity(p));
        p.mBypass = true;
        ensure("bypass is identity", isIdentity(p));
        const LLVector4 v = pack(p);
        ensure_equals("bypass pack d", v.mV[VX], 0.f);
        ensure_equals("bypass pack s", v.mV[VY], 1.f);
        ensure_equals("bypass pack k", v.mV[VZ], 1.f);
        ensure_equals("bypass pack g", v.mV[VW], 1.f);

        Params q;
        q.mGlow = 1.00005f;
        ensure("within 1e-4 is identity", isIdentity(q));
        q.mGlow = 1.001f;
        ensure("beyond 1e-4 is not identity", !isIdentity(q));
    }

    // Presets are exact and "keep" preserves other fields.
    template<> template<>
    void avatar_light_response_object::test<4>()
    {
        Params cur;
        cur.mDiffuse    = 0.11f;
        cur.mTame       = 0.22f;
        cur.mExposureEV = -1.5f;
        cur.mGlow       = 0.7f;

        const Params reset = applyPreset(cur, PRESET_RESET);
        ensure("reset is identity", isIdentity(reset));
        ensure_equals("reset glow", reset.mGlow, 1.f);

        const Params powd = applyPreset(cur, PRESET_POWDERED);
        ensure_equals("powdered d", powd.mDiffuse, 0.30f);
        ensure_equals("powdered t", powd.mTame, 0.25f);
        ensure_equals("powdered keeps ev", powd.mExposureEV, -1.5f);
        ensure_equals("powdered keeps glow", powd.mGlow, 0.7f);

        const Params matte = applyPreset(cur, PRESET_MATTE_SKIN);
        ensure_equals("matte d", matte.mDiffuse, 0.50f);
        ensure_equals("matte t", matte.mTame, 0.50f);

        const Params tg = applyPreset(cur, PRESET_TAME_GLOSS);
        ensure_equals("tame gloss d", tg.mDiffuse, 0.15f);
        ensure_equals("tame gloss t", tg.mTame, 0.65f);

        const Params ks = applyPreset(cur, PRESET_KILL_SHEEN);
        ensure_equals("kill sheen d", ks.mDiffuse, 0.70f);
        ensure_equals("kill sheen t", ks.mTame, 0.90f);

        const Params half = applyPreset(cur, PRESET_MINUS_HALF_STOP);
        ensure_equals("-1/2 ev", half.mExposureEV, -0.5f);
        ensure_equals("-1/2 keeps d", half.mDiffuse, 0.11f);
        ensure_equals("-1/2 keeps t", half.mTame, 0.22f);
        ensure_equals("-1/2 keeps g", half.mGlow, 0.7f);

        const Params one = applyPreset(cur, PRESET_MINUS_ONE_STOP);
        ensure_equals("-1 ev", one.mExposureEV, -1.0f);

        const Params off = applyPreset(cur, PRESET_GLOW_OFF);
        ensure_equals("glow off g", off.mGlow, 0.f);
        ensure_equals("glow off keeps ev", off.mExposureEV, -1.5f);
    }

    // albedoGain: exact for k <= 1; hue-preserving ceiling for k > 1.
    template<> template<>
    void avatar_light_response_object::test<5>()
    {
        ensure_equals("k=1 exact", albedoGain(1.f, 0.9f), 1.f);
        ensure_equals("k=.5 exact", albedoGain(0.5f, 0.9f), 0.5f);
        ensure_equals("k=.25 exact", albedoGain(0.25f, 5.f), 0.25f);
        ensure_approximately_equals_range("maxRGB .8, k 2 => 1.25", albedoGain(2.f, 0.8f), 1.25f, 1.0e-6f);
        ensure("k>1 never below 1", albedoGain(2.f, 4.f) >= 1.f);
        ensure_equals("k>1, dark albedo not clipped", albedoGain(1.5f, 0.1f), 1.5f);
        ensure("k>1, near-zero albedo finite", std::isfinite(albedoGain(2.f, 0.f)));
        ensure_equals("k>1, saturated albedo => exactly 1", albedoGain(2.f, 1.f), 1.f);
    }

    // legacyGloss rule.
    template<> template<>
    void avatar_light_response_object::test<6>()
    {
        const F32 glosses[] = { 0.f, 0.001f, 0.004f, 0.02f, 0.5f, 1.f };
        for (F32 g : glosses)
        {
            ensure_equals("d=0 unchanged", legacyGloss(g, 0.f), g);
        }
        ensure_approximately_equals_range("0.02, d=1 => 1/255", legacyGloss(0.02f, 1.f), 1.f / 255.f, 1.0e-7f);
        ensure_equals("gloss 0 stays 0", legacyGloss(0.f, 1.f), 0.f);
        ensure_equals("gloss .001 below half-code stays", legacyGloss(0.001f, 1.f), 0.001f);
        ensure_approximately_equals_range("gloss 1, d .5", legacyGloss(1.f, 0.5f), 0.525f, 1.0e-6f);
        ensure("surviving gloss never below 1/255", legacyGloss(0.5f / 255.f, 1.f) >= 1.f / 255.f);
    }

    // graphicCover algebra.
    template<> template<>
    void avatar_light_response_object::test<7>()
    {
        const LLVector3 c(0.3f, 0.5f, 0.7f);
        const LLVector3 lit(0.2f, 0.2f, 0.2f);

        const LLVector3 s0 = graphicCover(c, lit, 0.f, 0.5f);
        ensure_equals("sigma 0 => c (x)", s0.mV[VX], c.mV[VX]);
        ensure_equals("sigma 0 => c (y)", s0.mV[VY], c.mV[VY]);
        ensure_equals("sigma 0 => c (z)", s0.mV[VZ], c.mV[VZ]);

        const F32 k = 0.5f;
        const LLVector3 s1 = graphicCover(c, lit, 1.f, k);
        for (S32 i = 0; i < 3; ++i)
        {
            ensure_approximately_equals_range("sigma 1 => k*c", s1.mV[i], k * c.mV[i], 1.0e-6f);
        }

        // sigma .5: c = mix(L, F, .5) = .5L + .5F  =>  result .5L + .5kF
        const F32 sigma = 0.5f;
        const LLVector3 fx(0.9f, 0.1f, 0.6f);
        LLVector3 mixed;
        for (S32 i = 0; i < 3; ++i)
        {
            mixed.mV[i] = lit.mV[i] * (1.f - sigma) + fx.mV[i] * sigma;
        }
        const LLVector3 r = graphicCover(mixed, lit, sigma, k);
        for (S32 i = 0; i < 3; ++i)
        {
            const F32 expected = (1.f - sigma) * lit.mV[i] + sigma * k * fx.mV[i];
            ensure_approximately_equals_range("sigma .5 => .5L + .5kF", r.mV[i], expected, 1.0e-6f);
        }
    }

    // LLSD round trip incl. missing keys and explicit identity entries.
    template<> template<>
    void avatar_light_response_object::test<8>()
    {
        Params p;
        p.mDiffuse    = 0.4f;
        p.mTame       = 0.6f;
        p.mExposureEV = -0.75f;
        p.mGlow       = 1.5f;
        p.mBypass     = true;
        const Params back = fromLLSD(toLLSD(p));
        ensure_equals("rt d", back.mDiffuse, 0.4f);
        ensure_equals("rt t", back.mTame, 0.6f);
        ensure_equals("rt e", back.mExposureEV, -0.75f);
        ensure_equals("rt g", back.mGlow, 1.5f);
        ensure("rt bypass", back.mBypass);

        const Params identity = fromLLSD(toLLSD(Params()));
        ensure("explicit identity round-trips as identity", isIdentity(identity));
        ensure("explicit identity has bypass false", !identity.mBypass);

        LLSD partial = LLSD::emptyMap();
        partial["tame"] = 0.3;
        const Params part = fromLLSD(partial);
        ensure_equals("missing d => 0", part.mDiffuse, 0.f);
        ensure_equals("present t", part.mTame, 0.3f);
        ensure_equals("missing e => 0", part.mExposureEV, 0.f);
        ensure_equals("missing g => 1", part.mGlow, 1.f);
        ensure("missing bypass => false", !part.mBypass);

        const Params undef = fromLLSD(LLSD());
        ensure("undefined LLSD => identity", isIdentity(undef));

        LLSD wild = LLSD::emptyMap();
        wild["diffuse"] = 9.0;
        wild["glow"] = -4.0;
        wild["ev"] = std::numeric_limits<F64>::quiet_NaN();
        const Params w = fromLLSD(wild);
        ensure_equals("loaded d clamped", w.mDiffuse, 1.f);
        ensure_equals("loaded g clamped", w.mGlow, 0.f);
        ensure_equals("loaded nan ev default", w.mExposureEV, 0.f);
    }
}

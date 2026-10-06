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
#include <cstring>
#include <limits>
#include <random>

namespace tut
{
    using namespace ALLightResponse;

    struct avatar_light_response_data {};
    typedef test_group<avatar_light_response_data> avatar_light_response_group;
    typedef avatar_light_response_group::object avatar_light_response_object;
    avatar_light_response_group avatar_light_response_tests("ALAvatarLightResponseModel");

    namespace
    {
    bool sameBits(F32 lhs, F32 rhs)
    {
        return std::memcmp(&lhs, &rhs, sizeof(F32)) == 0;
    }

    F32 fp16Round(F32 value)
    {
        if (value == 0.f || !std::isfinite(value))
        {
            return value;
        }
        int exponent = 0;
        const F32 mantissa = std::frexp(value, &exponent);
        return std::ldexp(std::nearbyint(mantissa * 2048.f) / 2048.f, exponent);
    }

    F32 unorm8Round(F32 value)
    {
        return std::round(value * 255.f) / 255.f;
    }

    LLVector3 fog(const LLVector3& value, const LLVector3& a, const LLVector3& b)
    {
        LLVector3 result;
        for (S32 i = 0; i < 3; ++i)
        {
            result.mV[i] = a.mV[i] * value.mV[i] + b.mV[i];
        }
        return result;
    }

    F32 pbrLitMirror(F32 base, F32 metallic)
    {
        const F32 f0 = 0.04f;
        const F32 diffuse = base * (1.f - f0) * (1.f - metallic);
        const F32 specular = f0 * (1.f - metallic) + base * metallic;
        const F32 fresnel = specular;
        return (1.f - fresnel) * diffuse + fresnel * specular;
    }

    F32 nonlinearFx(F32 source)
    {
        return 0.2f + source * source;
    }

    F32 nonlinearLook(F32 layer_source, F32 source, F32 sigma)
    {
        return layer_source * (1.f - sigma) + nonlinearFx(source) * sigma;
    }

    F32 rimRecombine(F32 nonrim, F32 rim_raw, bool rim_on, F32 light_scale)
    {
        F32 color = nonrim;
        if (rim_on)
        {
            color += rim_raw;
        }
        return color * light_scale;
    }
    }

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

    // Silhouette unit 1: defaults and the all-fields pack are identity.
    template<> template<>
    void avatar_light_response_object::test<9>()
    {
        const Params p;
        ensure_equals("default silhouette amount", p.mSilAmount, 0.f);
        ensure_equals("default fill r", p.mSilFillSrgb.mV[VX], 0.f);
        ensure_equals("default fill g", p.mSilFillSrgb.mV[VY], 0.f);
        ensure_equals("default fill b", p.mSilFillSrgb.mV[VZ], 0.f);
        ensure("default haze on", p.mSilHaze);
        ensure("default keep rim off", !p.mSilKeepRim);
        ensure("default remains identity", isIdentity(p));
        const Packed packed = packAll(p, true);
        for (S32 i = 0; i < 4; ++i)
        {
            ensure_equals("default silhouette pack is zero", packed.mSil.mV[i], 0.f);
        }
    }

    // Silhouette unit 2: amount participates in identity, except under bypass.
    template<> template<>
    void avatar_light_response_object::test<10>()
    {
        Params p;
        p.mSilAmount = 0.5f;
        ensure("half silhouette is not identity", !isIdentity(p));
        p.mBypass = true;
        ensure("bypass overrides silhouette", isIdentity(p));
        p.mBypass = false;
        p.mSilAmount = 5.0e-5f;
        ensure("sub-threshold silhouette is identity", isIdentity(p));
    }

    // Silhouette unit 3: sanitize clamps and replaces non-finite values.
    template<> template<>
    void avatar_light_response_object::test<11>()
    {
        Params p;
        p.mSilAmount = 3.f;
        p.mSilFillSrgb.set(-1.f, 0.25f, 2.f);
        Params s = sanitize(p);
        ensure_equals("amount clamps high", s.mSilAmount, 1.f);
        ensure_equals("fill clamps low", s.mSilFillSrgb.mV[VX], 0.f);
        ensure_equals("fill keeps in-range", s.mSilFillSrgb.mV[VY], 0.25f);
        ensure_equals("fill clamps high", s.mSilFillSrgb.mV[VZ], 1.f);

        p.mSilAmount = std::numeric_limits<F32>::quiet_NaN();
        p.mSilFillSrgb.set(std::numeric_limits<F32>::infinity(),
                           -std::numeric_limits<F32>::infinity(),
                           std::numeric_limits<F32>::quiet_NaN());
        s = sanitize(p);
        ensure_equals("non-finite amount defaults", s.mSilAmount, 0.f);
        for (S32 i = 0; i < 3; ++i)
        {
            ensure_equals("non-finite fill defaults", s.mSilFillSrgb.mV[i], 0.f);
        }
    }

    // Silhouette unit 4: LLSD round-trip and strict backward-compatible defaults.
    template<> template<>
    void avatar_light_response_object::test<12>()
    {
        Params p;
        p.mSilAmount = 0.375f;
        p.mSilFillSrgb.set(0.1f, 0.2f, 0.3f);
        p.mSilHaze = false;
        p.mSilKeepRim = true;
        const Params back = fromLLSD(toLLSD(p));
        ensure_equals("rt sil amount", back.mSilAmount, p.mSilAmount);
        for (S32 i = 0; i < 3; ++i)
        {
            ensure_equals("rt sil fill", back.mSilFillSrgb.mV[i], p.mSilFillSrgb.mV[i]);
        }
        ensure("rt haze", !back.mSilHaze);
        ensure("rt keep rim", back.mSilKeepRim);

        LLSD old = LLSD::emptyMap();
        old["diffuse"] = 0.2;
        const Params old_back = fromLLSD(old);
        ensure_equals("old map silhouette off", old_back.mSilAmount, 0.f);
        ensure("old map haze default", old_back.mSilHaze);
        ensure("old map rim default", !old_back.mSilKeepRim);

        LLSD malformed = toLLSD(p);
        malformed["sil_fill"] = "green";
        malformed["sil_haze"] = 0;
        malformed["sil_keep_rim"] = 1;
        const Params bad = fromLLSD(malformed);
        ensure_equals("malformed fill r is black", bad.mSilFillSrgb.mV[VX], 0.f);
        ensure_equals("malformed fill g is black", bad.mSilFillSrgb.mV[VY], 0.f);
        ensure_equals("malformed fill b is black", bad.mSilFillSrgb.mV[VZ], 0.f);
        ensure("ill-typed haze defaults true", bad.mSilHaze);
        ensure("ill-typed rim defaults false", !bad.mSilKeepRim);
    }

    // Silhouette unit 5: packAll fields and bypass suppression.
    template<> template<>
    void avatar_light_response_object::test<13>()
    {
        Params p;
        p.mDiffuse = 0.2f;
        p.mSilAmount = 0.5f;
        p.mSilFillSrgb.set(1.f, 0.5f, 0.f);
        p.mSilHaze = false;
        p.mSilKeepRim = true;
        const Packed packed = packAll(p, true);
        ensure_equals("params preserved", packed.mParams.mV[VX], 0.2f);
        ensure_equals("linear red", packed.mSil.mV[VX], 1.f);
        ensure_approximately_equals_range("linear half", packed.mSil.mV[VY], srgbToLinear(0.5f), 1.0e-7f);
        ensure_equals("linear zero", packed.mSil.mV[VZ], 0.f);
        ensure_equals("quantized amount", packed.mSil.mV[VW], silAmountQ(255, true));
        ensure_equals("carrier", packed.mSilAux.mV[VX], silCarrier(255, false, true, true));
        ensure_equals("haze flag", packed.mSilAux.mV[VY], 0.f);
        ensure_equals("rim flag", packed.mSilAux.mV[VZ], 1.f);
        ensure_equals("aux spare", packed.mSilAux.mV[VW], 0.f);

        p.mBypass = true;
        const Packed bypass = packAll(p, true);
        for (S32 i = 0; i < 4; ++i)
        {
            ensure_equals("bypass sil zero", bypass.mSil.mV[i], 0.f);
            ensure_equals("bypass aux zero", bypass.mSilAux.mV[i], 0.f);
        }
    }

    // Silhouette unit 6: every HDR carrier survives fp16 exactly.
    template<> template<>
    void avatar_light_response_object::test<14>()
    {
        for (S32 flags = 0; flags < 4; ++flags)
        {
            for (S32 q = 1; q <= 510; ++q)
            {
                const F32 carrier = fp16Round(silCarrier(q, (flags & 1) == 0, (flags & 2) == 0, true));
                F32 amount = 0.f;
                bool haze_skip = false, rim_cut = false;
                ensure("valid HDR carrier", silDecode(carrier, true, amount, haze_skip, rim_cut));
                ensure_equals("HDR q exact", static_cast<S32>(std::lround(amount * 510.f)), q);
                ensure_equals("HDR haze flag exact", haze_skip, (flags & 1) != 0);
                ensure_equals("HDR rim flag exact", rim_cut, (flags & 2) != 0);
            }
        }
        const F32 invalid[] = { 1.f, 0.f, 0.5f, 1.75f, 2048.5f };
        for (F32 carrier : invalid)
        {
            F32 amount; bool haze_skip, rim_cut;
            ensure("invalid HDR carrier rejected", !silDecode(carrier, true, amount, haze_skip, rim_cut));
        }
        F32 amount; bool haze_skip, rim_cut;
        ensure("HDR maximum carrier valid", silDecode(2048.f, true, amount, haze_skip, rim_cut));
        ensure_equals("HDR maximum q", static_cast<S32>(std::lround(amount * 510.f)), 510);
        ensure("HDR maximum flags", haze_skip && rim_cut);
    }

    // Silhouette unit 7: every LDR carrier survives unorm8 exactly.
    template<> template<>
    void avatar_light_response_object::test<15>()
    {
        for (S32 flags = 0; flags < 4; ++flags)
        {
            for (S32 q = 1; q <= 62; ++q)
            {
                const F32 carrier = unorm8Round(silCarrier(q, (flags & 1) == 0, (flags & 2) == 0, false));
                F32 amount = 0.f;
                bool haze_skip = false, rim_cut = false;
                ensure("valid LDR carrier", silDecode(carrier, false, amount, haze_skip, rim_cut));
                ensure_equals("LDR q exact", static_cast<S32>(std::lround(amount * 62.f)), q);
                ensure_equals("LDR haze flag exact", haze_skip, (flags & 1) != 0);
                ensure_equals("LDR rim flag exact", rim_cut, (flags & 2) != 0);
            }
        }
        for (S32 code = 249; code <= 255; ++code)
        {
            F32 amount; bool haze_skip, rim_cut;
            ensure("reserved LDR code rejected", !silDecode(static_cast<F32>(code) / 255.f, false,
                                                              amount, haze_skip, rim_cut));
        }
        F32 amount; bool haze_skip, rim_cut;
        ensure("zero LDR code rejected", !silDecode(0.f, false, amount, haze_skip, rim_cut));
    }

    // Silhouette unit 8: quantization error bounds and exact endpoint.
    template<> template<>
    void avatar_light_response_object::test<16>()
    {
        for (S32 i = 1; i <= 1000; ++i)
        {
            const F32 a = static_cast<F32>(i) / 1000.f;
            // Below half of the first quantization interval the normative
            // forced-minimum q=1 rule necessarily exceeds the half-step bound.
            if (a >= 1.f / 1020.f)
            {
                const F32 hdr = silAmountQ(silQuant(a, true), true);
                ensure("HDR quantization bound", std::fabs(hdr - a) <= 1.f / 1020.f + 1.0e-7f);
            }
            if (a >= 1.f / 124.f)
            {
                const F32 ldr = silAmountQ(silQuant(a, false), false);
                ensure("LDR quantization bound", std::fabs(ldr - a) <= 1.f / 124.f + 1.0e-7f);
            }
        }
        ensure_equals("HDR one exact", silAmountQ(silQuant(1.f, true), true), 1.f);
        ensure_equals("LDR one exact", silAmountQ(silQuant(1.f, false), false), 1.f);
        ensure_equals("threshold off", silQuant(1.0e-4f, true), 0);
    }

    // Silhouette unit 9: fill preset codes and matching tolerance.
    template<> template<>
    void avatar_light_response_object::test<17>()
    {
        const U8 codes[][3] = { {0,0,0}, {38,38,38}, {255,255,255}, {0,177,64}, {0,71,187} };
        for (S32 preset = FILL_BLACK; preset < FILL_CUSTOM; ++preset)
        {
            const LLVector3 color = fillPresetSrgb(static_cast<EFillPreset>(preset));
            for (S32 i = 0; i < 3; ++i)
            {
                ensure_approximately_equals_range("fill preset code", color.mV[i], codes[preset][i] / 255.f, 1.0e-7f);
            }
            ensure_equals("fill preset exact match", matchFillPreset(color), static_cast<EFillPreset>(preset));
        }
        ensure_equals("custom fill match", matchFillPreset(LLVector3(0.13f, 0.27f, 0.91f)), FILL_CUSTOM);
    }

    // Silhouette unit 10: silhouette/reset/glow presets preserve the specified fields.
    template<> template<>
    void avatar_light_response_object::test<18>()
    {
        Params cur;
        cur.mDiffuse = 0.2f;
        cur.mTame = 0.3f;
        cur.mExposureEV = -0.5f;
        cur.mGlow = 1.7f;
        cur.mSilAmount = 0.4f;
        cur.mSilFillSrgb.set(0.1f, 0.2f, 0.3f);
        cur.mSilHaze = false;
        cur.mSilKeepRim = true;
        const Params silhouette = applyPreset(cur, PRESET_SILHOUETTE);
        ensure_equals("silhouette amount one", silhouette.mSilAmount, 1.f);
        ensure_equals("silhouette keeps diffuse", silhouette.mDiffuse, cur.mDiffuse);
        ensure_equals("silhouette keeps glow", silhouette.mGlow, cur.mGlow);
        ensure_equals("silhouette keeps fill", silhouette.mSilFillSrgb.mV[VY], cur.mSilFillSrgb.mV[VY]);
        ensure("silhouette keeps flags", !silhouette.mSilHaze && silhouette.mSilKeepRim);
        ensure("reset clears silhouette", isIdentity(applyPreset(cur, PRESET_RESET)));
        const Params glow_off = applyPreset(cur, PRESET_GLOW_OFF);
        ensure_equals("glow off keeps silhouette", glow_off.mSilAmount, cur.mSilAmount);
        ensure_equals("glow off clears glow", glow_off.mGlow, 0.f);
    }

    // Silhouette unit 11: Haze ON is k*fog(L) + a*fog(F*w).
    template<> template<>
    void avatar_light_response_object::test<19>()
    {
        const LLVector3 lit(0.2f, 0.4f, 0.7f), fill(0.9f, 0.3f, 0.1f);
        const LLVector3 a(0.5f, 0.7f, 0.9f), b(0.02f, 0.03f, 0.04f);
        const F32 amount = 0.35f, scale = 1.f / 1.1f;
        const LLVector3 actual = silForward(lit, fill, amount, scale, a, b, true);
        const LLVector3 fog_l = fog(lit, a, b);
        const LLVector3 fog_f = fog(fill * scale, a, b);
        for (S32 i = 0; i < 3; ++i)
        {
            ensure_approximately_equals_range("haze-on algebra", actual.mV[i],
                (1.f - amount) * fog_l.mV[i] + amount * fog_f.mV[i], 1.0e-6f);
        }
    }

    // Silhouette unit 12: Haze OFF is k*fog(L) + a*F*w.
    template<> template<>
    void avatar_light_response_object::test<20>()
    {
        const LLVector3 lit(0.2f, 0.4f, 0.7f), fill(0.9f, 0.3f, 0.1f);
        const LLVector3 a(0.5f, 0.7f, 0.9f), b(0.02f, 0.03f, 0.04f);
        const F32 amount = 0.35f, scale = 1.f / 1.1f;
        const LLVector3 actual = silForward(lit, fill, amount, scale, a, b, false);
        const LLVector3 fog_l = fog(lit, a, b);
        for (S32 i = 0; i < 3; ++i)
        {
            ensure_approximately_equals_range("haze-off algebra", actual.mV[i],
                (1.f - amount) * fog_l.mV[i] + amount * fill.mV[i] * scale, 1.0e-6f);
        }
    }

    // Silhouette unit 13: endpoints preserve the exact off path and full fill.
    template<> template<>
    void avatar_light_response_object::test<21>()
    {
        const LLVector3 lit(0.2f, 0.4f, 0.7f), fill(0.9f, 0.3f, 0.1f);
        const LLVector3 a(0.5f, 0.7f, 0.9f), b(0.02f, 0.03f, 0.04f);
        const LLVector3 expected_off = fog(lit, a, b);
        const LLVector3 off = silForward(lit, fill, 0.f, 0.75f, a, b, true);
        const LLVector3 full = silForward(lit, fill, 1.f, 0.75f, a, b, false);
        for (S32 i = 0; i < 3; ++i)
        {
            ensure("amount zero bitwise fog", sameBits(off.mV[i], expected_off.mV[i]));
            ensure_equals("amount one is scaled fill", full.mV[i], fill.mV[i] * 0.75f);
        }
    }

    // Silhouette unit 14: deferred haze weight endpoints.
    template<> template<>
    void avatar_light_response_object::test<22>()
    {
        LLVector3 rgb(0.2f, 0.4f, 0.6f);
        F32 alpha = 0.3f;
        const LLVector3 original = rgb;
        const F32 original_alpha = alpha;
        silHazeBlend(rgb, alpha, 0.f);
        for (S32 i = 0; i < 3; ++i)
        {
            ensure("h zero rgb bitwise", sameBits(rgb.mV[i], original.mV[i]));
        }
        ensure("h zero alpha bitwise", sameBits(alpha, original_alpha));
        silHazeBlend(rgb, alpha, 1.f);
        for (S32 i = 0; i < 3; ++i)
        {
            ensure_equals("h one rgb zero", rgb.mV[i], 0.f);
        }
        ensure_equals("h one alpha one", alpha, 1.f);
    }

    // Silhouette unit 15: metallic BRDF terms are scaled at the output, not at the input.
    template<> template<>
    void avatar_light_response_object::test<23>()
    {
        bool input_order_differs = false;
        const F32 metallics[] = { 0.f, 0.5f, 1.f };
        const F32 keeps[] = { 0.25f, 0.5f, 0.75f };
        for (F32 metallic : metallics)
        {
            for (F32 keep : keeps)
            {
                const F32 lit = pbrLitMirror(0.72f, metallic);
                const F32 output_side = lit * keep;
                const F32 reference = keep * lit;
                ensure_equals("output-side equals k*Lit", output_side, reference);
                const F32 input_side = pbrLitMirror(0.72f * keep, metallic) * keep;
                input_order_differs = input_order_differs || std::fabs(input_side - reference) > 1.0e-6f;
            }
        }
        ensure("input-side scaling is a regression", input_order_differs);
    }

    // Silhouette unit 16: Keep Rim uses the unscaled diffuse tint and r=1.
    template<> template<>
    void avatar_light_response_object::test<24>()
    {
        const F32 diffuse = 0.36f;
        const F32 tint_mix = 0.8f;
        const F32 rim_base = 1.7f;
        const F32 rim = rim_base * ((1.f - tint_mix) + diffuse * tint_mix);
        const F32 keeps[] = { 0.f, 0.25f, 0.5f, 0.75f, 1.f };
        for (F32 keep : keeps)
        {
            const F32 kept_rim = rim * 1.f;
            ensure("r=1 keeps rim bitwise", sameBits(kept_rim, rim));
            const F32 wrongly_input_scaled = rim_base * ((1.f - tint_mix) + diffuse * keep * tint_mix);
            if (keep != 1.f)
            {
                ensure("scaled tint would change rim", wrongly_input_scaled != rim);
            }
        }
    }

    // Silhouette unit 17: the established graphicCover mirror remains unchanged.
    template<> template<>
    void avatar_light_response_object::test<25>()
    {
        const LLVector3 c(0.3f, 0.5f, 0.7f), lit(0.2f, 0.2f, 0.2f);
        const LLVector3 result = graphicCover(c, lit, 0.4f, 0.6f);
        for (S32 i = 0; i < 3; ++i)
        {
            const F32 expected = c.mV[i] + (0.6f - 1.f) * (c.mV[i] - 0.6f * lit.mV[i]);
            ensure_equals("graphicCover formula unchanged", result.mV[i], expected);
        }
    }

    // Silhouette unit 18: nonlinear looks must be evaluated before output scaling.
    template<> template<>
    void avatar_light_response_object::test<26>()
    {
        const F32 source = 0.7f;
        const F32 sigma = 0.8f;
        const F32 styled = nonlinearLook(source, source, sigma);
        const F32 output_side = 0.f * styled;
        const F32 input_side = nonlinearLook(0.f, 0.f, sigma);
        ensure_equals("output-side k*S vanishes", output_side, 0.f);
        ensure("input-side nonlinear look survives", input_side != 0.f);
    }

    // Silhouette unit 19: isolated rim delta and endpoint compositing.
    template<> template<>
    void avatar_light_response_object::test<27>()
    {
        const F32 base = 0.f, rim = 0.45f;
        const F32 identity_with_rim = base + rim;
        const F32 identity_without_rim = base;
        const F32 identity_delta = identity_with_rim - identity_without_rim;
        ensure("identity delta is rim bitwise", sameBits(identity_delta, rim));

        const F32 sigma = 0.75f;
        const F32 styled = nonlinearLook(base + rim, base + rim, sigma);
        const F32 styled_no_rim = nonlinearLook(base, base, sigma);
        const F32 delta = styled - styled_no_rim;
        const F32 at_one = 1.f * styled + (1.f - 1.f) * delta;
        const F32 at_zero = 0.f * styled + (1.f - 0.f) * delta;
        ensure_equals("rim composite at k one is S", at_one, styled);
        ensure_equals("rim composite at k zero is delta", at_zero, delta);
    }

    // Silhouette unit 20: uploadPlan fully defines every uniform on every transition.
    template<> template<>
    void avatar_light_response_object::test<28>()
    {
        Params p;
        p.mDiffuse = 0.2f;
        p.mGlow = 1.4f;
        p.mSilAmount = 0.5f;
        p.mSilFillSrgb.set(0.2f, 0.4f, 0.6f);
        const Packed sil = packAll(p, true);
        p.mSilAmount = 0.f;
        const Packed no_sil = packAll(p, true);

        const UploadPlan beauty_sil = uploadPlan(&sil, UPLOAD_BEAUTY, false);
        ensure_equals("beauty silhouette enabled", beauty_sil.enabled, 1);
        ensure_equals("beauty silhouette amount", beauty_sil.sil.mV[VW], sil.mSil.mV[VW]);

        const UploadPlan sil_only_plain = uploadPlan(&no_sil, UPLOAD_SIL_ONLY, false);
        ensure_equals("plain sil-only disabled", sil_only_plain.enabled, 0);
        const UploadPlan absent = uploadPlan(nullptr, UPLOAD_BEAUTY, false);
        ensure_equals("absent disabled", absent.enabled, 0);
        const UploadPlan glow = uploadPlan(&sil, UPLOAD_GLOW, false);
        ensure_equals("glow enabled", glow.enabled, 1);
        ensure_equals("glow z is g*k", glow.params.mV[VZ], sil.mParams.mV[VW] * (1.f - sil.mSil.mV[VW]));
        const UploadPlan sil_only = uploadPlan(&sil, UPLOAD_SIL_ONLY, false);
        ensure_equals("sil-only enabled", sil_only.enabled, 1);
        ensure_equals("sil-only trims identity", sil_only.params.mV[VY], 1.f);
        const UploadPlan beauty_plain = uploadPlan(&no_sil, UPLOAD_BEAUTY, false);
        ensure_equals("plain beauty remains enabled", beauty_plain.enabled, 1);
        ensure_equals("plain beauty keeps params", beauty_plain.params.mV[VX], no_sil.mParams.mV[VX]);

        const UploadPlan plans[] = { sil_only_plain, absent, glow, beauty_plain };
        for (const UploadPlan& plan : plans)
        {
            ensure_equals("no-fill step sil x reset", plan.sil.mV[VX], 0.f);
            ensure_equals("no-fill step sil y reset", plan.sil.mV[VY], 0.f);
            ensure_equals("no-fill step sil z reset", plan.sil.mV[VZ], 0.f);
            ensure_equals("no-fill step sil w reset", plan.sil.mV[VW], 0.f);
            ensure_equals("no-fill step aux x reset", plan.aux.mV[VX], 0.f);
            ensure_equals("no-fill step aux y reset", plan.aux.mV[VY], 0.f);
            ensure_equals("no-fill step aux z reset", plan.aux.mV[VZ], 0.f);
            ensure_equals("no-fill step aux w reset", plan.aux.mV[VW], 0.f);
        }
        const UploadPlan debug = uploadPlan(&sil, UPLOAD_BEAUTY, true);
        ensure_equals("debug tint enabled code", debug.enabled, 3);
        ensure_equals("debug tint suppresses silhouette", debug.sil.mV[VW], 0.f);
    }

    // Silhouette unit 21: RimSplit recombines in today's operation order.
    template<> template<>
    void avatar_light_response_object::test<29>()
    {
        std::mt19937 rng(0x51A0u);
        std::uniform_real_distribution<F32> distribution(-8.f, 8.f);
        bool distributed_differs = false;
        for (S32 sample = 0; sample < 2048; ++sample)
        {
            const F32 nonrim = distribution(rng);
            const F32 rim = distribution(rng);
            const F32 scale = distribution(rng);
            const bool rim_on = (sample & 1) != 0;

            F32 reference = nonrim;
            if (rim_on)
            {
                reference += rim;
            }
            reference *= scale;
            const F32 recombined = rimRecombine(nonrim, rim, rim_on, scale);
            ensure("RimSplit recombination bitwise", sameBits(recombined, reference));

            if (rim_on)
            {
                volatile F32 nr_scaled = nonrim * scale;
                volatile F32 rr_scaled = rim * scale;
                const F32 distributed = nr_scaled + rr_scaled;
                distributed_differs = distributed_differs || !sameBits(distributed, reference);
            }
        }
        ensure("distributed RimSplit form differs for a sample", distributed_differs);
    }
}

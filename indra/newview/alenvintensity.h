/**
 * @file alenvintensity.h
 * @brief Personal Lighting exposure strip: settings -> effective light factors.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * [EnvIntensity v2] One place that turns the AlchemyEnv* viewer settings
 * (Sun / Moon EV, GI master + offsets, probe reflections EV, local lights
 * EV, Kelvin, tints, shadow lift) into the multipliers the renderer uses:
 *   - LLSettingsVOSky::applySpecial uploads Factors as SG_ANY uniforms
 *     (sky_sun_ev_scale, sky_gi_scale, sky_amb_scale, sky_probe_rad_scale,
 *     sky_sun_tint, sky_amb_tint, shadow_lift_gain);
 *   - LLPipeline multiplies every local-light colour site by
 *     localLightEVScale(emitter);
 *   - the SSAO irradiance ceiling in renderDeferredLighting scales with
 *     ssaoCeilingScale().
 * Everything is a viewer-side multiplier on top of the EEP sky; nothing here
 * is ever written into a sky asset. All defaults are exact identity
 * (exp2f(0) == 1, white tints, gain 0), so the stock render is bit-identical.
 */

#pragma once

#ifndef AL_ENVINTENSITY_H
#define AL_ENVINTENSITY_H

#include "v3color.h"
#include "v4color.h"

class LLViewerObject;

namespace ALEnvIntensity
{
    struct Factors
    {
        F32      mSunMul   = 1.f;                    // sky_sun_ev_scale: sun (day) or moon (night) EV factor
        F32      mAmbMul   = 1.f;                    // sky_amb_scale: sky ambient (amblit)
        F32      mDiffMul  = 1.f;                    // sky_gi_scale: probe irradiance + probes-off fallback
        F32      mSpecMul  = 1.f;                    // sky_probe_rad_scale: probe radiance (reflections)
        F32      mLocalMul = 1.f;                    // local lights EV factor (C++ side, not a uniform)
        LLColor3 mSunTint  = LLColor3(1.f, 1.f, 1.f); // sky_sun_tint
        LLColor3 mAmbTint  = LLColor3(1.f, 1.f, 1.f); // sky_amb_tint
        F32      mLiftGain = 0.f;                    // shadow_lift_gain
    };

    // Effective factors for the current pass.
    //  classic      : classic sky (legacy sky, RenderSkyAutoAdjustLegacy off) ->
    //                 every sky-side term is identity (local lights still apply)
    //  legacy_gamma : probe ambiance == 0 (no tonemapper) -> sun boost capped at
    //                 0 EV, tints normalised so no channel exceeds stock
    //  capture      : reflection-probe capture (NOT a hero mirror pass) ->
    //                 sampling-time terms (amb/diff/spec, amb tint, lift) are
    //                 identity; source-light terms (sun, sun tint, local) stay
    //  sun_up       : same predicate as the sun_up_factor uniform; picks the
    //                 sun or the moon EV / tint
    // Returns identity for everything while a ScopedNeutral is alive.
    Factors compute(bool classic, bool legacy_gamma, bool capture, bool sun_up);

    // gCubeSnapshot && !gPipeline.mHeroProbeManager.isMirrorPass()
    bool isSamplingCapture();

    // Per-emitter factor for the local-light colour sites in LLPipeline:
    // 2^AlchemyEnvLocalLightEV, or exactly 1 for a Cine Light Rig emitter
    // unless AlchemyEnvLocalLightIncludeRig is on. 1 while ScopedNeutral is
    // alive. Never gated on captures (source-light term). obj may be null.
    F32 localLightEVScale(const LLViewerObject* obj);

    // Factor for the absolute SSAO irradiance ceiling (probe-diffuse factor;
    // identity in classic mode via LLRender::sClassicMode). SSAO never runs
    // inside a probe capture, so no capture gate is needed.
    F32 ssaoCeilingScale();

    // RAII: while alive, compute() returns identity and localLightEVScale()
    // returns 1 (GLTF material preview renders its neutral direct rig).
    struct ScopedNeutral
    {
        ScopedNeutral();
        ~ScopedNeutral();
        ScopedNeutral(const ScopedNeutral&) = delete;
        ScopedNeutral& operator=(const ScopedNeutral&) = delete;
    };
    bool isNeutral();

    // Colour maths, exposed for tests / tooling.
    // Linear-sRGB ratio of a Planckian (Krystek 1985) radiator at `kelvin`
    // over the one at 6500 K; exactly (1,1,1) at 6500 K.
    LLColor3 kelvinRatio(F32 kelvin);
    // Combined multiplier with linear weighted luminance exactly 1 against a
    // neutral light: product of the Kelvin ratio and the (strength-scaled)
    // sRGB swatch, luminance-normalised, saturation-limited to a max channel
    // of 4 by lerp toward grey. White swatch / strength <= 0 removes only the
    // swatch term. Exact identity when kelvin_ratio == 1 and the swatch term
    // is bypassed.
    LLColor3 combinedTint(const LLColor3& kelvin_ratio, const LLColor4& swatch, F32 strength);
    // 0.2126 R + 0.7152 G + 0.0722 B
    F32 linearLuminance(const LLColor3& c);
}

#endif // AL_ENVINTENSITY_H

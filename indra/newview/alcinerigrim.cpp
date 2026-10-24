/**
 * @file alcinerigrim.cpp
 * @brief Rig Rim Light ("rig rim"): settings -> shader uniforms.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "alcinerigrim.h"

#include "alcinelightrigmanager.h"
#include "altron.h"
#include "llglslshader.h"
#include "llrender.h"
#include "llshadermgr.h"
#include "llviewercontrol.h"
#include "llviewershadermgr.h"
#include "llvovolume.h"
#include "pipeline.h"

#include <cmath>

extern bool gCubeSnapshot;

namespace
{
// [RigRim] P2-2: replace NaN/inf with a fallback before it ever reaches
// llclamp (which passes NaN through unchanged) or a shader uniform.
F32 finiteOr(F32 v, F32 fallback)
{
    return std::isfinite(v) ? v : fallback;
}

// [RigRim] Round-2 P1 fix: sharedActorFxPbrF.glsl declares light_position[8]
// (and rig_rim_lights[8]) UNCONDITIONALLY for its OPAQUE, MASK and BLEND
// alpha-mode permutations alike (verified in the shader source: unlike
// materialF.glsl, where that block is gated behind
// `#if DIFFUSE_ALPHA_MODE == DIFFUSE_ALPHA_MODE_BLEND` and pbralphaF/alphaF,
// which are single-purpose always-alpha programs with no opaque/mask
// sibling). That makes the LIGHT_POSITION discriminator alone misclassify
// the OPAQUE and MASK Shared Actor FX variants as "alpha surfaces", so
// toggling CineRigRimIncludeAlpha off would incorrectly strip rim from
// fully-opaque and hard-cutout Actor FX draws too.
//
// Depth-prepass variants (gSharedActorFx*Depth*Program) are excluded here:
// they link sharedActorFxPbrDepthF.glsl, a separate file that declares
// neither rig_rim_mode nor light_position, so bindGlobals() already
// early-returns for them regardless.
//
// This is a small fixed registry of the OPAQUE/MASK program addresses
// (populated from the same extern arrays llviewershadermgr.h declares),
// checked once per bindGlobals() call. No change to LLGLSLShader/llrender
// needed: comparing addresses works identically from both bind paths
// (LLPipeline::bindDeferredShader and lldrawpoolalpha.cpp's
// prepare_alpha_shader) since it happens inside bindGlobals() itself.
bool isNonBlendSharedActorFx(const LLGLSLShader& shader)
{
#define AL_RIGRIM_NON_BLEND(ARRAY) \
    &ARRAY[SHARED_ACTOR_FX_PBR_ALPHA_OPAQUE], &ARRAY[SHARED_ACTOR_FX_PBR_ALPHA_MASK]
    static const LLGLSLShader* const non_blend[] = {
        AL_RIGRIM_NON_BLEND(gSharedActorFxPBRProgram),
        AL_RIGRIM_NON_BLEND(gSharedActorFxSkinnedPBRProgram),
        AL_RIGRIM_NON_BLEND(gSharedActorFxPBRSlotProgram),
        AL_RIGRIM_NON_BLEND(gSharedActorFxSkinnedPBRSlotProgram),
        AL_RIGRIM_NON_BLEND(gSharedActorFxPBRGlowProgram),
        AL_RIGRIM_NON_BLEND(gSharedActorFxSkinnedPBRGlowProgram),
        AL_RIGRIM_NON_BLEND(gSharedActorFxPBRGlowSlotProgram),
        AL_RIGRIM_NON_BLEND(gSharedActorFxSkinnedPBRGlowSlotProgram),
        AL_RIGRIM_NON_BLEND(gSharedActorFxPBRSyntheticGlowProgram),
        AL_RIGRIM_NON_BLEND(gSharedActorFxSkinnedPBRSyntheticGlowProgram),
        AL_RIGRIM_NON_BLEND(gSharedActorFxPBRSyntheticGlowSlotProgram),
        AL_RIGRIM_NON_BLEND(gSharedActorFxSkinnedPBRSyntheticGlowSlotProgram),
    };
#undef AL_RIGRIM_NON_BLEND
    for (const LLGLSLShader* candidate : non_blend)
    {
        if (candidate == &shader)
        {
            return true;
        }
    }
    return false;
}

// [TronT1] Shared by bindGlobals() (which has a caller-supplied hint) and
// bindAnimated() (which does not -- the fast path has no forward_alpha of
// its own to pass in): the SAME LIGHT_POSITION/isNonBlendSharedActorFx
// discriminator bindGlobals() already used, factored out so both agree on
// whether a given program instance is a forward-lit / alpha-blended surface.
bool resolveForwardAlpha(LLGLSLShader& shader, bool hint)
{
    bool forward_alpha = hint || (shader.getUniformLocation(LLShaderMgr::LIGHT_POSITION) >= 0);
    if (isNonBlendSharedActorFx(shader))
    {
        forward_alpha = false;
    }
    return forward_alpha;
}

// bindAnimated() is refreshed directly on every fast bind. bindGlobals()'s
// own uniforms are different: the fast path skips them, so isEnabled() below
// tracks their settings/validity signature and invalidates cached full binds
// only when that signature changes.
} // namespace

// [RigRim]
bool ALCineRigRim::isEnabled()
{
    static LLCachedControl<bool> enable(gSavedSettings, "CineRigRimEnabled", false);
    static LLCachedControl<F32>  master_gain(gSavedSettings, "CineRigRimMasterGain", 1.f);
    static LLCachedControl<F32>  smoothness(gSavedSettings, "CineRigRimSmoothness", 0.f);
    static LLCachedControl<F32>  anti_shimmer(gSavedSettings, "CineRigRimAntiShimmer", 0.f);
    static LLCachedControl<bool> debug_rim_only(gSavedSettings, "CineRigRimDebugRimOnly", false);
    static LLCachedControl<U32> debug_view(gSavedSettings, "CineRigRimDebugView", 0);
    // zero gain / no lit rig slot == disabled: no work, bit-exact off path
    const bool active = enable() && master_gain() > 0.f &&
        ALCineLightRigManager::instance().enabledMask() != 0;
    const F32 mode = active ? ((debug_rim_only() || debug_view() == 1) ? 2.f : 1.f) : 0.f;
    const LLVector4 bind_state(
        active ? llclamp(finiteOr(master_gain, 0.f), 0.f, 4.f) : 0.f,
        active ? llclamp(finiteOr(smoothness, 0.f), 0.f, 1.f) : 0.f,
        active ? llclamp(finiteOr(anti_shimmer, 0.f), 0.f, 1.f) : 0.f,
        mode);
    if (gGL.setRigRimActive(active, bind_state))
    {
        // bindDeferredShaderFast() skips bindGlobals(). Invalidate every
        // program when a bindGlobals-owned value changes. Per-frame aux/sil
        // validity is refreshed directly by bindRigRimAux on every fast bind.
        for (LLGLSLShader* shader : LLGLSLShader::sInstances)
        {
            if (shader)
            {
                shader->mCanBindFast = false;
            }
        }
    }
    return active;
}

bool ALCineRigRim::isForwardAlphaProgram(LLGLSLShader& shader)
{
    return resolveForwardAlpha(shader, false);
}

// [RigRim]
S32 ALCineRigRim::modeForCurrentPass(bool forward_alpha)
{
    if (!isEnabled())
    {
        return 0;
    }
    if (LLPipeline::sRenderingHUDs)
    {
        return 0;
    }
    static LLCachedControl<bool> include_probes(gSavedSettings, "CineRigRimIncludeProbes", false);
    if (gCubeSnapshot && !include_probes())
    {
        return 0;
    }
    static LLCachedControl<bool> include_alpha(gSavedSettings, "CineRigRimIncludeAlpha", true);
    if (forward_alpha && !include_alpha())
    {
        return 0;
    }
    static LLCachedControl<bool> debug_rim_only(gSavedSettings, "CineRigRimDebugRimOnly", false);
    static LLCachedControl<U32> debug_view(gSavedSettings, "CineRigRimDebugView", 0);
    return (debug_rim_only() || debug_view() == 1) ? 2 : 1;
}

// [RigRim]
void ALCineRigRim::bindGlobals(LLGLSLShader& shader, bool forward_alpha)
{
    if (shader.getUniformLocation(LLShaderMgr::RIG_RIM_MODE) < 0)
    {
        return; // this program does not link deferredUtil.glsl
    }

    // [RigRim] P1-A fix: derive forward-alpha from the program itself instead
    // of trusting the caller's flag. LLGLSLShader program objects are SHARED
    // between the deferred G-buffer pass and the forward alpha pass (e.g.
    // materialF / pbralphaF / sharedActorFxPbrF / alphaF do both), and
    // bindDeferredShaderFast()'s slow path re-invokes bindDeferredShader()
    // -> bindGlobals(shader) [default forward_alpha=false] on the very next
    // bind of a shader that prepare_alpha_shader() just bound with true
    // (whenever mCanBindFast was reset), silently overwriting the mode.
    // Discriminator: only the four forward-lit programs that also carry
    // rig_rim_lights[8] declare light_position[8] (verified against every
    // deferred/*.glsl source: pointLightF/spotLightF/multiPointLightF do not
    // declare it), so any program that reaches here AND declares
    // light_position is unambiguously a forward-lit program. The explicit
    // caller flag is kept as an OR so callers can still force it.
    // [RigRim] Round-2 P1 fix: the LIGHT_POSITION discriminator cannot tell
    // an opaque/masked Shared Actor FX draw from a blended one (see
    // isNonBlendSharedActorFx() above) — resolveForwardAlpha() overrides it
    // back to false for the known-opaque/mask program instances so
    // CineRigRimIncludeAlpha only ever gates genuinely alpha-blended
    // surfaces.
    forward_alpha = resolveForwardAlpha(shader, forward_alpha);

    const S32 mode = modeForCurrentPass(forward_alpha);
    shader.uniform1i(LLShaderMgr::RIG_RIM_MODE, mode);
    if (mode == 0)
    {
        // [TronT1] Still update the Tron tint uniforms even when the base
        // rim is off: bindAnimated() has its own gate (isEnabledMaster() &&
        // CineRigRimTronMode != 0 && modeForCurrentPass(...) != 0) and
        // always uploads mode 0 in that case, so this is a cheap no-op, not
        // a behaviour change.
        bindAnimated(shader);
        return; // the shader never reads rig_rim_globals when mode 0
    }

    // Forward alpha has a local RigRimPixel. Upload the two sampler-free
    // quality terms here; bindRigRimAux repeats this on every fast bind so
    // probes, mirrors and Prism remain current without an auxiliary pass.
    if (forward_alpha)
    {
        static LLCachedControl<F32> smoothness(gSavedSettings, "CineRigRimSmoothness", 0.f);
        static LLCachedControl<F32> anti_shimmer(gSavedSettings, "CineRigRimAntiShimmer", 0.f);
        shader.uniform4f(LLShaderMgr::RIG_RIM_QUALITY,
            llclamp(finiteOr(smoothness, 0.f), 0.f, 1.f),
            llclamp(finiteOr(anti_shimmer, 0.f), 0.f, 1.f),
            0.f, 0.f);
    }

    static LLCachedControl<F32> master_gain(gSavedSettings, "CineRigRimMasterGain", 1.f);
    static LLCachedControl<F32> roughness_soften(gSavedSettings, "CineRigRimRoughnessSoften", 0.5f);
    static LLCachedControl<F32> back_softness(gSavedSettings, "CineRigRimBackSoftness", 0.5f);
    static LLCachedControl<F32> tint(gSavedSettings, "CineRigRimTint", 0.25f);

    // [RigRim] P2-2: sanitize before clamp (llclamp passes NaN through).
    shader.uniform4f(LLShaderMgr::RIG_RIM_GLOBALS,
                     llclamp(finiteOr(master_gain, 1.f), 0.f, 4.f),
                     llclamp(finiteOr(roughness_soften, 0.5f), 0.f, 1.f),
                     llclamp(finiteOr(back_softness, 0.5f), 0.01f, 2.f),
                     llclamp(finiteOr(tint, 0.25f), 0.f, 1.f));

    // [RigRim] Rim shadow (spotLightF only; location -1 elsewhere is a no-op).
    static LLCachedControl<F32> rim_shadow(gSavedSettings, "CineRigRimShadow", 1.f);
    shader.uniform1f(LLShaderMgr::RIG_RIM_SHADOW, llclamp(finiteOr(rim_shadow, 1.f), 0.f, 1.f));

    // [TronT1] Tron tint (after RIG_RIM_SHADOW, per the T1 contract).
    bindAnimated(shader);
}

// [TronT1]
void ALCineRigRim::bindAnimated(LLGLSLShader& shader)
{
    // [RelightQ] This shared full/fast/forward bind hook runs even with Rim off.
    // Limits are live uniforms, independent of light hashes and shader reloads.
    static LLCachedControl<bool> shape_enabled(gSavedSettings, "RenderLightShapeEnable", false);
    gGL.setLightShapeActive(shape_enabled());
    // Always refresh the master, including on fast binds and while disabled.
    // Shape arrays and limits remain untouched while off.
    shader.uniform1i(LLShaderMgr::RELIGHT_ENABLED, (shape_enabled() ? 1 : 0) | (gPipeline.areaShadowsActive() ? 2 : 0));
    static const LLStaticHashedString limits("relight_limits");
    if (shape_enabled() && shader.getUniformLocation(limits) >= 0)
    {
        static LLCachedControl<F32> near_floor(gSavedSettings, "RenderLocalLightNearFloor", .1f);
        static LLCachedControl<F32> max_boost(gSavedSettings, "RenderLocalLightPhysicalMaxBoost", 4.f);
        const F32 floor = near_floor(), boost = max_boost();
        shader.uniform2f(limits, std::isfinite(floor) ? llclamp(floor, .0001f, 10.f) : .1f,
                         std::isfinite(boost) ? llclamp(boost, 0.f, 100.f) : 4.f);
    }
    // .w in the per-light shape is the area-shadow channel, including forward
    // paths. Highlight softness is independent of both diffuse and visibility.
    static const LLStaticHashedString highlight_scale("relight_highlight_scale");
    if (shape_enabled() && shader.getUniformLocation(highlight_scale) >= 0)
    {
        static LLCachedControl<F32> scale(gSavedSettings, "RenderLocalLightHighlightScale", 1.f);
        const F32 value = scale();
        shader.uniform1f(highlight_scale, std::isfinite(value) ? llclamp(value, 0.f, 1.f) : 1.f);
    }
    static const LLStaticHashedString legacy_exponent("relight_legacy_exponent");
    if (shape_enabled() && shader.getUniformLocation(legacy_exponent) >= 0)
    {
        static LLCachedControl<F32> exponent(gSavedSettings, "RenderSpecularExponent", 368.f);
        const F32 value = exponent();
        shader.uniform1f(legacy_exponent, std::isfinite(value) ? llmax(value, 0.f) : 368.f);
    }

    if (shader.getUniformLocation(LLShaderMgr::RIG_RIM_TRON) < 0)
    {
        return; // this program does not link deferredUtil.glsl
    }

    static LLCachedControl<S32> tron_mode_setting(gSavedSettings, "CineRigRimTronMode", 0);
    static LLCachedControl<F32> tron_mix_setting(gSavedSettings, "CineRigRimTronMix", 1.f);
    static LLCachedControl<S32> tron_color_source_setting(gSavedSettings, "CineRigRimTronColorSource", 0);

    const S32 tron_mode_wanted = std::clamp((S32)tron_mode_setting(), 0, 2);

    // [TronT1 P2-8 fix] Short-circuit BEFORE modeForCurrentPass() when the
    // tint isn't even wanted -- modeForCurrentPass()/resolveForwardAlpha()
    // do real per-call work (several LLCachedControl reads, the
    // isNonBlendSharedActorFx() address-table scan, gCubeSnapshot/HUD
    // checks), and this function runs on every fast bind of every
    // Rig-Rim-capable program every frame, so paying that cost just to
    // discover mode should be 0 is pure waste on the overwhelmingly common
    // "Tron not in use" path. mode stays 0 here exactly as it would have
    // after the old unconditional AND-chain, so the off-path upload below
    // (rig_rim_tron.x == 0) is unchanged.
    S32 mode = 0;
    if (tron_mode_wanted != 0 && ALTron::isEnabledMaster())
    {
        // modeForCurrentPass(...) != 0 in the T1 contract: whether Rig Rim
        // itself is active for THIS program/pass, using the same
        // forward-alpha discriminator bindGlobals() uses (no caller hint
        // available here, so hint=false -- resolveForwardAlpha() still
        // derives it from the program itself via LIGHT_POSITION /
        // isNonBlendSharedActorFx).
        const bool rig_rim_active = modeForCurrentPass(resolveForwardAlpha(shader, false)) != 0;
        mode = rig_rim_active ? tron_mode_wanted : 0;
    }

    // Always upload the mode (including 0) so a program that was fast-bound
    // while the tint was on goes back to the off path the next frame.
    shader.uniform4f(LLShaderMgr::RIG_RIM_TRON,
                     (F32)mode,
                     llclamp(finiteOr((F32)tron_mix_setting(), 1.f), 0.f, 1.f),
                     mode == 2 ? llmax(ALTron::pulseMul(), 0.f) : 1.f,
                     0.f);
    if (mode != 0)
    {
        const ALTron::Palette pal = ALTron::palette();
        LLColor3 c = pal.mPrimary;
        switch (std::clamp((S32)tron_color_source_setting(), 0, 2))
        {
            case 1: c = pal.mSecondary; break;
            case 2: c = pal.mAccent;    break;
            default: break;
        }
        shader.uniform4f(LLShaderMgr::RIG_RIM_TRON_COLOR, c.mV[0], c.mV[1], c.mV[2], 0.f);
    }
}

// [RigRim]
LLVector4 ALCineRigRim::paramsForVolume(const LLVOVolume* volume)
{
    static const LLVector4 zero(0.f, 0.f, 0.f, 0.f);
    // [RigRim] P2-1: cheap off-path short-circuit. isEnabled() is a single
    // bool + F32 setting read plus enabledMask(), far cheaper than walking
    // every lit slot's projector/omni ids below, and this is called once per
    // visible local light per frame.
    if (!isEnabled())
    {
        return zero;
    }
    LLVector4 out = zero;
    if (volume && volume->isCineRigEmitter() &&
        ALCineLightRigManager::instance().rigRimParamsFor(volume->getID(), out))
    {
        return out;
    }
    return zero;
}

// [RigRim]
LLVector4 ALCineRigRim::makeParams(F32 gain, F32 sharpness, F32 wrap, F32 back_bias)
{
    // [RigRim] P2-2: sanitize before clamp (llclamp passes NaN through
    // unchanged, and a NaN component would still reach a shader uniform).
    // [RigRim] Round-2 P2-E: sharpness/wrap/back-bias fall back to the same
    // centralized contract defaults as ALCineLightRigParamBlob::fromLLSD()
    // and the Light struct's own in-class defaults. `gain` deliberately
    // stays a plain 0.f (not ALCineLightRigParamBlob::defaultRimGain(i)):
    // this function has no light index to look up, so "no rim" is the only
    // safe generic fallback for a NaN gain regardless of which light it
    // came from.
    return LLVector4(
        llclamp(finiteOr(gain, 0.f), 0.f, 4.f),
        llclamp(finiteOr(sharpness, ALCineLightRigParamBlob::kDefaultRimSharpness), 0.25f, 8.f),
        llclamp(finiteOr(wrap, ALCineLightRigParamBlob::kDefaultRimWrap), 0.f, 1.f),
        llclamp(finiteOr(back_bias, ALCineLightRigParamBlob::kDefaultRimBackBias), -1.f, 1.f));
}

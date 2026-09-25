/**
 * @file alcinerigrim.h
 * @brief Rig Rim Light ("rig rim"): settings -> shader uniforms.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * [RigRim] Phase A. Adds a per-pixel grazing-angle rim term to the Cine
 * Light Rig's own local lights (deferred point/spot/multi-light and forward
 * alpha), additive on top of each light's own diffuse+specular. The shader
 * side (deferredUtil.glsl / spotLightF.glsl / pointLightF.glsl /
 * multiPointLightF.glsl / pbralphaF.glsl / sharedActorFxPbrF.glsl /
 * alphaF.glsl / materialF.glsl) and the reserved uniforms in llshadermgr.h
 * are implemented separately; this file is the C++ bridge from settings /
 * the light rig's per-light state to those uniforms.
 *
 * Default OFF. When disabled (or master gain <= 0, or no rig slot is lit)
 * rig_rim_mode is uploaded as 0 and every shader path takes a bit-exact
 * no-op early-out.
 */

#pragma once

#ifndef AL_CINERIGRIM_H
#define AL_CINERIGRIM_H

#include "v4math.h"

class LLGLSLShader;
class LLVOVolume;

namespace ALCineRigRim
{
    // RENDER-THREAD ONLY: also pushes the result into gGL.setRigRimActive()
    // (LLRender light-sync gate). Do not call from UI/status code.
    // CineRigRimEnabled && master gain > 0 &&
    // ALCineLightRigManager::instance().enabledMask() != 0.
    bool isEnabled();

    // 0 / 1 / 2 for the CURRENT pass:
    //   !isEnabled()                                   -> 0
    //   LLPipeline::sRenderingHUDs                     -> 0
    //   gCubeSnapshot && !CineRigRimIncludeProbes      -> 0   (reflection/irradiance probe capture)
    //   forward_alpha && !CineRigRimIncludeAlpha       -> 0
    //   CineRigRimDebugRimOnly                         -> 2
    //   else                                           -> 1
    // (Not gated on sImpostorRender: impostors are lit by the same lights
    // and should match.)
    S32 modeForCurrentPass(bool forward_alpha);

    // Upload RIG_RIM_MODE (+ RIG_RIM_GLOBALS when mode != 0) to a freshly
    // bound shader. Early-returns when the program does not link
    // deferredUtil.glsl (getUniformLocation(RIG_RIM_MODE) < 0).
    void bindGlobals(LLGLSLShader& shader, bool forward_alpha = false);

    // Per-light params for a drawable's LLVOVolume. Returns (0,0,0,0) unless
    // volume && volume->isCineRigEmitter() and the manager resolves it to a
    // lit rig fixture.
    LLVector4 paramsForVolume(const LLVOVolume* volume);

    // Builds the per-light vec4 from the four per-light settings/blob
    // fields, clamped: (gain 0..4, sharpness 0.25..8, wrap 0..1,
    // back bias -1..1).
    LLVector4 makeParams(F32 gain, F32 sharpness, F32 wrap, F32 back_bias);
}

#endif // AL_CINERIGRIM_H

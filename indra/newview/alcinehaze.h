/**
 * @file alcinehaze.h
 * @brief Cinematic Depth Atmosphere ("cine haze"): settings -> shader uniforms.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * Distance / height haze that reduces contrast toward an atmosphere colour,
 * composed after the WindLight aerial perspective in linear HDR (before
 * exposure / bloom / tonemap).  The shader side lives in
 * app_settings/shaders/class1/windlight/cineHazeF.glsl and is consumed by the
 * deferred haze pass (hazeF.glsl) and applySkyAndWaterFog() (waterFogF.glsl).
 *
 * Default OFF.  When disabled (or density / strength are zero) the shaders
 * take a uniform-gated early-out and the image is bit-identical to before.
 */

#pragma once

#ifndef AL_CINEHAZE_H
#define AL_CINEHAZE_H

#include <string>

class LLGLSLShader;

namespace ALCineHaze
{
    // Master enable && density > 0 && strength > 0 (the "does any work" gate).
    bool isEnabled();

    // True while the froxel volumetrics master (BDMergeFroxelVolumetrics) is on:
    // that system already applies its own distance + height medium, so the two
    // are mutually exclusive and this layer no-ops.
    bool isSuppressedByFroxel();

    // isEnabled(), not suppressed by froxel, and not a probe / HUD / impostor pass.
    bool isActiveForCurrentPass();

    // Upload the cine-haze uniforms to a freshly bound shader.  Cheap: reads
    // cached settings, derives the camera world height and world-up-in-eye-space
    // from the CURRENT modelview (so probes / mirrors / VCam get their own
    // camera), and relies on LLGLSLShader's value cache to skip redundant
    // glUniform calls.  No-op on programs that do not link cineHazeF.glsl.
    void bind(LLGLSLShader& shader);

    // Lightbox helpers.
    void applyPreset(const std::string& name);   // "subtle"
    void resetToDefaults();
    void setReferenceHeightFromCamera();
}

#endif // AL_CINEHAZE_H

/**
 * @file onLensFiltersF.glsl
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Machinima Viewer Source Code
 * Copyright (C) 2026, Alchemy Machinima
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * $/LicenseInfo$
 */

/*[EXTRA_CODE_HERE]*/

// On-lens filters (Graduated ND + Polarizer) as a standalone PRE-PASS.
//
// Runs on the linear HDR scene buffer after exposure is METERED but before it is
// APPLIED, and before bloom and lens-flare generation, so those optics respect
// the filtered scene the way a real on-lens ND/polarizer does. The filter
// functions themselves live in
// postEffectUtilsF.glsl (attached via mFeatures.hasPostEffects) and are shared
// with colorCorrectF's declarations; only the call site moved here.

out vec4 frag_color;

uniform sampler2D diffuseRect;   // linear HDR scene (read; draw target is the scratch)
uniform sampler2D depthMap;      // deferred hardware depth (sky ~ 1.0)
uniform sampler2D exposureMap;   // 1x1 auto-exposure scale
uniform float     exposure;      // manual exposure knob (RenderExposure)

in vec2 vary_fragcoord;

vec3 applyNightMask(vec3 color, vec2 uv, sampler2D depth);
vec3 applyGradND(vec3 color, vec2 uv, sampler2D depth);
vec3 applyPolarizer(vec3 color, vec2 uv, sampler2D depth, float exposure_scale);

void main()
{
    // Preserve the full texel — alpha carries the prim-glow tag that the bloom
    // extract reads downstream, so only the RGB is filtered.
    vec4 diff = texture(diffuseRect, vary_fragcoord);

    // Effective exposure the tonemap will apply later (manual * auto). The
    // polarizer's glare gate uses this so its threshold matches exposed luma
    // even though this pass runs before exposure is applied.
    float exposure_scale = texture(exposureMap, vec2(0.5, 0.5)).r * exposure;

    // Night Mask runs FIRST: it is a world-lighting fake (not a camera-lens
    // artifact like the ND/polarizer below it), and LLPipeline::applyOnLensFilters
    // snapshots the ReShade raw-scene capture right after this draw, so the
    // mask must already be baked into `diff` by the time that capture happens.
    diff.rgb = applyNightMask(diff.rgb, vary_fragcoord, depthMap);
    diff.rgb = applyGradND(diff.rgb, vary_fragcoord, depthMap);
    diff.rgb = applyPolarizer(diff.rgb, vary_fragcoord, depthMap, exposure_scale);
    frag_color = diff;
}

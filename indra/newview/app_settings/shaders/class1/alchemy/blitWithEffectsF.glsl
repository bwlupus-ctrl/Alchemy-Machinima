/**
 * @file blitWithEffectsF.glsl
 *
 * Final display-space blit. All post-effects (vignette, film grain, CVD
 * compensation, dither, preview overlays) live in postEffectUtilsF.glsl
 * and are auto-linked here via mFeatures.hasPostEffects; this file is just
 * the orchestrator that sequences them in the canonical order.
 *
 * Order matters:
 *   vignette  → CVD compensation → film grain → dither → preview
 * Vignette runs before CVD so the accessibility pass considers the final
 * darkening. Film grain runs after CVD so the grain stays neutral in tint.
 * Dither runs last (before preview) so quantization is resolved against
 * the true final color. It is only active here when the post chain is
 * HDR; in the 8-bit case, colorCorrectF dithers earlier instead. Preview overlays are
 * debug-only.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright © 2026, Rye <rye@alchemyviewer.org>
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

in vec2 vary_fragcoord;
out vec4 frag_color;

// =============================================================================
// Uniforms
// =============================================================================

uniform sampler2D diffuseRect;      // Linear Rec.709 / linear-sRGB.
uniform sampler2D depthMap;

// [Ultimate Diopter / Cine Fisheye] warped present depth for ReShade.
uniform sampler2D diopterWarpMap;   // RG16F: diopter output uv -> scene uv
uniform vec4 depth_warp_params;     // x diopter active (0/1), y fisheye active (0/1), z/w reserved
uniform vec4 fisheye_params;        // x k1, y k2, z vignette radius, w vignette softness
uniform vec4 fisheye_params2;       // x/y lens center, z zoom, w reserved
uniform vec2 uResolution;           // shared with postEffectUtilsF.glsl

// =============================================================================
// Forward Declarations
// =============================================================================

vec3 clampHDRRange(vec3 color);

// From postEffectUtilsF.glsl (auto-linked via mFeatures.hasPostEffects).
vec3 applyVignette(vec3 color, vec2 uv);
vec3 applyCVDCompensation(vec3 color);
vec3 applyFilmGrain(vec3 color, vec2 fragCoord);
#ifdef DITHER
vec3 applyDither(vec3 color, vec2 fragCoord);
#endif
vec3 applyPreview(vec3 color);

// =============================================================================
// Main
// =============================================================================
void main()
{
    // === DISPLAY SPACE =======================================================

    vec4 diff = texture(diffuseRect, vary_fragcoord.xy);

    diff.rgb = applyVignette(diff.rgb, vary_fragcoord.xy);
    diff.rgb = applyCVDCompensation(diff.rgb);
    diff.rgb = applyFilmGrain(diff.rgb, gl_FragCoord.xy);
#ifdef DITHER
    diff.rgb = applyDither(diff.rgb, gl_FragCoord.xy);
#endif
    diff.rgb = applyPreview(diff.rgb);   // debug only — no-op when uPreviewMode == 0

    diff.rgb = clampHDRRange(diff.rgb);
    frag_color = diff;

    // Depth must ride through the same lens warps as color, or ReShade's
    // depth-driven effects outline the unwarped scene. Order: fisheye
    // mapping FIRST (present pixel -> fisheye source = diopter output
    // space), then the diopter warp map (diopter output -> scene space).
    vec2 duv = vary_fragcoord.xy;
    if (depth_warp_params.y > 0.5)
    {
        // exact forward source-uv mapping from cineFisheyeF.glsl; the color
        // pass blacks out-of-range samples, depth just clamps instead.
        // aspect comes from depth_warp_params.z (the actual post-chain
        // target) so it is byte-identical to the fisheye pass's screen_res
        // aspect even under a resolution divisor.
        float aspect = max(depth_warp_params.z, 1e-4);
        vec2 c = duv * 2.0 - 1.0 - fisheye_params2.xy * 2.0;
        c.x *= aspect;
        float r = length(c);
        float rn = r / max(fisheye_params.z, 0.05);
        float rn2 = rn * rn;
        float g = (1.0 + fisheye_params.x * rn2 +
                   fisheye_params.y * rn2 * rn2) /
                  (1.0 + fisheye_params.x + fisheye_params.y);
        vec2 src = c * g / max(fisheye_params2.z, 0.25);
        duv = clamp(vec2(src.x / aspect, src.y) * 0.5 + 0.5 +
                    fisheye_params2.xy, 0.0, 1.0);
    }
    if (depth_warp_params.x > 0.5)
    {
        duv = clamp(texture(diopterWarpMap, duv).rg, 0.0, 1.0);
    }
    gl_FragDepth = texture(depthMap, duv).r;
}

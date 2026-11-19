/**
 * @file luminanceF.glsl
 *
 * $LicenseInfo:firstyear=2023&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2023, Linden Research, Inc.
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
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */


/*[EXTRA_CODE_HERE]*/

// take a luminance sample of diffuseRect and emissiveRect

out vec4 frag_color;

in vec2 vary_fragcoord;

uniform sampler2D diffuseRect;
uniform sampler2D emissiveRect;
uniform sampler2D normalMap;
uniform float diffuse_luminance_scale;

// [Night Mask B1] emissiveRect (bloomMip[0]) here is LAST frame's bloom, which
// generateBloomHDR() builds from the (possibly masked) scene AFTER this same
// luminance pass runs each frame. When Night Mask is active, that bloom was
// generated from the darkened scene, so folding it into metering indirectly
// suppresses next frame's exposure compensation and exposure creeps upward
// over time. LLPipeline::generateLuminance uploads a value exponentially
// ramped toward 0 while Night Mask is active (from the same FINAL
// will-render resolve applyOnLensFilters reads via mActive) and back toward 1
// otherwise — B1(b): ramped, not snapped, so a Night Mask toggle can't pump
// exposure in a single frame. Metering is unaffected (scale settles at 1)
// when the feature is off.
uniform float night_mask_bloom_scale;

float lum(vec3 col)
{
    vec3 l = vec3(0.2126, 0.7152, 0.0722);
    return dot(l, col);
}

void main()
{
    vec2 tc = vary_fragcoord*0.6+0.2;
    tc.y -= 0.1; // HACK - nudge exposure sample down a little bit to favor ground over sky
    vec3 c = texture(diffuseRect, tc).rgb;

    vec4  norm         = texture(normalMap, tc);

    if (!GET_GBUFFER_FLAG(norm.w, GBUFFER_FLAG_HAS_HDRI) &&
        !GET_GBUFFER_FLAG(norm.w, GBUFFER_FLAG_SKIP_ATMOS))
    {
        // Apply the diffuse luminance scale to objects but not the sky
        // Prevents underexposing when looking at bright environments
        // while still allowing for realistically bright skies.
        c *= diffuse_luminance_scale;
    }

    c += texture(emissiveRect, tc).rgb * night_mask_bloom_scale;

    float L = lum(c);
    frag_color = vec4(max(L, 0.0));
}


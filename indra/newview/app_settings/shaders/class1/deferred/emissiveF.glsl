/**
 * @file emissiveF.glsl
 *
 * $LicenseInfo:firstyear=2005&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2005, Linden Research, Inc.
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

out vec4 frag_color;

in vec4 vertex_color;
in float vertex_alpha;
in vec2 vary_texcoord0;
#ifdef HAS_ACTOR_FX
uniform int actorFxUseCoverageAlpha;
// Shared Cover replays authored legacy glow separately from its treatment
// bloom.  Keep dissolve coverage active, but omit actorFxEmissive so the
// actorghost glow-only pass remains the single synthetic contributor.
uniform int actorFxSharedAuthoredOnly;
uniform float actorFxSharedGlowAttenuation;
uniform float actorFxSharedMinimumAlpha;
// 0: no cutoff, 1: texture alpha, 2: texture * vertex alpha,
// 3: texture alpha with the legacy-material 1/512 cutoff bias.
uniform int actorFxSharedAlphaCutoffMode;
#endif
#ifdef HAS_ACTOR_FX
vec3 actorFxEmissive(vec3 authored_emissive, vec3 styled_color);
float actorFxAuthoredMaterialResponse();
bool actorFxDissolveEnabled();
float actorFxBeautyDissolveCoverage();
#endif

void main()
{
#ifdef HAS_ACTOR_FX
    if (actorFxDissolveEnabled() && actorFxBeautyDissolveCoverage() < 0.0)
    {
        discard;
    }
#endif
    // NOTE: when this shader is used, only alpha is being written to
    float diffuse_alpha = diffuseLookup(vary_texcoord0.xy).a;
#ifdef HAS_ACTOR_FX
    float cutoff_alpha = actorFxSharedAlphaCutoffMode == 2
        ? diffuse_alpha * vertex_alpha : diffuse_alpha;
    float cutoff_bias = actorFxSharedAlphaCutoffMode == 3
        ? (1.0 / 512.0) : 0.0;
    if (actorFxSharedAuthoredOnly != 0 &&
        actorFxSharedAlphaCutoffMode != 0 &&
        cutoff_alpha < actorFxSharedMinimumAlpha - cutoff_bias)
    {
        discard;
    }
#endif
    float a = diffuse_alpha * vertex_color.a;
#ifdef HAS_ACTOR_FX
    // PASS_ALPHA has no duplicate glow DrawInfo. Shared Cover therefore replays
    // its base range through this authored-only path, where native BLEND
    // coverage is diffuse texture alpha * diffuse vertex alpha. Ordinary glow
    // sidecars and the historical native path keep their existing contract.
    if (actorFxSharedAuthoredOnly != 0 && actorFxUseCoverageAlpha != 0)
    {
        a *= vertex_alpha;
    }
    a *= (1.0 - clamp(actorFxSharedGlowAttenuation, 0.0, 1.0))
        * actorFxAuthoredMaterialResponse();
    if (actorFxSharedAuthoredOnly == 0)
    {
        vec3 fx_emissive = actorFxEmissive(vec3(0.0), vec3(1.0));
        float fx_coverage = actorFxUseCoverageAlpha != 0
            ? diffuse_alpha * vertex_alpha : 1.0;
        a += max(max(fx_emissive.r, fx_emissive.g), fx_emissive.b) * fx_coverage;
    }
#endif
    frag_color = max(vec4(0, 0, 0, a), vec4(0));
}

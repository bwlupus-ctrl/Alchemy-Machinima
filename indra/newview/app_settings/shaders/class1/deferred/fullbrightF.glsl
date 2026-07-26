/**
 * @file deferred/fullbrightF.glsl
 *
 * $LicenseInfo:firstyear=2007&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2007, Linden Research, Inc.
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

#ifdef HAS_VISIBLE_DIFFUSE
layout(location = 0) out vec4 frag_color;
layout(location = 1) out vec4 visible_diffuse;
layout(location = 2) out vec2 surface_coverage;
#else
out vec4 frag_color;
#endif

#if !defined(HAS_DIFFUSE_LOOKUP)
uniform sampler2D diffuseMap;
#endif

in vec3 vary_position;
in vec4 vertex_color;
in vec2 vary_texcoord0;

vec3 srgb_to_linear(vec3 cs);
vec3 linear_to_srgb(vec3 cl);

#ifdef HAS_ALPHA_MASK
uniform float minimum_alpha;
#endif

#ifdef IS_ALPHA
uniform vec4 waterPlane;
void waterClip(vec3 pos);
void calcAtmosphericVars(vec3 inPositionEye, vec3 light_dir, float ambFactor, out vec3 sunlit, out vec3 amblit, out vec3 additive,
                         out vec3 atten);
vec4 applySkyAndWaterFog(vec3 pos, vec3 additive, vec3 atten, vec4 color);
#endif

void mirrorClip(vec3 pos);

void main()
{
    mirrorClip(vary_position);
#ifdef IS_ALPHA
    waterClip(vary_position.xyz);
#endif

#ifdef HAS_DIFFUSE_LOOKUP
    vec4 color = diffuseLookup(vary_texcoord0.xy);
#else
    vec4 color = texture(diffuseMap, vary_texcoord0.xy);
#endif

    float final_alpha = color.a * vertex_color.a;

#ifdef HAS_ALPHA_MASK
    if (color.a < minimum_alpha)
    {
        discard;
    }
#endif

    color.rgb *= vertex_color.rgb;

    vec3 pos = vary_position;

    color.a = final_alpha;
    vec3 visible_diffuse_color = color.rgb;
#ifndef IS_HUD
    color.rgb = srgb_to_linear(color.rgb);
    visible_diffuse_color = color.rgb;
#ifdef IS_ALPHA

    vec3 sunlit;
    vec3 amblit;
    vec3 additive;
    vec3 atten;
    calcAtmosphericVars(pos.xyz, vec3(0), 1.0, sunlit, amblit, additive, atten);

    color.rgb = applySkyAndWaterFog(pos, additive, atten, color).rgb;

#endif

#endif

    frag_color = max(color, vec4(0));
#ifdef HAS_VISIBLE_DIFFUSE
    // A legacy fullbright surface has NO diffuse-lighting response. Its colour
    // is unlit OUTPUT, not diffuse reflectance -- publishing it as albedo tells
    // a GI consumer that a glowing red candle is a near-perfect red diffuse
    // REFLECTOR, so it bounces red light in proportion to whatever illuminates
    // it, while the consumer separately gathers the emitted red from the
    // backbuffer.
    //
    // Zero diffuse with K from coverage is the same statement the seed already
    // makes about metals (visibleDiffuseSeedF.glsl: diffuse *= 1.0 - orm.b):
    // it is not a missing measurement, it is this renderer's declared BRDF for
    // the surface class. K=0 ("no answer") would be more cautious but hands the
    // pixel back to the incumbent estimate, which is derived from a backbuffer
    // that CONTAINS the emission and would reconstruct the same bad albedo.
    //
    // NOTE: legacy fullbright gives no independent emission/reflectance
    // parameters, so treating its texture as reflectance would be inventing a
    // BRDF. PBR handles emit-and-reflect correctly and is unaffected -- see
    // pbralphaF.glsl, which publishes diffuseColor and never colorEmissive.
    // S4: an ALPHA-MASK fragment that survived the discard above is fully
    // present -- it is a binary cutout, not a blend. Publishing K = final_alpha
    // there reported a fully-known opaque pixel as only partially known.
    // Blended fullbright still reports its true fractional coverage.
// S2 + S4. Blending is DISABLED for this pool (LLDrawPoolSimple leaves it off
// and LLDrawPoolFullbright only changes factors), so a surviving opaque
// fullbright fragment OVERWRITES the beauty pixel completely -- whatever its
// texture alpha happens to be. Publishing K = final_alpha there contradicted
// attachment 0, which had already treated the pixel as fully opaque, and it
// made K draw-order dependent because the overwrite is unblended.
// Alpha-MASK fragments that survive their discard are likewise binary.
// Only genuinely alpha-BLENDED fullbright carries fractional coverage.
//
// ⛔ The other way to make this deterministic -- enabling GL_BLEND for the pool
// -- was tried and caused a visible regression: opaque surfaces with alpha in
// their textures went see-through. Do not go near the global blend state.
#if defined(HAS_ALPHA_MASK) || !defined(IS_ALPHA)
    float sl_known_coverage = 1.0;
#else
    float sl_known_coverage = final_alpha;
#endif

    visible_diffuse = vec4(vec3(0.0), sl_known_coverage);
    surface_coverage = vec2(0.0, sl_known_coverage);
#endif
}

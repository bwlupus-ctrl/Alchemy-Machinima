/**
 * @file class1\deferred\projectorVolumetricBloomFeedF.glsl
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
 *
 * [BDMerge G3.3 Phase 3 item 4] Projector-volumetric bloom feed. Phase 1
 * deliberately composites the shaft AFTER the bloom pyramid is generated from the
 * clean scene, so shafts never skew the scene bloom. This pass adds a SEPARATE,
 * controlled contribution: it samples the half-resolution shaft (mProjVolHalf,
 * bound to diffuseMap), applies a cheap 3x3 tent blur to soften it into a halo,
 * scales it by projvol_bloom_feed, and is additively composited into the HDR bloom
 * pyramid base (bloomMip[0]) by the C++ side. So only a deliberate, gated amount of
 * the shaft ever reaches bloom - default feed 0 => this pass is never run and the
 * scene bloom is byte-for-byte the Phase 1 look.
 */

/*[EXTRA_CODE_HERE]*/

out vec4 frag_color;

in vec2 vary_fragcoord;

uniform sampler2D diffuseMap;       // the half-res marched shaft (rgb additive)
uniform vec2  bloom_texel_size;     // 1 / half-res dimensions (blur tap spacing)
uniform float projvol_bloom_feed;   // controlled feed scale (0 = off)

void main()
{
    vec2 uv = vary_fragcoord.xy;
    vec2 t  = bloom_texel_size;

    // 3x3 tent blur (same weights as the bloom upsample) so the fed shaft reads as
    // a soft glow rather than a sharp copy of the beam.
    vec3 r  = texture(diffuseMap, uv + t * vec2(-1.0, -1.0)).rgb * (1.0 / 16.0);
    r += texture(diffuseMap, uv + t * vec2( 0.0, -1.0)).rgb * (2.0 / 16.0);
    r += texture(diffuseMap, uv + t * vec2( 1.0, -1.0)).rgb * (1.0 / 16.0);
    r += texture(diffuseMap, uv + t * vec2(-1.0,  0.0)).rgb * (2.0 / 16.0);
    r += texture(diffuseMap, uv                       ).rgb * (4.0 / 16.0);
    r += texture(diffuseMap, uv + t * vec2( 1.0,  0.0)).rgb * (2.0 / 16.0);
    r += texture(diffuseMap, uv + t * vec2(-1.0,  1.0)).rgb * (1.0 / 16.0);
    r += texture(diffuseMap, uv + t * vec2( 0.0,  1.0)).rgb * (2.0 / 16.0);
    r += texture(diffuseMap, uv + t * vec2( 1.0,  1.0)).rgb * (1.0 / 16.0);

    frag_color = vec4(r * projvol_bloom_feed, 0.0);
}

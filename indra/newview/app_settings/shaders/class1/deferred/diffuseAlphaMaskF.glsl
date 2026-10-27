bool gbufferTagPreferred();
/**
 * @file diffuseAlphaMaskF.glsl
 *
 * $LicenseInfo:firstyear=2011&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2011, Linden Research, Inc.
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

out vec4 frag_data[4];

uniform float minimum_alpha;

uniform sampler2D diffuseMap;

in vec3 vary_position;

in vec3 vary_normal;
in vec4 vertex_color;
in vec2 vary_texcoord0;

void mirrorClip(vec3 pos);

uniform int avatar_tag_draw;
float gbufferAvatarFlag(float flag);
vec4 encodeNormal(vec3 n, float env, float gbuffer_flag);
float gbufferAvatarFlag(float flag);   // [TronA0] globalF.glsl


// WP2 material contract: after Actor FX, before G-buffer packing/forward lighting.
bool weatherMaterialActive();
void weatherMaterialLinear(vec3 p, inout vec3 n, inout vec3 c, inout float r, float m);
void weatherMaterialLegacy(vec3 p, inout vec3 n, inout vec3 c, inout vec3 s, inout float g);
void weatherMaterialLegacyLinear(vec3 p, inout vec3 n, inout vec3 c, inout vec3 s, inout float g);

void main()
{
    mirrorClip(vary_position);

    vec4 col = texture(diffuseMap, vary_texcoord0.xy) * vertex_color;

    if (col.a < minimum_alpha)
    {
        discard;
    }

    vec3 nvn = normalize(vary_normal);
    // Match forward alpha when a card is rendered with culling disabled.
    nvn *= gl_FrontFacing ? 1.0 : -1.0;
    vec4 weather_spec = vec4(0.0);
#if defined(HAS_WEATHER_MATERIAL) && !defined(HAS_SKIN) && !defined(AVATAR_GEOMETRY) && !defined(IS_AVATAR_SKIN)
    if (!gbufferTagPreferred())
    {
    weatherMaterialLegacy(vary_position, nvn, col.rgb, weather_spec.rgb, weather_spec.a);
    }
#endif
    frag_data[0] = vec4(col.rgb, 0.0);
    frag_data[1] = weather_spec; // spec
#ifdef HAS_SKIN
    // [TronA0] rigged mesh: avatar geometry -> tagged flag
    frag_data[2] = encodeNormal(nvn.xyz, 0, gbufferAvatarFlag(GBUFFER_FLAG_HAS_ATMOS));
#else
    frag_data[2] = encodeNormal(nvn.xyz, 0, (avatar_tag_draw != 0 ? gbufferAvatarFlag(GBUFFER_FLAG_HAS_ATMOS) : GBUFFER_FLAG_HAS_ATMOS));
#endif

#if defined(HAS_EMISSIVE)
    frag_data[3] = vec4(0);
#endif
}


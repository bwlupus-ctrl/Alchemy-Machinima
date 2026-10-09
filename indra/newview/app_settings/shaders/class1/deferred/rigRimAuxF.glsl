/**
 * @file rigRimAuxF.glsl
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

// [RigRimQ] Rig Rim Aux pre-pass (doc/RIGRIM_QUALITY_CONTRACT.md, pass A).
//
// Full-res, MAIN view only, drawn inside renderDeferredLighting after the
// sun/soften passes and before the local-light loop, into rigRimAuxMap
// (RGBA16 unorm):
//   xy  octahedral-encoded SMOOTH normal (eye space): the depth-reconstructed
//       geometric normal (5 depth taps, min-difference per axis), oriented
//       toward the camera; falls back to the shading normal where the
//       reconstruction is degenerate or disagrees with it (back-facing).
//   z   Toksvig |n_bar| of the shading normals in the 5-tap footprint
//       (depth-continuous taps only, so a silhouette never counts as "noisy").
//   w   subject mask 0/1 from rig_rim_aux_params.x:
//         0 = none      -> 1.0 everywhere (world-wide rim; host shows a LOUD status)
//         1 = avatar tag-> GBUFFER_AVATAR_TAG_OF(normal.w) (needs RenderGBufferAvatarTag;
//                          1.0 when the tag is not compiled in, host never selects it then)
//         2 = Actor Mask-> (actorMaskMap bits & actor_mask_bits) != 0
// Sky (depth ~1) writes the neutral pixel: shading normal, |n_bar| = 1, subject 0.
//
// Program: isDeferred (deferredUtil.glsl: depthMap / normalMap / inv_proj /
// screen_res / getDepth / getNorm / getNormRaw / getPositionWithDepth /
// rigRimOctEncode).  Vertex: deferred/postDeferredNoTCV.glsl.
// GL: blend off, depth test off, depth write off, viewport = aux target.

out vec4 frag_color;

in vec2 vary_fragcoord;

uniform vec2 screen_res;

// x subject source (0 none, 1 avatar tag, 2 actor mask), y seed threshold
// (rigRimSilSeedF), z seed softness (rigRimSilSeedF), w debug view (rigRimDebugF)
uniform vec4 rig_rim_aux_params;

// [ActorMask] R8 recipient-bit mask (valid this view only) + the bits to accept
uniform sampler2D actorMaskMap;
uniform int       actor_mask_bits;

float getDepth(vec2 pos_screen);
vec4  getNorm(vec2 screenpos);
vec4  getNormRaw(vec2 screenpos);
vec4  getPositionWithDepth(vec2 pos_screen, float depth);
vec2  rigRimOctEncode(vec3 n);

float rigRimAuxSubject(vec2 uv)
{
    int source = int(rig_rim_aux_params.x + 0.5);
    if (source == 1)
    {
#ifdef GBUFFER_AVATAR_TAG
        return GBUFFER_AVATAR_TAG_OF(getNormRaw(uv).w) ? 1.0 : 0.0;
#else
        return 1.0; // tag not compiled in: world-wide (host reports the fallback)
#endif
    }
    if (source == 2)
    {
        uint b = uint(texelFetch(actorMaskMap, ivec2(gl_FragCoord.xy), 0).r * 255.0 + 0.5);
        return ((b & uint(actor_mask_bits)) != 0u) ? 1.0 : 0.0;
    }
    return 1.0;
}

void main()
{
    vec2  uv   = vary_fragcoord;
    float d_c  = getDepth(uv);
    vec3  n_sh = getNorm(uv).xyz;

    if (d_c >= 0.999999)
    {
        // sky / no geometry: neutral pixel
        frag_color = vec4(rigRimOctEncode(n_sh), 1.0, 0.0);
        return;
    }

    float subject = rigRimAuxSubject(uv);

    vec2  px  = 1.0 / screen_res;
    vec3  p_c = getPositionWithDepth(uv, d_c).xyz;
    float z_c = max(-p_c.z, 1.0e-3);

    // 4 neighbours (clamped to the frame)
    vec2 uv_l = clamp(uv - vec2(px.x, 0.0), vec2(0.0), vec2(1.0));
    vec2 uv_r = clamp(uv + vec2(px.x, 0.0), vec2(0.0), vec2(1.0));
    vec2 uv_d = clamp(uv - vec2(0.0, px.y), vec2(0.0), vec2(1.0));
    vec2 uv_u = clamp(uv + vec2(0.0, px.y), vec2(0.0), vec2(1.0));

    float d_l = getDepth(uv_l);
    float d_r = getDepth(uv_r);
    float d_d = getDepth(uv_d);
    float d_u = getDepth(uv_u);

    vec3 p_l = getPositionWithDepth(uv_l, d_l).xyz;
    vec3 p_r = getPositionWithDepth(uv_r, d_r).xyz;
    vec3 p_d = getPositionWithDepth(uv_d, d_d).xyz;
    vec3 p_u = getPositionWithDepth(uv_u, d_u).xyz;

    // --- smooth geometric normal: min-difference derivative per axis ------
    vec3 dl = p_c - p_l;
    vec3 dr = p_r - p_c;
    vec3 dd = p_c - p_d;
    vec3 du = p_u - p_c;
    vec3 ddx = (abs(dr.z) < abs(dl.z)) ? dr : dl;
    vec3 ddy = (abs(du.z) < abs(dd.z)) ? du : dd;

    vec3  n_geo = cross(ddx, ddy);
    float l2    = dot(n_geo, n_geo);
    if (l2 > 1.0e-20)
    {
        n_geo *= inversesqrt(l2);
        if (dot(n_geo, -p_c) < 0.0)
        {
            n_geo = -n_geo; // face the camera
        }
        if (dot(n_geo, n_sh) < 0.0)
        {
            n_geo = n_sh;   // disagrees with the shading normal (two-sided / normal-map extreme)
        }
    }
    else
    {
        n_geo = n_sh;
    }

    // --- Toksvig length of the shading normals (depth-continuous taps) -----
    float tol   = 0.04 * z_c + 0.02;
    vec3  n_bar = n_sh;
    float w_sum = 1.0;
    if (d_l < 0.999999 && abs(-p_l.z - z_c) < tol) { n_bar += getNorm(uv_l).xyz; w_sum += 1.0; }
    if (d_r < 0.999999 && abs(-p_r.z - z_c) < tol) { n_bar += getNorm(uv_r).xyz; w_sum += 1.0; }
    if (d_d < 0.999999 && abs(-p_d.z - z_c) < tol) { n_bar += getNorm(uv_d).xyz; w_sum += 1.0; }
    if (d_u < 0.999999 && abs(-p_u.z - z_c) < tol) { n_bar += getNorm(uv_u).xyz; w_sum += 1.0; }
    float n_len = clamp(length(n_bar / w_sum), 0.0, 1.0);

    frag_color = vec4(rigRimOctEncode(n_geo), n_len, subject);
}

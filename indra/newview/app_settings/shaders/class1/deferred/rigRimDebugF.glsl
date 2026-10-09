/**
 * @file rigRimDebugF.glsl
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

// [RigRimQ] Developer views (doc/RIGRIM_QUALITY_CONTRACT.md, pass D).
// Full-res REPLACE draw (blend off) over mRT->screen, after the wrap pass,
// when CineRigRimDebugView selects one of the aux-driven views:
//   rig_rim_aux_params.w  2 = silhouette band : seed pixels red, band (near
//                             side) green = prox, far side / rejected blue,
//                             sky black
//                         3 = smooth normal   : n_geo * 0.5 + 0.5
//                         4 = subject mask    : grey
// (1 "rim only" is rig_rim_mode 2 in the light shaders; 5 "wrap only" is the
// wrap pass drawn with blend off and rig_rim_wrap2.z = 1 -- neither uses this
// program.)  Output is linear and goes through the normal tonemap; values are
// kept inside 0..1 so they read the same under any exposure.
//
// Program: isDeferred.  Vertex: deferred/postDeferredNoTCV.glsl.

out vec4 frag_color;

in vec2 vary_fragcoord;

uniform vec4 rig_rim_aux_params;  // w = view id
uniform vec4 rig_rim_sil;

uniform sampler2D rigRimAuxMap;   // same declaration as deferredUtil.glsl (separate unit)
uniform sampler2D rigRimSilMap;

float getDepth(vec2 pos_screen);
vec4  getPositionWithDepth(vec2 pos_screen, float depth);
vec3  rigRimOctDecode(vec2 e);
float rigRimSilBandWidth(float z_view);

const float RIG_RIM_DT_BIG = 1.0e4;

void main()
{
    vec2 uv   = vary_fragcoord;
    int  view = int(rig_rim_aux_params.w + 0.5);
    vec4 a    = textureLod(rigRimAuxMap, uv, 0.0);
    vec3 col  = vec3(0.0);

    if (view == 3)
    {
        col = rigRimOctDecode(a.xy) * 0.5 + 0.5;
    }
    else if (view == 4)
    {
        col = vec3(clamp(a.w, 0.0, 1.0));
    }
    else // 2: silhouette band
    {
        float d = getDepth(uv);
        if (d < 0.999999)
        {
            vec4  s      = textureLod(rigRimSilMap, uv, 0.0);
            vec3  p      = getPositionWithDepth(uv, d).xyz;
            float z_view = max(-p.z, 1.0e-3);
            if (s.w < 0.5)
            {
                col = vec3(1.0, 0.0, 0.0);                 // seed (near side of the edge)
            }
            else if (s.w < RIG_RIM_DT_BIG)
            {
                float w    = max(rigRimSilBandWidth(z_view), 1.0e-3);
                float soft = clamp(rig_rim_sil.y, 0.0, 1.0);
                float band = 1.0 - smoothstep(w * (1.0 - soft), w * (1.0 + soft) + 1.0e-4, s.w);
                float gap  = max(rig_rim_sil.z, 1.0e-4);
                float rel  = abs(z_view - s.z) / max(s.z, 1.0e-3);
                float near_ok = 1.0 - smoothstep(gap, 2.0 * gap, rel);
                col = vec3(0.0, band * near_ok, band * (1.0 - near_ok));
            }
            else
            {
                col = vec3(0.05);                           // geometry, no edge within the window
            }
        }
    }

    frag_color = vec4(col, 1.0);
}

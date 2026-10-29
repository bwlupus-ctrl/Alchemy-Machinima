/**
 * @file blurLightF.glsl
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

out vec4 frag_color;

uniform sampler2D lightMap;

uniform float dist_factor;
uniform float blur_size;
uniform vec2 delta;
uniform vec2 screen_res;
uniform vec3 kern[4];
uniform float kern_scale;

// [ShadowDist P1] Control D: sun-shadow (.r) softening decoupled from the SSAO
// (.g) blur. 0 = stock (the shadow channel shares the SSAO kernel; the original
// code below runs untouched), 1 = Gaussian sigma in pixels, 2 = Gaussian sigma
// in world metres (converted per pixel from depth, so fine shadows survive as
// the camera pulls back), 3 = no shadow blur at all. SSAO is identical in every
// mode. GLSL initialisers keep a program that never receives these stock.
uniform int   shadow_soften_mode = 0;
uniform float shadow_soften_sigma = 1.0;   // pixels (mode 1) or metres (mode 2)
uniform float shadow_px_per_m = 1.0;       // light-target pixels per metre at 1 m depth (H * P11 / 2)

in vec2 vary_fragcoord;

vec4 getPosition(vec2 pos_screen);
vec4 getNorm(vec2 pos_screen);

void main()
{
    vec2 tc = vary_fragcoord.xy;
    vec4 norm = getNorm(tc);
    vec3 pos = getPosition(tc).xyz;
    vec4 ccol = texture(lightMap, tc).rgba;

    vec2 dlt = kern_scale * delta / (1.0+norm.xy*norm.xy);
    dlt /= max(-pos.z*dist_factor, 1.0);

    vec2 defined_weight = kern[0].xy; // special case the first (centre) sample's weight in the blur; we have to sample it anyway so we get it for 'free'
    vec4 col = defined_weight.xyxx * ccol;

    // relax tolerance according to distance to avoid speckling artifacts, as angles and distances are a lot more abrupt within a small screen area at larger distances
    float pointplanedist_tolerance_pow2 = pos.z*pos.z*0.00005;

    // perturb sampling origin slightly in screen-space to hide edge-ghosting artifacts where smoothing radius is quite large
    tc *= screen_res;
    float tc_mod = 0.5*(tc.x + tc.y);
    tc_mod -= floor(tc_mod);
    tc_mod *= 2.0;
    tc += ( (tc_mod - 0.5) * kern[1].z * dlt * 0.5 );

    // TODO: move this to kern instead of building kernel per pixel
    vec3 k[7];
    k[0] = kern[0];
    k[2] = kern[1];
    k[4] = kern[2];
    k[6] = kern[3];

    k[1] = (k[0]+k[2])*0.5f;
    k[3] = (k[2]+k[4])*0.5f;
    k[5] = (k[4]+k[6])*0.5f;

    for (int i = 1; i < 7; i++)
    {
        vec2 samptc = tc + k[i].z*dlt*2.0;
        samptc /= screen_res;
        vec3 samppos = getPosition(samptc).xyz;

        float d = dot(norm.xyz, samppos.xyz-pos.xyz);// dist from plane

        if (d*d <= pointplanedist_tolerance_pow2)
        {
            col += texture(lightMap, samptc)*k[i].xyxx;
            defined_weight += k[i].xy;
        }
    }

    for (int i = 1; i < 7; i++)
    {
        vec2 samptc = tc - k[i].z*dlt*2.0;
        samptc /= screen_res;
        vec3 samppos = getPosition(samptc).xyz;

        float d = dot(norm.xyz, samppos.xyz-pos.xyz);// dist from plane

        if (d*d <= pointplanedist_tolerance_pow2)
        {
            col += texture(lightMap, samptc)*k[i].xyxx;
            defined_weight += k[i].xy;
        }
    }

    col /= defined_weight.xyxx;
    //col.y *= col.y;

    // [ShadowDist P1] Control D. The stock loops above have already produced
    // .g (SSAO) -- and .b/.a, which softenLight never reads -- exactly as
    // before; only the shadow channel is replaced here. Dedicated taps sit on
    // the UNPERTURBED pixel grid at whole-pixel offsets (the light map is
    // point-sampled; fractional offsets would round asymmetrically) and take
    // their weights from the real integer distances (design v4 s4.2, v5 s4).
    // Each tap keeps the stock plane test so depth-edge behaviour matches.
    if (shadow_soften_mode != 0)
    {
        float shadow_soft = ccol.r;                 // mode 3: centre only (pass-through)
        if (shadow_soften_mode != 3)
        {
            float s = shadow_soften_sigma;
            if (shadow_soften_mode == 2)
            {
                // world metres -> pixels at this pixel's depth
                s = s * shadow_px_per_m / max(-pos.z, 0.05);
            }
            // [ShadowDist P1 fix] cap: extreme mm radii / tiny FOVs would give
            // sigma in the thousands of px (taps spread over the whole screen).
            s = clamp(s, 0.5, 64.0);
            float acc  = ccol.r;
            float wsum = 1.0;
            // s <= 2 px: three taps per side at round(t*s), t = 1..3;
            // s >  2 px: six taps per side at round(t*s/2), t = 1..6, so the tap
            //            spacing stays <= sigma (no comb / ghost copies).
            int   ntaps = (s > 2.0) ? 6 : 3;
            float pitch = (s > 2.0) ? (0.5 * s) : s;
            int   prev  = 0;
            vec2  tc_px = vary_fragcoord.xy * screen_res;   // unperturbed pixel position
            for (int t = 1; t <= 6; ++t)
            {
                if (t > ntaps)
                {
                    break;
                }
                int n = max(prev + 1, int(floor(float(t) * pitch + 0.5)));
                prev = n;
                float q = float(n) / s;                    // [ShadowDist P1 fix] no int n*n overflow
                float w = exp(-0.5 * q * q);
                vec2 off = float(n) * delta;                // delta is a unit axis vector (pixels)

                vec2 tcp = (tc_px + off) / screen_res;
                vec3 pp  = getPosition(tcp).xyz;
                float dp = dot(norm.xyz, pp - pos);
                if (dp * dp <= pointplanedist_tolerance_pow2)
                {
                    acc  += texture(lightMap, tcp).r * w;
                    wsum += w;
                }

                vec2 tcm = (tc_px - off) / screen_res;
                vec3 pm  = getPosition(tcm).xyz;
                float dm = dot(norm.xyz, pm - pos);
                if (dm * dm <= pointplanedist_tolerance_pow2)
                {
                    acc  += texture(lightMap, tcm).r * w;
                    wsum += w;
                }
            }
            shadow_soft = acc / wsum;
        }
        col.r = shadow_soft;
    }

    frag_color = max(col, vec4(0));

#ifdef IS_AMD_CARD
    // If it's AMD make sure the GLSL compiler sees the arrays referenced once by static index. Otherwise it seems to optimise the storage awawy which leads to unfun crashes and artifacts.
    vec3 dummy1 = kern[0];
    vec3 dummy2 = kern[3];
#endif
}


/**
 * @file postDeferredF.glsl
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

uniform sampler2D diffuseRect;

uniform mat4 inv_proj;
uniform vec2 screen_res;
uniform float max_cof;
uniform float res_scale;

in vec2 vary_fragcoord;

void dofSample(inout vec4 diff, inout float w, float min_sc, vec2 tc)
{
    vec4 s = texture(diffuseRect, tc);

    float sc = abs(s.a*2.0-1.0)*max_cof;

    if (sc > min_sc) //sampled pixel is more "out of focus" than current sample radius
    {
        float wg = 0.25;

        // de-weight dull areas to make highlights 'pop'
#ifdef AL_SENSOR_HDR
        float luma = max(dot(s.rgb, vec3(0.2126, 0.7152, 0.0722)), 0.0);
        wg += luma / (1.0 + luma);
#else
        wg += s.r+s.g+s.b;
#endif

        diff += wg*s;

        w += wg;
    }
}

void dofSampleNear(inout vec4 diff, inout float w, float min_sc, vec2 tc)
{
    vec4 s = texture(diffuseRect, tc);

    float wg = 0.25;

    // de-weight dull areas to make highlights 'pop'
#ifdef AL_SENSOR_HDR
    float luma = max(dot(s.rgb, vec3(0.2126, 0.7152, 0.0722)), 0.0);
    wg += luma / (1.0 + luma);
#else
    wg += s.r+s.g+s.b;
#endif

    diff += wg*s;

    w += wg;
}

vec3 clampHDRRange(vec3 color);

#ifdef AL_SENSOR_HDR
// Conservative area reduction onto the actual integer gather viewport.
// Each source cell scatters unit mass into a discrete circular aperture.
// Gathering those same coefficients preserves isolated-highlight flux;
// luminance never changes a source's total weight.
const int sensorDiskCounts[65] = int[65](1,5,13,29,49,81,113,149,197,253,317,377,441,529,613,709,797,901,1009,1129,1257,1373,1517,1653,1793,1961,2121,2289,2453,2629,2821,3001,3209,3409,3625,3853,4053,4293,4513,4777,5025,5261,5525,5789,6077,6361,6625,6921,7213,7525,7845,8173,8497,8809,9145,9477,9845,10189,10557,10913,11289,11681,12061,12453,12853);
int sensorDiskCount(int radius)
{
    if (radius <= 64) return sensorDiskCounts[radius];
    int count = 0;
    for (int y = -radius; y <= radius; ++y)
        count += 2 * int(floor(sqrt(float(radius*radius-y*y)))) + 1;
    return count;
}
ivec2 sensorReflect(ivec2 p, ivec2 size)
{
    if (all(greaterThanEqual(p,ivec2(0))) && all(lessThan(p,size))) return p;
    // GLSL integer remainder is undefined for negative operands.
    p = ivec2(mod(vec2(p), vec2(2*size)));
    return min(p, 2*size-1-p);
}
vec4 sensorCell(ivec2 p, ivec2 size)
{
    p = sensorReflect(p, size);
    vec2 cell = screen_res / vec2(size);
    vec2 lo = vec2(p)*cell, hi = vec2(p+1)*cell;
    vec4 value = vec4(0.0);
    float cell_coc = 0.0;
    for (int y = int(floor(lo.y)); y < int(ceil(hi.y)); ++y)
    for (int x = int(floor(lo.x)); x < int(ceil(hi.x)); ++x)
    {
        vec2 overlap = max(min(hi, vec2(x+1,y+1))-max(lo,vec2(x,y)),vec2(0.0));
        vec4 sample_value = texelFetch(diffuseRect, ivec2(x,y), 0);
        float coc = (sample_value.a*2.0-1.0)*max_cof;
#if !FRONT_BLUR
        coc = min(coc, 0.0);
#endif
        value.rgb += sample_value.rgb*overlap.x*overlap.y;
        if (abs(coc) > abs(cell_coc)) cell_coc = coc;
    }
    return vec4(value.rgb/(cell.x*cell.y),cell_coc/max(max_cof,1e-6)*0.5+0.5);
}
vec4 sensorDof()
{
    ivec2 size = max(ivec2(screen_res*res_scale), ivec2(1));
    ivec2 dest = ivec2(gl_FragCoord.xy);
    float scale = min(float(size.x)/screen_res.x, float(size.y)/screen_res.y);
    int limit = int(ceil(max_cof*scale));
    vec4 center = texelFetch(diffuseRect, dest, 0);
    float center_coc = (center.a*2.0-1.0)*max_cof;
#if !FRONT_BLUR
    // Background scatter cannot cross an opaque focused foreground. With
    // front blur disabled, this is exact and skips the entire shipped-cap disk.
    if (center_coc >= -0.5) return center;
#endif
    vec3 foreground = vec3(0.0), background = vec3(0.0);
    float front_coverage = 0.0, back_coverage = 0.0;
    for (int y=-limit; y<=limit; ++y)
    for (int x=-limit; x<=limit; ++x)
    {
        if (x*x+y*y > limit*limit) continue;
        vec4 source = texelFetch(diffuseRect, sensorReflect(dest+ivec2(x,y), size), 0);
        float coc = (source.a*2.0-1.0)*max_cof;
        if (abs(coc) <= 0.5 || coc < min(center_coc, -0.5)) continue;
#if !FRONT_BLUR
        if (coc > 0.0) continue;
#endif
        int radius = int(ceil(abs(coc)*scale));
        if (radius > 0 && x*x+y*y <= radius*radius)
        {
            float coverage = 1.0/float(sensorDiskCount(radius));
            if (coc > 0.0) { foreground += source.rgb*coverage; front_coverage += coverage; }
            else { background += source.rgb*coverage; back_coverage += coverage; }
        }
    }
    // Resolve each depth layer by its coverage. Foreground transmittance
    // attenuates the background; a focused receiver retains its sharp layer.
    vec3 base = center.rgb;
    if (center_coc < -0.5 && back_coverage > 0.0) base = background/back_coverage;
    if (center_coc > 0.5 && front_coverage > 0.0) base = foreground/front_coverage;
    else base = base*(1.0-min(front_coverage,1.0))+foreground/max(front_coverage,1.0);
    return vec4(base, center.a);
}
#endif

void main()
{
#ifdef AL_SENSOR_HDR
#ifdef AL_SENSOR_DOF_REDUCE
    frag_color = sensorCell(ivec2(gl_FragCoord.xy), max(ivec2(screen_res*res_scale),ivec2(1)));
#else
    frag_color = sensorDof();
#endif
    return;
#endif
    vec2 tc = vary_fragcoord.xy;

    vec4 diff = texture(diffuseRect, vary_fragcoord.xy);

    {
        float w = 1.0;

        float sc = (diff.a*2.0-1.0)*max_cof;

        float PI = 3.14159265358979323846264;

        // sample quite uniformly spaced points within a circle, for a circular 'bokeh'
#if FRONT_BLUR
        if (sc > 0.5)
        {
            while (sc > 0.5)
            {
                int its = int(max(1.0,(sc*3.7)));
                for (int i=0; i<its; ++i)
                {
                    float ang = sc+i*2*PI/its; // sc is added for rotary perturbance
                    float samp_x = sc*sin(ang);
                    float samp_y = sc*cos(ang);
                    // you could test sample coords against an interesting non-circular aperture shape here, if desired.
                    dofSampleNear(diff, w, sc, vary_fragcoord.xy + (vec2(samp_x,samp_y) / screen_res));
                }
                sc -= 1.0;
            }
        }
        else if (sc < -0.5)
#else
        if (sc < -0.5)
#endif
        {
            sc = abs(sc);
            while (sc > 0.5)
            {
                int its = int(max(1.0,(sc*3.7)));
                for (int i=0; i<its; ++i)
                {
                    float ang = sc+i*2*PI/its; // sc is added for rotary perturbance
                    float samp_x = sc*sin(ang);
                    float samp_y = sc*cos(ang);
                    // you could test sample coords against an interesting non-circular aperture shape here, if desired.
                    dofSample(diff, w, sc, vary_fragcoord.xy + (vec2(samp_x,samp_y) / screen_res));
                }
                sc -= 1.0;
            }
        }

        diff /= w;
    }

#ifndef AL_SENSOR_HDR
    diff.rgb = clampHDRRange(diff.rgb);
#endif
    frag_color = diff;
}

uniform int relight_enabled;
/**
 * @file class1/deferred/deferredUtil.glsl
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
#define RELIGHT_PARAM vec4 shape,
#define RELIGHT_FORWARD shape,
#define RELIGHT_DEFAULT vec4(0.0,0.0,1.0,-1.0),
#define RELIGHT_DATA(i) light_shapes[i],



/*  Parts of this file are taken from Sascha Willem's Vulkan GLTF refernce implementation
MIT License

Copyright (c) 2018 Sascha Willems

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

uniform sampler2D normalMap;
uniform sampler2D depthMap;
uniform sampler2D projectionMap; // rgba
uniform sampler2D brdfLut;

// projected lighted params
uniform mat4 proj_mat; //screen space to light space projector
uniform vec3 proj_n; // projector normal
uniform vec3 proj_p; //plane projection is emitting from (in screen space)
uniform float proj_focus; // distance from plane to begin blurring
uniform float proj_lod  ; // (number of mips in proj map)
uniform float proj_range; // range between near clip and far clip plane of projection
uniform float proj_ambiance;

uniform int classic_mode;

// [BDMerge Batch 2] Feature 2 - gobo/cookie mip + anisotropic filtering.
// When non-zero, the cookie LOD samplers clamp their focus-based LOD up to the
// screen-space (derivative) minification LOD, so a projected gobo viewed at a
// grazing angle or from far away samples a coarser mip instead of aliasing.
// Only the surface projector programs upload this (setupSpotLight); it stays 0
// for every other consumer, keeping their sampling byte-identical.
uniform int gobo_aniso;
uniform float gobo_time;
uniform int gobo_pattern;
// x = animation mode, y = speed, z = zoom, w = surface-only dispersion.
uniform vec4 gobo_anim_params;
// [Gobo v2] Per-projector cookie color multiply. Default (1,1,1) = no change.
uniform vec3 gobo_tint;
// [Gobo v2] Per-pattern variation. Each component 0 = the pattern's built-in
// default (so an override with zero params reproduces the v1 look exactly):
// x = detail/count, y = thickness/tilt, z = softness, w = invert (>=0.5).
uniform vec4 gobo_pattern_params;

// light params
uniform vec3 color; // light_color
uniform float size; // light_size

// [BDMerge Batch 2] Screen-space minification LOD from the cookie UV derivatives.
// Returns the base (focus) LOD raised to the minification LOD so grazing/distant
// gobos never sample a finer mip than the pixel footprint warrants (anti-alias).
// The C++ side sets a trilinear-mip + anisotropic filter state on the cookie so
// the extra mip levels this selects are actually present and crisply filtered.
float goboLod(vec2 tc, float base_lod)
{
#ifndef FXAA_GLSL_120
    if (gobo_aniso != 0)
    {
        vec2  ts  = vec2(textureSize(projectionMap, 0));
        vec2  dx  = dFdx(tc) * ts;
        vec2  dy  = dFdy(tc) * ts;
        float d2  = max(dot(dx, dx), dot(dy, dy));
        float slod = 0.5 * log2(max(d2, 1.0e-8));
        return max(base_lod, slod);
    }
#endif
    return base_lod;
}

float goboHash(vec2 p)
{
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float goboValueNoise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(goboHash(i), goboHash(i + vec2(1.0, 0.0)), f.x),
               mix(goboHash(i + vec2(0.0, 1.0)),
                   goboHash(i + vec2(1.0, 1.0)), f.x), f.y);
}

// Analytic library. `pp` = gobo_pattern_params (x detail/count, y thickness/tilt,
// z softness, w invert). EVERY component 0 reproduces the pattern's v1 constants
// exactly, so a default override renders byte-identically to the pre-v2 build.
// Pattern ids 100+ stay reserved for a future PNG library (loader modeled on
// loadProjVolDustMap(), shipped via viewer_manifest.py) with no caller change.
vec4 proceduralGobo(int id, vec2 uv, vec4 pp)
{
    float pattern = 1.0;
    float detail = exp2(pp.x);   // 1.0 at default; scales count/frequency
    if (id == 0)
    {
        // Venetian blinds: soft horizontal slats.
        float band = abs(fract(uv.y * 12.0 * detail) - 0.5);
        float t = pp.y * 0.12;
        float soft = pp.z * 0.15;
        pattern = 1.0 - smoothstep(0.30 - t - soft, 0.40 - t + soft, band);
    }
    else if (id == 1)
    {
        // Multi-pane window with a frame and central mullions.
        vec2 p = abs(fract(uv * 2.0 * detail) - 0.5);
        float th = 0.43 - pp.y * 0.15;
        float bars = max(step(th, p.x), step(th, p.y));
        pattern = 1.0 - bars;
    }
    else if (id == 2)
    {
        // Crosshatched grate.
        vec2 bars = abs(fract(uv * 10.0 * detail) - 0.5);
        float th = 0.34 + pp.y * 0.10;
        pattern = 1.0 - max(step(th, bars.x), step(th, bars.y));
    }
    else if (id == 3)
    {
        // Ceiling-fan blades around the hub (blade count via detail).
        vec2 p = uv - vec2(0.5);
        float radius = length(p);
        float blade = abs(fract((atan(p.y, p.x) / 6.2831853) * 5.0 * detail) - 0.5);
        pattern = (1.0 - smoothstep(0.13, 0.23, blade)) *
                  smoothstep(0.06, 0.12, radius) * (1.0 - smoothstep(0.46, 0.50, radius));
    }
    else if (id == 4)
    {
        // Softbox square-cell grid.
        vec2 cell = abs(fract(uv * 8.0 * detail) - 0.5);
        float seam = max(cell.x, cell.y);
        float soft = pp.z * 0.10;
        pattern = 1.0 - smoothstep(0.38 - soft, 0.47 + soft, seam);
    }
    else if (id == 5)
    {
        // Organic foliage/dapple breakup from two value-noise octaves.
        float sc = 8.0 * detail;
        float n = goboValueNoise(uv * sc) * 0.65 +
                  goboValueNoise(uv * sc * 2.125 + vec2(3.7, 9.2)) * 0.35;
        float lo = 0.42 - pp.y * 0.2;
        pattern = smoothstep(lo, lo + 0.20, n);
    }
    else if (id == 6)
    {
        // Cheap caustic-like interference field; UV motion supplies the scroll.
        vec2 p = (uv - vec2(0.5)) * 9.0 * detail;
        float a = sin(p.x + sin(p.y * 1.7));
        float b = sin(p.y * 1.3 - sin(p.x * 1.9));
        pattern = smoothstep(0.25, 0.82, abs(a + b) * 0.5);
    }
    else if (id == 7)
    {
        // Prison / warehouse heavy vertical bars.
        float bar = abs(fract(uv.x * 6.0 * detail) - 0.5);
        float soft = pp.z * 0.10;
        pattern = 1.0 - smoothstep(0.24 - soft, 0.32 + soft, bar);
    }
    else if (id == 8)
    {
        // Cathedral arched window: rounded-top aperture with mullions.
        float body = step(0.20, uv.x) * step(uv.x, 0.80) *
                     step(0.10, uv.y) * step(uv.y, 0.65);
        float arch = step(length((uv - vec2(0.5, 0.65)) / vec2(0.30, 0.35)), 1.0) *
                     step(0.65, uv.y);
        float shape = clamp(body + arch, 0.0, 1.0);
        vec2 m = abs(fract(uv * vec2(3.0, 4.0) * detail) - 0.5);
        float bars = max(step(0.42, m.x), step(0.42, m.y));
        pattern = shape * (1.0 - bars);
    }
    else if (id == 9)
    {
        // Iris / vignette framing aperture (oval via thickness, soft edge).
        float rad = 0.45 * exp2(-pp.x * 0.5);
        float soft = 0.04 + pp.z * 0.40;
        vec2 d = (uv - vec2(0.5)) * vec2(1.0 + pp.y, 1.0);
        pattern = 1.0 - smoothstep(rad - soft, rad, length(d));
    }
    else if (id == 10)
    {
        // Dot-matrix / disco grid.
        float count = 8.0 * detail;
        vec2 cell = fract(uv * count) - 0.5;
        float rad = 0.30 + pp.y * 0.15;
        float soft = 0.05 + pp.z * 0.20;
        pattern = 1.0 - smoothstep(rad, rad + soft, length(cell));
    }
    else if (id == 11)
    {
        // Concentric rings blending to radial starburst (blend via thickness).
        float count = 8.0 * detail;
        float rings = abs(fract(length(uv - vec2(0.5)) * count) - 0.5);
        float ringp = 1.0 - smoothstep(0.22, 0.32, rings);
        float ang = atan(uv.y - 0.5, uv.x - 0.5);
        float sp = abs(fract(ang / 6.2831853 * max(count, 1.0)) - 0.5);
        float spokep = 1.0 - smoothstep(0.22, 0.32, sp);
        pattern = mix(ringp, spokep, clamp(pp.y, 0.0, 1.0));
    }
    else if (id == 12)
    {
        // Soft cloud / god-ray breakup (softer threshold than foliage).
        float sc = 4.0 * detail;
        float n = goboValueNoise(uv * sc) * 0.6 +
                  goboValueNoise(uv * sc * 2.25 + vec2(5.0, 2.0)) * 0.4;
        float lo = 0.35 - pp.y * 0.2;
        pattern = smoothstep(lo, lo + 0.40, n);
    }
    pattern = clamp(pattern, 0.0, 1.0);
    if (pp.w >= 0.5)
    {
        pattern = 1.0 - pattern; // negative cookie
    }
    return vec4(vec3(pattern), 1.0);
}

// UV-space animation modes: 1 rotate, 2 wind-sway, 3 pan, 4 pulse (osc zoom).
// Modes 0/5/6 leave the UV unchanged (5/6 are intensity-only, see below).
vec2 animatedGoboUV(vec2 uv)
{
    int mode = int(gobo_anim_params.x + 0.5);
    float speed = gobo_anim_params.y;
    float zoom = max(gobo_anim_params.z, 0.001);
    if (mode == 4) // pulse/breathe oscillates the zoom about its base
    {
        zoom = max(zoom * (1.0 + 0.25 * sin(speed * gobo_time)), 0.001);
    }
    // Preserve the legacy texture coordinates bit-for-bit when nothing moves
    // the UV (mode 0/5/6 with default zoom).
    if (mode != 1 && mode != 2 && mode != 3 && zoom == 1.0)
    {
        return uv;
    }
    uv = (uv - vec2(0.5)) / zoom + vec2(0.5);
    if (mode == 1) // continuous rotation
    {
        float angle = speed * gobo_time;
        float c = cos(angle);
        float s = sin(angle);
        mat2 rotation = mat2(c, s, -s, c);
        uv = rotation * (uv - vec2(0.5)) + vec2(0.5);
    }
    else if (mode == 2) // wind sway domain warp
    {
        vec2 phase = uv.yx * vec2(9.0, 7.0) +
                     gobo_time * speed * vec2(1.0, 1.31);
        uv += 0.018 * sin(phase);
    }
    else if (mode == 3) // pan / scroll along a fixed diagonal
    {
        uv += normalize(vec2(1.0, 0.35)) * speed * gobo_time * 0.1;
    }
    return uv;
}

// Intensity-space animation modes: 5 flicker (candle/fire), 6 strobe. Every
// other mode returns 1.0, so this is a no-op unless flicker/strobe is chosen.
float goboAnimIntensity()
{
    int mode = int(gobo_anim_params.x + 0.5);
    float speed = max(abs(gobo_anim_params.y), 0.001);
    if (mode == 5)
    {
        float n = goboValueNoise(vec2(gobo_time * speed * 3.0, 0.0));
        return mix(0.6, 1.0, n);
    }
    if (mode == 6)
    {
        return step(fract(gobo_time * speed), 0.5);
    }
    return 1.0;
}

// Shared by surface lighting, froxel injection and the volumetric march. Tint
// (default white) and intensity (default 1.0) keep the off-path byte-identical.
vec4 sampleGobo(vec2 uv, float lod)
{
    vec2 animated_uv = animatedGoboUV(uv);
    // Camera-feed-only marker. Existing texture gobos remain -1 and therefore
    // keep their original UVs. xy is gate weave; zw maps the light cone to the
    // capture camera FOV without another sampler or uniform.
    if (gobo_pattern == -2)
    {
        animated_uv = (animated_uv - vec2(0.5)) * gobo_pattern_params.zw +
                      vec2(0.5) + gobo_pattern_params.xy;
    }
    vec4 col;
    if (gobo_pattern == -2 &&
        (any(lessThan(animated_uv, vec2(0.0))) ||
         any(greaterThan(animated_uv, vec2(1.0)))))
    {
        col = vec4(0.0);
    }
    else if (gobo_pattern >= 0)
    {
        col = proceduralGobo(gobo_pattern, animated_uv, gobo_pattern_params);
    }
    else
    {
#ifndef FXAA_GLSL_120
        col = textureLod(projectionMap, animated_uv, goboLod(animated_uv, lod));
#else
        col = texture(projectionMap, animated_uv);
#endif
    }
    col.rgb *= gobo_tint;
    col.rgb *= goboAnimIntensity();
    return col;
}

uniform mat4 inv_proj;
uniform vec2 screen_res;

const float M_PI = 3.14159265;
const float ONE_OVER_PI = 0.3183098861;

const float MIN_PBR_ROUGHNESS = 0.045;

vec3 srgb_to_linear(vec3 cs);
vec3 linear_to_srgb(vec3 cs);
vec3 atmosFragLightingLinear(vec3 light, vec3 additive, vec3 atten);

vec4 decodeNormal(vec4 norm);

vec3 clampHDRRange(vec3 color)
{
    // Why do this?
    // There are situations where the color range will go to something insane - potentially producing infs and NaNs even.
    // This is a safety measure to prevent that.
    // -Geenz 2025-03-05
    color = mix(color, vec3(1), isinf(color));
    color = mix(color, vec3(0.0), isnan(color));
    return color;
}

float calcLegacyDistanceAttenuation(float distance, float falloff)
{
    float dist_atten = 1.0 - clamp((distance + falloff)/(1.0 + falloff), 0.0, 1.0);
    dist_atten *= dist_atten;

    // Tweak falloff slightly to match pre-EEP attenuation
    // NOTE: this magic number also shows up in a great many other places, search for dist_atten *= to audit
    dist_atten *= 2.0;
    return dist_atten;
}

// In:
//   lv  unnormalized surface to light vector
//   n   normal of the surface
//   pos unnormalized camera to surface vector
// Out:
//   l   normalized surace to light vector
//   nl  diffuse angle
//   nh  specular angle
void calcHalfVectors(vec3 lv, vec3 n, vec3 v,
    out vec3 h, out vec3 l, out float nh, out float nl, out float nv, out float vh, out float lightDist)
{
    l  = normalize(lv);
    h  = normalize(l + v);

    // lower bound to avoid divide by zero
    float eps = 0.000001;
    nh = clamp(dot(n, h), eps, 1.0);
    nl = clamp(dot(n, l), eps, 1.0);
    nv = clamp(dot(n, v), eps, 1.0);
    vh = clamp(dot(v, h), eps, 1.0);

    lightDist = length(lv);
}

// In:
//   light_center
//   pos
// Out:
//   dist
//   l_dist
//   lv
//   proj_tc  Projector Textue Coordinates
bool clipProjectedLightVars(vec3 light_center, vec3 pos, out float dist, out float l_dist, out vec3 lv, out vec4 proj_tc )
{
    lv = light_center - pos.xyz;
    dist = length(lv);
    bool clipped = (dist >= size);
    if ( !clipped )
    {
        dist /= size;

        l_dist = -dot(lv, proj_n);
        vec4 projected_point = (proj_mat * vec4(pos.xyz, 1.0));
        clipped = (projected_point.z < 0.0);
        projected_point.xyz /= projected_point.w;
        proj_tc = projected_point;
    }

    return clipped;
}

vec2 getScreenCoordinate(vec2 screenpos)
{
    vec2 sc = screenpos.xy * 2.0;
    return sc - vec2(1.0, 1.0);
}

vec4 getNorm(vec2 screenpos)
{
    vec4 norm = decodeNormal(texture(normalMap, screenpos.xy));
    return norm;
}

vec4 getNormRaw(vec2 screenpos)
{
    vec4 norm = texture(normalMap, screenpos.xy);
    return norm;
}

#if defined(HAS_EMISSIVE)
uniform int alr_sil_active; // [AvatarSilhouette] frame mask; bit 4 selects the fp16 carrier

bool alrSilDecode(float carrier, float gbuffer_flag, out float k, out float r, out bool haze_skip)
{
    k = 1.0;
    r = 1.0;
    haze_skip = false;

    if ((alr_sil_active & 1) == 0 ||
        !(GET_GBUFFER_FLAG(gbuffer_flag, GBUFFER_FLAG_HAS_PBR) ||
          GET_GBUFFER_FLAG(gbuffer_flag, GBUFFER_FLAG_HAS_ATMOS)))
    {
        return false;
    }

    int q;
    int flags;
    if ((alr_sil_active & 16) != 0)
    {
        float encoded = carrier - 2.0;
        float rounded = floor(encoded + 0.5);
        if (!(abs(encoded - rounded) < 0.25) || !(rounded >= 1.0 && rounded <= 2046.0))
        {
            return false;
        }
        int code = int(rounded);
        q = code & 511;
        if (q < 1 || q > 510)
        {
            return false;
        }
        flags = code >> 9;
        k = 1.0 - float(q) / 510.0;
    }
    else
    {
        float rounded = floor(carrier * 255.0 + 0.5);
        if (!(rounded >= 1.0 && rounded <= 248.0))
        {
            return false;
        }
        int code = int(rounded);
        flags = (code - 1) / 62;
        q = code - flags * 62;
        k = 1.0 - float(q) / 62.0;
    }

    haze_skip = (flags & 1) != 0;
    r = (flags & 2) != 0 ? k : 1.0;
    return true;
}
#endif

// get linear depth value given a depth buffer sample d and znear and zfar values
float linearDepth(float d, float znear, float zfar)
{
    d = d * 2.0 - 1.0;
    return znear * 2.0 * zfar / (zfar + znear - d * (zfar - znear));
}

float linearDepth01(float d, float znear, float zfar)
{
    return linearDepth(d, znear, zfar) / zfar;
}

float getDepth(vec2 pos_screen)
{
    float depth = texture(depthMap, pos_screen).r;
    return depth;
}

vec4 getTexture2DLodAmbient(vec2 tc, float lod)
{
    vec4 ret = sampleGobo(tc, lod);
    ret.rgb = srgb_to_linear(ret.rgb);

    vec2 dist = tc-vec2(0.5);
    float d = dot(dist,dist);
    ret *= min(clamp((0.25-d)/0.25, 0.0, 1.0), 1.0);

    return ret;
}

vec4 getTexture2DLodDiffuse(vec2 tc, float lod)
{
    vec4 ret = sampleGobo(tc, lod);
    ret.rgb = srgb_to_linear(ret.rgb);

    vec2 dist = vec2(0.5) - abs(tc-vec2(0.5));
    float det = min(lod/(proj_lod*0.5), 1.0);
    float d = min(dist.x, dist.y);
    float edge = 0.25*det;
    ret *= clamp(d/edge, 0.0, 1.0);

    return ret;
}

// lit     This is set by the caller: if (nl > 0.0) { lit = attenuation * nl * noise; }
// Uses:
//   color   Projected spotlight color
vec3 getProjectedLightAmbiance(float amb_da, float attenuation, float lit, float nl, float noise, vec2 projected_uv)
{
    vec4 amb_plcol = getTexture2DLodAmbient(projected_uv, proj_lod);
    vec3 amb_rgb   = amb_plcol.rgb * amb_plcol.a;

    amb_da += proj_ambiance;
    amb_da += (nl*nl*0.5+0.5) * proj_ambiance;
    amb_da *= attenuation * noise;
    amb_da = min(amb_da, 1.0-lit);

    return (amb_da * color.rgb * amb_rgb);
}

// Returns projected light in Linear
// Uses global spotlight color:
//  color
// NOTE: projected.a will be pre-multiplied with projected.rgb
vec3 getProjectedLightDiffuseColor(float light_distance, vec2 projected_uv)
{
    float diff = clamp((light_distance - proj_focus)/proj_range, 0.0, 1.0);
    float lod = diff * proj_lod;
    vec4 plcol = getTexture2DLodDiffuse(projected_uv.xy, lod);
    // Surface-only chromatic split. Volumetric consumers call the single-tap
    // getTexture2DLodDiffuse funnel directly and never enter this branch.
    float dispersion = max(gobo_anim_params.w, 0.0);
    if (dispersion > 0.0)
    {
        vec2 radial = projected_uv - vec2(0.5);
        vec4 red_sample = getTexture2DLodDiffuse(projected_uv + radial * dispersion, lod);
        vec4 blue_sample = getTexture2DLodDiffuse(projected_uv - radial * dispersion, lod);
        plcol.rgb = vec3(red_sample.r, plcol.g, blue_sample.b);
    }

    return color.rgb * plcol.rgb * plcol.a;
}

vec4 texture2DLodSpecular(vec2 tc, float lod)
{
    vec4 ret = sampleGobo(tc, lod);
    ret.rgb = srgb_to_linear(ret.rgb);

    vec2 dist = vec2(0.5) - abs(tc-vec2(0.5));
    float det = min(lod/(proj_lod*0.5), 1.0);
    float d = min(dist.x, dist.y);
    d *= min(1, d * (proj_lod - lod)); // BUG? extra factor compared to diffuse causes N repeats
    float edge = 0.25*det;
    ret *= clamp(d/edge, 0.0, 1.0);

    return ret;
}

// See: clipProjectedLightVars()
vec3 getProjectedLightSpecularColor(vec3 pos, vec3 n )
{
    vec3 slit = vec3(0);
    vec3 ref = reflect(normalize(pos), n);

    //project from point pos in direction ref to plane proj_p, proj_n
    vec3 pdelta = proj_p-pos;
    float l_dist = length(pdelta);
    float ds = dot(ref, proj_n);
    if (ds < 0.0)
    {
        vec3 pfinal = pos + ref * dot(pdelta, proj_n)/ds;
        vec4 stc = (proj_mat * vec4(pfinal.xyz, 1.0));
        if (stc.z > 0.0)
        {
            stc /= stc.w;
            slit = getProjectedLightDiffuseColor( l_dist, stc.xy ); // NOTE: Using diffuse due to texture2DLodSpecular() has extra: d *= min(1, d * (proj_lod - lod));
        }
    }
    return slit; // specular light
}

vec3 getProjectedLightSpecularColor(float light_distance, vec2 projected_uv)
{
    float diff = clamp((light_distance - proj_focus)/proj_range, 0.0, 1.0);
    float lod = diff * proj_lod;
    vec4 plcol = getTexture2DLodDiffuse(projected_uv.xy, lod); // NOTE: Using diffuse due to texture2DLodSpecular() has extra: d *= min(1, d * (proj_lod - lod));

    return color.rgb * plcol.rgb * plcol.a;
}

vec4 getPosition(vec2 pos_screen)
{
    float depth = getDepth(pos_screen);
    vec2 sc = getScreenCoordinate(pos_screen);
    vec4 ndc = vec4(sc.x, sc.y, 2.0*depth-1.0, 1.0);
    vec4 pos = inv_proj * ndc;
    pos /= pos.w;
    pos.w = 1.0;
    return pos;
}

// get position given a normalized device coordinate
vec3 getPositionWithNDC(vec3 ndc)
{
    vec4 pos = inv_proj * vec4(ndc, 1.0);
    return pos.xyz / pos.w;
}

vec4 getPositionWithDepth(vec2 pos_screen, float depth)
{
    vec2 sc = getScreenCoordinate(pos_screen);
    vec3 ndc = vec3(sc.x, sc.y, 2.0*depth-1.0);
    return vec4(getPositionWithNDC(ndc), 1.0);
}

vec2 getScreenCoord(vec4 clip)
{
    vec4 ndc = clip;
         ndc.xyz /= clip.w;
    vec2 screen = vec2( ndc.xy * 0.5 );
         screen += 0.5;
    return screen;
}

vec2 getScreenXY(vec4 clip)
{
    vec4 ndc = clip;
         ndc.xyz /= clip.w;
    vec2 screen = vec2( ndc.xy * 0.5 );
         screen += 0.5;
         screen *= screen_res;
    return screen;
}

// Color utils

vec3 colorize_dot(float x)
{
    if (x > 0.0) return vec3( 0, x, 0 );
    if (x < 0.0) return vec3(-x, 0, 0 );
                 return vec3( 0, 0, 1 );
}

vec3 hue_to_rgb(float hue)
{
    if (hue > 1.0) return vec3(0.5);
    vec3 rgb = abs(hue * 6. - vec3(3, 2, 4)) * vec3(1, -1, -1) + vec3(-1, 2, 2);
    return clamp(rgb, 0.0, 1.0);
}

// PBR Utils

vec2 BRDF(float NoV, float roughness)
{
    return texture(brdfLut, vec2(NoV, roughness)).rg;
}

// Lagarde and de Rousiers 2014, "Moving Frostbite to PBR"
float computeSpecularAO(float NoV, float ao, float roughness)
{
    return clamp(pow(NoV + ao, exp2(-16.0 * roughness - 1.0)) - 1.0 + ao, 0.0, 1.0);
}

// set colorDiffuse and colorSpec to the results of GLTF PBR style IBL
void pbrIbl(vec3 diffuseColor,
            vec3 specularColor,
            vec3 radiance, // radiance map sample
            vec3 irradiance, // irradiance map sample
            float ao,       // ambient occlusion factor
            float nv,       // normal dot view vector
            float perceptualRough,
            out vec3 diffuseOut,
            out vec3 specularOut)
{
    // retrieve a scale and bias to F0. See [1], Figure 3
    vec2 brdf = BRDF(clamp(nv, 0, 1), 1.0-perceptualRough);
    vec3 diffuseLight = irradiance;
    vec3 specularLight = radiance;

    vec3 diffuse = diffuseLight * diffuseColor;
    vec3 specular = specularLight * (specularColor * brdf.x + brdf.y);

    diffuseOut = diffuse * ao;
    specularOut = specular * computeSpecularAO(nv, ao, perceptualRough * perceptualRough);
}


// Encapsulate the various inputs used by the various functions in the shading equation
// We store values in this struct to simplify the integration of alternative implementations
// of the shading terms, outlined in the Readme.MD Appendix.
struct PBRInfo
{
    float NdotL;                  // cos angle between normal and light direction
    float NdotV;                  // cos angle between normal and view direction
    float NdotH;                  // cos angle between normal and half vector
    float LdotH;                  // cos angle between light direction and half vector
    float VdotH;                  // cos angle between view direction and half vector
    float perceptualRoughness;    // roughness value, as authored by the model creator (input to shader)
    float metalness;              // metallic value at the surface
    vec3 reflectance0;            // full reflectance color (normal incidence angle)
    vec3 reflectance90;           // reflectance color at grazing angle
    float alphaRoughness;         // roughness mapped to a more linear change in the roughness (proposed by [2])
    vec3 diffuseColor;            // color contribution from diffuse lighting
    vec3 specularColor;           // color contribution from specular lighting
};

// Basic Lambertian diffuse
// Implementation from Lambert's Photometria https://archive.org/details/lambertsphotome00lambgoog
// See also [1], Equation 1
vec3 diffuse(PBRInfo pbrInputs)
{
    return pbrInputs.diffuseColor / M_PI;
}

// The following equation models the Fresnel reflectance term of the spec equation (aka F())
// Implementation of fresnel from [4], Equation 15
vec3 specularReflection(PBRInfo pbrInputs)
{
    return pbrInputs.reflectance0 + (pbrInputs.reflectance90 - pbrInputs.reflectance0) * pow(clamp(1.0 - pbrInputs.VdotH, 0.0, 1.0), 5.0);
}

// This calculates the specular geometric attenuation (aka G()),
// where rougher material will reflect less light back to the viewer.
// This implementation is based on [1] Equation 4, and we adopt their modifications to
// alphaRoughness as input as originally proposed in [2].
float geometricOcclusion(PBRInfo pbrInputs)
{
    float NdotL = pbrInputs.NdotL;
    float NdotV = pbrInputs.NdotV;
    float r = pbrInputs.alphaRoughness;

    float attenuationL = 2.0 * NdotL / (NdotL + sqrt(r * r + (1.0 - r * r) * (NdotL * NdotL)));
    float attenuationV = 2.0 * NdotV / (NdotV + sqrt(r * r + (1.0 - r * r) * (NdotV * NdotV)));
    return attenuationL * attenuationV;
}

// The following equation(s) model the distribution of microfacet normals across the area being drawn (aka D())
// Implementation from "Average Irregularity Representation of a Roughened Surface for Ray Reflection" by T. S. Trowbridge, and K. P. Reitz
// Follows the distribution function recommended in the SIGGRAPH 2013 course notes from EPIC Games [1], Equation 3.
float microfacetDistribution(PBRInfo pbrInputs)
{
    float roughnessSq = pbrInputs.alphaRoughness * pbrInputs.alphaRoughness;
    float f = (pbrInputs.NdotH * roughnessSq - pbrInputs.NdotH) * pbrInputs.NdotH + 1.0;
    return roughnessSq / (M_PI * f * f);
}

void pbrPunctual(vec3 diffuseColor, vec3 specularColor,
                    float perceptualRoughness,
                    float metallic,
                    vec3 n, // normal
                    vec3 v, // surface point to camera
                    vec3 l,
                    out float nl,
                    out vec3 diff,
                    out vec3 spec) //surface point to light
{
    // make sure specular highlights from punctual lights don't fall off of polished surfaces
    perceptualRoughness = max(perceptualRoughness, MIN_PBR_ROUGHNESS);

    float alphaRoughness = perceptualRoughness * perceptualRoughness;

    // Compute reflectance.
    float reflectance = max(max(specularColor.r, specularColor.g), specularColor.b);

    // For typical incident reflectance range (between 4% to 100%) set the grazing reflectance to 100% for typical fresnel effect.
    // For very low reflectance range on highly diffuse objects (below 4%), incrementally reduce grazing reflecance to 0%.
    float reflectance90 = clamp(reflectance * 25.0, 0.0, 1.0);
    vec3 specularEnvironmentR0 = specularColor.rgb;
    vec3 specularEnvironmentR90 = vec3(1.0, 1.0, 1.0) * reflectance90;

    vec3 h = normalize(l+v);                        // Half vector between both l and v
    vec3 reflection = -normalize(reflect(v, n));
    reflection.y *= -1.0f;

    float NdotL = clamp(dot(n, l), 0.001, 1.0);
    float NdotV = clamp(abs(dot(n, v)), 0.001, 1.0);
    float NdotH = clamp(dot(n, h), 0.0, 1.0);
    float LdotH = clamp(dot(l, h), 0.0, 1.0);
    float VdotH = clamp(dot(v, h), 0.0, 1.0);

    PBRInfo pbrInputs = PBRInfo(
        NdotL,
        NdotV,
        NdotH,
        LdotH,
        VdotH,
        perceptualRoughness,
        metallic,
        specularEnvironmentR0,
        specularEnvironmentR90,
        alphaRoughness,
        diffuseColor,
        specularColor
    );

    // Calculate the shading terms for the microfacet specular shading model
    vec3 F = specularReflection(pbrInputs);
    float G = geometricOcclusion(pbrInputs);
    float D = microfacetDistribution(pbrInputs);

    // Calculation of analytical lighting contribution
    vec3 diffuseContrib = (1.0 - F) * diffuse(pbrInputs);
    vec3 specContrib = F * G * D / (4.0 * NdotL * NdotV);

    nl = NdotL;

    diff = diffuseContrib;
    spec = specContrib;
}


uniform vec2 relight_limits; // near floor (m), maximum physical boost
// [RelightQ] Karis 2013 eqs. 10/14; Lagarde & de Rousiers 2014 sphere/disk
// illuminance. No history, random samples, visibility or render-state work.
vec3 relightUnit(vec3 v, vec3 fallback)
{
    return dot(v,v) > 1.e-12 ? v * inversesqrt(dot(v,v)) : fallback;
}
void relightPunctual(vec3 diffuseColor, vec3 specularColor,
                    float perceptualRoughness,
                    float metallic,
                    vec3 n, // normal
                    vec3 v, // surface point to camera
                    vec3 l,
                    out float nl,
                    out vec3 diff,
                    out vec3 spec) //surface point to light
{
    // make sure specular highlights from punctual lights don't fall off of polished surfaces
    perceptualRoughness = max(perceptualRoughness, MIN_PBR_ROUGHNESS);

    float alphaRoughness = perceptualRoughness * perceptualRoughness;

    // Compute reflectance.
    float reflectance = max(max(specularColor.r, specularColor.g), specularColor.b);

    // For typical incident reflectance range (between 4% to 100%) set the grazing reflectance to 100% for typical fresnel effect.
    // For very low reflectance range on highly diffuse objects (below 4%), incrementally reduce grazing reflecance to 0%.
    float reflectance90 = clamp(reflectance * 25.0, 0.0, 1.0);
    vec3 specularEnvironmentR0 = specularColor.rgb;
    vec3 specularEnvironmentR90 = vec3(1.0, 1.0, 1.0) * reflectance90;

    vec3 h = relightUnit(l+v,n);                        // Half vector between both l and v
    vec3 reflection = -normalize(reflect(v, n));
    reflection.y *= -1.0f;

    float NdotL = clamp(dot(n, l), 0.0, 1.0);
    float NdotV = clamp(abs(dot(n, v)), 0.001, 1.0);
    float NdotH = clamp(dot(n, h), 0.0, 1.0);
    float LdotH = clamp(dot(l, h), 0.0, 1.0);
    float VdotH = clamp(dot(v, h), 0.0, 1.0);

    PBRInfo pbrInputs = PBRInfo(
        NdotL,
        NdotV,
        NdotH,
        LdotH,
        VdotH,
        perceptualRoughness,
        metallic,
        specularEnvironmentR0,
        specularEnvironmentR90,
        alphaRoughness,
        diffuseColor,
        specularColor
    );

    // Calculate the shading terms for the microfacet specular shading model
    vec3 F = specularReflection(pbrInputs);
    float G = geometricOcclusion(pbrInputs);
    float D = microfacetDistribution(pbrInputs);

    // Calculation of analytical lighting contribution
    vec3 diffuseContrib = (1.0 - F) * diffuse(pbrInputs);
    vec3 specContrib = F * G * D / (4.0 * max(NdotL, 0.001) * NdotV);

    nl = NdotL;

    diff = diffuseContrib;
    spec = specContrib;
}
float relightSphere(float c, float s2)
{
    c = clamp(c, -1.0, 1.0);
    s2 = clamp(s2, 0.0, 1.0);
    if (s2 < 1.e-6) return M_PI*s2*max(c,0.0); // stable point-source limit
    if (c*c > s2) return M_PI * s2 * max(c, 0.0);
    float st = sqrt(max(1.0-c*c, 1.e-8));
    float x = sqrt(max(1.0/max(s2, 1.e-8)-1.0, 0.0));
    float y = clamp(-x*c/st, -1.0, 1.0);
    float sy = sqrt(max(1.0-y*y, 0.0));
    return clamp((c*acos(y)-x*st*sy)*s2 + atan(st*sy, x), 0.0, M_PI);
}
float relightRatio(vec3 n, vec3 lv, float radius)
{
    float d2 = dot(lv,lv);
    if (d2 < radius*radius) return 1.0; // full hemisphere, no centre singularity
    float s2 = min(1.0, radius*radius/max(d2, 1.e-12));
    float c = dot(n, relightUnit(lv,n));
    // Fully visible sphere: return N.L directly, without multiply/divide roundoff.
    if (c >= sqrt(s2)) return c;
    return relightSphere(c, s2) / (M_PI * max(s2, 1.e-20));
}
float relightAtten(float d, float range, float falloff, vec4 shape)
{
    if (range <= 0.0 || d >= range) return 0.0;
    if (shape.y == 0.0) return calcLegacyDistanceAttenuation(d/range, falloff);
    float q = d/range;
    float window = max(1.0-q*q*q*q, 0.0);
    float near = max(shape.x, relight_limits.x);
    float physical = window*window / max(d*d, near*near);
    return min(shape.z*physical, relight_limits.y*calcLegacyDistanceAttenuation(0.0, falloff));
}
vec3 relightRepresentative(vec3 n, vec3 v, vec3 lv, float radius)
{
    vec3 ray = reflect(-v,n);
    // Move TOWARD the closest point on the forward reflection ray.
    vec3 delta = ray * max(dot(lv,ray), 0.0) - lv;
    return relightUnit(lv + delta * min(radius/max(length(delta), 1.e-6), 1.0), n);
}
vec3 relightPbr(vec3 diffuseColor, vec3 specularColor, float roughness, float metallic,
                vec3 n, vec3 v, vec3 lv, float range, float falloff, vec4 shape, float specKeep, float attenDistance)
{
    if (dot(n,lv) <= -shape.x) return vec3(0); // whole sphere below horizon
    float d = length(lv);
    vec3 centre = relightUnit(lv,n);
    vec3 representative = relightRepresentative(n,v,lv,shape.x);
    float nl; vec3 diff, unused, spec;
    relightPunctual(diffuseColor,specularColor,roughness,metallic,n,v,centre,nl,diff,unused);
    relightPunctual(diffuseColor,specularColor,roughness,metallic,n,v,representative,nl,unused,spec);
    float alpha = pow(max(roughness, MIN_PBR_ROUGHNESS), 2.0);
    // Verified Karis eq. 10: r/(3d), NOT r/(2d). Original alpha stays in D.
    float widened = clamp(alpha + shape.x/(3.0*max(d,1.e-4)), 0.0, 1.0);
    float norm = alpha / max(widened,1.e-6);
    float attenuation = relightAtten(attenDistance,range,falloff,shape);
    // Mode 0: E_sphere/(pi*sin^2 sigma) replaces N.L, retaining legacy falloff.
    // Mode 1: algebraically E_sphere * window*gain /
    // (pi*sin^2 sigma*max(d^2,near^2)); one distance term, with floor and cap.
    return attenuation * clamp(diff*relightRatio(n,lv,shape.x) +
        spec*max(dot(n,representative),0.0)*norm*norm*specKeep, vec3(0),vec3(10));
}
vec3 relightPbr(vec3 diffuseColor, vec3 specularColor, float roughness, float metallic,
                vec3 n, vec3 v, vec3 lv, float range, float falloff, vec4 shape, float specKeep)
{
    return relightPbr(diffuseColor,specularColor,roughness,metallic,n,v,lv,range,falloff,shape,specKeep,length(lv));
}
vec3 relightLegacy(sampler2D glossLut, vec3 diffuse, vec4 spec, vec3 n, vec3 v,
                  vec3 lv, float range, float falloff, vec4 shape, out vec3 highlight, float attenDistance)
{
    highlight = vec3(0);
    if (dot(n,lv) <= -shape.x) return vec3(0); // whole sphere below horizon
    float attenuation = relightAtten(attenDistance,range,falloff,shape);
    vec3 result = diffuse * relightRatio(n,lv,shape.x);
    if (spec.a > 0.0)
    {
        vec3 l = relightUnit(lv,n);
        vec3 h = relightUnit(l+v,n);
        // Legacy LUT: move the highlight only. Keep the point light's Fresnel,
        // geometry and angular weight; representative N.L would boost its peak.
        vec3 representative = relightRepresentative(n,v,lv,shape.x);
        float lookupNh = max(dot(n,relightUnit(representative+v,n)),0.0);
        float nh = max(dot(n,h),0.0);
        float nl = max(dot(n,l),0.0);
        float nv = max(dot(n,v),0.0);
        float vh = max(dot(v,h),1.e-5);
        float fres = pow(1.0-vh,5.0)*.4+.5;
        float gt = max(0.0,min(2.0*nh*nv/vh,2.0*nh*nl/vh));
        if (nh > 0.0 && nl > 0.0)
            highlight = min(nl*6.0,1.0)*fres*texture(glossLut,vec2(lookupNh,spec.a)).r*gt/(nh*nl)*spec.rgb;
    }
    highlight *= attenuation;
    return result*attenuation + highlight; // LUT model: no alpha normalization
}
vec3 relightLegacy(sampler2D glossLut, vec3 diffuse, vec4 spec, vec3 n, vec3 v,
                  vec3 lv, float range, float falloff, vec4 shape, out vec3 highlight)
{
    return relightLegacy(glossLut,diffuse,spec,n,v,lv,range,falloff,shape,highlight,length(lv));
}
// [RigRim] ------------------------------------------------------------------
// Rig Rim Light (Cine Light Rig, Phase A).  A per-pixel rim LIGHTING term that
// is evaluated inside every local-light path a rig fixture can reach:
//   * deferred projector / spot        class3/deferred/spotLightF.glsl
//   * deferred omni box + fullscreen   class3/deferred/pointLightF.glsl,
//                                      class3/deferred/multiPointLightF.glsl
//   * forward alpha (PBR + legacy)     pbrCalcPointLightOrSpotLightRim() below,
//                                      class2/deferred/alphaF.glsl,
//                                      class3/deferred/materialF.glsl,
//                                      class2/deferred/sharedActorFxPbrF.glsl
// Because it is a lighting term (no neighbour taps, no edge mask, no history)
// it can never draw an outline - it only brightens grazing, back-lit pixels the
// light already reaches.  This is the "fix by construction" for the screen-space
// rim-glow outline complaint. (The old Phase-1 screen-space Rim Glow post
// pass was a separate feature and has been removed.)
//
// Math (all vectors eye space, unit length; v = surface->camera, l = surface->light):
//   NoV   = |n.v|
//   k_eff = mix(k, min(k, 1), roughness * soften)      grazing exponent, softened
//   g     = (1 - NoV)^k_eff                            grazing width / sharpness
//   w_l   = saturate((n.l + wrap) / (1 + wrap))        wrap lighting
//   b     = smoothstep(b0, b0 + backSoft, -(v.l))      back-light-ness
//   rim   = masterGain * gain * c_l * g * w_l * b * mix(1, diffuseColor, tint)
// c_l is the light's colour x distance attenuation x cone/gobo x shadow EXACTLY
// as the calling path already computed it, but WITHOUT the 3.25 / 3.0 legacy
// balance magic, so gain = 1 means "rim peak ~ this light's own diffuse peak".
//
// Energy / balance: the RIM fixture already lights the subject through the
// normal diffuse+specular path; the rim term is purely ADDITIVE on top of it.
// Balance is therefore per-light gain x master gain, and the result is clamped
// to the same [0,10] range the punctual lights use.
//
// Off path: rig_rim_mode == 0 is the bit-exact no-op.  Every caller wraps its
// rim block in `if (rigRimMode() != 0)` so no extra texture taps / ALU run and
// nothing is added to the existing result; the helper additionally early-outs
// when the effective gain is zero, so an individual light with gain 0 costs
// nothing beyond the uniform branch even while the feature is on.
//
// Packing (uploaded by C++, see rigrim_contract.md):
//   rig_rim_mode      int   0 off | 1 on | 2 debug "rim only" (local lights' base
//                           contribution suppressed; sun/sky untouched)
//   rig_rim_globals   vec4  x master gain, y roughness softening 0..1,
//                           z back-light softness (width of b in -(v.l)),
//                           w surface tint amount 0..1
//   per-light vec4    x gain, y grazing exponent k, z wrap 0..1, w back bias b0
//                     (single light: rig_rim_light; batched: rig_rim_lights[])
//   [TronT1] rig_rim_tron        vec4  x mode (0 off | 1 palette tint | 2 tint + pulse),
//                                      y tint mix 0..1, z CPU pulse multiplier (>= 0,
//                                      1 + amount * (pulse01 - 0.5) * 2), w reserved
//            rig_rim_tron_color  vec4  rgb Tron palette colour (linear), w reserved
//            Uploaded by ALCineRigRim::bindGlobals() (slow bind) AND
//            ALCineRigRim::bindAnimated() (bindDeferredShaderFast fast path,
//            so the per-frame pulse reaches programs that stay fast-bound).
//            Off path: rig_rim_tron.x == 0 (also the GL default for a
//            never-uploaded uniform) -> the branch in rigRimTerm() is not
//            taken and c_l is untouched: bit-exact no-op.
uniform int  rig_rim_mode;
uniform vec4 rig_rim_globals;
uniform vec4 rig_rim_tron;
uniform vec4 rig_rim_tron_color;

// [RigRimQ] Rig Rim Quality (doc/RIGRIM_QUALITY_CONTRACT.md).  Every term is a
// MULTIPLY on the rim, never added light.
//   rig_rim_quality  vec4  x Smoothness   0..1  depth-reconstructed smooth normal mix
//                          y AntiShimmer  0..1  Toksvig softening of the grazing exponent
//                          z Silhouette   0..1  silhouette-band gating (deferred only, Phase 1)
//                          w ActorsOnly   0..1  subject-mask gating (deferred only, Phase 1)
//                    vec4(0) (also the GL default of a never-uploaded uniform) is the
//                    bit-exact OFF path: every `if (rig_rim_quality.N > 0.0)` below is
//                    untaken and rigRimTermEx() evaluates rigRimTerm()'s statements.
//   rig_rim_sil      vec4  x band width: metres at the subject when w > 0, else half-res texels
//                          y band softness 0..1 (fraction of the width)
//                          z near gap: relative depth tolerance |z - z_seed| <= gap * z_seed
//                          w half-res texels per metre at 1 m view depth (0 = x already texels)
//   rigRimAuxMap     sampler2D RGBA16  full-res : xy octahedral smooth normal (eye space),
//                                                 z Toksvig |n_bar| 0..1, w subject mask 0/1
//   rigRimSilMap     sampler2D RGBA16F half-res : xy signed OFFSET to the nearest seed in half-res
//                                                 texels (fp16-exact integers), z seed view depth (m),
//                                                 w Euclidean distance to the seed (half-res texels,
//                                                 >= RIG_RIM_DT_BIG when none within RIG_RIM_DT_R).
//                                                 Host binds it NEAREST (TFO_POINT); read with
//                                                 textureLod(..., 0) because consumers sit in
//                                                 non-uniform flow / after discards.
// Both samplers are only referenced from rigRimLoadPixel(); programs that never
// call it (forward alpha, soften, ...) leave them inactive (no texture unit).
uniform vec4 rig_rim_quality;
uniform vec4 rig_rim_sil;
uniform sampler2D rigRimAuxMap;
uniform sampler2D rigRimSilMap;

// [RigRimQ] Per-pixel quality inputs.  Defined identically in every compile
// unit that passes it across the unit boundary (GLSL 4.20 4.1.8: structures
// with the same name / member sequence are the same type), guarded so a
// preamble-injected copy (like GBufferInfo) would not duplicate it.
#ifndef RIG_RIM_PIXEL_DEFINED
#define RIG_RIM_PIXEL_DEFINED 1
struct RigRimPixel
{
    vec3  n_geo;    // smooth / geometric normal, eye space, unit (neutral: the shading normal)
    float n_len;    // Toksvig |n_bar| 0..1 (neutral 1.0)
    float prox;     // silhouette proximity 0..1 (neutral 1.0)
    float subject;  // subject mask 0..1 (neutral 1.0)
};
#endif

const float RIG_RIM_DT_R   = 24.0;   // windowed DT radius, half-res texels (rigRimSilDtF.glsl)
const float RIG_RIM_DT_BIG = 1.0e4;  // "no seed" distance sentinel

int rigRimMode()
{
    return rig_rim_mode;
}

RigRimPixel rigRimNeutralPixel(vec3 n)
{
    RigRimPixel px;
    px.n_geo   = n;
    px.n_len   = 1.0;
    px.prox    = 1.0;
    px.subject = 1.0;
    return px;
}

// [RigRimQ] Forward-alpha local pixel: the interpolated vertex normal is the
// "smooth" normal, Toksvig length from the shading normal's screen derivative
// (|avg of two unit normals an angle fw apart| = cos(fw/2) ~ 1 - fw^2/8).  The
// caller evaluates fwidth() itself, in uniform control flow.  Silhouette and
// subject stay neutral (Phase 2).
RigRimPixel rigRimForwardPixel(vec3 n_sh, vec3 n_geo, float n_fwidth)
{
    RigRimPixel px = rigRimNeutralPixel(n_sh);
    float l2 = dot(n_geo, n_geo);
    if (l2 > 1.0e-12)
    {
        px.n_geo = n_geo * inversesqrt(l2);
        if (dot(px.n_geo, n_sh) < 0.0)
        {
            px.n_geo = -px.n_geo; // two-sided faces: follow the shading normal's side
        }
    }
    px.n_len = clamp(1.0 - 0.125 * n_fwidth * n_fwidth, 0.0, 1.0);
    return px;
}

// Octahedral normal encoding (Cigolle et al. 2014) for rigRimAuxMap.xy.
vec2 rigRimSignNotZero(vec2 v)
{
    return vec2(v.x >= 0.0 ? 1.0 : -1.0, v.y >= 0.0 ? 1.0 : -1.0);
}

vec2 rigRimOctEncode(vec3 n)
{
    vec2 p = n.xy * (1.0 / max(abs(n.x) + abs(n.y) + abs(n.z), 1.0e-6));
    p = (n.z <= 0.0) ? ((1.0 - abs(p.yx)) * rigRimSignNotZero(p)) : p;
    return p * 0.5 + 0.5;
}

vec3 rigRimOctDecode(vec2 e)
{
    e = e * 2.0 - 1.0;
    vec3 n = vec3(e.xy, 1.0 - abs(e.x) - abs(e.y));
    if (n.z < 0.0)
    {
        n.xy = (1.0 - abs(n.yx)) * rigRimSignNotZero(n.xy);
    }
    float l2 = dot(n, n);
    return l2 > 1.0e-12 ? n * inversesqrt(l2) : vec3(0.0, 0.0, 1.0);
}

// Silhouette band width in half-res texels for a pixel at view depth z_view
// (metres, > 0).  cm mode (rig_rim_sil.w > 0) is resolution / tile invariant:
// width_m * texels_per_metre_at_1m / z.  Clamped to the DT window so the soft
// outer edge never reaches the sentinel; the host shows a status when the
// subject-distance width is clamped.
float rigRimSilBandWidth(float z_view)
{
    float w = max(rig_rim_sil.x, 0.0);
    if (rig_rim_sil.w > 0.0)
    {
        w = w * rig_rim_sil.w / max(z_view, 0.05);
    }
    float soft = clamp(rig_rim_sil.y, 0.0, 1.0);
    return min(w, RIG_RIM_DT_R / (1.0 + soft));
}

// band(dist) * near_ok(z vs seed depth): 1 inside the band on the near side of
// the silhouette, 0 on the far (background) side and away from edges.
float rigRimSilProx(vec2 tc, float z_view)
{
    vec4  s    = textureLod(rigRimSilMap, tc, 0.0); // NEAREST: offset / depth must not blend across seeds
    if (s.w >= RIG_RIM_DT_BIG)
    {
        return 0.0;
    }
    float w    = max(rigRimSilBandWidth(z_view), 1.0e-3);
    float soft = clamp(rig_rim_sil.y, 0.0, 1.0);
    float band = 1.0 - smoothstep(w * (1.0 - soft), w * (1.0 + soft) + 1.0e-4, s.w);
    float gap  = max(rig_rim_sil.z, 1.0e-4);
    float rel  = abs(z_view - s.z) / max(s.z, 1.0e-3);
    float near_ok = 1.0 - smoothstep(gap, 2.0 * gap, rel);
    return band * near_ok;
}

// [RigRimQ] Deferred pixel loader (main view, RIG_RIM_AUX programs).  Each
// sampler is touched only when its own quality component is non-zero, so the
// host can skip the matching pre-pass and the texture is never read stale.
RigRimPixel rigRimLoadPixel(vec2 tc, vec3 n_sh, float z_view)
{
    RigRimPixel px = rigRimNeutralPixel(n_sh);
    if (rig_rim_quality.x > 0.0 || rig_rim_quality.y > 0.0 || rig_rim_quality.w > 0.0)
    {
        vec4 a     = textureLod(rigRimAuxMap, tc, 0.0);
        px.n_geo   = rigRimOctDecode(a.xy);
        px.n_len   = clamp(a.z, 0.0, 1.0);
        px.subject = clamp(a.w, 0.0, 1.0);
    }
    if (rig_rim_quality.z > 0.0)
    {
        px.prox = rigRimSilProx(tc, z_view);
    }
    return px;
}

// [TronT1] Recolour a rim light's colour toward the Tron palette: keep the
// light's own luminance (attenuation / cone / gobo / shadow weighting stays
// physical) and swap the hue by `mix`; mode 2 additionally scales by the CPU
// pulse multiplier from the shared Tron clock.
vec3 tronRimTint(vec3 c_l)
{
    float cl = dot(c_l, vec3(0.2126, 0.7152, 0.0722));
    // [TronT1 P2-6 fix] rig_rim_tron_color is the raw palette colour (e.g.
    // Ares Red ~= (1.00, 0.08, 0.06), luminance ~0.27), not a hue-only unit
    // vector -- multiplying cl by it unnormalised made the result's
    // luminance cl * lum(tint) instead of cl, silently darkening the tint
    // (Ares Red down to ~27% of the light's real brightness) even though
    // this function's whole contract is "keep the light's own luminance,
    // swap the hue". Normalise by the tint's own luminance (guarded against
    // a black/near-black palette colour) so cl * tint_norm has luminance
    // cl again, matching the documented behaviour.
    vec3  tint     = max(rig_rim_tron_color.rgb, vec3(0.0));
    float tint_lum = max(dot(tint, vec3(0.2126, 0.7152, 0.0722)), 1.0e-4);
    // [TronT1 P2] Pure luminance normalisation over-drives saturated palettes
    // (pure blue x14); pure peak normalisation darkens them. Use the geometric
    // mean of luminance and peak: blue x3.7, Ares red keeps ~52% luminance.
    float tint_max = max(max(tint.r, tint.g), max(tint.b, 1.0e-4));
    float tint_den = sqrt(tint_lum * tint_max);
    c_l = mix(c_l, cl * (tint / tint_den), clamp(rig_rim_tron.y, 0.0, 1.0));
    if (rig_rim_tron.x > 1.5)
    {
        c_l *= max(rig_rim_tron.z, 0.0);
    }
    return c_l;
}

vec3 rigRimTerm(vec3 n,            // surface normal (eye space)
                vec3 v,            // surface -> camera, normalized
                vec3 l,            // surface -> light, normalized
                vec3 c_l,          // light colour x atten x cone/gobo x shadow
                vec3 diffuseColor, // for the optional surface tint
                float roughness,   // perceptual roughness (legacy: 1 - glossiness)
                vec4 rim)          // x gain, y k, z wrap, w back bias
{
    float gain = rim.x * rig_rim_globals.x;
    if (gain <= 0.0)
    {
        return vec3(0.0);
    }

    // [TronT1] Tron palette tint of the rim light colour only (mode 0 = untaken).
    if (rig_rim_tron.x > 0.5)
    {
        c_l = tronRimTint(c_l);
    }

    float NoV    = abs(dot(n, v));
    float soften = clamp(roughness, 0.0, 1.0) * clamp(rig_rim_globals.y, 0.0, 1.0);
    float k      = max(rim.y, 0.05);
    float k_eff  = mix(k, min(k, 1.0), soften); // rough surfaces get a wider, softer rim
    float g      = pow(clamp(1.0 - NoV, 0.0, 1.0), k_eff);

    float wrap   = clamp(rim.z, 0.0, 1.0);
    float w_l    = clamp((dot(n, l) + wrap) / (1.0 + wrap), 0.0, 1.0);

    float b0     = rim.w;
    float b1     = b0 + max(rig_rim_globals.z, 1.0e-4);
    float b      = smoothstep(b0, b1, -dot(v, l));

    vec3  tint   = mix(vec3(1.0), diffuseColor, clamp(rig_rim_globals.w, 0.0, 1.0));

    return clamp(gain * c_l * (g * w_l * b) * tint, vec3(0.0), vec3(10.0));
}

// [RigRimQ] rigRimTerm() plus the quality terms.  The statements of
// rigRimTerm() above are reproduced here in the same order; with
// rig_rim_quality == vec4(0) every quality branch is untaken, n_r == n and
// k_eff is untouched, so the arithmetic is identical (bit-exact off path).
// All light paths (deferred and forward) call this; rigRimTerm() itself is
// kept verbatim as the reference.
vec3 rigRimTermEx(vec3 n,            // shading normal (eye space)
                  vec3 v,            // surface -> camera, normalized
                  vec3 l,            // surface -> light, normalized
                  vec3 c_l,          // light colour x atten x cone/gobo x shadow
                  vec3 diffuseColor, // for the optional surface tint
                  float roughness,   // perceptual roughness (legacy: 1 - glossiness)
                  vec4 rim,          // x gain, y k, z wrap, w back bias
                  RigRimPixel px)    // quality inputs (rigRimNeutralPixel == plain rim)
{
    float gain = rim.x * rig_rim_globals.x;
    if (gain <= 0.0)
    {
        return vec3(0.0);
    }

    // [TronT1] Tron palette tint of the rim light colour only (mode 0 = untaken).
    if (rig_rim_tron.x > 0.5)
    {
        c_l = tronRimTint(c_l);
    }

    // [RigRimQ] 1. smooth normal: blend the shading normal toward the
    // depth-reconstructed (deferred) / vertex (forward) normal.
    vec3 n_r = n;
    if (rig_rim_quality.x > 0.0)
    {
        n_r = normalize(mix(n, px.n_geo, clamp(rig_rim_quality.x, 0.0, 1.0)));
    }

    float NoV    = abs(dot(n_r, v));
    float soften = clamp(roughness, 0.0, 1.0) * clamp(rig_rim_globals.y, 0.0, 1.0);
    float k      = max(rim.y, 0.05);
    float k_eff  = mix(k, min(k, 1.0), soften); // rough surfaces get a wider, softer rim

    // [RigRimQ] 1. anti-shimmer: Toksvig.  Where the normals inside the pixel
    // footprint disagree (|n_bar| < 1) the grazing exponent is softened, so a
    // noisy normal map cannot sparkle the thin high-k rim.
    if (rig_rim_quality.y > 0.0)
    {
        float len = clamp(px.n_len, 0.0, 1.0);
        float ft  = len / max(len + k_eff * (1.0 - len), 1.0e-4);
        k_eff     = max(k_eff * mix(1.0, ft, clamp(rig_rim_quality.y, 0.0, 1.0)), 0.05);
    }

    float g      = pow(clamp(1.0 - NoV, 0.0, 1.0), k_eff);

    float wrap   = clamp(rim.z, 0.0, 1.0);
    float w_l    = clamp((dot(n_r, l) + wrap) / (1.0 + wrap), 0.0, 1.0);

    float b0     = rim.w;
    float b1     = b0 + max(rig_rim_globals.z, 1.0e-4);
    float b      = smoothstep(b0, b1, -dot(v, l));

    vec3  tint   = mix(vec3(1.0), diffuseColor, clamp(rig_rim_globals.w, 0.0, 1.0));

    vec3 rim_rgb = clamp(gain * c_l * (g * w_l * b) * tint, vec3(0.0), vec3(10.0));

    // [RigRimQ] 2. silhouette-hugging band and 3. actors only: pure multiplies.
    if (rig_rim_quality.z > 0.0)
    {
        rim_rgb *= mix(1.0, clamp(px.prox, 0.0, 1.0), clamp(rig_rim_quality.z, 0.0, 1.0));
    }
    if (rig_rim_quality.w > 0.0)
    {
        rim_rgb *= mix(1.0, clamp(px.subject, 0.0, 1.0), clamp(rig_rim_quality.w, 0.0, 1.0));
    }
    return rim_rgb;
}

// [RigRim] Forward-alpha punctual light WITH the rim term.  The rim-less
// pbrCalcPointLightOrSpotLight() below forwards here with rim = 0, which the
// helper early-outs on, so every existing caller keeps its exact result.
// [AvatarLightResponse] spec_keep scales ONLY the evaluated specular term
// (specPunc) after the BRDF; the old name forwards 1.0 (x * 1.0 is exact).
// [AvatarSilhouette] Verbatim pre-RimSplit reference for the T21 permutation.
// [RigRimQ] Trailing RigRimPixel so the parity check still holds with the
// quality terms on; the px-less name below forwards a neutral pixel.
// [RigRimQ review P2-10] No longer behind `#ifdef ALR_RIMSPLIT_PARITY`: this
// file is compiled ONCE with the global defines (llshadermgr.cpp caches
// fragment objects by filename), while ALR_RIMSPLIT_PARITY is a PER-PROGRAM
// permutation (llviewershadermgr.cpp 2295 / 2382), so the guarded bodies never
// existed and a parity build of pbralphaF / sharedActorFxPbrF could not link.
// Unconditional definitions cost nothing: every non-parity program strips
// them as unreferenced.
vec3 pbrCalcPointLightOrSpotLightRimKeepRef(vec3 diffuseColor, vec3 specularColor,
                    float perceptualRoughness,
                    float metallic,
                    vec3 n, // normal
                    vec3 p, // pixel position
                    vec3 v, // view vector (negative normalized pixel position)
                    vec3 lp, // light position
                    vec3 ld, // light direction (for spotlights)
                    vec3 lightColor,
                    float lightSize, float falloff, float is_pointlight, float ambiance,
                    RELIGHT_PARAM vec4 rim, // [RigRim] per-light rim params (rig_rim_lights[i])
                    float spec_keep, // [AvatarLightResponse] 1.0 = untouched
                    RigRimPixel px) // [RigRimQ]
{
    vec3 color = vec3(0,0,0);

    if ((relight_enabled & 1) != 0 && shape.x > 0.0)
    {
        vec3 delta = lp-p;
        vec3 centre = relightUnit(delta,n);
        float spot = max(dot(-ld,centre),is_pointlight);
        float attenuation = calcLegacyDistanceAttenuation(length(delta)/lightSize,falloff); // Rig Rim is shape-independent
        vec3 direct = spot*spot*lightColor*3.0 * relightPbr(diffuseColor,specularColor,
            perceptualRoughness,metallic,n,v,delta,lightSize,falloff,shape,spec_keep);
        vec3 rimLight = vec3(0);
        bool hasRim = rig_rim_mode != 0;
        if (hasRim)
        {
            rimLight = rigRimTermEx(n,v,centre,spot*spot*attenuation*lightColor,
                diffuseColor,perceptualRoughness,rim,px);
            if (rig_rim_mode == 2) direct = vec3(0);
        }
        return (direct+rimLight) * (classic_mode > 0 ? .9 : 1.0);
    }
    vec3 lv = lp.xyz - p;

    float lightDist = length(lv);

    float dist = lightDist / lightSize;
    if (dist <= 1.0)
    {
        lv /= lightDist;

        float dist_atten = calcLegacyDistanceAttenuation(dist, falloff);

        // spotlight coefficient.
        float spot = max(dot(-ld, lv), is_pointlight);
        // spot*spot => GL_SPOT_EXPONENT=2
        float spot_atten = spot*spot;

        vec3 intensity = spot_atten * dist_atten * lightColor * 3.0; //magic number to balance with legacy materials

        float nl = 0;
        vec3 diffPunc = vec3(0);
        vec3 specPunc = vec3(0);

        pbrPunctual(diffuseColor, specularColor, perceptualRoughness, metallic, n.xyz, v, lv, nl, diffPunc, specPunc);
        specPunc *= spec_keep; // [AvatarLightResponse]
        color = intensity * clamp(nl * (diffPunc + specPunc), vec3(0), vec3(10));

        // [RigRim] additive rim from this forward light (cone + attenuation
        // weighted; forward lights carry no per-light shadow).
        if (rig_rim_mode != 0)
        {
            vec3 rim_col = rigRimTermEx(n.xyz, v, lv, spot_atten * dist_atten * lightColor,
                                        diffuseColor, perceptualRoughness, rim, px);
            if (rig_rim_mode == 2)
            {
                color = vec3(0.0); // debug: rim only
            }
            color += rim_col;
        }
    }
    float final_scale = 1.0;
    if (classic_mode > 0)
        final_scale = 0.9;
    return color * final_scale;
}

vec3 pbrCalcPointLightOrSpotLightRimKeepRef(vec3 diffuseColor, vec3 specularColor,
                    float perceptualRoughness,
                    float metallic,
                    vec3 n, // normal
                    vec3 p, // pixel position
                    vec3 v, // view vector (negative normalized pixel position)
                    vec3 lp, // light position
                    vec3 ld, // light direction (for spotlights)
                    vec3 lightColor,
                    float lightSize, float falloff, float is_pointlight, float ambiance,
                    RELIGHT_PARAM vec4 rim, // [RigRim] per-light rim params (rig_rim_lights[i])
                    float spec_keep) // [AvatarLightResponse] 1.0 = untouched
{
    return pbrCalcPointLightOrSpotLightRimKeepRef(diffuseColor, specularColor, perceptualRoughness, metallic,
                                                  n, p, v, lp, ld, lightColor, lightSize, falloff,
                                                  is_pointlight, ambiance, RELIGHT_FORWARD rim, spec_keep,
                                                  rigRimNeutralPixel(n.xyz));
}

// [AvatarSilhouette] Evaluate the raw non-rim and rim components separately.
// final_scale is returned, never distributed, so the compatibility wrapper
// below preserves today's add-then-multiply operation order exactly.
// [RigRimQ] Trailing RigRimPixel; the px-less overload forwards a neutral pixel.
void pbrCalcPointLightOrSpotLightRimSplit(vec3 diffuseColor, vec3 specularColor,
                    float perceptualRoughness,
                    float metallic,
                    vec3 n, // normal
                    vec3 p, // pixel position
                    vec3 v, // view vector (negative normalized pixel position)
                    vec3 lp, // light position
                    vec3 ld, // light direction (for spotlights)
                    vec3 lightColor,
                    float lightSize, float falloff, float is_pointlight, float ambiance,
                    RELIGHT_PARAM vec4 rim, // [RigRim] per-light rim params (rig_rim_lights[i])
                    float spec_keep, // [AvatarLightResponse] 1.0 = untouched
                    RigRimPixel px, // [RigRimQ]
                    out vec3 nonrim, out vec3 rim_raw, out bool rim_on,
                    out float light_scale)
{
    nonrim = vec3(0,0,0);
    rim_raw = vec3(0.0);
    rim_on = false;

    if ((relight_enabled & 1) != 0 && shape.x > 0.0)
    {
        vec3 delta = lp-p;
        vec3 centre = relightUnit(delta,n);
        float spot = max(dot(-ld,centre),is_pointlight);
        float attenuation = calcLegacyDistanceAttenuation(length(delta)/lightSize,falloff); // Rig Rim is shape-independent
        vec3 direct = spot*spot*lightColor*3.0 * relightPbr(diffuseColor,specularColor,
            perceptualRoughness,metallic,n,v,delta,lightSize,falloff,shape,spec_keep);
        vec3 rimLight = vec3(0);
        bool hasRim = rig_rim_mode != 0;
        if (hasRim)
        {
            rimLight = rigRimTermEx(n,v,centre,spot*spot*attenuation*lightColor,
                diffuseColor,perceptualRoughness,rim,px);
            if (rig_rim_mode == 2) direct = vec3(0);
        }
        nonrim = direct;
        rim_raw = rimLight;
        rim_on = hasRim;
        light_scale = classic_mode > 0 ? .9 : 1.0;
        return;
    }
    vec3 lv = lp.xyz - p;

    float lightDist = length(lv);

    float dist = lightDist / lightSize;
    if (dist <= 1.0)
    {
        lv /= lightDist;

        float dist_atten = calcLegacyDistanceAttenuation(dist, falloff);

        // spotlight coefficient.
        float spot = max(dot(-ld, lv), is_pointlight);
        // spot*spot => GL_SPOT_EXPONENT=2
        float spot_atten = spot*spot;

        vec3 intensity = spot_atten * dist_atten * lightColor * 3.0; //magic number to balance with legacy materials

        float nl = 0;
        vec3 diffPunc = vec3(0);
        vec3 specPunc = vec3(0);

        pbrPunctual(diffuseColor, specularColor, perceptualRoughness, metallic, n.xyz, v, lv, nl, diffPunc, specPunc);
        specPunc *= spec_keep; // [AvatarLightResponse]
        nonrim = intensity * clamp(nl * (diffPunc + specPunc), vec3(0), vec3(10));

        // [RigRim] additive rim from this forward light (cone + attenuation
        // weighted; forward lights carry no per-light shadow).
        rim_on = rig_rim_mode != 0;
        if (rim_on)
        {
            rim_raw = rigRimTermEx(n.xyz, v, lv, spot_atten * dist_atten * lightColor,
                                   diffuseColor, perceptualRoughness, rim, px);
            if (rig_rim_mode == 2)
            {
                nonrim = vec3(0.0); // debug: rim only
            }
        }
    }
    light_scale = 1.0;
    if (classic_mode > 0)
        light_scale = 0.9;
}

void pbrCalcPointLightOrSpotLightRimSplit(vec3 diffuseColor, vec3 specularColor,
                    float perceptualRoughness,
                    float metallic,
                    vec3 n, // normal
                    vec3 p, // pixel position
                    vec3 v, // view vector (negative normalized pixel position)
                    vec3 lp, // light position
                    vec3 ld, // light direction (for spotlights)
                    vec3 lightColor,
                    float lightSize, float falloff, float is_pointlight, float ambiance,
                    RELIGHT_PARAM vec4 rim, // [RigRim] per-light rim params (rig_rim_lights[i])
                    float spec_keep, // [AvatarLightResponse] 1.0 = untouched
                    out vec3 nonrim, out vec3 rim_raw, out bool rim_on,
                    out float light_scale)
{
    pbrCalcPointLightOrSpotLightRimSplit(diffuseColor, specularColor,
        perceptualRoughness, metallic, n, p, v, lp, ld, lightColor, lightSize,
        falloff, is_pointlight, ambiance, RELIGHT_FORWARD rim, spec_keep, rigRimNeutralPixel(n.xyz),
        nonrim, rim_raw, rim_on, light_scale);
}

vec3 pbrCalcPointLightOrSpotLightRimKeep(vec3 diffuseColor, vec3 specularColor,
                    float perceptualRoughness,
                    float metallic,
                    vec3 n, // normal
                    vec3 p, // pixel position
                    vec3 v, // view vector (negative normalized pixel position)
                    vec3 lp, // light position
                    vec3 ld, // light direction (for spotlights)
                    vec3 lightColor,
                    float lightSize, float falloff, float is_pointlight, float ambiance,
                    RELIGHT_PARAM vec4 rim, // [RigRim] per-light rim params (rig_rim_lights[i])
                    float spec_keep, // [AvatarLightResponse] 1.0 = untouched
                    RigRimPixel px) // [RigRimQ]
{
    vec3 nonrim;
    vec3 rim_raw;
    bool rim_on;
    float light_scale;
    pbrCalcPointLightOrSpotLightRimSplit(diffuseColor, specularColor,
        perceptualRoughness, metallic, n, p, v, lp, ld, lightColor, lightSize,
        falloff, is_pointlight, ambiance, RELIGHT_FORWARD rim, spec_keep, px, nonrim, rim_raw,
        rim_on, light_scale);
    vec3 color = nonrim;
    if (rim_on)
    {
        color += rim_raw;
    }
    return color * light_scale;
}

vec3 pbrCalcPointLightOrSpotLightRimKeep(vec3 diffuseColor, vec3 specularColor,
                    float perceptualRoughness,
                    float metallic,
                    vec3 n, // normal
                    vec3 p, // pixel position
                    vec3 v, // view vector (negative normalized pixel position)
                    vec3 lp, // light position
                    vec3 ld, // light direction (for spotlights)
                    vec3 lightColor,
                    float lightSize, float falloff, float is_pointlight, float ambiance,
                    RELIGHT_PARAM vec4 rim, // [RigRim] per-light rim params (rig_rim_lights[i])
                    float spec_keep) // [AvatarLightResponse] 1.0 = untouched
{
    return pbrCalcPointLightOrSpotLightRimKeep(diffuseColor, specularColor, perceptualRoughness, metallic,
                                               n, p, v, lp, ld, lightColor, lightSize, falloff,
                                               is_pointlight, ambiance, RELIGHT_FORWARD rim, spec_keep,
                                               rigRimNeutralPixel(n.xyz));
}

// [AvatarLightResponse] old name: forwards spec_keep = 1.0.
vec3 pbrCalcPointLightOrSpotLightRim(vec3 diffuseColor, vec3 specularColor,
                    float perceptualRoughness,
                    float metallic,
                    vec3 n, // normal
                    vec3 p, // pixel position
                    vec3 v, // view vector (negative normalized pixel position)
                    vec3 lp, // light position
                    vec3 ld, // light direction (for spotlights)
                    vec3 lightColor,
                    float lightSize, float falloff, float is_pointlight, float ambiance,
                    RELIGHT_PARAM vec4 rim) // [RigRim] per-light rim params (rig_rim_lights[i])
{
    return pbrCalcPointLightOrSpotLightRimKeep(diffuseColor, specularColor, perceptualRoughness, metallic,
                                               n, p, v, lp, ld, lightColor, lightSize, falloff,
                                               is_pointlight, ambiance, RELIGHT_FORWARD rim, 1.0);
}

vec3 pbrCalcPointLightOrSpotLight(vec3 diffuseColor, vec3 specularColor,
                    float perceptualRoughness,
                    float metallic,
                    vec3 n, // normal
                    vec3 p, // pixel position
                    vec3 v, // view vector (negative normalized pixel position)
                    vec3 lp, // light position
                    vec3 ld, // light direction (for spotlights)
                    vec3 lightColor,
                    float lightSize, float falloff, float is_pointlight, float ambiance)
{
    // [RigRim] rim = 0 -> rigRimTerm() early-outs -> result identical to the
    // pre-RigRim body of this function.
    return pbrCalcPointLightOrSpotLightRim(diffuseColor, specularColor, perceptualRoughness, metallic,
                                           n, p, v, lp, ld, lightColor, lightSize, falloff,
                                           is_pointlight, ambiance, RELIGHT_DEFAULT vec4(0.0));
}

void calcDiffuseSpecular(vec3 baseColor, float metallic, inout vec3 diffuseColor, inout vec3 specularColor)
{
    vec3 f0 = vec3(0.04);
    diffuseColor = baseColor*(vec3(1.0)-f0);
    diffuseColor *= 1.0 - metallic;
    specularColor = mix(f0, baseColor, metallic);
}

// [EnvIntensity v2] Shadow lift: raises the diffuse ambient / irradiance term
// where the sun (or moon) does not reach. shadow_lift_gain = 2^EV - 1 (0 = off;
// 0 in classic mode and in every probe capture; uploaded via SG_ANY from
// LLSettingsVOSky::applySpecial). Weight has a knee: any pixel with
// NdotL * scol >= SHADOW_LIFT_KNEE is EXACTLY unchanged, full shadow (scol 0
// or NdotL <= 0) gets the full gain, the terminator ramps over 0..knee.
// Callers apply it AFTER adjustIrradiance (SSAO ceiling) and BEFORE the sun
// term / pbrBaseLight, so material AO still multiplies the lifted value and
// occluded corners lift by the same relative factor as open shadow.
uniform float shadow_lift_gain;
const float SHADOW_LIFT_KNEE = 0.25;

vec3 applyShadowLift(vec3 irradiance, vec3 norm, vec3 light_dir, float scol)
{
    if (shadow_lift_gain <= 0.0)
    {
        return irradiance; // off path untouched
    }
    float sun_vis = clamp(dot(norm, light_dir), 0.0, 1.0) * scol; // 1 = fully lit
    float w = 1.0 - smoothstep(0.0, SHADOW_LIFT_KNEE, sun_vis);   // 0 for sun_vis >= knee
    return irradiance * (1.0 + shadow_lift_gain * w);
}

// [AvatarLightResponse] spec_keep scales ONLY the evaluated specular terms after the BRDF
// (IBL specular and the sun's punctual specular); F0, specularColor, radiance and every
// diffuse term are untouched. The old name below forwards 1.0 (x * 1.0 is exact).
vec3 pbrBaseLightKeep(vec3 diffuseColor, vec3 specularColor, float metallic, vec3 v, vec3 norm, float perceptualRoughness, vec3 light_dir, vec3 sunlit, float scol, vec3 radiance, vec3 irradiance, vec3 colorEmissive, float ao, vec3 additive, vec3 atten, float spec_keep)
{
    perceptualRoughness = max(perceptualRoughness, MIN_PBR_ROUGHNESS);
    vec3 color = vec3(0);

    float NdotV = clamp(abs(dot(norm, v)), 0.001, 1.0);
    vec3 iblDiff = vec3(0);
    vec3 iblSpec = vec3(0);
    pbrIbl(diffuseColor, specularColor, radiance, irradiance, ao, NdotV, perceptualRoughness, iblDiff, iblSpec);
    iblSpec *= spec_keep; // [AvatarLightResponse]

    color += iblDiff;

    // For classic mode, we use a special version of pbrPunctual that basically gives us a deconstructed form of the lighting.
    float nl = 0;
    vec3 diffPunc = vec3(0);
    vec3 specPunc = vec3(0);
    pbrPunctual(diffuseColor, specularColor, perceptualRoughness, metallic, norm, v, normalize(light_dir), nl, diffPunc, specPunc);
    specPunc *= spec_keep; // [AvatarLightResponse]

    // Depending on the sky, we combine these differently.
    if (classic_mode > 0)
    {
        irradiance.rgb = srgb_to_linear(irradiance * 0.9); // BINGO

        // Reconstruct the diffuse lighting that we do for blinn-phong materials here.
        // A special note about why we do some really janky stuff for classic mode.
        // Since adding classic mode, we've moved the lambertian diffuse multiply out from pbrPunctual and instead handle it in the different light type calcs.
        // This will never be 100% correct, but at the very least we can make it look mostly correct with legacy skies and classic mode.

        float da = pow(nl, 1.2);

        vec3 sun_contrib = vec3(min(da, scol));

        // Multiply by PI to account for lambertian diffuse colors.  Otherwise things will be too dark when lit by the sun on legacy skies.
        sun_contrib = srgb_to_linear(linear_to_srgb(sun_contrib) * sunlit * 0.7) * M_PI;

        // Manually recombine everything here.  We have to separate the shading to ensure that lighting is able to more closely match blinn-phong.
        vec3 finalAmbient = irradiance.rgb * diffuseColor.rgb; // BINGO
        vec3 finalSun = clamp(sun_contrib * ((diffPunc.rgb + specPunc.rgb) * scol), vec3(0), vec3(10)); // QUESTIONABLE BINGO?
        color.rgb = srgb_to_linear(linear_to_srgb(finalAmbient) + (linear_to_srgb(finalSun) * 1.1));
        //color.rgb = sun_contrib * diffuseColor.rgb;
    }
    else
    {
        color += clamp(nl * (diffPunc + specPunc), vec3(0), vec3(10)) * sunlit * 3.0 * scol;
    }

    color.rgb += iblSpec.rgb;

    color += colorEmissive;

    return color;
}

// [AvatarLightResponse] old name: forwards spec_keep = 1.0.
vec3 pbrBaseLight(vec3 diffuseColor, vec3 specularColor, float metallic, vec3 v, vec3 norm, float perceptualRoughness, vec3 light_dir, vec3 sunlit, float scol, vec3 radiance, vec3 irradiance, vec3 colorEmissive, float ao, vec3 additive, vec3 atten)
{
    return pbrBaseLightKeep(diffuseColor, specularColor, metallic, v, norm, perceptualRoughness, light_dir, sunlit, scol, radiance, irradiance, colorEmissive, ao, additive, atten, 1.0);
}

uniform vec4 waterPlane;
uniform float waterSign;

// discard if given position in eye space is on the wrong side of the waterPlane according to waterSign
void waterClip(vec3 pos)
{
    // TODO: make this less branchy
    if (waterSign > 0)
    {
        if ((dot(pos.xyz, waterPlane.xyz) + waterPlane.w) < 0.0)
        {
            discard;
        }
    }
    else
    {
        if ((dot(pos.xyz, waterPlane.xyz) + waterPlane.w) > 0.0)
        {
            discard;
        }
    }

}

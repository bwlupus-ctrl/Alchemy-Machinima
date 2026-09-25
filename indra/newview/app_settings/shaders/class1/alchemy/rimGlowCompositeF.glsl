/**
 * @file rimGlowCompositeF.glsl
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

// Virtual Cinema Rim Glow -- Phase-1 COMPOSITE (Auto Rim only).
//
// Three-lobe interior fill + outward halo, real-normal Schlick Fresnel, and a
// directional form term driven by the gathered light direction. Read-scratch-
// blit idiom: diffuseRect is the scratch copy of mRT->screen, the draw target
// is mRT->screen. Output = scene + rim in SCENE-linear units (exposure and
// tonemap are applied later by colorCorrect / tonemap, once). Alpha is passed
// through untouched (it carries the prim-glow tag the bloom extract reads).
//
// Lobes (all premultiplied by edge coverage; rgb/a = coverage-weighted colour):
//   core = rimMask (sigma 0), wrap = rimWrap (sigma_w, full-res),
//   atmosphere = rimGlow (sigma_g, half-res, bilinear upsample).
// Each blurred lobe is peak-normalized by the host-computed WrapNorm/GlowNorm
// (exact discrete response of the blur kernel to a RimWidth-wide band), then
// shaped by pow(envelope, falloff) on the COVERAGE only (never on colour), and
// the envelope is clamped to 1 (corners / thin closed features overshoot).

out vec4 frag_color;

in vec2 vary_fragcoord;

uniform sampler2D diffuseRect;   // linear HDR scene (scratch copy of mRT->screen)
uniform sampler2D exposureMap;
uniform sampler2D rimMask;       // full-res: rgb = rad*edge, a = edge
uniform sampler2D rimSubj;       // full-res: M
uniform sampler2D rimWrap;       // full-res: blur(sigma_w) of rimMask
uniform sampler2D rimGlow;       // half-res: blur(sigma_g) of downsampled rimMask (LINEAR filter)
uniform sampler2D rimGlowDir;    // half-res: blur(sigma_g) of downsampled dir (LINEAR filter)
uniform float     exposure;
uniform int       rim_exposure_lock;
uniform vec2      screen_res;

// --- Fresnel / form ---
uniform float RimFresnel;        // 0 flat wash .. 1 full Schlick
uniform float FresnelBase;       // F0 floor kept on camera-facing surfaces (never below this)
uniform float FresnelPower;      // Schlick exponent (5 = physical)
uniform float FormDirectional;   // weight of the light-direction term
uniform float DirectionalWrap;   // 0 tight lobe .. 1 wide
uniform float DirCoherenceLo;    // |mean dir| below -> fully neutral wash
uniform float DirCoherenceHi;    // |mean dir| above -> fully directional

// --- Lobes ---
uniform float AutoWrapFalloff;   // envelope exponent, wrap lobe (sigma_eff = sigma / sqrt(p))
uniform float AutoAtmFalloff;    // envelope exponent, atmosphere lobe
uniform float CoreGain;
uniform float WrapGain;
uniform float AtmGain;
uniform float AutoInteriorFill;  // master interior gain
uniform float AutoInteriorResponse; // 0 even wash .. 1 follows subject brightness
uniform float RespKnee;          // exposed-linear knee of that response
uniform float AutoInteriorTint;  // fill picks up surface chroma (albedo bounce proxy)
uniform float AutoHalo;          // outward atmosphere lobe gain
uniform float HaloTight;         // outward wrap lobe gain
uniform float HaloOcclusion;     // halo respects foreground (1 = physical)
uniform float WrapNorm;          // host: 1 / lobePeak(sigma_w, RimWidth, taps_w)   (or 1.0 energy mode)
uniform float GlowNorm;          // host: 1 / lobePeak(sigma_g/2, RimWidth/2, taps_g) (or 1.0)

// --- Focus ---
uniform float focusZ;
uniform float FocusFeatherRel;

float getDepth(vec2 pos_screen);
vec4  getNorm(vec2 screenpos);
vec4  getNormRaw(vec2 screenpos);
vec4  getPosition(vec2 pos_screen);
vec4  getPositionWithDepth(vec2 pos_screen, float depth);

const vec3  LUMA          = vec3(0.2126, 0.7152, 0.0722);
const float RIM_SKY_DEPTH = 0.999999;

float rimExposure()   // identical to rimGlowMaskF.glsl
{
    float s = texture(exposureMap, vec2(0.5, 0.5)).r;
    if (!(s > 0.0)) s = 1.0;
    if (rim_exposure_lock != 0) s = 1.0;
    return max(exposure, 1e-4) * s;
}

void readNormal(vec2 uv, float raw, out vec3 n, out float ndotv)   // identical to rimGlowMaskF.glsl (B1)
{
    vec4 nraw = getNormRaw(uv);
    bool hasN = (raw < RIM_SKY_DEPTH) && !GET_GBUFFER_FLAG(nraw.w, GBUFFER_FLAG_SKIP_ATMOS);
    n = vec3(0.0);
    ndotv = 1.0;
    if (!hasN) return;
    vec3 nn = getNorm(uv).xyz;
    float nl = length(nn);
    if (!(nl > 1e-4) || any(isnan(nn))) return;
    n = nn / nl;
    vec3 p = getPositionWithDepth(uv, raw).xyz;
    vec3 v = -p * inversesqrt(max(dot(p, p), 1e-8));
    ndotv = clamp(abs(dot(n, v)), 0.0, 1.0);
}

vec2 screenOutward(vec3 n) { return n.xy; }   // keep in sync with rimGlowMaskF.glsl

// Schlick Fresnel from the real N.V. SOFT: floored by FresnelBase and blended
// by RimFresnel; a pixel without a usable normal gets 1 (flat wash, no darkening).
float rimFresnelForm(vec3 n, float ndotv)
{
    if (n == vec3(0.0)) return 1.0;
    float F0 = clamp(FresnelBase, 0.0, 1.0);
    float F  = F0 + (1.0 - F0) * pow(1.0 - ndotv, max(FresnelPower, 1.0));   // base >= 0
    return mix(1.0, F, clamp(RimFresnel, 0.0, 1.0));
}

// Light-facing side of the body gets the fill. dir is the coverage-weighted
// mean gather direction (unit, or zero); coh in [0,1] blends toward the
// neutral 0.5^p wash when opposing lights cancel or nothing was captured.
float rimDirectionalForm(vec3 n, vec2 dir, float coh)
{
    if (n == vec3(0.0)) return 1.0;
    float p        = mix(4.0, 0.5, clamp(DirectionalWrap, 0.0, 1.0));
    float fNeutral = pow(0.5, p);
    float f        = pow(clamp(dot(screenOutward(n), dir) * 0.5 + 0.5, 0.0, 1.0), p);
    return mix(1.0, mix(fNeutral, f, coh), clamp(FormDirectional, 0.0, 1.0));
}

// N2 read: coverage = blurred alpha (sum of weights), coherence = |sum dir*w| / sum w.
void readGatherDir(vec2 uv, float blurredA, out vec2 dir, out float coh)
{
    vec2 v = texture(rimGlowDir, uv).rg / max(blurredA, 1e-4);   // |v| <= 1
    if (any(isnan(v))) v = vec2(0.0);
    float len = length(v);
    dir = (len > 1e-4) ? v / len : vec2(0.0);                     // zero -> f == fNeutral, no NaN path
    float lo = max(DirCoherenceLo, 0.0);
    float hi = max(DirCoherenceHi, lo + 1e-3);
    coh = smoothstep(lo, hi, len);
}

// premultiplied lobe -> (mean colour, shaped coverage envelope)
void lobe(vec4 b, float norm, float p, out vec3 col, out float env)
{
    env = pow(clamp(b.a * max(norm, 0.0), 0.0, 1.0), max(p, 0.05));   // clamp is mandatory
    col = b.rgb / max(b.a, 1e-4);
}

float behindSubject(float zC)
{
    float fz = max(focusZ, 1e-3);
    return smoothstep(fz * (1.0 - max(FocusFeatherRel, 1e-4)), fz, zC);
}

void main()
{
    vec2 uv   = vary_fragcoord;
    vec4 diff = texture(diffuseRect, uv);
    vec3 scene = max(diff.rgb, vec3(0.0));
    if (any(isnan(scene)) || any(isinf(scene))) scene = vec3(0.0);

    float E   = rimExposure();
    float raw = getDepth(uv);
    float zC  = max(-getPositionWithDepth(uv, raw).z, 0.0);
    float M   = texture(rimSubj, uv).r;

    vec4 mk = texture(rimMask, uv);
    vec4 gw = texture(rimGlow, uv);      // bilinear upsample from half-res

    vec3 cW, cA; float eW, eA;
    lobe(texture(rimWrap, uv), WrapNorm, AutoWrapFalloff, cW, eW);
    lobe(gw,                   GlowNorm, AutoAtmFalloff,  cA, eA);

    vec2 gDir; float coh;
    readGatherDir(uv, gw.a, gDir, coh);

    vec3 n; float ndotv;
    readNormal(uv, raw, n, ndotv);
    float form = rimFresnelForm(n, ndotv) * rimDirectionalForm(n, gDir, coh);

    // interior response, bounded on HDR: Ys/(Ys+knee) in [0,1)
    float Ys   = dot(scene, LUMA) * E;
    float resp = mix(1.0, Ys / (Ys + max(RespKnee, 1e-4)), clamp(AutoInteriorResponse, 0.0, 1.0));

    // albedo bounce proxy: scene chroma (exposure-invariant ratio), clamped
    vec3 chroma = scene / max(dot(scene, LUMA), 1e-4);
    vec3 tint   = mix(vec3(1.0), min(chroma, vec3(4.0)), clamp(AutoInteriorTint, 0.0, 1.0));

    vec3 interior = (mk.rgb * CoreGain + cW * eW * WrapGain + cA * eA * AtmGain)
                  * M * resp * tint * form * AutoInteriorFill;

    vec3 halo = (cW * eW * HaloTight + cA * eA * AutoHalo) * (1.0 - M)
              * mix(1.0, behindSubject(zC), clamp(HaloOcclusion, 0.0, 1.0));

    vec3 rim = max(interior + halo, vec3(0.0));
    if (any(isnan(rim)) || any(isinf(rim))) rim = vec3(0.0);

    frag_color = vec4(scene + rim, diff.a);
}

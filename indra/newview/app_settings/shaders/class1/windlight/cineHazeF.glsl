/**
 * @file class1/windlight/cineHazeF.glsl
 * @brief Cinematic Depth Atmosphere ("cine haze") shared shader library.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * Distance / height haze that pulls distant geometry toward an atmosphere
 * colour (contrast reduction toward a colour -- NOT spatial blur).  It is an
 * artist-controlled layer composed AFTER the WindLight aerial perspective, in
 * linear HDR scene radiance, before exposure / bloom / tonemap.
 *
 * Consumers (one application per fragment, never twice):
 *   - class3/deferred/hazeF.glsl        deferred haze pass: opaque G-buffer
 *                                       surfaces + water surface (depth-tested)
 *   - class1/environment/waterFogF.glsl applySkyAndWaterFog(): every forward
 *                                       (alpha / fullbright / material / PBR
 *                                       alpha / Actor FX / ghost) fragment.
 *
 * Coordinate conventions: every position is camera-relative EYE space in
 * metres (SL world units), so region-crossing world-origin rebasing can never
 * move the layer.  World height is recovered analytically from
 *   h(P) = cine_haze_height.w + dot(P_eye, cine_haze_up)
 * (camera world height + projection on world-up-in-eye-space) instead of a
 * full inverse-modelview transform: no large-translation cancellation.
 *
 * Off path: cine_haze_active == 0 makes every entry point return its input
 * untouched (bit-exact with the pre-feature image) and skips all the math.
 */

uniform int   cine_haze_active;       // 0 = bypass everything (default)
uniform vec4  cine_haze_params;       // x density (1/m), y start distance (m), z max opacity [0,1], w master strength [0,1]
uniform vec4  cine_haze_height;       // x height enable (0/1), y reference height (world m), z falloff k (1/m, >= 0), w camera world height (m)
uniform vec3  cine_haze_up;           // world +Z expressed in eye space (unit)
uniform vec4  cine_haze_color;        // rgb manual atmosphere colour (sRGB picker value), w intensity multiplier
uniform int   cine_haze_color_mode;   // 0 manual colour, 1 environment-linked (WindLight far haze colour)
uniform vec4  cine_haze_sun;          // rgb sun tint (LINEAR) * strength, w lobe exponent (<= 0 disables the lobe)
uniform int   cine_haze_debug;        // 0 off, 1 distance, 2 density at surface, 3 optical depth, 4 transmittance, 5 fog amount, 6 sky exclusion
uniform int   cine_haze_additive;     // 1 = current draw is additive-blended: apply transmittance only (no atmosphere colour)

// Declared identically elsewhere (atmosphericsF.glsl / atmosphericsFuncs.glsl /
// softenLightF.glsl); GLSL merges matching uniform declarations across objects.
uniform float sky_hdr_scale;
uniform vec3  sun_dir;
uniform vec3  moon_dir;
uniform int   sun_up_factor;

vec3 srgb_to_linear(vec3 c);
vec3 calcAtmosphericFarHazeColor(vec3 view_dir_eye, vec3 light_dir_eye);

const float CINE_HAZE_MAX_EXP = 60.0;   // exp() argument clamp: every intermediate stays finite in fp32
const float CINE_HAZE_MAX_OD  = 80.0;   // exp(-80) ~ 1.8e-35: transmittance is zero for all purposes

// f(x) = (exp(x) - 1) / x with a Taylor branch near zero.  The Taylor branch is
// what makes the k*uy -> 0 limit (horizontal rays, tiny falloff) exact and
// cancellation-free; f(0) == 1.0 exactly, so height fog with k == 0 reduces to
// uniform fog bit-for-bit (D0 * L * exp(0) * 1.0).
float cineHazeExpm1OverX(float x)
{
    if (abs(x) < 0.02)
    {
        // |error| <= x^4/120 < 1.4e-9 at the switch point
        return 1.0 + x * (0.5 + x * (1.0 / 6.0 + x * (1.0 / 24.0)));
    }
    return (exp(x) - 1.0) / x;
}

// Optical depth along the camera -> surface segment.
//
//   d   = |P_eye|                     (true ray length, NOT view-space Z)
//   L   = max(d - startDistance, 0)   fog-bearing length
//   uniform:  OD = D0 * L
//   height:   density(h) = D0 * exp(-k (h - h_ref)),  h(t) = h_cam + uy * t,
//             uy = dot(P_eye / d, world_up_eye)
//             OD = D0 * L * exp(-k (h0 - h_ref)) * f(-k uy L),   f(x) = (e^x - 1) / x
//             h0 = h_cam + uy * t0 = height where the fog segment starts on this ray.
//
// Edge cases: d == 0 -> L == 0 -> OD == 0 (and uy uses max(d, 1e-6), no NaN);
// near-horizontal rays / k -> 0 -> Taylor branch -> OD -> D0 L exp(-k (h_cam - h_ref));
// extreme elevation / distance -> exponents are clamped so nothing overflows and
// the result saturates to "fully fogged" (later capped by max opacity).
float cineHazeOpticalDepth(vec3 pos_eye, out float dist, out float density_at_surface)
{
    float d  = length(pos_eye);
    dist = d;
    float D0 = cine_haze_params.x;
    float L  = max(d - cine_haze_params.y, 0.0);
    density_at_surface = D0;

    if (cine_haze_height.x < 0.5 || L <= 0.0)
    {
        return D0 * L;
    }

    float k     = cine_haze_height.z;
    float h_ref = cine_haze_height.y;
    float h_cam = cine_haze_height.w;
    float uy    = dot(pos_eye, cine_haze_up) / max(d, 1.0e-6);
    float t0    = d - L;
    float h0    = h_cam + uy * t0;

    float e0 = clamp(-k * (h0 - h_ref), -CINE_HAZE_MAX_EXP, CINE_HAZE_MAX_EXP);
    float x  = clamp(-k * uy * L,       -CINE_HAZE_MAX_EXP, CINE_HAZE_MAX_EXP);
    density_at_surface = D0 * exp(clamp(-k * (h_cam + uy * d - h_ref), -CINE_HAZE_MAX_EXP, CINE_HAZE_MAX_EXP));

    return D0 * L * exp(e0) * cineHazeExpm1OverX(x);
}

float cineHazeTransmittance(float optical_depth)
{
    return exp(-clamp(optical_depth, 0.0, CINE_HAZE_MAX_OD));
}

// Artistic amount: min(1 - T, maxOpacity) * masterStrength.  Range clamps of
// maxOpacity / masterStrength are done on the CPU (ALCineHaze::bind).
float cineHazeAmountFromOD(float optical_depth)
{
    float fog = 1.0 - cineHazeTransmittance(optical_depth);
    return min(fog, cine_haze_params.z) * cine_haze_params.w;
}

float cineHazeAmount(vec3 pos_eye)
{
    float dist, dens;
    return cineHazeAmountFromOD(cineHazeOpticalDepth(pos_eye, dist, dens));
}

// Atmosphere colour in LINEAR scene radiance (pre-exposure).  Scaled by
// sky_hdr_scale exactly like the WindLight sky / haze (hazeF.glsl,
// atmosphericsF.glsl), so a white picker value equals a white sky pixel and
// auto-exposure treats the fog like the sky it fades into.  The optional sun
// lobe is folded in HERE so it is only ever applied THROUGH the fog amount:
// zero fog => zero glow.  It is an unshadowed directional approximation (no
// light shafts / occlusion).
vec3 cineHazeAtmosphereColor(vec3 pos_eye, float dist)
{
    vec3 view_dir  = pos_eye / max(dist, 1.0e-6);              // camera -> surface, eye space
    vec3 light_dir = (sun_up_factor == 1) ? sun_dir : moon_dir; // toward the active light, eye space

    vec3 atm;
    if (cine_haze_color_mode == 1)
    {
        // WindLight far-field haze colour in this view direction: the colour the
        // existing aerial perspective converges to, i.e. the sky at the horizon.
        atm = srgb_to_linear(calcAtmosphericFarHazeColor(view_dir, light_dir) * 2.0);
    }
    else
    {
        atm = srgb_to_linear(cine_haze_color.rgb);
    }
    atm *= sky_hdr_scale * cine_haze_color.w;

    if (cine_haze_sun.w > 0.0)
    {
        // dot(view, light) > 0 when looking toward the light (sun_dir points AT the sun).
        float cos_theta = max(dot(view_dir, light_dir), 0.0);
        float lobe      = pow(cos_theta, cine_haze_sun.w);
        float night     = (sun_up_factor == 1) ? 1.0 : 0.25;   // moon lobe kept modest
        atm += cine_haze_sun.rgb * lobe * sky_hdr_scale * night;
    }
    return atm;
}

// Developer views.  Values are linear scene radiance, so they are shown through
// the normal exposure / tonemap chain (grey ramps read darker than the raw number).
vec3 cineHazeDebugColor(vec3 pos_eye, bool is_sky)
{
    if (is_sky)
    {
        // sky exclusion view paints excluded sky magenta; other views leave it black
        return (cine_haze_debug == 6) ? vec3(1.0, 0.0, 1.0) : vec3(0.0);
    }
    float dist, dens;
    float od     = cineHazeOpticalDepth(pos_eye, dist, dens);
    float T      = cineHazeTransmittance(od);
    float amount = cineHazeAmountFromOD(od);
    if (cine_haze_debug == 1) return vec3(dist / 512.0);                                  // 0..512 m ramp
    if (cine_haze_debug == 2) return vec3(min(dens / max(cine_haze_params.x, 1.0e-6), 4.0) * 0.25); // density / base (1.0 = base, at 0.25 grey)
    if (cine_haze_debug == 3) return vec3(od * 0.25);                                     // optical depth (1.0 grey == OD 4)
    if (cine_haze_debug == 4) return vec3(T);                                             // transmittance
    if (cine_haze_debug == 5) return vec3(amount);                                        // final fog amount
    if (cine_haze_debug == 6) return vec3(0.0, amount, 0.0);                              // fogged geometry green, sky magenta
    return vec3(0.0);
}

// Forward path entry point (alpha / fullbright / material / PBR alpha / Actor FX
// via applySkyAndWaterFog).  `color` is this fragment's fully lit and
// WindLight-fogged linear radiance; straight-alpha coverage is applied by the
// blend stage afterwards, which is correct for a per-fragment fog on the
// surface's own radiance.  Additive-blended draws get transmittance only so the
// atmosphere colour is never accumulated once per layer.  The function is
// affine in `color`, so glow sub-passes that use fogged(c) - fogged(0) keep the
// transmittance and drop the atmosphere term automatically.
vec3 cineHazeApply(vec3 pos_eye, vec3 color)
{
    if (cine_haze_active == 0)
    {
        return color;
    }
    if (cine_haze_debug != 0)
    {
        return cineHazeDebugColor(pos_eye, false);
    }
    float dist, dens;
    float amount = cineHazeAmountFromOD(cineHazeOpticalDepth(pos_eye, dist, dens));
    if (cine_haze_additive != 0)
    {
        return color * (1.0 - amount);
    }
    return mix(color, cineHazeAtmosphereColor(pos_eye, dist), amount);
}

// Deferred haze-pass entry point.  hazeF.glsl emits (additive_linear, atten)
// under blendFunc(ONE, SRC_ALPHA):   dst' = additive + dst * atten.
// Folding the cine layer in:
//   dst'' = (dst * atten + additive) * (1 - a) + atm * a
//         = dst * (atten * (1 - a))  +  (additive * (1 - a) + atm * a)
// so the pass stays a single blend and the WindLight haze is applied exactly
// once underneath the cine layer.
void cineHazeDeferred(vec3 pos_eye, inout vec3 additive_linear, inout float atten)
{
    float dist, dens;
    float amount = cineHazeAmountFromOD(cineHazeOpticalDepth(pos_eye, dist, dens));
    vec3  atm    = cineHazeAtmosphereColor(pos_eye, dist);
    additive_linear = additive_linear * (1.0 - amount) + atm * amount;
    atten *= (1.0 - amount);
}

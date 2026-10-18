/**
 * @file prismProjectorFeedF.glsl
 * @brief Converts retained Prism HDR into an opaque display-sRGB cookie.
 */
in vec2 prism_uv;
in vec3 prism_eye_pos;
in vec3 prism_eye_normal;
out vec4 frag_color;

vec4 prismSignalSource(vec2 surface_uv, vec3 eye_pos, vec3 eye_normal, bool incoming);
bool prismSignalSourceInFrame(vec2 uv, bool incoming);
vec3 linear_to_srgb(vec3 color);
uniform float prismGateAlpha;
uniform int prismPrimaryMediaSDR;
uniform int prismGateMediaSDR;
uniform int projectorFollowMainExposure;
#ifdef PROJECTOR_LOOK
// Uploaded only by the non-default look program family. The default family is
// the pre-look shader and receives no additional uniforms.
uniform vec4 projectorLookWarp;    // keystone H/V, hotspot, time seed
uniform vec4 projectorLookFilm;    // dust, scratches, burn, grain
uniform vec4 projectorLookGlow;    // halation, texel X/Y, reserved
uniform vec3 projectorLookLampGel; // D65-normalized lamp RGB * gel

float projectorHash(vec2 p)
{
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

vec2 projectorKeystone(vec2 uv)
{
    vec2 p = uv - vec2(0.5);
    vec2 divisor = vec2(1.0 + projectorLookWarp.x * p.y * 1.5,
                        1.0 + projectorLookWarp.y * p.x * 1.5);
    return p / divisor + vec2(0.5);
}
#endif
#ifdef TONEMAP
vec3 applyExposure(vec3 color);
vec3 applyToneMap(vec3 color);
#endif
#ifdef COLOR_GRADE
uniform int tonemap_type;
vec3 applyWhiteBalance(vec3 color);
vec3 applyLiftGammaGain(vec3 color);
vec3 applySplitToning(vec3 color);
vec3 applyBlackWhitePoint(vec3 color);
vec3 applyBrightnessContrast(vec3 color);
vec3 applyShadowHighlightRecovery(vec3 color);
vec3 applySaturation(vec3 color);
vec3 applyVibrance(vec3 color);
vec3 applyHueShift(vec3 color);
vec3 applyLUTGrading(vec3 color);
vec3 applyChannelCurves(vec3 color);
#endif
#ifdef LEGACY_GAMMA
uniform float gamma;
vec3 projectorLegacyGamma(vec3 color)
{
    vec3 c = vec3(1.0) - clamp(color, vec3(0.0), vec3(1.0));
    return vec3(1.0) - pow(c, vec3(gamma));
}
#endif

vec4 prismProjectorSource(bool incoming)
{
    bool camera = (incoming ? prismGateMediaSDR : prismPrimaryMediaSDR) == 0;
    vec2 sample_uv = prism_uv;
#ifdef PROJECTOR_LOOK
    sample_uv = projectorKeystone(sample_uv);
    if (any(lessThan(sample_uv, vec2(0.0))) ||
        any(greaterThan(sample_uv, vec2(1.0))))
    {
        return vec4(0.0, 0.0, 0.0, 1.0);
    }
#endif
    // Mask after sampling so the signal's derivatives stay in uniform flow.
    bool media_bar = !camera && !prismSignalSourceInFrame(sample_uv, incoming);
    vec4 color = prismSignalSource(sample_uv, prism_eye_pos, prism_eye_normal, incoming);
#ifdef PROJECTOR_LOOK
    if (projectorLookGlow.x > 0.001)
    {
        vec2 tap = projectorLookGlow.yz * 2.0;
        vec3 blur = prismSignalSource(sample_uv + vec2(tap.x, 0.0), prism_eye_pos, prism_eye_normal, incoming).rgb;
        blur += prismSignalSource(sample_uv - vec2(tap.x, 0.0), prism_eye_pos, prism_eye_normal, incoming).rgb;
        blur += prismSignalSource(sample_uv + vec2(0.0, tap.y), prism_eye_pos, prism_eye_normal, incoming).rgb;
        blur += prismSignalSource(sample_uv - vec2(0.0, tap.y), prism_eye_pos, prism_eye_normal, incoming).rgb;
        blur *= 0.25;
        color.rgb += max(blur - vec3(0.7), vec3(0.0)) * projectorLookGlow.x * 0.35;
    }
#endif
#ifdef TONEMAP
    if (camera && projectorFollowMainExposure != 0)
    {
        color.rgb = applyExposure(color.rgb);
    }
#endif
#ifdef COLOR_GRADE
    if (camera && tonemap_type != 8)
    {
        color.rgb = applyWhiteBalance(color.rgb);
        color.rgb = applyLiftGammaGain(color.rgb);
    }
#endif
#ifdef TONEMAP
    color.rgb = camera ? applyToneMap(color.rgb) : clamp(color.rgb, vec3(0.0), vec3(1.0));
#else
    color.rgb = clamp(color.rgb, vec3(0.0), vec3(1.0));
#endif
#ifdef COLOR_GRADE
    if (camera && tonemap_type != 8) color.rgb = applySplitToning(color.rgb);
#endif
#ifdef PROJECTOR_LOOK
    vec2 centred = sample_uv - vec2(0.5);
    float radius2 = dot(centred, centred) * 2.0;
    color.rgb *= 1.0 + projectorLookWarp.z * (0.35 - 0.45 * radius2);

    float seed = floor(projectorLookWarp.w * 12.0);
    float dust_cell = projectorHash(floor(sample_uv * vec2(180.0, 120.0)) + seed);
    float dust = step(0.997 - projectorLookFilm.x * 0.012, dust_cell);
    color.rgb *= 1.0 - dust * projectorLookFilm.x * 0.75;

    float scratch_cell = projectorHash(vec2(floor(sample_uv.x * 420.0), floor(seed * 0.25)));
    float scratch = step(0.998 - projectorLookFilm.y * 0.02, scratch_cell);
    color.rgb *= 1.0 - scratch * projectorLookFilm.y * 0.65;

    float edge = smoothstep(0.2, 0.5, max(abs(centred.x), abs(centred.y)));
    color.rgb *= mix(vec3(1.0), vec3(0.52, 0.26, 0.10),
                     edge * projectorLookFilm.z * 0.75);

    float grain = projectorHash(sample_uv * vec2(913.7, 527.3) + seed) - 0.5;
    color.rgb += grain * projectorLookFilm.w * 0.12;
    color.rgb *= projectorLookLampGel;
#endif
    color.rgb = linear_to_srgb(color.rgb);
#ifdef LEGACY_GAMMA
    if (camera) color.rgb = projectorLegacyGamma(color.rgb);
#endif
#ifdef COLOR_GRADE
    if (camera && tonemap_type != 8)
    {
        color.rgb = applyBlackWhitePoint(color.rgb);
        color.rgb = applyBrightnessContrast(color.rgb);
        color.rgb = applyShadowHighlightRecovery(color.rgb);
        color.rgb = applySaturation(color.rgb);
        color.rgb = applyVibrance(color.rgb);
        color.rgb = applyHueShift(color.rgb);
        color.rgb = applyLUTGrading(color.rgb);
        color.rgb = applyChannelCurves(color.rgb);
    }
#endif
    if (media_bar) return vec4(0.0, 0.0, 0.0, 1.0);
    return vec4(clamp(color.rgb, vec3(0.0), vec3(1.0)), 1.0);
}

void main()
{
    if (prismGateAlpha <= 0.0) frag_color = prismProjectorSource(false);
    else if (prismGateAlpha >= 1.0) frag_color = prismProjectorSource(true);
    else frag_color = mix(prismProjectorSource(false), prismProjectorSource(true), prismGateAlpha);
}

/** @file prismPreviewF.glsl
 * Display-referred RGBA8 atlas entry point. BLOOM_COMPOSITE is deliberately
 * absent: retained signal optics/effects remain, main-view bloom does not.
 */
in vec2 prism_uv;
in vec3 prism_eye_pos;
in vec3 prism_eye_normal;
out vec4 frag_color;
vec4 prismSignalSample(vec2 surface_uv, vec3 eye_pos, vec3 eye_normal);
vec3 linear_to_srgb(vec3 color);
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
vec3 prismLegacyGamma(vec3 color)
{
    vec3 c = vec3(1.0) - clamp(color, vec3(0.0), vec3(1.0));
    return vec3(1.0) - pow(c, vec3(gamma));
}
#endif
void main()
{
    vec4 color = prismSignalSample(prism_uv, prism_eye_pos, prism_eye_normal);
#ifdef TONEMAP
    color.rgb = applyExposure(color.rgb);
#endif
#ifdef COLOR_GRADE
    if (tonemap_type != 8) { color.rgb = applyWhiteBalance(color.rgb); color.rgb = applyLiftGammaGain(color.rgb); }
#endif
#ifdef TONEMAP
    color.rgb = applyToneMap(color.rgb);
#else
    color.rgb = clamp(color.rgb, vec3(0.0), vec3(1.0));
#endif
#ifdef COLOR_GRADE
    if (tonemap_type != 8) color.rgb = applySplitToning(color.rgb);
#endif
    color.rgb = linear_to_srgb(color.rgb);
#ifdef LEGACY_GAMMA
    color.rgb = prismLegacyGamma(color.rgb);
#endif
#ifdef COLOR_GRADE
    if (tonemap_type != 8)
    {
        color.rgb = applyBlackWhitePoint(color.rgb); color.rgb = applyBrightnessContrast(color.rgb);
        color.rgb = applyShadowHighlightRecovery(color.rgb); color.rgb = applySaturation(color.rgb);
        color.rgb = applyVibrance(color.rgb); color.rgb = applyHueShift(color.rgb);
        color.rgb = applyLUTGrading(color.rgb); color.rgb = applyChannelCurves(color.rgb);
    }
#endif
    color.rgb = clamp(color.rgb, vec3(0.0), vec3(1.0));
    color.a = 1.0; // retained alpha is a glow channel, never LLUI opacity
    frag_color = color;
}

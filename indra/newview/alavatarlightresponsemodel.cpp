/**
 * @file alavatarlightresponsemodel.cpp
 * @brief [AvatarLightResponse] Pure deterministic model for the per-avatar,
 *        light-independent response trim.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include "alavatarlightresponsemodel.h"

#include <algorithm>
#include <cmath>

namespace ALLightResponse
{
namespace
{
F32 clampField(F32 v, F32 lo, F32 hi, F32 fallback)
{
    if (!std::isfinite(v))
    {
        return fallback;
    }
    return std::min(std::max(v, lo), hi);
}

F32 realField(const LLSD& s, const char* key, F32 fallback)
{
    if (!s.isMap() || !s.has(key))
    {
        return fallback;
    }
    const LLSD& v = s[key];
    if (!(v.isReal() || v.isInteger()))
    {
        return fallback;
    }
    return static_cast<F32>(v.asReal());
}

bool boolField(const LLSD& s, const char* key, bool fallback)
{
    if (!s.isMap() || !s.has(key) || !s[key].isBoolean())
    {
        return fallback;
    }
    return s[key].asBoolean();
}

LLVector3 vector3Field(const LLSD& s, const char* key, const LLVector3& fallback)
{
    if (!s.isMap() || !s.has(key) || !s[key].isArray() || s[key].size() != 3)
    {
        return fallback;
    }

    LLVector3 result;
    for (S32 i = 0; i < 3; ++i)
    {
        const LLSD& value = s[key][i];
        if (!(value.isReal() || value.isInteger()))
        {
            return fallback;
        }
        result.mV[i] = static_cast<F32>(value.asReal());
        if (!std::isfinite(result.mV[i]))
        {
            return fallback;
        }
    }
    return result;
}

F32 dot3(const LLVector3& a, const LLVector3& b)
{
    return a.mV[VX] * b.mV[VX] + a.mV[VY] * b.mV[VY] + a.mV[VZ] * b.mV[VZ];
}

LLVector3 normalize3(const LLVector3& value)
{
    const F32 inv_length = 1.f / std::sqrt(dot3(value, value));
    return LLVector3(value.mV[VX] * inv_length,
                     value.mV[VY] * inv_length,
                     value.mV[VZ] * inv_length);
}

F32 clamp01(F32 value)
{
    return std::min(std::max(value, 0.f), 1.f);
}

F32 smoothstepMirror(F32 edge0, F32 edge1, F32 value)
{
    const F32 t = clamp01((value - edge0) / (edge1 - edge0));
    return t * t * (3.f - 2.f * t);
}
}

Params sanitize(const Params& p)
{
    Params r;
    r.mDiffuse    = clampField(p.mDiffuse, DIFFUSE_MIN, DIFFUSE_MAX, 0.f);
    r.mTame       = clampField(p.mTame, TAME_MIN, TAME_MAX, 0.f);
    r.mExposureEV = clampField(p.mExposureEV, EV_MIN, EV_MAX, 0.f);
    r.mGlow       = clampField(p.mGlow, GLOW_MIN, GLOW_MAX, GLOW_DEFAULT);
    r.mSilAmount  = clampField(p.mSilAmount, 0.f, 1.f, 0.f);
    for (S32 i = 0; i < 3; ++i)
    {
        r.mSilFillSrgb.mV[i] = clampField(p.mSilFillSrgb.mV[i], 0.f, 1.f, 0.f);
    }
    r.mSilHaze    = p.mSilHaze;
    r.mSilKeepRim = p.mSilKeepRim;
    r.mBypass     = p.mBypass;
    return r;
}

bool isIdentity(const Params& p)
{
    if (p.mBypass)
    {
        return true;
    }
    const Params s = sanitize(p);
    return std::fabs(s.mDiffuse) <= IDENTITY_EPSILON
        && std::fabs(s.mTame) <= IDENTITY_EPSILON
        && std::fabs(s.mExposureEV) <= IDENTITY_EPSILON
        && std::fabs(s.mGlow - GLOW_DEFAULT) <= IDENTITY_EPSILON
        && s.mSilAmount <= SILHOUETTE_EPSILON;
}

S32 silQuant(F32 a, bool hdr)
{
    if (!std::isfinite(a) || a <= SILHOUETTE_EPSILON)
    {
        return 0;
    }
    const S32 levels = hdr ? 510 : 62;
    const F32 clamped = std::min(std::max(a, 0.f), 1.f);
    return std::min(std::max(static_cast<S32>(std::lround(clamped * levels)), 1), levels);
}

F32 silAmountQ(S32 q, bool hdr)
{
    const S32 levels = hdr ? 510 : 62;
    if (q <= 0)
    {
        return 0.f;
    }
    return static_cast<F32>(std::min(q, levels)) / static_cast<F32>(levels);
}

F32 silCarrier(S32 q, bool haze_over, bool keep_rim, bool hdr)
{
    const S32 levels = hdr ? 510 : 62;
    if (q < 1 || q > levels)
    {
        return 0.f;
    }
    const S32 flags = (haze_over ? 0 : 1) | (keep_rim ? 0 : 2);
    if (hdr)
    {
        return static_cast<F32>(2 + q + 512 * flags);
    }
    return static_cast<F32>(q + 62 * flags) / 255.f;
}

bool silDecode(F32 carrier, bool hdr, F32& a_q, bool& haze_skip, bool& rim_cut)
{
    a_q = 0.f;
    haze_skip = false;
    rim_cut = false;
    if (!std::isfinite(carrier))
    {
        return false;
    }

    S32 q = 0;
    S32 flags = 0;
    if (hdr)
    {
        const F32 encoded = carrier - 2.f;
        const F32 rounded = std::round(encoded);
        if (std::fabs(encoded - rounded) >= 0.25f || rounded < 1.f || rounded > 2046.f)
        {
            return false;
        }
        const S32 code = static_cast<S32>(rounded);
        q = code & 511;
        flags = code >> 9;
        if (q < 1 || q > 510 || flags > 3)
        {
            return false;
        }
    }
    else
    {
        // LDR has no spare precision for an integrality check.  Rejecting
        // blended carriers therefore relies on every G-buffer writer that can
        // produce one running with blending disabled; HDR rejects by encoding.
        const S32 code = static_cast<S32>(std::round(carrier * 255.f));
        if (code < 1 || code > 248)
        {
            return false;
        }
        q = ((code - 1) % 62) + 1;
        flags = (code - 1) / 62;
    }

    a_q = silAmountQ(q, hdr);
    haze_skip = (flags & 1) != 0;
    rim_cut = (flags & 2) != 0;
    return true;
}

void calcDiffuseSpecularMirror(const LLVector3& base_color, F32 metallic,
                               LLVector3& diffuse_color, LLVector3& specular_color)
{
    // deferredUtil.glsl calcDiffuseSpecular, including its statement order.
    const LLVector3 f0(0.04f, 0.04f, 0.04f);
    for (S32 i = 0; i < 3; ++i)
    {
        diffuse_color.mV[i] = base_color.mV[i] * (1.f - f0.mV[i]);
        diffuse_color.mV[i] *= 1.f - metallic;
        specular_color.mV[i] = f0.mV[i] * (1.f - metallic) + base_color.mV[i] * metallic;
    }
}

PbrPunctualMirrorResult pbrPunctualMirror(const LLVector3& diffuse_color,
                                          const LLVector3& specular_color,
                                          F32 perceptual_roughness, F32 metallic,
                                          const LLVector3& n, const LLVector3& v,
                                          const LLVector3& l)
{
    // deferredUtil.glsl pbrPunctual and its three BRDF helpers, in shader order.
    perceptual_roughness = std::max(perceptual_roughness, 0.045f);
    const F32 alpha_roughness = perceptual_roughness * perceptual_roughness;
    const F32 reflectance = std::max(std::max(specular_color.mV[VX], specular_color.mV[VY]),
                                     specular_color.mV[VZ]);
    const F32 reflectance90 = clamp01(reflectance * 25.f);

    const LLVector3 h = normalize3(LLVector3(l.mV[VX] + v.mV[VX],
                                             l.mV[VY] + v.mV[VY],
                                             l.mV[VZ] + v.mV[VZ]));
    const F32 v_dot_n = dot3(v, n);
    LLVector3 reflection = normalize3(LLVector3(v.mV[VX] - 2.f * v_dot_n * n.mV[VX],
                                                v.mV[VY] - 2.f * v_dot_n * n.mV[VY],
                                                v.mV[VZ] - 2.f * v_dot_n * n.mV[VZ]));
    reflection *= -1.f;
    reflection.mV[VY] *= -1.f;

    const F32 ndotl = std::min(std::max(dot3(n, l), 0.001f), 1.f);
    const F32 ndotv = std::min(std::max(std::fabs(dot3(n, v)), 0.001f), 1.f);
    const F32 ndoth = clamp01(dot3(n, h));
    const F32 ldoth = clamp01(dot3(l, h));
    const F32 vdoth = clamp01(dot3(v, h));

    PbrPunctualMirrorResult result;
    result.nl = ndotl;
    const F32 fresnel_weight = std::pow(clamp01(1.f - vdoth), 5.f);
    LLVector3 fresnel;
    for (S32 i = 0; i < 3; ++i)
    {
        fresnel.mV[i] = specular_color.mV[i]
            + (reflectance90 - specular_color.mV[i]) * fresnel_weight;
    }
    const F32 r = alpha_roughness;
    const F32 attenuation_l = 2.f * ndotl /
        (ndotl + std::sqrt(r * r + (1.f - r * r) * (ndotl * ndotl)));
    const F32 attenuation_v = 2.f * ndotv /
        (ndotv + std::sqrt(r * r + (1.f - r * r) * (ndotv * ndotv)));
    const F32 geometric = attenuation_l * attenuation_v;
    const F32 roughness_sq = alpha_roughness * alpha_roughness;
    const F32 distribution_f = (ndoth * roughness_sq - ndoth) * ndoth + 1.f;
    const F32 distribution = roughness_sq / (F_PI * distribution_f * distribution_f);

    for (S32 i = 0; i < 3; ++i)
    {
        const F32 lambert = diffuse_color.mV[i] / F_PI;
        result.diffuse.mV[i] = (1.f - fresnel.mV[i]) * lambert;
        result.specular.mV[i] = fresnel.mV[i] * geometric * distribution / (4.f * ndotl * ndotv);
    }
    (void)reflection;
    (void)ldoth;
    (void)metallic; // Present in the GLSL PBRInfo even though these helpers do not read it.
    return result;
}

LLVector3 rigRimTermMirror(const LLVector3& n, const LLVector3& v, const LLVector3& l,
                           const LLVector3& light_color, const LLVector3& diffuse_color,
                           F32 roughness, const LLVector4& rim,
                           const RigRimMirrorUniforms& uniforms)
{
    const F32 gain = rim.mV[VX] * uniforms.globals.mV[VX];
    if (gain <= 0.f)
    {
        return LLVector3::zero;
    }

    LLVector3 c_l = light_color;
    if (uniforms.tron.mV[VX] > 0.5f)
    {
        const F32 luminance = dot3(c_l, LLVector3(0.2126f, 0.7152f, 0.0722f));
        LLVector3 tint(std::max(uniforms.tron_color.mV[VX], 0.f),
                       std::max(uniforms.tron_color.mV[VY], 0.f),
                       std::max(uniforms.tron_color.mV[VZ], 0.f));
        const F32 tint_lum = std::max(dot3(tint, LLVector3(0.2126f, 0.7152f, 0.0722f)), 1.0e-4f);
        const F32 tint_max = std::max(std::max(tint.mV[VX], tint.mV[VY]),
                                      std::max(tint.mV[VZ], 1.0e-4f));
        const F32 tint_den = std::sqrt(tint_lum * tint_max);
        const F32 tint_mix = clamp01(uniforms.tron.mV[VY]);
        for (S32 i = 0; i < 3; ++i)
        {
            c_l.mV[i] = c_l.mV[i] * (1.f - tint_mix)
                       + luminance * (tint.mV[i] / tint_den) * tint_mix;
        }
        if (uniforms.tron.mV[VX] > 1.5f)
        {
            c_l *= std::max(uniforms.tron.mV[VZ], 0.f);
        }
    }

    const F32 nov = std::fabs(dot3(n, v));
    const F32 soften = clamp01(roughness) * clamp01(uniforms.globals.mV[VY]);
    const F32 rim_k = std::max(rim.mV[VY], 0.05f);
    const F32 k_eff = rim_k * (1.f - soften) + std::min(rim_k, 1.f) * soften;
    const F32 grazing = std::pow(clamp01(1.f - nov), k_eff);
    const F32 wrap = clamp01(rim.mV[VZ]);
    const F32 light_weight = clamp01((dot3(n, l) + wrap) / (1.f + wrap));
    const F32 back0 = rim.mV[VW];
    const F32 back1 = back0 + std::max(uniforms.globals.mV[VZ], 1.0e-4f);
    const F32 back = smoothstepMirror(back0, back1, -dot3(v, l));
    const F32 surface_mix = clamp01(uniforms.globals.mV[VW]);

    LLVector3 result;
    for (S32 i = 0; i < 3; ++i)
    {
        const F32 tint = 1.f * (1.f - surface_mix) + diffuse_color.mV[i] * surface_mix;
        result.mV[i] = std::min(std::max(gain * c_l.mV[i]
                                         * (grazing * light_weight * back) * tint, 0.f), 10.f);
    }
    return result;
}

LLVector3 rimRecombine(const LLVector3& nonrim, const LLVector3& rim_raw,
                       bool rim_on, F32 light_scale)
{
    LLVector3 color = nonrim;
    if (rim_on)
    {
        color += rim_raw;
    }
    return color * light_scale;
}

LLVector3 pbrPunctualSilhouette(const LLVector3& base_color, F32 metallic,
                               F32 perceptual_roughness,
                               const LLVector3& n, const LLVector3& v,
                               const LLVector3& l, const LLVector3& fill,
                               F32 amount)
{
    LLVector3 diffuse_color, specular_color;
    calcDiffuseSpecularMirror(base_color, metallic, diffuse_color, specular_color);
    const PbrPunctualMirrorResult punctual = pbrPunctualMirror(
        diffuse_color, specular_color, perceptual_roughness, metallic, n, v, l);
    const F32 keep = 1.f - amount;
    LLVector3 result;
    for (S32 channel = 0; channel < 3; ++channel)
    {
        const F32 lit = punctual.nl
            * (punctual.diffuse.mV[channel] + punctual.specular.mV[channel]);
        result.mV[channel] = std::min(std::max(lit, 0.f), 10.f) * keep
                           + fill.mV[channel] * amount;
    }
    return result;
}

LLVector3 keepRimSilhouette(const LLVector3& nonrim, const LLVector3& rim,
                           const LLVector3& fill, F32 amount, bool keep_rim)
{
    const F32 keep = 1.f - amount;
    LLVector3 result;
    if (keep_rim)
    {
        result = nonrim * keep;
        result += rim;
    }
    else
    {
        result = (nonrim + rim) * keep;
    }
    result += fill * amount;
    return result;
}

LLVector3 nonlinearLookRimSilhouette(const LLVector3& styled_with_rim,
                                    const LLVector3& styled_without_rim,
                                    const LLVector3& fill, F32 amount,
                                    bool keep_rim)
{
    const F32 keep = 1.f - amount;
    LLVector3 result = styled_with_rim * keep;
    if (keep_rim)
    {
        result += (styled_with_rim - styled_without_rim) * amount;
    }
    result += fill * amount;
    return result;
}

F32 srgbToLinear(F32 c)
{
    c = clampField(c, 0.f, 1.f, 0.f);
    return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
}

LLVector3 srgbToLinear(const LLVector3& c)
{
    return LLVector3(srgbToLinear(c.mV[VX]),
                     srgbToLinear(c.mV[VY]),
                     srgbToLinear(c.mV[VZ]));
}

Packed packAll(const Params& p, bool hdr_encoding)
{
    Packed result;
    result.mParams = pack(p);
    if (p.mBypass)
    {
        return result;
    }

    const Params s = sanitize(p);
    const S32 q = silQuant(s.mSilAmount, hdr_encoding);
    if (q == 0)
    {
        return result;
    }

    const LLVector3 fill = srgbToLinear(s.mSilFillSrgb);
    result.mSil.set(fill.mV[VX], fill.mV[VY], fill.mV[VZ], silAmountQ(q, hdr_encoding));
    result.mSilAux.set(silCarrier(q, s.mSilHaze, s.mSilKeepRim, hdr_encoding),
                       s.mSilHaze ? 1.f : 0.f,
                       s.mSilKeepRim ? 1.f : 0.f,
                       0.f);
    return result;
}

LLVector3 fillPresetSrgb(EFillPreset preset)
{
    switch (preset)
    {
    case FILL_CHARCOAL:     return LLVector3(38.f / 255.f, 38.f / 255.f, 38.f / 255.f);
    case FILL_WHITE:        return LLVector3(1.f, 1.f, 1.f);
    case FILL_CHROMA_GREEN: return LLVector3(0.f, 177.f / 255.f, 64.f / 255.f);
    case FILL_CHROMA_BLUE:  return LLVector3(0.f, 71.f / 255.f, 187.f / 255.f);
    case FILL_BLACK:
    case FILL_CUSTOM:
    default:                return LLVector3::zero;
    }
}

EFillPreset matchFillPreset(const LLVector3& fill_srgb)
{
    constexpr F32 tolerance = 0.5f / 255.f;
    for (S32 preset = FILL_BLACK; preset < FILL_CUSTOM; ++preset)
    {
        const LLVector3 candidate = fillPresetSrgb(static_cast<EFillPreset>(preset));
        bool match = true;
        for (S32 i = 0; i < 3; ++i)
        {
            match = match && std::isfinite(fill_srgb.mV[i])
                          && std::fabs(fill_srgb.mV[i] - candidate.mV[i]) <= tolerance;
        }
        if (match)
        {
            return static_cast<EFillPreset>(preset);
        }
    }
    return FILL_CUSTOM;
}

LLVector3 silForward(const LLVector3& lit, const LLVector3& fill_lin,
                     F32 a_q, F32 fill_scale,
                     const LLVector3& fog_A, const LLVector3& fog_B,
                     bool haze_over)
{
    if (a_q == 0.f)
    {
        LLVector3 fogged;
        for (S32 i = 0; i < 3; ++i)
        {
            fogged.mV[i] = fog_A.mV[i] * lit.mV[i] + fog_B.mV[i];
        }
        return fogged;
    }

    const F32 k = 1.f - a_q;
    LLVector3 result;
    for (S32 i = 0; i < 3; ++i)
    {
        const F32 fog_lit = fog_A.mV[i] * lit.mV[i] + fog_B.mV[i];
        const F32 fill = fill_lin.mV[i] * fill_scale;
        result.mV[i] = k * fog_lit + a_q * (haze_over ? fog_A.mV[i] * fill + fog_B.mV[i] : fill);
    }
    return result;
}

void silHazeBlend(LLVector3& src_rgb, F32& src_a, F32 h)
{
    if (h == 0.f)
    {
        return;
    }
    const F32 keep = 1.f - h;
    src_rgb *= keep;
    src_a = 1.f - keep * (1.f - src_a);
}

UploadPlan uploadPlan(const Packed* slot, EUploadSlot which, bool debug_tint)
{
    UploadPlan result;
    if (!slot)
    {
        return result;
    }

    const bool has_silhouette = slot->mSil.mV[VW] > 0.f;
    if (which == UPLOAD_SIL_ONLY && !has_silhouette)
    {
        return result;
    }

    result.enabled = debug_tint ? 3 : 1;
    if (which == UPLOAD_SIL_ONLY)
    {
        result.sil = slot->mSil;
        result.aux = slot->mSilAux;
    }
    else if (which == UPLOAD_GLOW)
    {
        result.params = slot->mParams;
        const F32 keep = has_silhouette ? 1.f - slot->mSil.mV[VW] : 1.f;
        result.params.mV[VZ] = result.params.mV[VW] * keep;
    }
    else
    {
        result.params = slot->mParams;
        result.sil = slot->mSil;
        result.aux = slot->mSilAux;
    }

    if (debug_tint)
    {
        result.sil.set(0.f, 0.f, 0.f, 0.f);
        result.aux.set(0.f, 0.f, 0.f, 0.f);
    }
    return result;
}

LLVector4 pack(const Params& p)
{
    if (p.mBypass)
    {
        return LLVector4(0.f, 1.f, 1.f, 1.f);
    }
    const Params s = sanitize(p);
    return LLVector4(s.mDiffuse, 1.f - s.mTame, std::exp2(s.mExposureEV), s.mGlow);
}

F32 albedoGain(F32 k, F32 max_rgb)
{
    if (k > 1.f)
    {
        const F32 ceiling = 1.f / std::max(max_rgb, 1.0e-4f);
        return std::max(1.f, std::min(k, ceiling));
    }
    return k;
}

F32 legacyGloss(F32 gloss, F32 d)
{
    const F32 dd = std::min(std::max(d, 0.f), 1.f);
    if (dd > 0.f && gloss >= 0.5f / 255.f)
    {
        return std::max(gloss * (1.f - 0.95f * dd), 1.f / 255.f);
    }
    return gloss;
}

LLVector3 graphicCover(const LLVector3& c, const LLVector3& lit_pre, F32 sigma, F32 k)
{
    if (sigma <= 0.f)
    {
        return c;
    }
    if (k == 1.f)
    {
        return c;
    }
    LLVector3 out;
    for (S32 i = 0; i < 3; ++i)
    {
        out.mV[i] = c.mV[i] + (k - 1.f) * (c.mV[i] - (1.f - sigma) * lit_pre.mV[i]);
    }
    return out;
}

Params applyPreset(const Params& cur, EPreset preset)
{
    Params r = sanitize(cur);
    switch (preset)
    {
    case PRESET_RESET:
        r = Params();
        break;
    case PRESET_POWDERED:
        r.mDiffuse = 0.30f;
        r.mTame    = 0.25f;
        break;
    case PRESET_MATTE_SKIN:
        r.mDiffuse = 0.50f;
        r.mTame    = 0.50f;
        break;
    case PRESET_TAME_GLOSS:
        r.mDiffuse = 0.15f;
        r.mTame    = 0.65f;
        break;
    case PRESET_KILL_SHEEN:
        r.mDiffuse = 0.70f;
        r.mTame    = 0.90f;
        break;
    case PRESET_MINUS_HALF_STOP:
        r.mExposureEV = -0.5f;
        break;
    case PRESET_MINUS_ONE_STOP:
        r.mExposureEV = -1.0f;
        break;
    case PRESET_GLOW_OFF:
        r.mGlow = 0.f;
        break;
    case PRESET_SILHOUETTE:
        r.mSilAmount = 1.f;
        break;
    default:
        break;
    }
    return r;
}

LLSD toLLSD(const Params& p)
{
    const Params s = sanitize(p);
    LLSD out = LLSD::emptyMap();
    out["diffuse"] = static_cast<LLSD::Real>(s.mDiffuse);
    out["tame"]    = static_cast<LLSD::Real>(s.mTame);
    out["ev"]      = static_cast<LLSD::Real>(s.mExposureEV);
    out["glow"]    = static_cast<LLSD::Real>(s.mGlow);
    out["sil_amount"] = static_cast<LLSD::Real>(s.mSilAmount);
    LLSD fill = LLSD::emptyArray();
    fill.append(static_cast<LLSD::Real>(s.mSilFillSrgb.mV[VX]));
    fill.append(static_cast<LLSD::Real>(s.mSilFillSrgb.mV[VY]));
    fill.append(static_cast<LLSD::Real>(s.mSilFillSrgb.mV[VZ]));
    out["sil_fill"] = fill;
    out["sil_haze"] = static_cast<LLSD::Boolean>(s.mSilHaze);
    out["sil_keep_rim"] = static_cast<LLSD::Boolean>(s.mSilKeepRim);
    out["bypass"]  = static_cast<LLSD::Boolean>(s.mBypass);
    return out;
}

Params fromLLSD(const LLSD& s)
{
    Params p;
    p.mDiffuse    = realField(s, "diffuse", 0.f);
    p.mTame       = realField(s, "tame", 0.f);
    p.mExposureEV = realField(s, "ev", 0.f);
    p.mGlow       = realField(s, "glow", GLOW_DEFAULT);
    p.mSilAmount  = realField(s, "sil_amount", 0.f);
    p.mSilFillSrgb = vector3Field(s, "sil_fill", LLVector3::zero);
    p.mSilHaze    = boolField(s, "sil_haze", true);
    p.mSilKeepRim = boolField(s, "sil_keep_rim", false);
    // Preserve the v1 coercion semantics of the established key.
    p.mBypass     = s.isMap() && s.has("bypass") && s["bypass"].asBoolean();
    return sanitize(p);
}

bool mayWriteSaveFile(const SaveGateInput& in)
{
    if (!in.mPersist || !in.mAgentKnown || !in.mAgentIsLoadedOwner || !in.mDirty)
    {
        return false;
    }
    // A file we could not read is never overwritten by an untouched session.
    return !in.mLoadFailed || in.mEditedSinceLoadFail;
}
}

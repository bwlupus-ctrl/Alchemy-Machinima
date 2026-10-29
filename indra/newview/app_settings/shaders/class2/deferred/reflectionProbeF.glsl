/**
 * @file class2/deferred/reflectionProbeF.glsl
 *
 * $LicenseInfo:firstyear=2022&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2022, Linden Research, Inc.
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

// Implementation for when reflection probes are disabled

uniform float reflection_probe_ambiance;

// [EnvIntensity] viewer-side Sky/GI intensity (2^AlchemyEnvSkyGIEV); mirrors the
// class3 probe-irradiance multiplier so probes-off behaves consistently.
uniform float sky_gi_scale;
// [EnvIntensity v2] probe reflections EV factor (1.0 = stock, 1.0 in captures)
// and the luminance-normalised ambient tint (vec3(1) = off). Both applied in
// uniform-gated branches so the off path is the unmodified expression.
uniform float sky_probe_rad_scale;
uniform vec3  sky_amb_tint;

uniform samplerCube environmentMap;

uniform mat3 env_mat;

vec3 srgb_to_linear(vec3 c);
vec3 linear_to_srgb(vec3 c);

void sampleReflectionProbes(inout vec3 ambenv, inout vec3 glossenv,
        vec2 tc, vec3 pos, vec3 norm, float glossiness, bool transparent, vec3 amblit_linear)
{
    vec3 probe_amb = vec3(reflection_probe_ambiance * 0.25 * sky_gi_scale); // [EnvIntensity]
    if (sky_amb_tint != vec3(1.0)) // [EnvIntensity v2] tinted fill
    {
        probe_amb *= sky_amb_tint;
    }
    ambenv = mix(ambenv, probe_amb, reflection_probe_ambiance);

    vec3 refnormpersp = normalize(reflect(pos.xyz, norm.xyz));
    vec3 env_vec = env_mat * refnormpersp;
    glossenv = srgb_to_linear(texture(environmentMap, env_vec).rgb);
    if (sky_probe_rad_scale != 1.0) // [EnvIntensity v2] decoded (linear) here
    {
        glossenv *= sky_probe_rad_scale;
    }
}

void sampleReflectionProbesWater(inout vec3 ambenv, inout vec3 glossenv,
        vec2 tc, vec3 pos, vec3 norm, float glossiness, vec3 amblit_linear)
{
    sampleReflectionProbes(ambenv, glossenv, tc, pos, norm, glossiness, false, amblit_linear);
}

vec4 sampleReflectionProbesDebug(vec3 pos)
{
    // show nothing in debug display
    return vec4(0, 0, 0, 0);
}

void sampleReflectionProbesLegacy(inout vec3 ambenv, inout vec3 glossenv, inout vec3 legacyenv,
        vec2 tc, vec3 pos, vec3 norm, float glossiness, float envIntensity, bool transparent, vec3 amblit_linear)
{
    vec3 probe_amb = vec3(reflection_probe_ambiance * 0.25 * sky_gi_scale); // [EnvIntensity]
    if (sky_amb_tint != vec3(1.0)) // [EnvIntensity v2] tinted fill
    {
        probe_amb *= sky_amb_tint;
    }
    ambenv = mix(ambenv, probe_amb, reflection_probe_ambiance);

    vec3 refnormpersp = normalize(reflect(pos.xyz, norm.xyz));
    vec3 env_vec = env_mat * refnormpersp;

    legacyenv = texture(environmentMap, env_vec).rgb;

    // [EnvIntensity v2] legacyenv stays sRGB-ENCODED here (applyLegacyEnv mixes
    // encoded values), so the EV factor is applied in linear light with a
    // bypass: the off path never round-trips the encoded value.
    if (sky_probe_rad_scale != 1.0)
    {
        legacyenv = linear_to_srgb(srgb_to_linear(legacyenv) * sky_probe_rad_scale);
    }

    glossenv = legacyenv;
}

void applyGlossEnv(inout vec3 color, vec3 glossenv, vec4 spec, vec3 pos, vec3 norm)
{

}

// [AvatarLightResponse] keep scales ONLY the (encoded) reflection addend; the mixing weight is
// untouched and keep == 1.0 is bit-exact. The old name forwards 1.0.
// This probes-off path mixes in ENCODED (sRGB) space, so the reflection addend is scaled in encoded
// space too: monotonic in keep (0 => none), not linear-light exact.
void applyLegacyEnvKeep(inout vec3 color, vec3 legacyenv, vec4 spec, vec3 pos, vec3 norm, float envIntensity, float keep)
{
    color = srgb_to_linear(mix(linear_to_srgb(color.rgb), legacyenv*2.0*keep, envIntensity));
}

void applyLegacyEnv(inout vec3 color, vec3 legacyenv, vec4 spec, vec3 pos, vec3 norm, float envIntensity)
{
    applyLegacyEnvKeep(color, legacyenv, spec, pos, norm, envIntensity, 1.0);
}


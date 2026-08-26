/**
 * Identity fallback for Actor FX beauty/material transforms.
 *
 * The signatures intentionally mirror actorFxF.glsl.  Material shaders can
 * keep their Actor FX permutation and authored coverage when the optional
 * effect module cannot be compiled or attached.
 */

bool actorFxActive()
{
    return false;
}

float actorFxAuthoredMaterialResponse()
{
    return 1.0;
}

float actorFxBeautyDissolveCoverage()
{
    return 1.0;
}

float actorFxBeautyDissolveAlpha()
{
    return 1.0;
}

float actorFxPbrDissolveCoverage()
{
    return 1.0;
}

float actorFxPbrDissolveAlpha(float coverage)
{
    return 1.0;
}

float actorFxPbrNormalAoResponse()
{
    return 1.0;
}

float actorFxPbrAuthoredEmissiveResponse()
{
    return 1.0;
}

vec3 actorFxPbrVhsColor(vec3 source)
{
    return source;
}

bool actorFxRgbSplitEnabled()
{
    return false;
}

bool actorFxUvTransformEnabled()
{
    return false;
}

vec2 actorFxUv(vec2 authored_uv, vec3 position_eye)
{
    return authored_uv;
}

vec2 actorFxRgbSplitUv(vec2 transformed_uv, float direction)
{
    return transformed_uv;
}

vec3 actorFxApply(vec3 source, vec3 normal_eye, vec3 position_eye, vec2 authored_uv)
{
    return source;
}

vec3 actorFxPbrPreLight(vec3 source)
{
    return source;
}

vec3 actorFxPbrPostLight(vec3 lit_color, vec3 authored_source,
                         vec3 geometry_normal_eye, vec3 position_eye,
                         vec2 authored_uv, float dissolve_coverage,
                         out vec3 synthetic_emission)
{
    synthetic_emission = vec3(0.0);
    return lit_color;
}

vec3 actorFxPbrSyntheticEmission(vec3 authored_source,
                                 vec3 geometry_normal_eye,
                                 vec3 position_eye, vec2 authored_uv,
                                 float dissolve_coverage)
{
    return vec3(0.0);
}

vec2 actorFxPbrMaterial(vec2 roughness_metallic)
{
    return roughness_metallic;
}

vec3 actorFxEmissive(vec3 authored_emissive, vec3 styled_color)
{
    return authored_emissive;
}

vec3 actorFxBeautyEmissive(vec3 authored_emissive, vec3 styled_color)
{
    return actorFxEmissive(authored_emissive, styled_color);
}

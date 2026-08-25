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

float actorFxBeautyDissolveCoverage()
{
    return 1.0;
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

vec2 actorFxPbrMaterial(vec2 roughness_metallic)
{
    return roughness_metallic;
}

vec3 actorFxEmissive(vec3 authored_emissive, vec3 styled_color)
{
    return authored_emissive;
}

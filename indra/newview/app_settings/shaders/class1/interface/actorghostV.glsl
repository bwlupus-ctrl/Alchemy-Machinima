/**
 * @file actorghostV.glsl
 * @brief Pose-ghost FX vertex shader (hologram / x-ray ghost styles).
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * [ActorMover] The skinned constant-colour transform of highlightV.glsl, plus
 * the eye-space position and normal the fragment side needs for its
 * fresnel-ish rim term. HAS_SKIN mirrors highlightV.glsl (make_rigged_variant
 * links avatar/objectSkinV.glsl for getObjectSkinnedTransform); the rigged
 * variant is what the ghost draw binds -- the base variant exists to satisfy
 * shader creation, same as the highlight pair.
 */

uniform mat4 texture_matrix0;
uniform mat4 modelview_matrix;
uniform mat4 projection_matrix;
uniform mat3 normal_matrix;

in vec3 position;
in vec3 normal;
in vec2 texcoord0;

out vec2 vary_texcoord0;
out vec3 vary_position;     // eye space
out vec3 vary_normal;       // eye space; normalized in the fragment shader

#ifdef HAS_SKIN
mat4 getObjectSkinnedTransform();
#endif

void main()
{
#ifdef HAS_SKIN
    // the rigged palette is world-space (invBind * joint world) -- the same
    // seam the ghost placement rides -- so modelview * skin takes the vertex
    // straight to eye space. The skin matrix is rigid enough that rotating
    // the normal through it and normalizing in the fragment shader is fine
    // for an FX rim term.
    mat4 mat = modelview_matrix * getObjectSkinnedTransform();
    vec4 pos = mat * vec4(position.xyz, 1.0);
    vary_normal = (mat * vec4(normal.xyz, 0.0)).xyz;
#else
    vec4 pos = modelview_matrix * vec4(position.xyz, 1.0);
    vary_normal = normal_matrix * normal;
#endif
    vary_position = pos.xyz;
    gl_Position = projection_matrix * pos;
    vary_texcoord0 = (texture_matrix0 * vec4(texcoord0, 0, 1)).xy;
}

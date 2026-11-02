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
 * variant skins the worn-mesh batches, and the BASE variant places the
 * non-rigged attachment faces (collar/jewelry/flexi) through the modelview
 * the ghost draw composes per face.
 */

#ifdef GHOST_INDEXED_WORLD
// True indexed shared-world replay: one draw covers every material slot.  The
// batcher has already baked legacy SL transforms into texcoord0. This
// permutation is never linked into Ghost Studio. The engine's indexedTextureV
// feature object owns texture_index/vary_texture_index and forwards it here.
void passTextureIndex();
#else
uniform mat4 texture_matrix0;
#endif
uniform mat4 modelview_matrix;
uniform mat4 projection_matrix;
uniform mat3 normal_matrix;
#ifndef GHOST_WORLD_PASS
uniform float ghostDisplace; // 0 base, 1 Ash, 2 Arc; Studio second sweep only
uniform float ghostTime;
#ifndef HAS_SKIN
uniform vec3 ghostWorldUp; // region +Z in eye space, excludes attachment rotation
#endif
uniform vec4 ghostAux;
#endif
invariant gl_Position; // [VelZFight] Match the main-view surface depth.

in vec3 position;
in vec3 normal;
in vec2 texcoord0;
// [R2-2] indexed-batch material slot (the batcher packs it into position.w;
// the attribute array is only enabled on buffers that carry it -- others read
// the integer generic, which the ghostSlot = -1 default never filters on)
#ifndef GHOST_INDEXED_WORLD
in int texture_index;
#endif
// [R2-4] per-vertex colour: legacy faces bake the editor's TE tint (rgb) and
// transparency (a) here. Buffers without a COLOR array read the generic
// attribute, which the ghost draw parks at WHITE -- so PBR batches (whose
// tint is the base-colour FACTOR already in the colour uniform) and untinted
// faces are byte-identical, never double-tinted.
in vec4 diffuse_color;

out vec2 vary_texcoord0;
out vec3 vary_position;     // eye space
out vec3 vary_normal;       // eye space; normalized in the fragment shader
#ifndef GHOST_INDEXED_WORLD
flat out int vary_texture_index;
#endif
out vec4 vary_vertex_color;
// Stable pre-skin/local position shared with native Actor FX dissolve. Using
// eye/world position here would make the field swim when the actor animates.
out vec3 vary_object_position;

#ifdef HAS_SKIN
mat4 getObjectSkinnedTransform();
#endif
#ifdef GHOST_WORLD_PASS
void calcAtmospherics(vec3 inPositionEye);
#endif

void main()
{
    vec3 ghost_position = position.xyz;
#ifndef GHOST_WORLD_PASS
    float ghost_rise = 0.0;
#endif
#ifndef GHOST_WORLD_PASS
    if (ghostDisplace > 0.0)
    {
        // A smooth shell avoids independent vertex hashes tearing long faces.
        // Keep vary_object_position unchanged: particle seeds remain in rest space.
        float age = fract(ghostTime * (ghostDisplace < 1.5 ? 0.65 : 6.0)
                          + ghostAux.w * 0.15915494);
        float seed = 0.65 + 0.35 * sin(dot(position.xyz, vec3(3.1, 5.7, 2.3)));
        ghost_position += normal * (0.08 + age * 0.42) * seed;
        ghost_rise = ghostDisplace < 1.5 ? age * 0.70 : age * 0.10;
#ifdef HAS_SKIN
        ghost_position.z += ghost_rise;
#endif
    }
#endif
#ifdef HAS_SKIN
    // the rigged palette is world-space (invBind * joint world) -- the same
    // seam the ghost placement rides -- so modelview * skin takes the vertex
    // straight to eye space. The skin matrix is rigid enough that rotating
    // the normal through it and normalizing in the fragment shader is fine
    // for an FX rim term.
    mat4 mat = modelview_matrix * getObjectSkinnedTransform();
    vec4 pos = mat * vec4(ghost_position, 1.0);
    vary_normal = (mat * vec4(normal.xyz, 0.0)).xyz;
#else
    vec4 pos = modelview_matrix * vec4(ghost_position, 1.0);
    vary_normal = normal_matrix * normal;
#endif
#if !defined(GHOST_WORLD_PASS) && !defined(HAS_SKIN)
    if (ghostDisplace > 0.0) pos.xyz += ghostWorldUp * ghost_rise;
#endif
    vary_position = pos.xyz;
    gl_Position = projection_matrix * pos;
#ifdef GHOST_WORLD_PASS
    // The dedicated shared replay runs in the world HDR stream.  Populate the
    // same atmospheric varyings as ordinary world geometry; the historical
    // Ghost Studio/interface permutation does not compile this branch.
    calcAtmospherics(pos.xyz);
#endif
#ifdef GHOST_INDEXED_WORLD
    vary_texcoord0 = texcoord0;
    passTextureIndex();
#else
    vary_texcoord0 = (texture_matrix0 * vec4(texcoord0, 0, 1)).xy;
    vary_texture_index = texture_index;
#endif
    vary_vertex_color = diffuse_color;
    vary_object_position = position.xyz;
}

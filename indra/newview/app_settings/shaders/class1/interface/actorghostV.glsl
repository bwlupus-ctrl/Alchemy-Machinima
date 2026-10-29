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
#ifdef GHOST_SHARED_DISSOLVE
// Stable pre-skin/local position shared with native Actor FX dissolve. Using
// eye/world position here would make the field swim when the actor animates.
out vec3 vary_object_position;
#endif

#ifdef HAS_SKIN
mat4 getObjectSkinnedTransform();
#endif
#ifdef GHOST_WORLD_PASS
void calcAtmospherics(vec3 inPositionEye);
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
#ifdef GHOST_SHARED_DISSOLVE
    vary_object_position = position.xyz;
#endif
}

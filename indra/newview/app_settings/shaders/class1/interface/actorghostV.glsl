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
// Clone transition coordinate (PLAN 3.1): final skinned position (before Slice/Interlace/Jitter) into body space.
uniform vec4 actorFxTransition;      // x = style (0 = off)
uniform mat4 transitionEyeToBody;
out vec3 vary_transition_pos;

#ifdef HAS_SKIN
mat4 getObjectSkinnedTransform();
// Delta rows, not a matrix: never-uploaded uniforms are zero and inert.
uniform vec4 ghostRestDelta[3];
#endif
#ifdef GHOST_WORLD_PASS
void calcAtmospherics(vec3 inPositionEye);
#endif

// BEGIN STACKABLE CLONE FX VERTEX
uniform vec4 ghostFx2; // slice, dropout, power flicker, wipe
uniform vec4 ghostFx3; // interlace, position jitter, viewport height, bounded FX time
uniform vec4 ghostAux;
uniform float ghostTime; // already stepped by Effect fps on the CPU
uniform vec3 ghostFxLateral; // ONE body lateral in eye space, shared by all parts
flat out int vary_fx_body;

float cloneFxHash(vec2 p)
{
    // Integer avalanche; exact 24-bit result in every linked program.
    // Bound even noncanonical mesh heights before the signed conversion/add.
    // Normal slab/beat/phase inputs are unchanged; valid in GLSL 1.40.
    p = clamp(p, vec2(-262144.0), vec2(262143.0));
    uvec2 bits = uvec2(ivec2(floor(p * 4096.0)) + ivec2(0x40000000));
    uint h = bits.x ^ (bits.y * 0x9e3779b9u);
    h ^= h >> 16;
    h *= 0x7feb352du;
    h ^= h >> 15;
    h *= 0x846ca68bu;
    h ^= h >> 16;
    return float(h >> 8) * (1.0 / 16777216.0);
}

void cloneFxVertex(inout vec4 pos, int body, vec3 rest)
{
    vary_fx_body = body; // 0 rigid, 1 anatomical body, 2 eyes (no Dropout/Interlace)
    // Uniform early-out: the original position arithmetic is untouched at zero.
    if (ghostFx2.x <= 0.0 && ghostFx3.x <= 0.0 && ghostFx3.y <= 0.0) return;
    float offset = 0.0;
    float t = ghostFx3.w + ghostAux.w * 0.73;
    // FX-only eye height. Never alter the coordinates consumed by looks.
    float fxHeight = body == 2 ? 1.762 : rest.z; // sl_eye
    if (body != 0 && ghostFx2.x > 0.0)
    {
        float slab = floor((fxHeight + 0.08) / 0.22);
        float cycle = (t + cloneFxHash(vec2(slab, ghostAux.w))) / 2.4;
        float u = fract(cycle);
        float settle = smoothstep(0.0, 0.12, u) * (1.0 - smoothstep(0.30, 0.92, u));
        offset += (2.0 * cloneFxHash(vec2(slab, mod(floor(cycle), 2560.0) + ghostAux.w)) - 1.0)
                  * 0.12 * clamp(ghostFx2.x, 0.0, 1.0) * settle;
    }
    if (body == 1 && ghostFx3.x > 0.0)
    {
        // Fixed 6 cm lines; suppress unresolved frequencies rather than changing
        // the lattice with camera distance. A line pair must span >= 4 pixels.
        float eyeDepth = abs(projection_matrix[3][3]) > 0.5 ? 1.0 : abs(pos.z);
        float pixel = 2.0 * eyeDepth / max(abs(projection_matrix[1][1])
                      * max(ghostFx3.z, 1.0) * length(ghostFxLateral), 1e-6);
        float detail = 1.0 - smoothstep(0.012, 0.030, pixel);
        float line = mod(floor(rest.z / 0.06), 2.0) * 2.0 - 1.0;
        float beat = mod(floor(t * 4.0), 24576.0);
        float direction = cloneFxHash(vec2(beat, ghostAux.w)) > 0.5 ? 1.0 : -1.0;
        offset += line * direction * 0.018 * clamp(ghostFx3.x, 0.0, 1.0) * detail;
    }
    if (ghostFx3.y > 0.0)
    {
        float beat = mod(floor(t * 3.0), 18432.0);
        float u = fract(t * 3.0);
        float jump = step(0.55, cloneFxHash(vec2(beat, ghostAux.w)))
                     * (1.0 - step(0.32, u));
        // Independent of position, skin, attachment axes and material slot.
        offset += (2.0 * cloneFxHash(vec2(beat + 9.0, ghostAux.w)) - 1.0)
                  * 0.035 * clamp(ghostFx3.y, 0.0, 1.0) * jump;
    }
    pos.xyz += ghostFxLateral * offset;
}
// END STACKABLE CLONE FX VERTEX

void main()
{
    vec3 rest = position;
#ifdef HAS_SKIN
    // Keep the exact raw path for normal/unset batches. Reconstruct the
    // corrected rows before the dot products: adding a delta to large stored
    // centimetre coordinates afterwards needlessly cancels significant bits.
    if (any(notEqual(ghostRestDelta[0], vec4(0.0))) ||
        any(notEqual(ghostRestDelta[1], vec4(0.0))) ||
        any(notEqual(ghostRestDelta[2], vec4(0.0))))
    {
        vec4 p = vec4(position, 1.0);
        rest = vec3(dot(ghostRestDelta[0] + vec4(1.0, 0.0, 0.0, 0.0), p),
                    dot(ghostRestDelta[1] + vec4(0.0, 1.0, 0.0, 0.0), p),
                    dot(ghostRestDelta[2] + vec4(0.0, 0.0, 1.0, 0.0), p));
    }
#endif
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
    vary_transition_pos = actorFxTransition.x > 0.5 ? (transitionEyeToBody * vec4(pos.xyz, 1.0)).xyz : vec3(0.0);
#ifdef HAS_SKIN
    cloneFxVertex(pos, 1, rest);
#else
    cloneFxVertex(pos, 0, rest);
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
    vary_object_position = rest;
}

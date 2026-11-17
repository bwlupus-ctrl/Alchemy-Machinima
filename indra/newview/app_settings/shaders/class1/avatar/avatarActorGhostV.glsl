/**
 * Classic/system-avatar Actor FX replay vertex shader.
 *
 * Uses the same classic AVATAR_MATRIX palette as avatarV.glsl, while exporting
 * actorghostF's eye-space inputs and the unskinned/rest-space position used by
 * shared Dissolve coverage.  No duplicate skeleton or geometry is created.
 */

uniform mat4 projection_matrix;
invariant gl_Position; // [VelZFight] Match classic-avatar depth positions.

in vec3 position;
in vec3 normal;
in vec2 texcoord0;
#ifdef AVATAR_CLOTH
in vec4 clothing;
#endif

out vec2 vary_texcoord0;
out vec3 vary_position;
out vec3 vary_normal;
out vec3 vary_object_position;
// Clone transition coordinate (PLAN 3.1): final skinned position (before Slice/Interlace/Jitter) into body space.
uniform vec4 actorFxTransition;      // x = style (0 = off)
uniform mat4 transitionEyeToBody;
out vec3 vary_transition_pos;
flat out int vary_texture_index;
out vec4 vary_vertex_color;

mat4 getSkinnedTransform();
void calcAtmospherics(vec3 inPositionEye);

#ifdef AVATAR_CLOTH
uniform vec4 gWindDir;
uniform vec4 gSinWaveParams;
uniform vec4 gGravity;
const vec4 gMinMaxConstants = vec4(1.0, 0.166666, 0.0083143, .00018542);
const vec4 gPiConstants = vec4(0.159154943, 6.28318530, 3.141592653, 1.5707963);
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
    vec4 rest = vec4(position, 1.0);
    mat4 skin = getSkinnedTransform();

    vec4 pos;
    pos.x = dot(skin[0], rest);
    pos.y = dot(skin[1], rest);
    pos.z = dot(skin[2], rest);
    pos.w = 1.0;

    vec3 norm;
    norm.x = dot(skin[0].xyz, normal);
    norm.y = dot(skin[1].xyz, normal);
    norm.z = dot(skin[2].xyz, normal);
    norm = normalize(norm);

#ifdef AVATAR_CLOTH
    vec4 windEffect = vec4(dot(norm, gWindDir.xyz));
    float along_skin_z = dot(skin[2], rest);
    windEffect.xyz = along_skin_z * vec3(0.015) + windEffect.xyz;
    windEffect.w = (windEffect.w * 2.0 + 1.0) * gWindDir.w;
    windEffect.xyz = windEffect.xyz * gSinWaveParams.xyz
                   + vec3(gSinWaveParams.w);

    vec4 temp1 = windEffect * gPiConstants.x;
    vec4 temp0 = vec4(0.0);
    temp0.y = mod(temp1.x, 1.0);
    windEffect.x = temp0.y * gPiConstants.y;
    temp1.z -= gPiConstants.w;
    temp0.y = mod(temp1.z, 1.0);
    windEffect.z = temp0.y * gPiConstants.y;
    windEffect.xyz += vec3(-3.141592);

    vec4 sinWave;
    temp1 = windEffect * windEffect;
    sinWave = -temp1 * gMinMaxConstants.w + vec4(gMinMaxConstants.z);
    sinWave = sinWave * -temp1 + vec4(gMinMaxConstants.y);
    sinWave = sinWave * -temp1 + vec4(gMinMaxConstants.x);
    sinWave *= windEffect;
    sinWave.xyz = sinWave.xyz * gWindDir.w + vec3(windEffect.w);

    temp1 = vec4(dot(norm, gGravity.xyz));
    temp1 = min(temp1, vec4(0.2, 0.0, 0.0, 0.0));
    temp1 *= vec4(1.5, 0.0, 0.0, 0.0);
    sinWave.x += temp1.x;
    sinWave.xyz *= clothing.w;
    sinWave.xyz = max(sinWave.xyz, vec3(-1.0));
    vec4 offsetPos = vec4(1.0, 1.0, 1.0, 0.0)
                   * (clothing * sinWave.x) + rest;
    vec4 temp2 = gWindDir * sinWave.z + vec4(norm, 0.0);
    norm = normalize(norm + temp2.xyz * 2.0);

    pos.x = dot(skin[0], offsetPos);
    pos.y = dot(skin[1], offsetPos);
    pos.z = dot(skin[2], offsetPos);
#endif

    vary_transition_pos = actorFxTransition.x > 0.5 ? (transitionEyeToBody * vec4(pos.xyz, 1.0)).xyz : vec3(0.0);
    cloneFxVertex(pos, 1, position);
    vary_texcoord0 = texcoord0;
    vary_position = pos.xyz;
    vary_normal = normalize(norm);
    vary_object_position = position;
    vary_texture_index = -1;
    vary_vertex_color = vec4(1.0);

    calcAtmospherics(pos.xyz);
    gl_Position = projection_matrix * pos;
}

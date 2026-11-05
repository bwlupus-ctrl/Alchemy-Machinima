/**
 * Rigid classic-eyeball Actor FX replay vertex shader.
 */

uniform mat4 projection_matrix;
uniform mat4 texture_matrix0;
uniform mat4 modelview_matrix;
uniform mat4 modelview_projection_matrix;
uniform mat3 normal_matrix;
invariant gl_Position; // [VelZFight] Match classic-eyeball depth positions.

in vec3 position;
in vec3 normal;
in vec2 texcoord0;

out vec2 vary_texcoord0;
out vec3 vary_position;
out vec3 vary_normal;
out vec3 vary_object_position;
flat out int vary_texture_index;
out vec4 vary_vertex_color;

void calcAtmospherics(vec3 inPositionEye);

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
    vec4 pos = modelview_matrix * vec4(position, 1.0);
    cloneFxVertex(pos, 2, position); // Slice follows the head slab; looks stay joint-local
    vary_texcoord0 = (texture_matrix0 * vec4(texcoord0, 0.0, 1.0)).xy;
    vary_position = pos.xyz;
    vary_normal = normalize(normal_matrix * normal);
    vary_object_position = position;
    vary_texture_index = -1;
    vary_vertex_color = vec4(1.0);

    calcAtmospherics(pos.xyz);
    gl_Position = modelview_projection_matrix * vec4(position, 1.0);
    if (ghostFx2.x > 0.0 || ghostFx3.y > 0.0) gl_Position = projection_matrix * pos;
}

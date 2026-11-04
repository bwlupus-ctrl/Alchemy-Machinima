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
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

void cloneFxVertex(inout vec4 pos, bool body)
{
    vary_fx_body = body ? 1 : 0;
    // Uniform early-out: the original position arithmetic is untouched at zero.
    if (ghostFx2.x <= 0.0 && ghostFx3.x <= 0.0 && ghostFx3.y <= 0.0) return;
    float offset = 0.0;
    float t = ghostFx3.w + ghostAux.w * 0.73;
    if (body && ghostFx2.x > 0.0)
    {
        float slab = floor(position.z / 0.22);
        float cycle = (t + cloneFxHash(vec2(slab, ghostAux.w))) / 2.4;
        float u = fract(cycle);
        float settle = smoothstep(0.0, 0.12, u) * (1.0 - smoothstep(0.30, 0.92, u));
        offset += (2.0 * cloneFxHash(vec2(slab, mod(floor(cycle), 2560.0) + ghostAux.w)) - 1.0)
                  * 0.12 * clamp(ghostFx2.x, 0.0, 1.0) * settle;
    }
    if (body && ghostFx3.x > 0.0)
    {
        // Fixed 6 cm lines; suppress unresolved frequencies rather than changing
        // the lattice with camera distance. A line pair must span >= 4 pixels.
        float eyeDepth = abs(projection_matrix[3][3]) > 0.5 ? 1.0 : abs(pos.z);
        float pixel = 2.0 * eyeDepth / max(abs(projection_matrix[1][1])
                      * max(ghostFx3.z, 1.0) * length(ghostFxLateral), 1e-6);
        float detail = 1.0 - smoothstep(0.012, 0.030, pixel);
        float line = mod(floor(position.z / 0.06), 2.0) * 2.0 - 1.0;
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
    cloneFxVertex(pos, false); // rigid eye coordinates are joint-local
    vary_texcoord0 = (texture_matrix0 * vec4(texcoord0, 0.0, 1.0)).xy;
    vary_position = pos.xyz;
    vary_normal = normalize(normal_matrix * normal);
    vary_object_position = position;
    vary_texture_index = -1;
    vary_vertex_color = vec4(1.0);

    calcAtmospherics(pos.xyz);
    gl_Position = modelview_projection_matrix * vec4(position, 1.0);
    if (ghostFx3.y > 0.0) gl_Position = projection_matrix * pos;
}

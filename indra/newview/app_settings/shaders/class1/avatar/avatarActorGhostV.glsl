/**
 * Classic/system-avatar Actor FX replay vertex shader.
 *
 * Uses the same classic AVATAR_MATRIX palette as avatarV.glsl, while exporting
 * actorghostF's eye-space inputs and the unskinned/rest-space position used by
 * shared Dissolve coverage.  No duplicate skeleton or geometry is created.
 */

uniform mat4 projection_matrix;

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

    vary_texcoord0 = texcoord0;
    vary_position = pos.xyz;
    vary_normal = normalize(norm);
    vary_object_position = position;
    vary_texture_index = -1;
    vary_vertex_color = vec4(1.0);

    calcAtmospherics(pos.xyz);
    gl_Position = projection_matrix * pos;
}

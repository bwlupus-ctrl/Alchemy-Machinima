/**
 * Actor FX synthetic glow vertex shader for legacy alpha surfaces that have
 * no authored emissive vertex stream. It deliberately consumes the ordinary
 * colour attribute, so enabling a cinematic style never grows or rebuilds the
 * surface VBO.
 */

uniform mat4 texture_matrix0;
uniform mat4 modelview_matrix;
uniform mat4 modelview_projection_matrix;
invariant gl_Position; // [VelZFight] Keep glow replay depth-consistent.

in vec3 position;
in vec4 diffuse_color;
in vec2 texcoord0;

out vec3 vary_actor_fx_position;
uniform vec4 actorFxTransition;      // clone transition: x = style (0 = off); see actorFxDissolveF.glsl
uniform mat4 transitionEyeToBody;     // eye space -> body-space transition coordinate (PLAN 3.1)
out vec3 vary_transition_pos;
out vec3 vary_position;
out vec4 vertex_color;
out vec2 vary_texcoord0;

void passTextureIndex();

#ifdef HAS_SKIN
mat4 getObjectSkinnedTransform();
uniform mat4 projection_matrix;
#endif

void main()
{
    vary_actor_fx_position = position;
    passTextureIndex();

#ifdef HAS_SKIN
    mat4 mat = modelview_matrix * getObjectSkinnedTransform();
    vec4 pos = mat * vec4(position, 1.0);
    gl_Position = projection_matrix * pos;
#else
    vec4 pos = modelview_matrix * vec4(position, 1.0);
    gl_Position = modelview_projection_matrix * vec4(position, 1.0);
#endif

    vary_position = pos.xyz;
    vary_transition_pos = actorFxTransition.x > 0.5 ? (transitionEyeToBody * vec4(pos.xyz, 1.0)).xyz : vec3(0.0);
    vary_texcoord0 = (texture_matrix0 * vec4(texcoord0, 0.0, 1.0)).xy;
    vertex_color = diffuse_color;
}

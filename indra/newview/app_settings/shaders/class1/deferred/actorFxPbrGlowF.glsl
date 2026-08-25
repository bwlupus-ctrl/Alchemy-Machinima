/** Synthetic PBR Actor FX glow for zero-authored-emissive alpha surfaces. */

/*[EXTRA_CODE_HERE]*/

uniform sampler2D diffuseMap;
uniform float minimum_alpha;

out vec4 frag_color;

in vec3 vary_position;
in vec4 vertex_color;
in vec2 base_color_texcoord;

vec3 srgb_to_linear(vec3 c);
vec3 actorFxApply(vec3 source, vec3 normal_eye, vec3 position_eye, vec2 authored_uv);
vec3 actorFxEmissive(vec3 authored_emissive, vec3 styled_color);
bool actorFxActive();
bool actorFxUvTransformEnabled();
bool actorFxRgbSplitEnabled();
vec2 actorFxUv(vec2 authored_uv, vec3 position_eye);
vec2 actorFxRgbSplitUv(vec2 transformed_uv, float direction);

void main()
{
    vec4 authored = texture(diffuseMap, base_color_texcoord);
    float authored_alpha = authored.a * vertex_color.a;

    // PBR BLEND beauty tests authored texture alpha against minimum_alpha and
    // applies the base-colour factor only to final coverage. The material bind
    // supplies -1 for ordinary BLEND and the alpha pool raises it to 0 for its
    // custom-blend policy.
    if (!actorFxActive() || authored.a < minimum_alpha ||
        authored_alpha <= 0.0)
    {
        frag_color = vec4(0.0);
        return;
    }

    vec2 fx_uv = base_color_texcoord;
    if (actorFxUvTransformEnabled())
    {
        fx_uv = actorFxUv(fx_uv, vary_position);
        authored.rgb = texture(diffuseMap, fx_uv).rgb;
    }
    if (actorFxRgbSplitEnabled())
    {
        authored.r = texture(diffuseMap, actorFxRgbSplitUv(fx_uv, -1.0)).r;
        authored.b = texture(diffuseMap, actorFxRgbSplitUv(fx_uv,  1.0)).b;
    }

    vec3 n = cross(dFdx(vary_position), dFdy(vary_position));
    float n_len2 = dot(n, n);
    n = n_len2 > 1e-12 ? n * inversesqrt(n_len2) : vec3(0.0, 0.0, 1.0);

    vec3 source = vertex_color.rgb * srgb_to_linear(authored.rgb);
    vec3 styled = actorFxApply(source, n, vary_position, base_color_texcoord);
    vec3 emitted = actorFxEmissive(vec3(0.0), styled);
    float glow = max(max(emitted.r, emitted.g), emitted.b) * authored_alpha;
    frag_color = vec4(0.0, 0.0, 0.0, max(glow, 0.0));
}

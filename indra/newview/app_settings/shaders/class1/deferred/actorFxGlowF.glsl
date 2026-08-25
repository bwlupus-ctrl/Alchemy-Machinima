/**
 * Synthetic legacy Actor FX glow. Beauty owns authored alpha and sorting; this
 * additive-alpha sub-pass only publishes the style's emissive contribution.
 */

/*[EXTRA_CODE_HERE]*/

out vec4 frag_color;

in vec3 vary_position;
in vec4 vertex_color;
in vec2 vary_texcoord0;

uniform float minimum_alpha;
// 0 = no cutout (legacy material BLEND), 1 = texture alpha (fullbright),
// 2 = texture * face alpha (ordinary lit alpha).
uniform int actorFxAlphaCutoffMode;

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
    vec4 authored = diffuseLookup(vary_texcoord0);
    float authored_alpha = authored.a * vertex_color.a;
    float cutoff_alpha = actorFxAlphaCutoffMode == 1
        ? authored.a : authored_alpha;

    if (!actorFxActive() || authored_alpha <= 0.0 ||
        (actorFxAlphaCutoffMode != 0 && cutoff_alpha < minimum_alpha))
    {
        frag_color = vec4(0.0);
        return;
    }

    vec2 fx_uv = vary_texcoord0;
    if (actorFxUvTransformEnabled())
    {
        fx_uv = actorFxUv(fx_uv, vary_position);
        authored.rgb = diffuseLookup(fx_uv).rgb;
    }
    if (actorFxRgbSplitEnabled())
    {
        authored.r = diffuseLookup(actorFxRgbSplitUv(fx_uv, -1.0)).r;
        authored.b = diffuseLookup(actorFxRgbSplitUv(fx_uv,  1.0)).b;
    }

    vec3 n = cross(dFdx(vary_position), dFdy(vary_position));
    float n_len2 = dot(n, n);
    n = n_len2 > 1e-12 ? n * inversesqrt(n_len2) : vec3(0.0, 0.0, 1.0);

    vec3 source = srgb_to_linear(authored.rgb * vertex_color.rgb);
    vec3 styled = actorFxApply(source, n, vary_position, vary_texcoord0);
    vec3 emitted = actorFxEmissive(vec3(0.0), styled);
    float glow = max(max(emitted.r, emitted.g), emitted.b) * authored_alpha;
    frag_color = vec4(0.0, 0.0, 0.0, max(glow, 0.0));
}

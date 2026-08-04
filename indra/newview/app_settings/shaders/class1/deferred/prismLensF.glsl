/**
 * @file prismLensF.glsl
 * @brief Samples the Prism Lens linear-HDR auxiliary beauty.
 */

uniform sampler2D prismLensMap;
uniform vec2 uvScale;
uniform vec2 uvOffset;
uniform float edgeFeather;

in vec2 prism_uv;

out vec4 frag_color;

void main()
{
    vec2 uv = prism_uv * uvScale + uvOffset;

    // Slice 1 is opaque-correct only. Edge feathering and transparent-pool
    // ordering are intentionally deferred; retain the uniform/API now.
    // Host-side Slice 1 clamps this reserved value non-negative. The branch
    // keeps the uniform live without adding feather math to this slice.
    if (edgeFeather < 0.0)
    {
        discard;
    }
    // Preserve the auxiliary beauty alpha as well as RGB; this viewer carries
    // its pre-tonemap prim-glow signal in that channel.
    frag_color = texture(prismLensMap, clamp(uv, 0.0, 1.0));
}

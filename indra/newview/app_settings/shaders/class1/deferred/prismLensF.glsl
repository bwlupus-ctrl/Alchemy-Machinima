/**
 * @file prismLensF.glsl
 * @brief Samples the Prism Lens linear-HDR auxiliary beauty.
 */

uniform sampler2D prismLensMap;
uniform vec2 displayToCaptureScale;
uniform vec2 displayToCaptureOffset;
uniform vec2 retainedOrientationScale;
uniform vec2 retainedOrientationOffset;
uniform vec2 textureRegionScale;
uniform vec2 textureRegionOffset;
uniform int letterbox;
uniform vec3 barColorLinear;
uniform float edgeFeather;

in vec2 prism_uv;

out vec4 frag_color;

void main()
{
    vec2 logical_uv = prism_uv * displayToCaptureScale +
                      displayToCaptureOffset;

    // Slice 1 is opaque-correct only. Edge feathering and transparent-pool
    // ordering are intentionally deferred; retain the uniform/API now.
    // Host-side Slice 1 clamps this reserved value non-negative. The branch
    // keeps the uniform live without adding feather math to this slice.
    if (edgeFeather < 0.0)
    {
        discard;
    }
    // FIT deliberately maps bars outside canonical capture UV. Test before
    // orientation and sampling so an edge texel is never smeared into a bar.
    if (letterbox != 0 &&
        (any(lessThan(logical_uv, vec2(0.0))) ||
         any(greaterThan(logical_uv, vec2(1.0)))))
    {
        // Alpha is the viewer's pre-tonemap Prism glow channel, not opacity.
        frag_color = vec4(barColorLinear, 0.0);
    }
    else
    {
        vec2 oriented_uv = logical_uv * retainedOrientationScale +
                           retainedOrientationOffset;
        vec2 texture_uv = oriented_uv * textureRegionScale +
                          textureRegionOffset;

        // Preserve the auxiliary beauty alpha as well as RGB.
        frag_color = texture(prismLensMap, texture_uv);
    }
}

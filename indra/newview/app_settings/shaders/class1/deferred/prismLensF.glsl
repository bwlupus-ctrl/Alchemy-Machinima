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

// Option C: Cinematic lens & post-processing parameters
// x: Chromatic Aberration (0.0 to 1.0)
// y: Film Grain (0.0 to 1.0)
// z: CRT Scanlines (0.0 to 1.0)
// w: Exposure Bias EV (-4.0 to +4.0)
uniform vec4 prismLensOptics;

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

        vec4 col;
        // Chromatic Aberration (Radial dispersion offset)
        if (prismLensOptics.x > 0.001)
        {
            vec2 dist = (oriented_uv - vec2(0.5)) * prismLensOptics.x * 0.015;
            vec2 r_uv = clamp(oriented_uv + dist, vec2(0.0), vec2(1.0)) * textureRegionScale + textureRegionOffset;
            vec2 b_uv = clamp(oriented_uv - dist, vec2(0.0), vec2(1.0)) * textureRegionScale + textureRegionOffset;
            float r = texture(prismLensMap, r_uv).r;
            float g = texture(prismLensMap, texture_uv).g;
            float b = texture(prismLensMap, b_uv).b;
            float a = texture(prismLensMap, texture_uv).a;
            col = vec4(r, g, b, a);
        }
        else
        {
            col = texture(prismLensMap, texture_uv);
        }

        // Exposure EV Bias Adjustment
        if (abs(prismLensOptics.w) > 0.001)
        {
            col.rgb *= exp2(prismLensOptics.w);
        }

        // CRT Scanlines
        if (prismLensOptics.z > 0.001)
        {
            float scanline = sin(oriented_uv.y * 600.0) * 0.5 + 0.5;
            col.rgb *= mix(1.0, scanline * 0.4 + 0.6, prismLensOptics.z);
        }

        // Film Grain Noise
        if (prismLensOptics.y > 0.001)
        {
            float grain = (fract(sin(dot(oriented_uv, vec2(12.9898, 78.233))) * 43758.5453) - 0.5) * prismLensOptics.y * 0.15;
            col.rgb += vec3(grain);
        }

        // Preserve the auxiliary beauty alpha as well as RGB.
        frag_color = col;
    }
}

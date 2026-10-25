/**
 * @file sensorVignetteF.glsl
 * @brief Optical transmission before collection; colour stays in presentation.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

/*[EXTRA_CODE_HERE]*/
in vec2 vary_fragcoord;
out vec4 frag_color;
uniform sampler2D diffuseRect;
uniform float uVignetteAmount, uVignetteRadius, uVignetteSoft, uVignetteShape;
uniform float uVignetteMidPoint, uVignetteFeather;
uniform vec2 uVignetteCenter, uVignetteAspect;
void main()
{
    vec2 d = vary_fragcoord - 0.5 - uVignetteCenter;
    float aspect = max(uVignetteAspect.x, 1e-4) / max(uVignetteAspect.y, 1e-4);
    d *= max(vec2(aspect, 1.0 / aspect), 1.0);
    float sq = max(abs(d.x), abs(d.y)) * 2.0;
    float dist = mix(length(d) * 2.0, sq * sqrt(sq), uVignetteShape);
    float start = uVignetteRadius - uVignetteSoft;
    float transmission;
    if (uVignetteMidPoint > 1e-4 && uVignetteMidPoint < 0.9999)
    {
        float mid = mix(start, uVignetteRadius, uVignetteMidPoint);
        float a = pow(smoothstep(start, mid, dist), uVignetteFeather) * uVignetteAmount;
        float b = pow(smoothstep(mid, uVignetteRadius, dist), uVignetteFeather) * uVignetteAmount;
        transmission = (1.0 - a) * (1.0 - b);
    }
    else transmission = 1.0 - pow(smoothstep(start, uVignetteRadius, dist), uVignetteFeather) * uVignetteAmount;
    vec4 scene = texture(diffuseRect, vary_fragcoord);
    frag_color = vec4(scene.rgb * transmission, scene.a);
}

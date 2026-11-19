/**
 * @file cineFisheyeF.glsl
 *
 * Pedro Cam Phase 1: edge-anchored radial barrel remap with an
 * aspect-correct circular peephole vignette.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2026, Linden Research, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * $/LicenseInfo$
 */

/*[EXTRA_CODE_HERE]*/

out vec4 frag_color;

in vec2 vary_fragcoord;

uniform sampler2D diffuseMap;
uniform vec2 screen_res;
// x = k1, y = k2, z = vignette radius, w = vignette softness.
uniform vec4 fisheye_params;
// x/y = lens center, z = zoom, w = reserved beat phase.
uniform vec4 fisheye_params2;

void main()
{
    vec2 uv = vary_fragcoord.xy;
    float aspect = screen_res.x / screen_res.y;
    vec2 c = uv * 2.0 - 1.0 - fisheye_params2.xy * 2.0;
    c.x *= aspect;

    float r = length(c);
    float rn = r / max(fisheye_params.z, 0.05);
    float rn2 = rn * rn;
    float g = (1.0 + fisheye_params.x * rn2 +
               fisheye_params.y * rn2 * rn2) /
              (1.0 + fisheye_params.x + fisheye_params.y);

    vec2 src = c * g / max(fisheye_params2.z, 0.25);
    vec2 suv = vec2(src.x / aspect, src.y) * 0.5 + 0.5 +
               fisheye_params2.xy;
    vec3 col = texture(diffuseMap, suv).rgb;
    if (any(lessThan(suv, vec2(0.0))) ||
        any(greaterThan(suv, vec2(1.0))))
    {
        col = vec3(0.0);
    }

    float v = 1.0 - smoothstep(
        fisheye_params.z - fisheye_params.w,
        fisheye_params.z + fisheye_params.w, r);
    frag_color = vec4(col * v, 1.0);
}

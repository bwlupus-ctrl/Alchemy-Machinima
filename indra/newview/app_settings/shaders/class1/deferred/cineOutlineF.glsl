/**
 * @file cineOutlineF.glsl
 *
 * Native deferred normal/depth edge detection for the Cine Outline pass.
 * Phase 1 uses a fixed eight-neighbor Sobel kernel and feeds both HDR bloom
 * (RGB) and legacy glow (alpha).
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

uniform vec2 screen_res;
uniform vec3 outline_color;
// x = sample spacing in pixels, y = depth sensitivity,
// z = normal sensitivity, w = HDR intensity.
uniform vec4 outline_params;
// x = legacy glow / HDR alpha feed. Remaining components are reserved.
uniform vec4 outline_params2;

// deferredUtil/gbufferUtil are linked in as separate compile units, so their
// functions must be forward-declared here or this unit fails to compile
// (this shader had been failing to load since inception for lack of these)
float getDepth(vec2 pos_screen);
vec4 getNorm(vec2 screenpos);
vec4 getPosition(vec2 pos_screen);
vec4 getPositionWithDepth(vec2 pos_screen, float depth);

float cineLinearDepth(vec2 tc, float center_depth)
{
    // Saturate a far-plane neighbor rather than allowing it to dominate the
    // Sobel gradient. This keeps the silhouette stable at grazing angles.
    return min(-getPosition(tc).z, center_depth + 100.0);
}

void main()
{
    vec2 tc = vary_fragcoord;

    float raw_depth = getDepth(tc);
    vec4 center_norm = getNorm(tc);

    // Only subject-side pixels may emit an edge. Besides avoiding work over an
    // empty background, this places an outer silhouette one pixel inside it.
    if (raw_depth >= 0.999999 ||
        GET_GBUFFER_FLAG(center_norm.w, GBUFFER_FLAG_SKIP_ATMOS))
    {
        frag_color = vec4(0.0);
        return;
    }

    // Reuse the raw center sample instead of fetching depth twice. This is the
    // same reconstruction used by getPosition(), with an explicit depth input.
    vec3 center_pos = getPositionWithDepth(tc, raw_depth).xyz;
    float center_depth = max(-center_pos.z, 0.0);
    vec3 view_dir = normalize(-center_pos);

    vec2 offset = clamp(outline_params.x, 0.5, 3.0) / screen_res;
    vec2 nw_tc = tc + vec2(-offset.x,  offset.y);
    vec2 n_tc  = tc + vec2( 0.0,       offset.y);
    vec2 ne_tc = tc + vec2( offset.x,  offset.y);
    vec2 w_tc  = tc + vec2(-offset.x,  0.0);
    vec2 e_tc  = tc + vec2( offset.x,  0.0);
    vec2 sw_tc = tc + vec2(-offset.x, -offset.y);
    vec2 s_tc  = tc + vec2( 0.0,      -offset.y);
    vec2 se_tc = tc + vec2( offset.x, -offset.y);

    float z_nw = cineLinearDepth(nw_tc, center_depth);
    float z_n  = cineLinearDepth(n_tc,  center_depth);
    float z_ne = cineLinearDepth(ne_tc, center_depth);
    float z_w  = cineLinearDepth(w_tc,  center_depth);
    float z_e  = cineLinearDepth(e_tc,  center_depth);
    float z_sw = cineLinearDepth(sw_tc, center_depth);
    float z_s  = cineLinearDepth(s_tc,  center_depth);
    float z_se = cineLinearDepth(se_tc, center_depth);

    vec3 n_nw = getNorm(nw_tc).xyz;
    vec3 n_n  = getNorm(n_tc).xyz;
    vec3 n_ne = getNorm(ne_tc).xyz;
    vec3 n_w  = getNorm(w_tc).xyz;
    vec3 n_e  = getNorm(e_tc).xyz;
    vec3 n_sw = getNorm(sw_tc).xyz;
    vec3 n_s  = getNorm(s_tc).xyz;
    vec3 n_se = getNorm(se_tc).xyz;

    float depth_x = -z_nw + z_ne - 2.0 * z_w + 2.0 * z_e - z_sw + z_se;
    float depth_y =  z_nw + 2.0 * z_n + z_ne - z_sw - 2.0 * z_s - z_se;
    float depth_gradient = length(vec2(depth_x, depth_y)) /
                           max(center_depth, 0.01);

    float facing = clamp(dot(center_norm.xyz, view_dir), 0.1, 1.0);
    float depth_edge = smoothstep(0.03 / facing, 0.10 / facing,
                                  depth_gradient * outline_params.y);

    vec3 normal_x = -n_nw + n_ne - 2.0 * n_w + 2.0 * n_e - n_sw + n_se;
    vec3 normal_y =  n_nw + 2.0 * n_n + n_ne - n_sw - 2.0 * n_s - n_se;
    float normal_gradient = sqrt(dot(normal_x, normal_x) +
                                 dot(normal_y, normal_y)) * 0.25;
    float normal_edge = smoothstep(0.4, 1.0,
                                   normal_gradient * outline_params.z);

    float edge = 1.0 - (1.0 - depth_edge) * (1.0 - normal_edge);
    frag_color = vec4(outline_color * outline_params.w * edge,
                      edge * outline_params2.x);
}

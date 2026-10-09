/**
 * @file cofF.glsl
 *
 * $LicenseInfo:firstyear=2007&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2007, Linden Research, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

/*[EXTRA_CODE_HERE]*/

out vec4 frag_color;

uniform sampler2D diffuseRect;
uniform sampler2D depthMap;

uniform float depth_cutoff;
uniform float norm_cutoff;
uniform float focal_distance;
uniform float blur_constant;
uniform float tan_pixel_angle;
uniform float magnification;
uniform float max_cof;

uniform mat4 inv_proj;
uniform vec2 screen_res;

in vec2 vary_fragcoord;

// [LensLooks] Soft-Edge "Defocus" blur type: widen the circle of confusion
// outside the sweet spot so DoF renders real bokeh there
// (doc/LENS_LOOKS_CONTRACT.md, stage S4).  dof_softedge.x == 0 (the GL
// default) is the bit-exact off path: the branch in main() is untaken and the
// original statements run unchanged.  The mask is the same function as
// lensSoftEdgeF.glsl (keep in sync); the host uploads the same lens_softedge /
// lens_softedge2 / lens_softedge5 values to both programs.
//   dof_softedge   x edge CoF as a fraction of max_cof, 0..1 (0 = off); the host
//                  uploads RenderSoftEdgeBlur directly, so the blur radius is
//                  LINEAR in the slider.  yzw reserved (ignored).
// Sign: calc_cof() gives a FAR pixel a NEGATIVE sc (focal_distance is
// -subject_distance, depth is -z), and both DoF programs blur the far side on
// sc < -0.5 (postDeferredF.glsl; FRONT_BLUR only adds the sc > 0.5 near path),
// so the injected edge blur must be written with the far (negative) sign.
uniform vec4 dof_softedge;
uniform vec4 lens_softedge;   // x shape, y size, z aspect, w angle
uniform vec4 lens_softedge2;  // xy centre, z feather, w (unused here)
uniform vec4 lens_softedge5;  // xy frame size px, zw tile offset px

float lensSoftEdgeMask(vec2 frag_px)
{
    vec2  frame  = max(lens_softedge5.xy, vec2(1.0));
    vec2  uv_f   = (frag_px + lens_softedge5.zw) / frame;
    float aspect = frame.x / frame.y;

    vec2 p = uv_f - lens_softedge2.xy;
    p.x *= aspect;
    float ang = lens_softedge.w;
    float ca = cos(ang), sa = sin(ang);
    p = vec2(ca * p.x + sa * p.y, -sa * p.x + ca * p.y);

    float size  = max(lens_softedge.y, 1.0e-3);
    int   shape = int(lens_softedge.x + 0.5);
    float dist;
    if (shape == 2)
    {
        dist = abs(p.y) / size;
    }
    else if (shape == 1)
    {
        dist = length(p / vec2(max(lens_softedge.z, 1.0e-3), 1.0)) / size;
    }
    else
    {
        dist = length(p) / size;
    }
    float feather = max(lens_softedge2.z, 1.0e-3);
    return smoothstep(1.0, 1.0 + feather, dist);
}

float calc_cof(float depth)
{
    float sc = (depth-focal_distance)/-depth*blur_constant;

    sc /= magnification;

    // tan_pixel_angle = pixel_length/-depth;
    float pixel_length =  tan_pixel_angle*-focal_distance;

    sc = sc/pixel_length;
    sc *= 1.414;

    return sc;
}

void main()
{
    vec2 tc = vary_fragcoord.xy;

    float z = texture(depthMap, tc).r;
    z = z*2.0-1.0;
    vec4 ndc = vec4(0.0, 0.0, z, 1.0);
    vec4 p = inv_proj*ndc;
    float depth = p.z/p.w;

    vec4 diff = texture(diffuseRect, vary_fragcoord.xy);

    float sc = calc_cof(depth);
    sc = min(sc, max_cof);
    sc = max(sc, -max_cof);

    // [LensLooks] Defocus: outside the sweet spot, push the CoF toward the
    // far-blur edge value (never shrinking an existing larger blur of either
    // sign).  Off (dof_softedge.x == 0) leaves sc exactly as computed above.
    // Far blur is NEGATIVE here (see the dof_softedge comment), which is the
    // branch both gDeferredPostProgram and gDeferredPostProgramNoNear take.
    if (dof_softedge.x > 0.0)
    {
        float m       = lensSoftEdgeMask(gl_FragCoord.xy);
        float sc_edge = clamp(dof_softedge.x, 0.0, 1.0) * max_cof * m;
        if (sc_edge > abs(sc))
        {
            sc = -sc_edge;
        }
    }

    frag_color.rgb = diff.rgb;
    frag_color.a = sc/max_cof*0.5+0.5;
}

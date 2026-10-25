/**
 * @file dofCombineF.glsl
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
uniform sampler2D lightMap;

uniform mat4 inv_proj;
uniform vec2 screen_res;

uniform float max_cof;
uniform float res_scale;
uniform float dof_width;
uniform float dof_height;

in vec2 vary_fragcoord;

vec4 dofSample(sampler2D tex, vec2 tc)
{
    tc.x = min(tc.x, dof_width);
    tc.y = min(tc.y, dof_height);

    return texture(tex, tc);
}

void main()
{
#ifdef AL_SENSOR_HDR
    // Integrate the reduced cells over each destination pixel. Bilinear
    // upsampling at a non-integer scale (the shipped .7) is not conservative.
    ivec2 size = max(ivec2(screen_res*res_scale), ivec2(1));
    vec2 scale = vec2(size)/screen_res;
    vec2 lo = floor(gl_FragCoord.xy)*scale, hi = (floor(gl_FragCoord.xy)+1.0)*scale;
    vec3 value = vec3(0.0);
    for (int y=int(floor(lo.y)); y<int(ceil(hi.y)); ++y)
    for (int x=int(floor(lo.x)); x<int(ceil(hi.x)); ++x)
    {
        vec2 overlap = max(min(hi,vec2(x+1,y+1))-max(lo,vec2(x,y)),vec2(0.0));
        value += texelFetch(diffuseRect, min(ivec2(x,y),size-1),0).rgb*overlap.x*overlap.y;
    }
    vec4 sharp = texture(lightMap,vary_fragcoord);
    float coc = (sharp.a*2.0-1.0)*max_cof;
#if !FRONT_BLUR
    coc = min(coc, 0.0);
#endif
    // Preserve the original alpha/CoC combine contract independently of the
    // conservative RGB reconstruction (gather still writes RGB only).
    float blend_a = min(abs(sharp.a*2.0-1.0)*max_cof*res_scale*res_scale,1.0);
    float output_alpha = sharp.a;
    if (blend_a > 0.25 && blend_a < 0.75)
    {
        vec2 delta = vec2(blend_a/res_scale)/screen_res;
        float neighbours = texture(lightMap,vary_fragcoord+delta).a;
        neighbours += texture(lightMap,vary_fragcoord-delta).a;
        neighbours += texture(lightMap,vary_fragcoord+vec2(delta.x,-delta.y)).a;
        neighbours += texture(lightMap,vary_fragcoord+vec2(-delta.x,delta.y)).a;
        output_alpha = mix(output_alpha,neighbours*0.25,blend_a);
    }
    output_alpha = mix(output_alpha,dofSample(diffuseRect,vary_fragcoord*res_scale).a,blend_a);
    frag_color = vec4(value/(scale.x*scale.y)+(abs(coc)<=0.5 ? sharp.rgb : vec3(0.0)),output_alpha);
    return;
#endif
    vec2 tc = vary_fragcoord.xy;

    vec4 dof = dofSample(diffuseRect, vary_fragcoord.xy*res_scale);

    vec4 diff = texture(lightMap, vary_fragcoord.xy);

    float a = min(abs(diff.a*2.0-1.0) * max_cof*res_scale*res_scale, 1.0);

    if (a > 0.25 && a < 0.75)
    { //help out the transition a bit
        float sc = a/res_scale;

        vec4 col;
        col = texture(lightMap, vary_fragcoord.xy+vec2(sc,sc)/screen_res);
        col += texture(lightMap, vary_fragcoord.xy+vec2(-sc,sc)/screen_res);
        col += texture(lightMap, vary_fragcoord.xy+vec2(sc,-sc)/screen_res);
        col += texture(lightMap, vary_fragcoord.xy+vec2(-sc,-sc)/screen_res);

        diff = mix(diff, col*0.25, a);
    }

    frag_color = mix(diff, dof, a);
}

/**
 * @file rigRimSmoothF.glsl
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Machinima
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * $/LicenseInfo$
 */

/*[EXTRA_CODE_HERE]*/

// Two deterministic full-resolution surface filters, Smoothness > 0 only.
// Filter BOTH shading and geometric normals. Local triangle-edge covariance
// tells us whether geometry steps occur in the shading normals too. Smooth
// vertex normals must not receive the inverse of a geometry step; flat shaded
// normal-mapped faces must. The classification is averaged over the filter
// footprint, with a broad transition and saturated endpoints for stability.
// H stores two packed 12-bit oct normals and the two covariance moments in
// RGBA32F. Integers up to 2^24-1 are exact floats (no NaN/bitcast packing).
// V transfers the original shading perturbation onto the smooth surface.

out vec4 frag_color;
in vec2 vary_fragcoord;
uniform vec2 screen_res;
uniform vec4 rig_rim_smooth_params; // radius metres, focal pixels xy, texture detail
uniform vec4 rig_rim_quality;       // x Smoothness (final-normal Toksvig)
uniform sampler2D rigRimAuxMap;     // ORIGINAL geometric normal/length/subject
uniform sampler2D rigRimSmoothMap;  // H result
float getDepth(vec2 uv);
vec4 getPositionWithDepth(vec2 uv, float d);
vec4 getNorm(vec2 uv);
vec2 rigRimOctEncode(vec3 n);
vec3 rigRimOctDecode(vec2 e);

vec3 rimUnit(vec3 n, vec3 fallback)
{
    return dot(n,n) > 1.e-12 ? normalize(n) : fallback;
}
float rimPack(vec3 n)
{
    vec2 e = floor(rigRimOctEncode(n)*4095.0+.5);
    return e.x + 4096.0*e.y;
}
vec3 rimUnpack(float n)
{
    return rigRimOctDecode(vec2(mod(n,4096.0),floor(n/4096.0))/4095.0);
}
void main()
{
    vec2 uv = vary_fragcoord;
    vec4 aux = textureLod(rigRimAuxMap,uv,0.0);
    float depth = getDepth(uv);
    vec3 shading = getNorm(uv).xyz;
    vec3 center = getPositionWithDepth(uv,depth).xyz;
    vec3 guide = rigRimOctDecode(aux.xy);
#ifdef RIG_RIM_SMOOTH_VERTICAL
    vec2 axis = vec2(0,1);
    vec4 input_c = textureLod(rigRimSmoothMap,uv,0.0);
    vec3 source = rimUnpack(input_c.x), geometry = rimUnpack(input_c.y);
    vec2 moments = input_c.zw;
#else
    vec2 axis = vec2(1,0);
    vec3 source = shading, geometry = guide;
    vec2 moments = vec2(0.0);
#endif
    float radius = max(rig_rim_smooth_params.x,.001);
    float pixels = min(32.0,radius*dot(axis,rig_rim_smooth_params.yz)/max(-center.z,.05));
    vec3 base_sum = source, geo_sum = geometry;
    float weight = 1.0;
    for (int side=-1;side<=1;side+=2)
    {
        vec3 previous = center, previous_n = guide, previous_sh = shading;
        float connected = 1.0;
        for (int i=1;i<=32;++i)
        {
            if (float(i)>=pixels+1.0 || depth>=.999999) break;
            vec2 tc = uv + axis*(float(side*i)/screen_res);
            if (any(lessThan(tc,vec2(0))) || any(greaterThan(tc,vec2(1)))) break;
            float d = getDepth(tc);
            if (d>=.999999) break;
            vec3 p = getPositionWithDepth(tc,d).xyz;
            vec4 a = textureLod(rigRimAuxMap,tc,0.0);
            vec3 g = rigRimOctDecode(a.xy);
            vec3 delta = p-previous;
            // Walk every intervening pixel: cannot hop over a hole, thin
            // foreground or silhouette. Soft attenuation avoids threshold crawl.
            float gap = .006+length(delta.xy);
            float residual = max(abs(dot(previous_n,delta)),abs(dot(g,delta)));
            connected *= 1.0-smoothstep(gap,2.0*gap,residual);
            if (connected<=0.0 || abs(a.w-aux.w)>.5) break;
            float q = length(p-center)/radius;
            float w = max(0.0,1.0-q*q);
            w *= w*connected*smoothstep(.3,.7,dot(g,guide));
#ifdef RIG_RIM_SMOOTH_VERTICAL
            vec4 input_n = textureLod(rigRimSmoothMap,tc,0.0);
            vec3 n = rimUnpack(input_n.x), gn = rimUnpack(input_n.y);
            moments += w*input_n.zw;
            // H already gathered horizontal edge evidence. Add vertical joins.
            vec3 sh = previous_sh;
#else
            vec3 n = getNorm(tc).xyz, gn = g;
            vec3 sh = n;
#endif
            vec3 dg = g-previous_n, ds = sh-previous_sh;
            if (rig_rim_smooth_params.w>0.0 && dot(g,previous_n)<.98)
            {
#ifdef RIG_RIM_SMOOTH_VERTICAL
                sh=getNorm(tc).xyz;
                vec2 previous_tc=tc-axis*(float(side)/screen_res);
                previous_sh=getNorm(previous_tc).xyz;
                ds=sh-previous_sh;
#endif
                moments += w*vec2(dot(ds,dg),dot(dg,dg));
            }
            previous = p; previous_n = g; previous_sh = sh;
            base_sum += w*n; geo_sum += w*gn; weight += w;
        }
    }
    vec3 base = rimUnit(base_sum/weight,source);
    vec3 smooth_geo = rimUnit(geo_sum/weight,geometry);
#ifdef RIG_RIM_SMOOTH_VERTICAL
    vec3 smooth_n=base;
    if (rig_rim_smooth_params.w>0.0)
    {
        // Distinguish a constant geometric plateau from continuous shading detail.
        // The second-smallest of four increments ignores isolated triangle steps;
        // taking the greater axis also preserves one-dimensional normal maps.
        vec4 variations[2];
        for (int k=0;k<2;++k)
        {
            vec2 step_uv = k==0 ? vec2(1.0/screen_res.x,0) : vec2(0,1.0/screen_res.y);
            vec3 n0=getNorm(uv-2.0*step_uv).xyz, n1=getNorm(uv-step_uv).xyz;
            vec3 n3=getNorm(uv+step_uv).xyz, n4=getNorm(uv+2.0*step_uv).xyz;
            variations[k]=vec4(dot(n0-n1,n0-n1),dot(n1-shading,n1-shading),
                dot(n3-shading,n3-shading),dot(n4-n3,n4-n3));
        }
        vec2 lo0=min(variations[0].xz,variations[0].yw), hi0=max(variations[0].xz,variations[0].yw);
        vec2 lo1=min(variations[1].xz,variations[1].yw), hi1=max(variations[1].xz,variations[1].yw);
        float variation=max(min(max(lo0.x,lo0.y),min(hi0.x,hi0.y)),
                            min(max(lo1.x,lo1.y),min(hi1.x,hi1.y)));
        float textured=smoothstep(1.e-9,1.e-7,variation);
        float faceted = smoothstep(.05,.25,moments.x/max(moments.y,1.e-6));
        vec3 surface = rimUnit(mix(base,smooth_geo,faceted*textured),base);
        vec3 reference = rimUnit(mix(base,guide,faceted),base);
        // Shortest-arc detail rotation, applied to the smooth base. At texture=1
        // a normal map keeps its full perturbation, but a flat face contributes no
        // texture perturbation. Interpolated vertex normals retain their shape.
        vec3 detail = rimUnit(mix(reference,shading,textured*clamp(rig_rim_smooth_params.w,0.0,1.0)),reference);
        vec3 v = cross(reference,detail);
        smooth_n = rimUnit(surface+cross(v,surface)+
            cross(v,cross(v,surface))/max(1.0+dot(reference,detail),.001),surface);
    }
    vec3 final_n = rimUnit(mix(shading,smooth_n,clamp(rig_rim_quality.x,0.0,1.0)),shading);
    // Compute Toksvig from the actual final normal, including texture and the
    // Smoothness blend. Depth boundaries do not count as normal-map variance.
    vec3 dx=dFdx(final_n), dy=dFdy(final_n);
    vec3 dpdx=dFdx(center), dpdy=dFdy(center);
    float tol=.001+.15*max(length(dpdx.xy),length(dpdy.xy));
    float vx=abs(dot(guide,dpdx))<=tol ? dot(dx,dx) : 0.0;
    float vy=abs(dot(guide,dpdy))<=tol ? dot(dy,dy) : 0.0;
    frag_color=vec4(rigRimOctEncode(smooth_n),clamp(1.0-.125*(vx+vy),0.0,1.0),aux.w);
#else
    frag_color=vec4(rimPack(base),rimPack(smooth_geo),moments);
#endif
}

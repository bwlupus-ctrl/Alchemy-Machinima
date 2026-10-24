/**
 * @file rigRimSmoothF.glsl
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Machinima
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * $/LicenseInfo$
 */
/*[EXTRA_CODE_HERE]*/
// Separable surface filtering on an adaptive 1/2/4 grid. Full-res continuity
// barriers certify every intervening pixel, including one-pixel holes.
// The immutable geometric guide is separate from the resolved rim normal.
// Maximum support is 1024 full-res pixels, with local ambiguous fallback.
out vec4 frag_color;
in vec2 vary_fragcoord;
uniform vec2 screen_res;
uniform vec4 rig_rim_smooth_params; // radius metres, focal pixels xy, texture detail
uniform vec4 rig_rim_smooth_stage;  // unused, axis 0 H / 1 V, grid size, actor identity valid
uniform vec4 rig_rim_quality;
uniform sampler2D rigRimAuxMap;
uniform sampler2D rigRimSmoothMap;
uniform sampler2D rigRimBarrierMap;
uniform sampler2D rigRimEdgeMap;
uniform sampler2D actorMaskMap;
float getDepth(vec2 uv);
vec4 getPositionWithDepth(vec2 uv, float d);
vec4 getNorm(vec2 uv);
vec4 getNormRaw(vec2 uv);
vec2 rigRimOctEncode(vec3 n);
vec3 rigRimOctDecode(vec2 e);
vec3 rimUnit(vec3 n, vec3 fallback) { return dot(n,n)>1.e-12 ? normalize(n):fallback; }
float rimPack(vec3 n) {
    vec2 e=floor(rigRimOctEncode(n)*4094.0+.5);
    return e.x+4096.0*e.y;
}
vec3 rimUnpack(float n) {
    return rigRimOctDecode(vec2(mod(n,4096.0),floor(n/4096.0))/4094.0);
}
vec2 rimUV(ivec2 p) { return (vec2(clamp(p,ivec2(0),ivec2(screen_res)-1))+.5)/screen_res; }
vec3 rimPosition(vec2 uv) { return getPositionWithDepth(uv,getDepth(uv)).xyz; }
bool rimJoin(vec2 a, vec2 b, vec3 p, vec3 q, vec3 n, vec3 m) {
    if (getDepth(a)>=.999999 || getDepth(b)>=.999999) return false;
    if (abs(textureLod(rigRimAuxMap,a,0).w-textureLod(rigRimAuxMap,b,0).w)>.5) return false;
#ifdef GBUFFER_AVATAR_TAG
    if (GBUFFER_AVATAR_TAG_OF(getNormRaw(a).w)!=GBUFFER_AVATAR_TAG_OF(getNormRaw(b).w)) return false;
#endif
    if (rig_rim_smooth_stage.w>.5 && textureLod(actorMaskMap,a,0).r!=textureLod(actorMaskMap,b,0).r) return false;
    vec3 delta=q-p;
    // Depth precision plus tangent-plane error from curvature, not a fixed
    // centimetre tolerance that connects millimetre-separated surfaces.
    float depth_noise=max(2.e-5,3.e-6*max(abs(p.z),abs(q.z)));
    float plane_a=dot(n,delta), plane_b=dot(m,delta);
    // At a subpixel triangle corner one reconstructed normal can be
    // ambiguous. A certified neighbour plane containing BOTH points is a
    // stronger continuity witness; an offset layer cannot pass this test.
    if ((textureLod(rigRimAuxMap,a,0).z>.999 && abs(plane_a)<2.0*depth_noise) ||
        (textureLod(rigRimAuxMap,b,0).z>.999 && abs(plane_b)<2.0*depth_noise)) return true;
    float spacing=min(abs(p.z),abs(q.z))*length((a-b)*screen_res/rig_rim_smooth_params.yz);
    float facing=min(abs(dot(n,normalize(p))),abs(dot(m,normalize(q))));
    float tolerance=max(2.e-5,3.e-6*max(abs(p.z),abs(q.z)))+
        min(.05*min(abs(p.z),abs(q.z)),1.05*spacing*length(n-m)/max(.001,facing));
    if (max(abs(dot(n,delta)),abs(dot(m,delta)))>tolerance || dot(n,m)<.3) return false;
    return true;
}
void main() {
    vec2 uv=vary_fragcoord;
    int grid_size=int(rig_rim_smooth_stage.z);
#if !defined(RIG_RIM_BARRIER_EDGES) && !defined(RIG_RIM_SMOOTH_RESOLVE)
    // Reduced-grid centres otherwise lie exactly on full-res texel edges.
    // Nearest sampling can then choose either side as tile dimensions change.
    uv=rimUV(ivec2(gl_FragCoord.xy)*grid_size+ivec2(grid_size/2));
#endif
#ifdef RIG_RIM_BARRIER_EDGES
    ivec2 at=ivec2(gl_FragCoord.xy);
    vec3 p=rimPosition(uv), n=rigRimOctDecode(textureLod(rigRimAuxMap,uv,0).xy);
    vec2 right=rimUV(at+ivec2(1,0)), up=rimUV(at+ivec2(0,1));
    bool rx=at.x+1<int(screen_res.x) && rimJoin(uv,right,p,rimPosition(right),n,rigRimOctDecode(textureLod(rigRimAuxMap,right,0).xy));
    bool ry=at.y+1<int(screen_res.y) && rimJoin(uv,up,p,rimPosition(up),n,rigRimOctDecode(textureLod(rigRimAuxMap,up,0).xy));
    frag_color=vec4(rx ? 1.0:0.0,ry ? 1.0:0.0,0,0);
#elif defined(RIG_RIM_BARRIER_INIT)
    ivec2 origin=ivec2(gl_FragCoord.xy)*grid_size;
    ivec2 limit=ivec2(screen_res)-1;
    vec4 distances=vec4(0); bool valid=true;
    for (int y=0;y<grid_size;++y) for (int x=0;x<grid_size;++x) {
        ivec2 at=clamp(origin+ivec2(x,y),ivec2(0),limit);
        vec2 edge=texelFetch(rigRimBarrierMap,at,0).xy;
        if (x<grid_size-1) valid=valid && edge.x>.5;
        if (y<grid_size-1) valid=valid && edge.y>.5;
    }
    // Cell validity controls downsampling/upsampling, independently of the
    // exact representative-to-representative paths used by the filter. A
    // corner in one row must not become a wall through every other row.
    ivec2 center=clamp(origin+ivec2(grid_size/2),ivec2(0),limit);
    const ivec2 dirs[4]=ivec2[4](ivec2(-1,0),ivec2(1,0),ivec2(0,-1),ivec2(0,1));
    for (int k=0;k<4;++k) {
        float connected=1.0; ivec2 previous=center;
        for (int i=1;i<=grid_size;++i) {
            ivec2 at=center+dirs[k]*i;
            if (any(lessThan(at,ivec2(0))) || any(greaterThan(at,limit))) { connected=0.0; break; }
            vec2 edge=texelFetch(rigRimBarrierMap,k==0 || k==2 ? at:previous,0).xy;
            connected=min(connected,k<2 ? edge.x:edge.y); previous=at;
        }
        distances[k]=connected+(valid ? 2.0:0.0);
    }
    frag_color=distances;
#elif defined(RIG_RIM_SMOOTH_DOWNSAMPLE)
    ivec2 origin=ivec2(gl_FragCoord.xy)*grid_size;
    vec3 gs=vec3(0), ss=vec3(0);
    for (int y=0;y<grid_size;++y) for (int x=0;x<grid_size;++x) {
        vec2 tc=rimUV(origin+ivec2(x,y));
        if (rig_rim_smooth_params.w>0.0) gs+=rigRimOctDecode(textureLod(rigRimAuxMap,tc,0).xy);
        ss+=getNorm(tc).xyz;
    }
    vec3 sh=getNorm(uv).xyz, geo=rigRimOctDecode(textureLod(rigRimAuxMap,uv,0).xy);
    bool valid=any(greaterThan(texelFetch(rigRimBarrierMap,ivec2(gl_FragCoord.xy),0),vec4(1.5)));
    vec2 moments=vec2(0);
    if (rig_rim_smooth_params.w>0.0) for (int k=0;k<2;++k) {
        vec2 axis=(k==0 ? vec2(2,0):vec2(0,2))/screen_res;
        vec3 dg=rigRimOctDecode(textureLod(rigRimAuxMap,uv+axis,0).xy)-rigRimOctDecode(textureLod(rigRimAuxMap,uv-axis,0).xy);
        vec3 ds=getNorm(uv+axis).xyz-getNorm(uv-axis).xyz;
        if (dot(dg,dg)>max(1.e-6,pow(1.e-6*abs(rimPosition(uv).z)*max(rig_rim_smooth_params.y,rig_rim_smooth_params.z),2.0))) moments+=vec2(dot(ds,dg),dot(dg,dg));
    }
    frag_color=vec4(rimPack(valid ? rimUnit(ss,sh):sh),rimPack(valid ? rimUnit(gs,geo):geo),moments);
#elif !defined(RIG_RIM_SMOOTH_RESOLVE)
    ivec2 c=ivec2(gl_FragCoord.xy), size=textureSize(rigRimSmoothMap,0);
    vec4 input_c=texelFetch(rigRimSmoothMap,c,0);
    vec3 sh=rimUnpack(input_c.x), geo=rimUnpack(input_c.y);
    bool vertical=rig_rim_smooth_stage.y>.5;
    ivec2 axis=vertical ? ivec2(0,1):ivec2(1,0);
    vec3 center=rimPosition(uv);
    float radius=rig_rim_smooth_params.x;
    float pixels=min(1024.0,radius*(vertical ? rig_rim_smooth_params.z:rig_rim_smooth_params.y)/max(-center.z,.05))/float(grid_size);
    vec3 ns=sh, ng=geo; vec2 moments=input_c.zw; float total=1.0;
    for (int side=-1;side<=1;side+=2) {
        vec3 previous_n=geo, previous_sh=sh;
        ivec2 previous=c;
        for (int i=1;i<=1024;++i) {
            if (float(i)>pixels) break;
            int k=(vertical ? 2:0)+(side>0 ? 1:0);
            ivec2 q=c+axis*(side*i);
            if (any(lessThan(q,ivec2(0))) || any(greaterThanEqual(q,size))) break;
            if (mod(texelFetch(rigRimBarrierMap,previous,0)[k],2.0)<.5 ||
                mod(texelFetch(rigRimBarrierMap,q,0)[k^1],2.0)<.5) break;
            vec2 tc=rimUV(q*grid_size+ivec2(grid_size/2));
            vec3 p=rimPosition(tc);
            float ratio=length(p-center)/radius;
            float weight=max(0.0,1.0-ratio*ratio); weight*=weight;
            vec4 a=texelFetch(rigRimSmoothMap,q,0);
            vec3 n=rimUnpack(a.x), g=rig_rim_smooth_params.w>0.0 ? rimUnpack(a.y):geo;
            ns+=weight*n; ng+=weight*g; moments+=weight*a.zw; total+=weight;
            vec3 dg=g-previous_n, ds=n-previous_sh;
            if (dot(dg,dg)>max(1.e-6,pow(1.e-6*abs(center.z)*max(rig_rim_smooth_params.y,rig_rim_smooth_params.z),2.0))) moments+=weight*vec2(dot(ds,dg),dot(dg,dg));
            previous=q; previous_n=g; previous_sh=n;
        }
    }
    frag_color=vec4(rimPack(rimUnit(ns,sh)),rimPack(rimUnit(ng,geo)),moments/total);
#else
    vec4 aux=textureLod(rigRimAuxMap,uv,0); vec3 shading=getNorm(uv).xyz, guide=rigRimOctDecode(aux.xy);
    ivec2 size=textureSize(rigRimSmoothMap,0);
    vec2 grid=uv*vec2(size)-.5, frac=fract(grid);
    ivec2 cell=ivec2(floor(grid)), own=clamp(ivec2(uv*vec2(size)),ivec2(0),size-1);
    vec4 own_barrier=texelFetch(rigRimBarrierMap,own,0);
    vec4 distance=mod(own_barrier,2.0);
    vec3 base=vec3(0), geometry=vec3(0); vec2 moments=vec2(0); float total=0.0;
    for (int y=0;y<2;++y) for (int x=0;x<2;++x) {
        if (all(lessThan(own_barrier,vec4(1.5)))) continue;
        ivec2 q=clamp(cell+ivec2(x,y),ivec2(0),size-1), delta=q-own;
        if ((delta.x<0 && distance.x<1.0)||(delta.x>0 && distance.y<1.0)||(delta.y<0 && distance.z<1.0)||(delta.y>0 && distance.w<1.0)) continue;
        vec4 other=texelFetch(rigRimBarrierMap,q,0);
        bool valid=any(greaterThan(other,vec4(1.5)));
        other=mod(other,2.0);
        if (!valid || (delta.x<0 && other.y<.5)||(delta.x>0 && other.x<.5)||
            (delta.y<0 && other.w<.5)||(delta.y>0 && other.z<.5)) continue;
        vec4 a=texelFetch(rigRimSmoothMap,q,0);
        float w=(x==0 ? 1.0-frac.x:frac.x)*(y==0 ? 1.0-frac.y:frac.y);
        base+=w*rimUnpack(a.x); geometry+=w*rimUnpack(a.y); moments+=w*a.zw; total+=w;
    }
    if (total<1.e-5) {
        // Mixed cells borrow filtered support only along a certified full-res
        // pixel path. Thin surfaces with no safe coarse support stay local.
        const ivec2 directions[4]=ivec2[4](ivec2(-1,0),ivec2(1,0),ivec2(0,-1),ivec2(0,1));
        ivec2 origin=ivec2(uv*screen_res);
        for (int k=0;k<4;++k) {
            ivec2 previous=origin;
            for (int i=1;i<=8;++i) {
                ivec2 at=origin+directions[k]*i;
                if (any(lessThan(at,ivec2(0))) || any(greaterThanEqual(at,ivec2(screen_res)))) break;
                ivec2 edge_at=k==0 || k==2 ? at:previous;
                vec2 edge=texelFetch(rigRimEdgeMap,edge_at,0).xy;
                if ((k<2 ? edge.x:edge.y)<.5) break;
                ivec2 q=clamp(at/grid_size,ivec2(0),size-1);
                if (any(notEqual(q,own)) && any(greaterThan(texelFetch(rigRimBarrierMap,q,0),vec4(1.5)))) {
                    vec4 a=texelFetch(rigRimSmoothMap,q,0);
                    float w=1.0/float(i);
                    base+=w*rimUnpack(a.x); geometry+=w*rimUnpack(a.y); moments+=w*a.zw; total+=w;
                    break;
                }
                previous=at;
            }
        }
    }
    base=rimUnit(base,shading); geometry=rimUnit(geometry,guide);
    // Thin/ambiguous cells cannot borrow any coarse shading contribution.
    if (total<1.e-5) { base=shading; geometry=guide; }
    float faceted=smoothstep(.1,.6,moments.x/max(moments.y,1.e-6)); vec3 smooth_n=base;
    if (rig_rim_smooth_params.w>0.0) {
        if (faceted>.8) {
            // At a subpixel corner the centre's small stencil may admit more
            // than one plane. Reuse certified neighbouring planes that also
            // contain the centre; shading alignment breaks only precision ties.
            vec3 center_p=rimPosition(uv);
            float noise=max(2.e-6,4.e-6*center_p.z*center_p.z);
            float best=dot(guide,shading);
            const vec2 offsets[8]=vec2[8](vec2(-1,0),vec2(1,0),vec2(0,-1),vec2(0,1),
                vec2(-2,0),vec2(2,0),vec2(0,-2),vec2(0,2));
            for (int k=0;k<8;++k) {
                vec2 tc=uv+offsets[k]/screen_res;
                vec4 a=textureLod(rigRimAuxMap,tc,0);
                vec3 candidate=rigRimOctDecode(a.xy);
                float score=dot(candidate,shading);
                if (a.z>.99 && score>best && abs(dot(candidate,center_p-rimPosition(tc)))<noise) {
                    guide=candidate; best=score;
                }
            }
        }
        // Transfer only the shading perturbation relative to its surface.
        // No derivative classifier can discard detail at a normal-map maximum.
        vec3 surface=rimUnit(mix(base,geometry,faceted),base);
        vec3 reference=rimUnit(mix(base,guide,faceted),base);
        vec3 detail=rimUnit(mix(reference,shading,rig_rim_smooth_params.w),reference);
        vec4 rotation=normalize(vec4(cross(reference,detail),1.0+dot(reference,detail)));
        vec2 pixel=1.0/screen_res;
        float edge=0.0;
        for (int k=0;k<4;++k) {
            vec2 axis=k<2 ? vec2(k==0 ? -1:1,0):vec2(0,k==2 ? -1:1);
            vec3 delta=rigRimOctDecode(textureLod(rigRimAuxMap,uv+axis*pixel,0).xy)-guide;
            edge=max(edge,dot(delta,delta));
        }
        if (faceted>.8 && (aux.z<.999 || edge>.001)) {
            vec4 sum=vec4(0); float weight=0.0, squared=0.0;
            const vec2 offsets[8]=vec2[8](vec2(-1,0),vec2(1,0),vec2(0,-1),vec2(0,1),
                vec2(-2,0),vec2(2,0),vec2(0,-2),vec2(0,2));
            for (int k=0;k<8;++k) {
                vec2 tc=uv+offsets[k]/screen_res;
                vec4 guide_sample=textureLod(rigRimAuxMap,tc,0);
                vec3 g=rigRimOctDecode(guide_sample.xy), sh=getNorm(tc).xyz;
                vec3 dn=rimUnit(mix(g,sh,rig_rim_smooth_params.w),g);
                vec4 q=normalize(vec4(cross(g,dn),1.0+dot(g,dn)));
                float w=guide_sample.z/(k<4 ? 1.0:2.0);
                sum+=w*q; squared+=w*dot(q.xyz,q.xyz); weight+=w;
            }
            if (weight>1.e-5) {
                vec4 average=normalize(sum);
                float deviation=sqrt(max(0.0,squared/weight-dot((sum/weight).xyz,(sum/weight).xyz)));
                float limit=max(.005,2.0*deviation);
                float outlier=smoothstep(limit,2.0*limit,length(rotation.xyz-average.xyz));
                float repair=max(1.0-aux.z,outlier);
                rotation=normalize(mix(rotation,average,repair));
            }
        }
        vec3 v=rotation.xyz;
        smooth_n=rimUnit(surface+2.0*cross(v,cross(v,surface)+rotation.w*surface),surface);
    }
    vec3 final_n=rimUnit(mix(shading,smooth_n,clamp(rig_rim_quality.x,0.0,1.0)),shading);
    vec3 dx=dFdx(final_n), dy=dFdy(final_n), p=rimPosition(uv), dpdx=dFdx(p), dpdy=dFdy(p);
    float tol=max(2.e-5,3.e-6*abs(p.z));
    float vx=abs(dot(guide,dpdx))<=tol+.75*length(dpdx)*length(dFdx(guide)) ? dot(dx,dx):0.0;
    float vy=abs(dot(guide,dpdy))<=tol+.75*length(dpdy)*length(dFdy(guide)) ? dot(dy,dy):0.0;
    float normal_length=clamp(1.0-.125*(vx+vy),0.0,1.0);
    frag_color=vec4(rigRimOctEncode(smooth_n),normal_length,aux.w);
#endif
}

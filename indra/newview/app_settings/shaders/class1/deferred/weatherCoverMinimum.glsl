/**
 * @file weatherCoverMinimum.glsl
 * @brief Conservative cover hierarchy; empty intervals skip without losing holes.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */
/*[EXTRA_CODE_HERE]*/
layout(local_size_x=8,local_size_y=8) in;
layout(rg32f,binding=0) writeonly uniform image2D cover_output;
layout(rg32f,binding=1) writeonly uniform image2D cover_output_1;
layout(rg32f,binding=2) writeonly uniform image2D cover_output_2;
layout(rg32f,binding=3) writeonly uniform image2D cover_output_3;
uniform sampler2D cover_input;
uniform int cover_level;
uniform int cover_depth_input;
uniform vec4 cover_rectangle;
uniform int cover_levels;
shared vec2 cover_shared[64];
vec2 coverMerge(vec2 a,vec2 b,vec2 c,vec2 d)
{
    return vec2(min(min(a.x,b.x),min(c.x,d.x)),max(max(a.y,b.y),max(c.y,d.y)));
}
void main()
{
    ivec2 local=ivec2(gl_LocalInvocationID.xy);
    ivec2 base=ivec2(cover_rectangle.xy)+ivec2(gl_WorkGroupID.xy)*8;
    ivec2 destination=base+local, source=destination*2;
    ivec2 size=textureSize(cover_input,cover_level)/2;
    vec2 interval=vec2(1,0); // Identity for lanes outside a small final level.
    if(all(lessThan(destination,size)))
    {
        vec2 a=texelFetch(cover_input,source,cover_level).rg;
        vec2 b=texelFetch(cover_input,source+ivec2(1,0),cover_level).rg;
        vec2 c=texelFetch(cover_input,source+ivec2(0,1),cover_level).rg;
        vec2 d=texelFetch(cover_input,source+ivec2(1,1),cover_level).rg;
        if(cover_depth_input!=0){a=a.rr;b=b.rr;c=c.rr;d=d.rr;}
        interval=coverMerge(a,b,c,d);
        imageStore(cover_output,destination,vec4(interval,0,0));
    }
    cover_shared[local.y*8+local.x]=interval;
    barrier();
    for(int level=1;level<cover_levels;++level)
    {
        int width=8>>level;
        bool lane_valid=all(lessThan(local,ivec2(width)));
        if(lane_valid)
        {
            int index=local.y*16+local.x*2;
            interval=coverMerge(cover_shared[index],cover_shared[index+1],
                cover_shared[index+8],cover_shared[index+9]);
        }
        // Every lane has read the old level before any lane overwrites it.
        barrier();
        if(lane_valid)
        {
            cover_shared[local.y*8+local.x]=interval;
            destination=(base>>level)+local;
            if(all(lessThan(destination,max(size>>level,ivec2(1)))))
            {
                if(level==1)imageStore(cover_output_1,destination,vec4(interval,0,0));
                if(level==2)imageStore(cover_output_2,destination,vec4(interval,0,0));
                if(level==3)imageStore(cover_output_3,destination,vec4(interval,0,0));
            }
        }
        barrier();
    }
}

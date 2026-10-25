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
uniform sampler2D cover_input;
uniform int cover_level;
uniform int cover_depth_input;
uniform vec4 cover_rectangle;
void main()
{
    ivec2 local=ivec2(gl_GlobalInvocationID.xy);
    if(any(greaterThanEqual(local,ivec2(cover_rectangle.zw))))return;
    ivec2 destination=local+ivec2(cover_rectangle.xy), source=destination*2;
    vec2 a=texelFetch(cover_input,source,cover_level).rg;
    vec2 b=texelFetch(cover_input,source+ivec2(1,0),cover_level).rg;
    vec2 c=texelFetch(cover_input,source+ivec2(0,1),cover_level).rg;
    vec2 d=texelFetch(cover_input,source+ivec2(1,1),cover_level).rg;
    if(cover_depth_input!=0){a=a.rr;b=b.rr;c=c.rr;d=d.rr;}
    imageStore(cover_output,destination,vec4(min(min(a.x,b.x),min(c.x,d.x)),max(max(a.y,b.y),max(c.y,d.y)),0,0));
}

/**
 * @file weatherPrecipCompositeF.glsl
 * @brief Depth-aware visibility reconstruction; one beauty extinction per view.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */
/*[EXTRA_CODE_HERE]*/
in vec2 vary_fragcoord;
uniform sampler2D depthMap, weather_veil_map;
uniform mat4 snow_inverse_projection;
uniform vec2 snow_veil_field_size;
uniform vec3 snow_veil_light;
out vec4 frag_color;
float veilEyeDepth(vec2 uv,float depth)
{
    vec4 p=snow_inverse_projection*vec4(uv*2-1,depth*2-1,1);
    return abs(p.z/p.w);
}
void main()
{
    vec2 position=vary_fragcoord*snow_veil_field_size-0.5;
    ivec2 base=ivec2(floor(position));vec2 fraction=fract(position);
    float depth=texture(depthMap,vary_fragcoord).r;
    float receiver=veilEyeDepth(vary_fragcoord,min(depth,0.999999));
    float value=0,weight=0;
    for(int y=0;y<2;++y)for(int x=0;x<2;++x)
    {
        ivec2 cell=clamp(base+ivec2(x,y),ivec2(0),ivec2(snow_veil_field_size)-1);
        vec2 uv=(vec2(cell)+0.5)/snow_veil_field_size;
        float sample_depth=texture(depthMap,uv).r;
        float w=(x==0?1-fraction.x:fraction.x)*(y==0?1-fraction.y:fraction.y);
        if((sample_depth>=0.999999)!=(depth>=0.999999))w=0;
        else if(depth<0.999999)
            w*=1-smoothstep(0.05,0.15,abs(veilEyeDepth(uv,sample_depth)-receiver)/max(receiver,1.0));
        value+=texelFetch(weather_veil_map,cell,0).r*w;weight+=w;
    }
    float alpha=weight>1e-6?clamp(value/weight,0,1):0;
    frag_color=vec4(max(snow_veil_light,vec3(0))*alpha,alpha);
}

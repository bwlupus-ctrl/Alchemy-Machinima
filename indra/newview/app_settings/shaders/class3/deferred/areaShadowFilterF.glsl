/** Spatial depth/normal bilateral blur and full-resolution upsample. */
/*[EXTRA_CODE_HERE]*/
out vec4 frag_color;
in vec2 vary_fragcoord;
uniform sampler2D area_source;
uniform vec2 area_axis; // nonzero: separable blur, zero: bilateral upsample
uniform float area_radius;
float getLinearDepth(vec2 uv);
vec3 areaNormal(vec2 uv);
float areaWeight(vec2 uv, float z, vec3 n)
{
    float dz = abs(getLinearDepth(uv)-z);
    return exp(-dz/max(0.03,z*0.005))*pow(max(dot(n,areaNormal(uv)),0.0),32.0);
}
void main()
{
    vec2 uv = vary_fragcoord;
    float z = getLinearDepth(uv);
    vec3 n = areaNormal(uv);
    vec2 res = vec2(textureSize(area_source,0));
    vec4 sum = vec4(0);
    float weight = 0.0;
    if (any(notEqual(area_axis,vec2(0))))
    {
        for (int tap = -8; tap <= 8; ++tap)
        {
            if (abs(float(tap)) > area_radius) continue;
            vec2 q = clamp(uv+area_axis*float(tap)/res,0.5/res,1.0-0.5/res);
            float w = areaWeight(q,z,n)*exp(-float(tap*tap)/max(area_radius*area_radius,1.0));
            sum += textureLod(area_source,q,0.0)*w;
            weight += w;
        }
    }
    else
    {
        vec2 p = uv*res-0.5;
        vec2 base = floor(p);
        vec2 f = fract(p);
        for (int y=0; y<2; ++y) for (int x=0; x<2; ++x)
        {
            vec2 q = clamp((base+vec2(x,y)+0.5)/res,0.5/res,1.0-0.5/res);
            float w = (x==0 ? 1.0-f.x:f.x)*(y==0 ? 1.0-f.y:f.y)*areaWeight(q,z,n);
            sum += textureLod(area_source,q,0.0)*w;
            weight += w;
        }
    }
    // If every neighbour is across an edge, preserve nearest visibility.
    frag_color = weight > 1.e-8 ? sum/weight : textureLod(area_source,uv,0.0);
}

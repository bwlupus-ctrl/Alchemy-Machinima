/** Current-frame subset of the public SSR projection/depth helpers. */
/*[EXTRA_CODE_HERE]*/
uniform mat4 projection_matrix;
uniform sampler2D depthMap;
uniform sampler2D normalMap;
vec4 getPositionWithDepth(vec2 uv, float depth);
vec4 decodeNormal(vec4 raw);
vec2 generateProjectedPosition(vec3 pos)
{
    vec4 clip = projection_matrix * vec4(pos,1.0);
    return clip.xy / clip.w * 0.5 + 0.5;
}
float getLinearDepth(vec2 uv)
{
    return -getPositionWithDepth(uv,textureLod(depthMap,uv,0.0).r).z;
}
vec3 areaNormal(vec2 uv)
{
    return decodeNormal(textureLod(normalMap,uv,0.0)).xyz;
}

/** Current-frame subset of the public SSR projection/depth helpers. */
/*[EXTRA_CODE_HERE]*/
uniform mat4 projection_matrix;
uniform sampler2D depthMap;
uniform sampler2D normalMap;
uniform vec2 area_trace_res;
vec4 getPositionWithDepth(vec2 uv, float depth);
vec4 decodeNormal(vec4 raw);
// Point depth/normal reads describe the FULL-resolution texel centre, even
// when the trace grid lands between texels (or has an odd-sized target).
vec2 areaTexelUV(vec2 uv)
{
    vec2 res = vec2(textureSize(depthMap,0));
    // Half-res centres can fall exactly on full-res texel boundaries. Resolve
    // that tie consistently despite normalized-coordinate roundoff (<.001 px).
    return (clamp(floor(uv*res+1.e-3),vec2(0),res-1.0)+0.5)/res;
}
vec2 generateProjectedPosition(vec3 pos)
{
    vec4 clip = projection_matrix * vec4(pos,1.0);
    return clip.xy / clip.w * 0.5 + 0.5;
}
float getLinearDepth(vec2 uv)
{
    uv = areaTexelUV(uv);
    return -getPositionWithDepth(uv,textureLod(depthMap,uv,0.0).r).z;
}
vec3 areaNormal(vec2 uv)
{
    uv = areaTexelUV(uv);
    return decodeNormal(textureLod(normalMap,uv,0.0)).xyz;
}
// Reconstruct adjacent pixels at RECEIVER depth, not the neighbour's depth.
// This scales with view distance, projection and full/half trace resolution
// without letting a silhouette discontinuity produce a huge normal offset.
float areaReceiverBias(vec2 uv, vec3 pos)
{
    vec2 pixel = 1.0/max(area_trace_res,vec2(1));
    float depth = textureLod(depthMap,uv,0.0).r;
    float footprint = max(length(getPositionWithDepth(uv+vec2(pixel.x,0),depth).xyz-pos),
                          length(getPositionWithDepth(uv+vec2(0,pixel.y),depth).xyz-pos));
    return max(0.02,footprint*1.5);
}
vec2 areaTrace(vec3 origin, vec3 target, vec3 light, float end_gap,
                float light_bias, float max_distance, float thickness, int steps, float offset,
                float contact_distance)
{
    vec3 delta = target-origin;
    float distance = length(delta);
    float limit = min(max(0.0,distance-end_gap),max_distance);
    if (limit <= 0.001) return vec2(1);
    vec3 direction = delta/max(distance,0.001);
    float dt = limit/float(steps);
    for (int step=0; step<steps; ++step)
    {
        vec3 p = origin+direction*((float(step)+offset)*dt);
        if (p.z >= -0.001) break;
        vec2 uv = generateProjectedPosition(p);
        if (any(lessThan(uv,vec2(0))) || any(greaterThanEqual(uv,vec2(1)))) break;
        uv = areaTexelUV(uv);
        float depth = textureLod(depthMap,uv,0.0).r;
        if (depth >= 1.0) continue;
        vec3 occluder = getPositionWithDepth(uv,depth).xyz;
        // A source-local exclusion is invariant under camera rotation. A
        // camera-depth slab would also erase distant, unrelated blockers.
        if (length(occluder-light) <= light_bias) continue;
        // A front-depth sample is a surface, not an infinitely deep column.
        // Only accept a crossing in this march interval; otherwise a ray far
        // behind a silhouette casts a camera-dependent, displaced shadow.
        float penetration = occluder.z-p.z;
        if (penetration > 0.0 && penetration <= thickness+dt)
        {
            // Refine to the sampled surface plane, then validate at the HIT's
            // projection. A coarse step behind a silhouette is only a candidate.
            vec3 normal = areaNormal(uv);
            float slope = dot(direction,normal);
            if (abs(slope) < 1.e-4) continue;
            float hit_t = dot(occluder-origin,normal)/slope;
            if (hit_t < 0.0 || hit_t > limit) continue;
            vec3 hit = origin+direction*hit_t;
            vec2 hit_uv = generateProjectedPosition(hit);
            if (any(lessThan(hit_uv,vec2(0))) || any(greaterThanEqual(hit_uv,vec2(1)))) continue;
            hit_uv = areaTexelUV(hit_uv);
            float hit_depth = textureLod(depthMap,hit_uv,0.0).r;
            if (hit_depth >= 1.0) continue;
            vec3 surface = getPositionWithDepth(hit_uv,hit_depth).xyz;
            if (length(hit-light) > light_bias &&
                abs(dot(hit-surface,areaNormal(hit_uv))) <= thickness)
            {
                // One unchanged full-length march: classify its validated hit.
                // Contact shares HEAD's step density, refinement and early-out.
                return vec2(0.0,contact_distance > 0.001 && hit_t <= contact_distance ? 0.0:1.0);
            }
        }
    }
    return vec2(1);
}
float areaTrace(vec3 origin, vec3 target, vec3 light, float end_gap,
                float light_bias, float max_distance, float thickness, int steps, float offset)
{
    return areaTrace(origin,target,light,end_gap,light_bias,max_distance,thickness,steps,offset,0.0).x;
}

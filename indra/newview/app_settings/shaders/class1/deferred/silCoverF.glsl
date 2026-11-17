// [AvatarSilhouette] Post-deferred fog/rim coverage replay.
/*[EXTRA_CODE_HERE]*/
uniform vec2 sil_cover;
uniform int sil_blended;
uniform float sil_clone_progress;
// Clone transition native mask (PLAN 3.4): chroma key / silhouette cover must see the same holes as beauty.
uniform vec4 actorFxTransition;   // x style (0 off), y p, z side, w direction
uniform vec4 actorFxTransition2;  // x body height
// seed: two exact 16-bit halves in actorFxTransition2.zw (float uniforms only: no uint uniform path)
uint ctSeed() { return uint(actorFxTransition2.z + 0.5) | (uint(actorFxTransition2.w + 0.5) << 16); }
in vec3 vary_transition_pos;
// BEGIN CLONE TRANSITION FIELD (exact port of alclonetransition.h fieldValue/keeps; gate: clone_transition_render_check.py)
float ctHash3(ivec3 c, uint seed)
{
    uint h = seed * 747796405u + 2891336453u;
    h ^= uint(c.x) * 0x8da6b343u; h ^= uint(c.y) * 0xd8163841u; h ^= uint(c.z) * 0xcb1ab31fu;
    h = (h ^ (h >> 16)) * 0x7feb352du;
    h = (h ^ (h >> 15)) * 0x846ca68bu;
    h ^= h >> 16;
    return float(h >> 8) * (1.0 / 16777216.0);
}
float ctLerp(float a, float b, float t) { return a + (b - a) * t; }
float ctValueNoise(vec3 pos, float cell, uint seed)
{
    vec3 q = pos / cell;
    vec3 f = floor(q);
    ivec3 i = ivec3(f);
    vec3 t = q - f;
    vec3 u = t * t * (3.0 - 2.0 * t);
    float c000 = ctHash3(i + ivec3(0, 0, 0), seed), c100 = ctHash3(i + ivec3(1, 0, 0), seed);
    float c010 = ctHash3(i + ivec3(0, 1, 0), seed), c110 = ctHash3(i + ivec3(1, 1, 0), seed);
    float c001 = ctHash3(i + ivec3(0, 0, 1), seed), c101 = ctHash3(i + ivec3(1, 0, 1), seed);
    float c011 = ctHash3(i + ivec3(0, 1, 1), seed), c111 = ctHash3(i + ivec3(1, 1, 1), seed);
    float x00 = ctLerp(c000, c100, u.x), x10 = ctLerp(c010, c110, u.x);
    float x01 = ctLerp(c001, c101, u.x), x11 = ctLerp(c011, c111, u.x);
    return ctLerp(ctLerp(x00, x10, u.y), ctLerp(x01, x11, u.y), u.z);
}
// style: 1 Dissolve, 2 Wipe, 3 Crossfade. direction: 0 up, 1 down.
float ctField(int style, vec3 pos, uint seed, float body_height, int direction)
{
    if (style == 1)
    {
        float n = 0.65 * ctValueNoise(pos, 0.11, seed) + 0.35 * ctValueNoise(pos, 0.037, seed ^ 0x5bd1e995u);
        return clamp((n - 0.15) / 0.70, 0.0, 0.99999);
    }
    if (style == 2)
    {
        float h = max(body_height, 0.1);
        float z = clamp(pos.z / h, 0.0, 0.99999);
        return direction == 1 ? 0.99999 - z : z;
    }
    if (style == 3)
    {
        ivec3 cell = ivec3(floor(pos / 0.015));
        return min(ctHash3(cell, seed), 0.99999);
    }
    return 0.0;
}
// side: 0 outgoing keeps f >= p, 1 incoming keeps f < p.
bool ctKeeps(int side, float field, float p)
{
    return side == 0 ? field >= p : field < p;
}
// END CLONE TRANSITION FIELD
float silCloneCoverage(float progress)
{
    if (progress <= 0.0) return 1.0;
    if (progress >= 1.0) return -1.0;
    ivec2 p = ivec2(mod(floor(gl_FragCoord.xy), 4.0));
    const float bayer[16] = float[16](0.,8.,2.,10.,12.,4.,14.,6.,3.,11.,1.,9.,15.,7.,13.,5.);
    return (bayer[p.y*4+p.x]+0.5)/16.0-progress;
}

#ifdef SIL_COVER_SYSTEM
uniform sampler2D diffuseMap;
uniform float minimum_alpha;
in vec3 vary_position;
in vec2 vary_texcoord0;
#else
uniform sampler2D mask_textures[MASK_CHANNELS];
uniform float mask_cutoff[8];
uniform int mask_material_slots;
uniform int mask_vertex_alpha;
in vec3 vary_position;
in vec2 vary_texcoord0;
in vec4 vertex_color;
flat in int mask_slot;
#endif

out vec4 frag_color;
void mirrorClip(vec3 pos);

#ifndef SIL_COVER_SYSTEM
float authoredAlpha()
{
    switch (mask_slot)
    {
#if MASK_CHANNELS > 0
    case 0: return texture(mask_textures[0], vary_texcoord0).a;
#endif
#if MASK_CHANNELS > 1
    case 1: return texture(mask_textures[1], vary_texcoord0).a;
#endif
#if MASK_CHANNELS > 2
    case 2: return texture(mask_textures[2], vary_texcoord0).a;
#endif
#if MASK_CHANNELS > 3
    case 3: return texture(mask_textures[3], vary_texcoord0).a;
#endif
#if MASK_CHANNELS > 4
    case 4: return texture(mask_textures[4], vary_texcoord0).a;
#endif
#if MASK_CHANNELS > 5
    case 5: return texture(mask_textures[5], vary_texcoord0).a;
#endif
#if MASK_CHANNELS > 6
    case 6: return texture(mask_textures[6], vary_texcoord0).a;
#endif
#if MASK_CHANNELS > 7
    case 7: return texture(mask_textures[7], vary_texcoord0).a;
#endif
#if MASK_CHANNELS > 8
    case 8: return texture(mask_textures[8], vary_texcoord0).a;
#endif
#if MASK_CHANNELS > 9
    case 9: return texture(mask_textures[9], vary_texcoord0).a;
#endif
#if MASK_CHANNELS > 10
    case 10: return texture(mask_textures[10], vary_texcoord0).a;
#endif
#if MASK_CHANNELS > 11
    case 11: return texture(mask_textures[11], vary_texcoord0).a;
#endif
#if MASK_CHANNELS > 12
    case 12: return texture(mask_textures[12], vary_texcoord0).a;
#endif
#if MASK_CHANNELS > 13
    case 13: return texture(mask_textures[13], vary_texcoord0).a;
#endif
#if MASK_CHANNELS > 14
    case 14: return texture(mask_textures[14], vary_texcoord0).a;
#endif
#if MASK_CHANNELS > 15
    case 15: return texture(mask_textures[15], vary_texcoord0).a;
#endif
#if MASK_CHANNELS > 16
    case 16: return texture(mask_textures[16], vary_texcoord0).a;
#endif
#if MASK_CHANNELS > 17
    case 17: return texture(mask_textures[17], vary_texcoord0).a;
#endif
#if MASK_CHANNELS > 18
    case 18: return texture(mask_textures[18], vary_texcoord0).a;
#endif
#if MASK_CHANNELS > 19
    case 19: return texture(mask_textures[19], vary_texcoord0).a;
#endif
#if MASK_CHANNELS > 20
    case 20: return texture(mask_textures[20], vary_texcoord0).a;
#endif
#if MASK_CHANNELS > 21
    case 21: return texture(mask_textures[21], vary_texcoord0).a;
#endif
#if MASK_CHANNELS > 22
    case 22: return texture(mask_textures[22], vary_texcoord0).a;
#endif
#if MASK_CHANNELS > 23
    case 23: return texture(mask_textures[23], vary_texcoord0).a;
#endif
#if MASK_CHANNELS > 24
    case 24: return texture(mask_textures[24], vary_texcoord0).a;
#endif
#if MASK_CHANNELS > 25
    case 25: return texture(mask_textures[25], vary_texcoord0).a;
#endif
#if MASK_CHANNELS > 26
    case 26: return texture(mask_textures[26], vary_texcoord0).a;
#endif
#if MASK_CHANNELS > 27
    case 27: return texture(mask_textures[27], vary_texcoord0).a;
#endif
#if MASK_CHANNELS > 28
    case 28: return texture(mask_textures[28], vary_texcoord0).a;
#endif
#if MASK_CHANNELS > 29
    case 29: return texture(mask_textures[29], vary_texcoord0).a;
#endif
#if MASK_CHANNELS > 30
    case 30: return texture(mask_textures[30], vary_texcoord0).a;
#endif
#if MASK_CHANNELS > 31
    case 31: return texture(mask_textures[31], vary_texcoord0).a;
#endif
    default: return 0.0;
    }
}
#endif

void main()
{
    mirrorClip(vary_position);
    if (sil_clone_progress > 0.0 && silCloneCoverage(sil_clone_progress) < 0.0) discard;
    if (actorFxTransition.x > 0.5)
    {
        float tf = ctField(int(actorFxTransition.x + 0.5), vary_transition_pos, ctSeed(),
                           actorFxTransition2.x, int(actorFxTransition.w + 0.5));
        if (!ctKeeps(int(actorFxTransition.z + 0.5), tf, actorFxTransition.y)) discard;
    }
#ifdef SIL_COVER_SYSTEM
    float alpha = texture(diffuseMap, vary_texcoord0).a;
    if (alpha < minimum_alpha) discard;
#else
    if (mask_slot < 0 || mask_slot >= MASK_CHANNELS) discard;
    int mi = mask_material_slots != 0 ? mask_slot : 0;
    if (mi >= 8) discard;
    float alpha = authoredAlpha();
    if (mask_vertex_alpha != 0) alpha *= vertex_color.a;
    if (alpha < mask_cutoff[mi]) discard;
#endif
    float cov = sil_blended != 0 ? alpha : 1.0;
    frag_color = vec4(sil_cover * cov, 0.0, cov);
}

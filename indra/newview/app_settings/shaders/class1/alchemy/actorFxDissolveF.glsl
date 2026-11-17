/**
 * Shared Actor FX Dissolve coverage.
 *
 * Beauty and shadow programs link this same module so their silhouettes cannot
 * drift.  The field is evaluated from the mesh's raw object/rest-space vertex
 * position before skinning.  It therefore sticks to animated surfaces and has
 * no camera-, shadow-map-, or texture-transform-dependent input. The shared,
 * snapshot-frozen Actor FX clock translates the field identically in beauty
 * and shadow passes, while actor phase decorrelates different cast members.
 */

uniform int actorFxEnabled;
uniform int actorFxLook;
uniform float actorFxTime;
uniform vec4 actorFxParams0;
uniform vec4 actorFxParams1;
uniform float actorFxDissolveProgress;

// Clone transition native mask (PLAN 3.2). Independent of actorFxEnabled and every Actor FX
// look: style 0 (the per-draw reset value) makes ctTransitionDiscard a uniform-only no-op.
//   actorFxTransition  x = style (0 off, 1 Dissolve, 2 Wipe, 3 Crossfade), y = p, z = side, w = direction
//   actorFxTransition2 x = body height (m), y = edge width (field units), zw = seed lo/hi 16 bits
uniform vec4 actorFxTransition;
uniform vec4 actorFxTransition2;
// seed: two exact 16-bit halves in actorFxTransition2.zw (float uniforms only: no uint uniform path)
uint ctSeed() { return uint(actorFxTransition2.z + 0.5) | (uint(actorFxTransition2.w + 0.5) << 16); }
in vec3 vary_transition_pos;   // body-space transition coordinate (vertex shaders, final skinned position)

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


float cloneFadeCoverage(float progress)
{
    if (progress <= 0.0) return 1.0;
    if (progress >= 1.0) return -1.0;
    ivec2 p = ivec2(mod(floor(gl_FragCoord.xy), 4.0));
    const float bayer[16] = float[16](0.,8.,2.,10.,12.,4.,14.,6.,3.,11.,1.,9.,15.,7.,13.,5.);
    return (bayer[p.y*4+p.x]+0.5)/16.0-progress;
}

float actor_fx_dissolve_hash(vec2 p)
{
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

float actor_fx_dissolve_noise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(actor_fx_dissolve_hash(i),
                   actor_fx_dissolve_hash(i + vec2(1.0, 0.0)), f.x),
               mix(actor_fx_dissolve_hash(i + vec2(0.0, 1.0)),
                   actor_fx_dissolve_hash(i + vec2(1.0, 1.0)), f.x), f.y);
}

float actor_fx_dissolve_fbm(vec2 p)
{
    float v = 0.0;
    v += 0.500 * actor_fx_dissolve_noise(p); p = p * 2.03 + 17.1;
    v += 0.250 * actor_fx_dissolve_noise(p); p = p * 2.01 + 11.7;
    v += 0.125 * actor_fx_dissolve_noise(p);
    return v / 0.875;
}

bool actorFxDissolveEnabled()
{
    // Look 10 is the persisted/UI ActorStyle mapping for Dissolve. Coverage is
    // independent of Layer Alpha; actorFxEnabled remains the shared style gate.
    return actorFxEnabled == 2 || (actorFxEnabled != 0 && actorFxLook == 10);
}

float actorFxDissolveCoverage(vec3 object_position)
{
    if (actorFxEnabled == 2) return cloneFadeCoverage(actorFxDissolveProgress);
    float progress = clamp(actorFxDissolveProgress, 0.0, 1.0);
    // Make the authored endpoints exact instead of depending on the sampled
    // FBM's practical min/max. This keeps Cover useful at progress zero and
    // guarantees a completely dissolved actor at progress one.
    if (progress <= 0.0)
    {
        return 1.0;
    }
    if (progress >= 1.0)
    {
        return -1.0;
    }

    float phase = actorFxParams1.w;
    vec2 actor_offset = vec2(cos(phase), sin(phase)) * 17.0;
    // Fold all three object axes into a stable 2-D noise domain.  Passing the
    // pre-skin position makes this identical in beauty and every shadow view.
    vec2 domain = object_position.xy
                + vec2(object_position.z * 0.73, object_position.z * 1.17);
    // Match Ghost Studio's slowly travelling breakup rather than freezing one
    // object-space threshold forever. Progress still owns the amount removed;
    // time only advects the field, so fixed progress keeps stable statistical
    // coverage while its boundary crawls. actorFxTime is frozen across every
    // tile of a high-resolution snapshot by the uploader.
    vec2 field_motion = vec2(0.0, actorFxTime * 0.22);
    float field = actor_fx_dissolve_fbm(domain * 3.2 + actor_offset
                                        + field_motion);
    float threshold = mix(0.0, 1.0, progress);
    return field - threshold;
}

// Shared entry for beauty (called from actorFxTronPrime in actorFxF.glsl) and every shadow /
// velocity / depth program (called from actorFxDissolveDiscard below). Never reads
// actorFxEnabled: the mask is independent of the Actor FX treatment (replay disables it).
bool actorFxTransitionDiscard()
{
    if (actorFxTransition.x < 0.5)
    {
        return false;
    }
    float f = ctField(int(actorFxTransition.x + 0.5), vary_transition_pos, ctSeed(),
                      actorFxTransition2.x, int(actorFxTransition.w + 0.5));
    return !ctKeeps(int(actorFxTransition.z + 0.5), f, actorFxTransition.y);
}

bool actorFxDissolveDiscard(vec3 object_position)
{
    // Transition first: a pixel is discarded if the transition OR the existing coverage discards it.
    if (actorFxTransitionDiscard())
    {
        return true;
    }
    // Keep every non-Dissolve fragment on the uniform-only fast path.
    if (!actorFxDissolveEnabled())
    {
        return false;
    }
    return actorFxDissolveCoverage(object_position) < 0.0;
}

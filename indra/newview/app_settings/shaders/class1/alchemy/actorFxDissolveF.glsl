/**
 * Shared Actor FX Dissolve coverage.
 *
 * Beauty and shadow programs link this same module so their silhouettes cannot
 * drift.  The field is evaluated from the mesh's raw object/rest-space vertex
 * position before skinning.  It therefore sticks to animated surfaces and has
 * no camera-, shadow-map-, texture-transform-, or time-space input.  The actor
 * phase only decorrelates different cast members without animating coverage.
 */

uniform int actorFxEnabled;
uniform int actorFxLook;
uniform vec4 actorFxParams0;
uniform vec4 actorFxParams1;

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
    // Look 10 is the persisted/UI ActorStyle mapping for Dissolve.  Strength
    // at or below epsilon is a strict identity operation and must not discard.
    return actorFxEnabled != 0 && actorFxLook == 10 && actorFxParams0.x > 0.001;
}

float actorFxDissolveCoverage(vec3 object_position)
{
    float phase = actorFxParams1.w;
    vec2 actor_offset = vec2(cos(phase), sin(phase)) * 17.0;
    // Fold all three object axes into a stable 2-D noise domain.  Passing the
    // pre-skin position makes this identical in beauty and every shadow view.
    vec2 domain = object_position.xy
                + vec2(object_position.z * 0.73, object_position.z * 1.17);
    float field = actor_fx_dissolve_fbm(domain * 3.2 + actor_offset);
    float threshold = mix(0.05, 0.56, clamp(actorFxParams0.x, 0.0, 1.0));
    return field - threshold;
}

bool actorFxDissolveDiscard(vec3 object_position)
{
    // Keep every non-Dissolve fragment on the uniform-only fast path.
    if (!actorFxDissolveEnabled())
    {
        return false;
    }
    return actorFxDissolveCoverage(object_position) < 0.0;
}

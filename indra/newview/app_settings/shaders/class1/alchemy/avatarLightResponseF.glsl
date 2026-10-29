/**
 * [AvatarLightResponse] Per-avatar, light-independent material trim.
 *
 * Attached to every hasActorFx fragment program (llshadermgr.cpp).  The CPU
 * uploads alrEnabled / alrParams per draw (lldrawpool.cpp upload_alr); the GL
 * link default alrEnabled == 0 is exact identity, so a program that never
 * receives an upload behaves exactly as before.
 *
 * Consumers declare only the prototypes they call, inside #ifdef HAS_ACTOR_FX.
 * Own sRGB helpers: not every hasActorFx program links srgbF.glsl.
 * See doc/AVATAR_LIGHT_RESPONSE_DESIGN.md section 6.
 */

uniform int  alrEnabled;   // 0 identity; 1 active; 3 active + debug override
uniform vec4 alrParams;    // x diffuse d, y specular keep s, z albedo gain k, w authored-emission gain g

const vec3 ALR_MAGENTA = vec3(1.0, 0.0, 1.0);

float alr_s2l(float c)
{
    c = max(c, 0.0);
    return c <= 0.04045 ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4);
}

float alr_l2s(float c)
{
    c = max(c, 0.0);
    return c <= 0.0031308 ? c * 12.92 : 1.055 * pow(c, 1.0 / 2.4) - 0.055;
}

vec3 alr_s2l3(vec3 c)
{
    return vec3(alr_s2l(c.r), alr_s2l(c.g), alr_s2l(c.b));
}

vec3 alr_l2s3(vec3 c)
{
    return vec3(alr_l2s(c.r), alr_l2s(c.g), alr_l2s(c.b));
}

vec3 alr_gain(vec3 c, float k)
{
    if (k > 1.0)
    {
        float m = max(max(c.r, c.g), c.b);
        k = max(1.0, min(k, 1.0 / max(m, 1e-4)));
    }
    return c * k;
}

vec3 alrAlbedoLinear(vec3 c)
{
    if (alrEnabled == 0) return c;
    if (alrEnabled == 3) return ALR_MAGENTA;
    return alrParams.z == 1.0 ? c : alr_gain(c, alrParams.z);
}

vec3 alrAlbedoSrgb(vec3 c)
{
    if (alrEnabled == 0 || (alrEnabled == 1 && alrParams.z == 1.0)) return c;   // no sRGB round trip at identity
    return alrEnabled == 3 ? ALR_MAGENTA : alr_l2s3(alr_gain(alr_s2l3(c), alrParams.z));
}

vec3 alrPresentedLinear(vec3 c)                       // unlit / fullbright output (HDR, no ceiling)
{
    if (alrEnabled == 0) return c;
    return alrEnabled == 3 ? ALR_MAGENTA : c * alrParams.z;
}

vec3 alrGraphicCover(vec3 c, vec3 lit_pre, float sigma)   // trims only the fx term of mix(lit_pre, fx, sigma)
{
    if (alrEnabled == 0 || sigma <= 0.0) return c;
    if (alrEnabled == 3) return ALR_MAGENTA;
    return alrParams.z == 1.0 ? c : c + (alrParams.z - 1.0) * (c - (1.0 - sigma) * lit_pre);
}

float alrRoughness(float r)
{
    if (alrEnabled == 0) return r;
    return alrEnabled == 3 ? 1.0 : mix(r, 1.0, clamp(alrParams.x, 0.0, 1.0));
}

void alrLegacySpec(inout vec3 spec_srgb, inout float gloss, inout float env)
{
    if (alrEnabled == 0) return;
    if (alrEnabled == 3)
    {
        spec_srgb = vec3(0.0);
        env = 0.0;
        return;
    }
    float d = clamp(alrParams.x, 0.0, 1.0);
    if (d > 0.0 && gloss >= 0.5 / 255.0)
    {
        gloss = max(gloss * (1.0 - 0.95 * d), 1.0 / 255.0);
    }
    if (alrParams.y != 1.0)
    {
        // Sun/local highlights and gloss reflection are linear in the specular colour. The legacy
        // ENVIRONMENT term is deliberately NOT scaled here: env is also the mixing weight that
        // fades the surface colour (1 - env), so scaling it would change diffuse. Deferred legacy
        // pixels therefore keep env and albedo exactly as authored; an exact deferred env-reflection
        // tame would need a new G-buffer channel (the forward paths use applyLegacyEnvKeep).
        spec_srgb = alr_l2s3(alr_s2l3(spec_srgb) * alrParams.y);
    }
}

float alrSpecKeep()
{
    return alrEnabled == 0 ? 1.0 : (alrEnabled == 3 ? 0.0 : alrParams.y);
}

// Glow scope (documented limits): Glow scales creator-authored emissive colour and the authored
// glow alpha. The legacy emissive MASK (frag_data[0].a / fullbright fraction written by the legacy
// writers) is not scaled, so legacy fullbright surfaces keep their mask. Above 100 % the legacy
// glow alpha (emissiveF / emissiveIndexedF) may clamp in an unorm glow target.
vec3 alrEmissive(vec3 e)
{
    return alrEnabled == 0 ? e : e * alrParams.w;
}

float alrEmissiveGain()
{
    return alrEnabled == 0 ? 1.0 : alrParams.w;
}

float alrPbrCarrier()
{
    return alrEnabled == 0 ? 0.0 : 1.0 - alrSpecKeep();
}

vec3 alrDebugEmission(vec3 e)                          // REPLACES emission in debug mode
{
    return alrEnabled == 3 ? ALR_MAGENTA * 4.0 : e;
}

bool alrDebug()
{
    return alrEnabled == 3;
}

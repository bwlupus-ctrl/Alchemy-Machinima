/**
 * @file visibleDiffuseSeedF.glsl
 *
 * Classifying seed for the visible-diffuse sidecar. Runs ONCE, fullscreen,
 * after the deferred G-buffer is complete and BEFORE any forward geometry, so
 * the forward passes can composite their own diffuse over the top of it.
 *
 * Writes ONLY colour attachment 1 of mRT->screen:
 *      RGB = scene-referred LINEAR diffuse reflectance of the deferred surface
 *      A   = exactness K in [0,1]   (1 = we know this pixel's answer)
 *
 * WHY A CLASSIFYING PASS AND NOT A BLIT: a straight copy of G-buffer
 * attachment 0 would seed exactness INVERTED. Deferred opaque writes
 * frag_data[0].a = 0.0 (pbropaqueF.glsl) while sky writes .a = 1.0
 * (skyF.glsl), so a blit yields K=0 exactly where a real surface exists and
 * K=1 at sky -- precisely backwards. Exactness must be DECIDED here.
 */

/*[EXTRA_CODE_HERE]*/

// Declared explicitly rather than relying on the deferred uniform injection.
// This shader is dispatched outside the usual deferred-lighting program set and
// the injection does not reach it: it failed to compile with
// "error C1503: undefined variable diffuseRect" until these were added.
uniform sampler2D diffuseRect;    // G-buffer 0: base colour (sRGB storage)
uniform sampler2D specularRect;   // G-buffer 1: PBR occlusion/roughness/metallic
uniform sampler2D normalMap;      // G-buffer 2: .w carries GBUFFER_FLAG_*
uniform sampler2D depthMap;       // shared depth

in vec2 vary_fragcoord;

layout(location = 1) out vec4 visible_diffuse;

void main()
{
    vec4  albedo = texture(diffuseRect,  vary_fragcoord);
    vec4  orm    = texture(specularRect, vary_fragcoord);
    float flag   = texture(normalMap,    vary_fragcoord).w;
    float depth  = texture(depthMap,     vary_fragcoord).r;

    // WINDOWED compare, matching the viewer's own GET_GBUFFER_FLAG decoder
    // (llshadermgr.cpp). Never equality, and never "flag > 0": the G-buffer
    // clear is glClearColor(1,0,1,1) (llviewerdisplay.cpp) so an UNWRITTEN
    // pixel carries .w = 1.0, colliding exactly with GBUFFER_FLAG_HAS_HDRI.
    // Only the two real-geometry buckets may be accepted.
    bool has_atmos = abs(flag - 0.34) < 0.1;   // ordinary deferred geometry
    bool has_pbr   = abs(flag - 0.67) < 0.1;   // PBR geometry

    // A deferred SURFACE is one of those two buckets AND in front of the far
    // plane. GBUFFER_FLAG_SKIP_ATMOS (0.0) is sky/clouds/stars/aurora and is
    // deliberately NOT a surface -- it belongs in the "no answer" branch below.
    bool surface = (has_atmos || has_pbr) && depth < 1.0;

    if (!surface)
    {
        // No deferred surface here: sky, background, or never written.
        // K = 0 means "NO ANSWER", which is a DIFFERENT state from a known
        // black answer (K = 1, RGB = 0). Conflating those two is the exact bug
        // class the explicit-validity contract exists to prevent. Forward
        // geometry drawn after this pass may still composite real diffuse over
        // the pixel and raise K.
        visible_diffuse = vec4(0.0, 0.0, 0.0, 0.0);
        return;
    }

    vec3 diffuse = max(albedo.rgb, vec3(0.0));

    if (has_pbr)
    {
        // METALLIC SPLIT: at metallic = 1 there is NO diffuse lobe -- base
        // colour tints the SPECULAR response instead. Publishing gold base
        // colour as diffuse would make a metal bounce coloured light it never
        // reflects diffusely. Exact zero here is a statement about the BRDF,
        // and it ships with K = 1 because we KNOW the answer is zero.
        diffuse *= (1.0 - orm.b);
    }

    visible_diffuse = vec4(diffuse, 1.0);
}

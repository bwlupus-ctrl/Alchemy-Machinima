# ROUND 3 — the gain conclusion was WRONG. Analyse the emissive-as-albedo hypothesis.

Read `doc/ALBEDO_TEST_RESULTS_ROUND1.md` (rounds 1 and 2) first.

## Correction to round 2

I (Claude) concluded from the Bounce Lighting sweep that "tuning dominates" because 1.000 and 0.500
looked believable. **That was premature.** The user continued the sweep down to **0.250** and
captured a NIGHT scene: the image is dark, but the candle clusters at the column bases are still
**glowing strongly RED**, with red bleed onto surrounding stone. The user reports the colours are
still incorrect, and they know this scene.

By YOUR OWN round-2 criterion this is the third branch, not the first:

> "Only values extremely close to zero look sane: that also argues against 'just 2.750 was high.'
> The albedo/RTGI contract or scope is incompatible enough that gain merely conceals it."

0.250 is extremely close to zero. **Treat the tuning hypothesis as REFUTED, or at minimum demoted.**

## Capture, 19:32

```
Bounce Lighting Intensity   = 0.250
Ambient Occlusion Intensity = 10.000
Object Thickness = 0.235, Temporal Responsiveness = 0.200
Denoiser Medium, Smoothness 0.604, Ambient Level 0.300, Fade-Out Range 0.400
RTGI Debug View = Disabled
RCAO Blend 0.215, Unoccluded Renorm 1.100, Shadow Floor 0.000, Highlight Protect 0.600
SL_PROVIDE_ALBEDO=1, SL_ALBEDO_TO_LINEAR=0, SL_PROVIDER_MODE=0,
SL_VISDIFF_K_THRESHOLD=0.55, RenderVisibleDiffuseSidecar=TRUE
```

Scene: night, exterior colonnade. Dark stone columns, blue glowing orbs on top, and at each column
base a cluster of RED CANDLES. Those candle clusters glow red intensely and bleed red light onto
adjacent geometry, far more than a 0.250 bounce gain should produce in a dark scene.

## MY HYPOTHESIS — attack it, and check the code

**Fullbright/emissive surfaces are publishing their EMISSIVE colour as sidecar ALBEDO.**

`indra/newview/app_settings/shaders/class1/deferred/fullbrightF.glsl`:

```glsl
    color.rgb *= vertex_color.rgb;          // ~:82
    color.a = final_alpha;                  // ~:86
    vec3 visible_diffuse_color = color.rgb; // :87
#ifndef IS_HUD
    color.rgb = srgb_to_linear(color.rgb);
    visible_diffuse_color = color.rgb;      // :90
    ...
#endif
    frag_color = max(color, vec4(0));
#ifdef HAS_VISIBLE_DIFFUSE
    visible_diffuse = vec4(max(visible_diffuse_color, vec3(0)), final_alpha);  // :107
#endif
```

A fullbright surface is an EMITTER: its displayed colour is unlit output, not diffuse reflectance.
SL candles/flames are typically fullbright and/or carry glow. So we publish bright red EMISSION as
if it were near-1.0 red ALBEDO. Consequences:

1. RTGI treats the candle cluster as a near-perfect red DIFFUSE REFLECTOR and bounces red light.
2. iMMERSE ALSO gathers the emitted red from the backbuffer. **The red is counted twice.**
3. Because albedo is near 1.0 and saturated, even a small bounce gain produces strong red — which is
   exactly why lowering Bounce Lighting to 0.250 does not fix it.

Note this is adjacent to hazard **H5** ("additive faces are emitters, must not overwrite") and to the
DEFERRED should-fixes **S2** (fullbright K under-accumulates) and **S4** (fullbright-alpha-mask writes
K=final_alpha on opaque pixels) — all of which were deliberately excluded from the fix batch.

## ANALYSE THE CODE AND ANSWER

1. **Is the hypothesis correct?** Read `fullbrightF.glsl` in full, plus `alphaF.glsl` and
   `pbralphaF.glsl` sidecar writes, `visibleDiffuseSeedF.glsl`, and the emissive handling in
   `lldrawpoolalpha.cpp` / `lldrawpoolsimple.cpp`. State precisely which surface classes publish
   emissive-as-albedo and which do not.
2. **What SHOULD a fullbright/emissive surface publish as visible diffuse?** Options seem to be:
   its underlying texture reflectance; ZERO with K=1 (a real answer: "this surface reflects nothing
   diffusely, it emits"); or K=0 (no answer, fall back to incumbent). Argue for one. Note the
   contract already treats metals as "zero diffuse with K=1, a BRDF statement not a gap" — is the
   emitter case analogous?
3. **Does the same defect exist for GLOW?** SL glow is a separate channel. Check whether glowing
   non-fullbright surfaces also contaminate the sidecar.
4. **Is the deferred G-buffer albedo path affected too**, or only the forward sidecar? The seed reads
   the deferred albedo attachment — does that attachment already contain emissive contamination for
   fullbright/emissive deferred surfaces, meaning this predates the sidecar entirely?
5. **Does this subsume the forward-only scoping error, or is it independent?** If emitters are the
   dominant contaminator, does restricting the sidecar to forward pixels even help, given candles
   ARE forward-rendered?
6. **Cheapest test that confirms or refutes it before any code is written.** Runtime controls only if
   possible. If it needs code, give the minimal diff and say whether it needs a viewer rebuild.
7. Re-rank everything: emissive-as-albedo vs forward-only scoping vs units (H-B) vs gain.

Be blunt if I am wrong again. I have now called this incorrectly twice — first blaming the provider
write, then declaring it a tuning problem. The user has lost most of an evening to my misdiagnoses
and is relying on you for the analysis.

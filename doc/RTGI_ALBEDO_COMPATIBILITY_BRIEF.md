# Is SL albedo even COMPATIBLE with a physically-based bounce integrator?

**User observation, 2026-07-25, after the fullbright fix landed and was validated in-world:**
*"I really do think that the iMMERSE RTGI could use a second look about the compatibility with
bounce lighting from the SL albedo. Even at low value, I'm getting some weird bounces."*

Take this seriously. The user has been ahead of the analysis twice tonight — they found the
RTGI-Diffuse localisation that cracked the case, and they refuted the "it's just gain tuning"
conclusion by pushing the sweep to 0.250 and finding red candles still glowing.

## State

HEAD `a2bfceed82a`. The sidecar publishes correct albedo — proven with a 2048 calibration card
(18% grey = sRGB byte 118 = linear 0.181164; neutral wedge; saturated R/G/B/C/M/Y). The sRGB round
trip was traced numerically: 0.180000 in, 0.181164 out. Legacy fullbright no longer publishes
emission as albedo. The image is usable at RTGI `Bounce Lighting ~= 1.000`.

**But "the data is correct" and "the consumer can use the data" are different claims, and only the
first is established.** H-B (units/calibration) was ranked third and has NEVER been settled.

## The hypothesis to examine — SL content is not physically-plausible albedo

RTGI is a physically-based bounce integrator. It assumes albedo is *reflectance*. SL textures are
not authored that way:

1. **Baked lighting.** An enormous amount of SL content has lighting, shadows and specular
   highlights **baked into the diffuse texture**. Feeding that to a GI integrator bounces
   already-baked highlights and shadows as if they were reflectance. This is probably the biggest
   SL-specific incompatibility and has no equivalent in the games iMMERSE targets.
2. **No energy conservation.** Real diffuse albedo is roughly 0.04-0.85 and rarely saturated; a pure
   (1,0,0) surface essentially does not exist. SL textures are routinely near-white, near-saturated,
   or both, because they are authored to look good once lit. Near-1.0 saturated albedo in a
   multi-bounce integrator produces colour amplification that is physically impossible.
3. **Fullbright/emissive content is everywhere** in SL, far more than in a AAA game. The legacy
   fullbright case is fixed; consider whether the general problem is broader.
4. **Vertex colour and tint** multiply into what we publish. Is that correct as reflectance?
5. **Display-referred vs scene-referred (the original H-B).** ReShade runs on the final backbuffer,
   post-exposure and post-tonemap, so the light RTGI *gathers* is display-referred, while our albedo
   is scene-referred linear. Launchpad's estimate is back-derived from that same backbuffer, so it
   self-tracks exposure AND is pre-distorted by the tonemap's per-channel curve. Tonemapping is
   non-linear PER CHANNEL, so the predicted mismatch is in CHROMA. **Never tested.** The decisive
   test was specified and never run: flat neutral grey card, measure the INCUMBENT estimate at SL
   noon vs SL midnight; drift => units.

## Questions

1. **Which of the five above is dominant?** Rank with reasoning.
2. **Is publishing raw SL diffuse as albedo the right contract at all**, or should the viewer publish
   a *conditioned* albedo for GI consumption? If conditioned, what conditioning is defensible and
   non-destructive — a max clamp (what value, why), desaturation toward physical plausibility, an
   energy-conservation renormalisation, or a user tunable? **State what each would break**, and note
   we must not invent a BRDF (the mistake legacy fullbright made).
3. **Can baked-in lighting be detected or mitigated at all** from the viewer side? Probably not
   per-pixel — but is there anything principled, or is this a limit of SL content that should be
   documented in the contract and exposed as a tunable?
4. **Design the decisive noon-vs-midnight units test** properly, using the calibration card that now
   exists. What exactly is measured, what does drift vs no-drift prove, and what are the predicted
   numbers for each outcome? If a better test exists, give that instead.
5. **What can actually be verified?** iMMERSE internals (`mmx_deferred.fxh`, the RTGI algorithm) are
   proprietary and NOT in this repo. Be explicit about the boundary between what the code proves,
   what published iMMERSE documentation/UI implies, and what is inference. Do not present inference
   as fact — that pattern cost hours tonight.
6. **Is "weird bounces at low values" diagnostic of something specific?** Distinguish: colour
   amplification from saturated albedo; light leaking through thin geometry (`Object Thickness` =
   0.235); disocclusion/temporal artifacts (motion ghosting is still unresolved and is NOT the flip
   settings); RCAO S-Log blend interaction (`RCAO Blend` 0.215, `Unoccluded Renorm` 1.100,
   `Highlight Protect` 0.600); or genuine units mismatch. **Give the shortest in-world observation
   that tells these apart** — the user is out of patience and every trip is expensive.

## Settings at the time of the observation

```
Bounce Lighting Intensity   ~0.250-1.000 (weird bounces reported across this range)
Ambient Occlusion Intensity 10.000       <-- NEVER varied; still at its original tuning
Object Thickness            0.235
Temporal Responsiveness     0.200
Denoiser Medium, Smoothness 0.604
Ambient Level 0.300, Fade-Out Range 0.400
RCAO Blend 0.215, Unoccluded Renorm 1.100, Shadow Floor 0.000, Highlight Protect 0.600
SL_PROVIDER_MODE = 0 (COEXIST, Launchpad loaded ABOVE us)
SL_VISDIFF_K_THRESHOLD = 0.55
```

`Ambient Occlusion Intensity` has been at **10.000** the whole time, tuned against Launchpad's muted
estimate, and has never been swept. Say whether it is implicated.

## Read

`reshade-addon/shaders/SL_GBufferProvider.fx`, `SL_Bridge.fxh`, `SL_BridgeDebug.fx`,
`reshade-addon/src/sl_reshade_bridge.cpp`,
`indra/newview/app_settings/shaders/class1/deferred/visibleDiffuseSeedF.glsl` and `fullbrightF.glsl`,
`class2/deferred/alphaF.glsl` and `pbralphaF.glsl`, `indra/newview/llreshadebridge.cpp`,
`doc/RESHADE_BRIDGE_CONTRACT.md` (colour-space and validity sections), and
`doc/ALBEDO_TEST_RESULTS_ROUND1.md`.

Workspace may be READ-ONLY. Put everything in your FINAL MESSAGE as text.

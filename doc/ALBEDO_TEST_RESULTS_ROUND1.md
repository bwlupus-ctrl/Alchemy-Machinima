# Grey-card test RESULTS, round 1 — 2026-07-25 19:17-19:18

**The user has run the test. These are the raw results. Diagnose them.**

Claude is NOT interpreting these before you do — the user explicitly asked for your read first.

## Confirmed settings at capture time (read off a full-resolution screenshot of the panel)

```
SL_ALBEDO_FLAG_COVERAGE_GATE = 1
SL_ALBEDO_TO_LINEAR          = 0     <-- confirmed 0 this time
SL_BRIDGE_FXH                = (blank)
SL_ENABLE_DEBUG              = 1
SL_INPUT_IS_UPSIDE_DOWN      = 1
SL_NORMAL_FLIP_Z             = 1
SL_PROVIDER_MODE             = 0     (COEXIST)
SL_PROVIDE_ALBEDO            = 1
SL_PROVIDE_MOTION            = 1

Motion flip X = TRUE, Motion flip Y = FALSE, Motion scale = 1.00
SL_VISDIFF_K_THRESHOLD = 0.55
RenderVisibleDiffuseSidecar = TRUE (user re-enabled it)
```

Effect list order unchanged: Launchpad top, SL_GBufferProvider below it, RTGI/MXAO/Specular,
VirtualCinema chain, SL_BridgeDebug last. The calibration card is rendered on a prim in-world,
roughly filling the centre of frame, camera stationary, same camera position for all three captures.

## CAPTURE A — `SL_BridgeDebug` -> "Visible diffuse"  (what the SIDECAR publishes)

**LOOKS CORRECT.**

- Neutral wedge across the top: all 8 steps render as a clean monotonic dark-to-white ramp. The
  patch labels (0, 36, 73, 109, 146, 182, 219, 255) and the `lin 0.xxxx` sub-labels are legible and
  the visual progression matches them.
- Bottom chroma band: renders as **FULLY SATURATED, PURE** primaries and secondaries — vivid red,
  vivid green, vivid blue, vivid cyan, vivid magenta, vivid yellow, then the brown skin patch, then
  the 18% grey patch. They look like exactly what was authored (255,0,0 / 0,255,0 / 0,0,255 /
  0,255,255 / 255,0,255 / 255,255,0 / 186,138,110 / 118,118,118).
- Centre 18% grey field: uniform neutral grey, no visible tint.

## CAPTURE B — `SL_GBufferProvider` -> DEBUG view -> "Albedo iMMERSE receives"

**SAME camera, SAME card, SAME frame-ish. LOOKS WRONG.**

- Neutral wedge: still a recognisable dark-to-light ramp, roughly plausible.
- Bottom chroma band: **HEAVILY DESATURATED AND HUE-SHIFTED.** Instead of pure primaries they read
  as muted pastel/earth tones — the red patch reads muted ORANGE, green reads a muted YELLOW-GREEN,
  blue reads a washed PERIWINKLE/PURPLE, cyan reads pale BLUE-GREY, magenta reads dusty PINK,
  yellow reads OLIVE/KHAKI. The saturation loss is severe and obvious side by side with capture A.
- The whole card, and the frame generally, looks tinted toward the scene's ambient lighting —
  i.e. it looks like a LIT or ESTIMATED albedo, not a clean reflectance map.
- Centre 18% grey field: grey, hard to distinguish from capture A by eye.

## CAPTURE C — DEBUG view Off (ordinary beauty render, for reference)

Card renders normally in-world, lit, washed out by the scene's daylight/haze. Chroma patches are
muted here too, consistent with them simply being lit.

## THE HEADLINE

**The sidecar's published data is CORRECT. What iMMERSE receives is NOT.**

This is your own decision-tree branch: *"Visible diffuse ~= 0.1812, but Albedo iMMERSE receives is
different: consumer/provider conversion or write problem."*

That appears to rule out, or at least demote:
- units/calibration (H-B) as the primary cause — the sidecar side is clean;
- any sRGB transfer error in the producer or the add-on transport — capture A is right;
- `SL_ALBEDO_TO_LINEAR` — confirmed 0 at capture time.

## WHAT TO DIAGNOSE

1. **Why does `Deferred::AlbedoTex` not contain what `SL_VisibleDiffuse` returns?** Trace
   `PS_ProvideAlbedo` and the `ProvideAlbedo` pass. Consider specifically:
   - Is the sidecar branch being taken at all on these pixels, or is control reaching a `discard`
     (COEXIST) so **Launchpad's estimate survives** — which would explain the "looks lit/estimated"
     appearance exactly?
   - `SL_ALBEDO_FLAG_COVERAGE_GATE = 1` and the `sl_flag_covered` / `SL_SurfaceCoverage` gates.
   - Is `SL_SemValid(SL_SEM_VALID_VISIBLE_DIFFUSE)` true in `SL_GBufferProvider`'s context? It is
     evidently true in `SL_BridgeDebug`. If the two effects disagree, why?
   - `RenderTargetWriteMask = 7` on the pass, and the RGBA16F destination.
   - Does `Deferred::get_albedo()` (used by `PS_ShowReceived`) apply any transform, scaling, or
     resolution difference that `PS_ProvideAlbedo`'s write does not — i.e. **could the debug view
     itself be lying?** Check the instrument before the subject.
   - Pass ORDER: does anything between `ProvideAlbedo` and the `ShowReceived` pass overwrite
     `Deferred::AlbedoTex`? Launchpad runs ABOVE us — but does anything run between?
2. **Is `K >= 0.55` actually satisfied on the card's pixels?** The card is a plain opaque prim, so
   the seed should give K=1. Confirm from the code path rather than assuming.
3. Give the SHORTEST next capture that distinguishes "we never wrote" from "we wrote and something
   overwrote us" from "the debug view misreports".

## LIMITATION — read this before relying on the numbers

Claude does **not** have these screenshots as files on disk and therefore has **not** sampled exact
pixel values. Everything above is a careful visual reading of images in the conversation. The
saturation difference between capture A and capture B is large and unambiguous; the neutral values
are NOT precisely known. If your diagnosis depends on exact numbers, say so explicitly and Claude
will ask the user for the PNGs.

---

# ROUND 2 — USER DISCOVERY, 19:23-19:24. Read this before acting on the round-1 verdict.

**Disabling the effect `iMMERSE Pro: RTGI (Diffuse) ? RCAO S-Log Blend [PATCH #4-SMOOTH]
[iMMERSE_RTGI_RCAO_Blended.fx]` MAKES THE BAD COLOURS GO AWAY.**

With RTGI Diffuse unchecked and everything else unchanged (SL_PROVIDE_ALBEDO=1,
SL_ALBEDO_TO_LINEAR=0, SL_PROVIDER_MODE=0, K threshold 0.55, RenderVisibleDiffuseSidecar=TRUE,
MXAO and RTGI Specular still ENABLED):

- The scene renders NORMALLY. Natural stone columns, green foliage, blue sky, correct exposure.
  None of the neon/rainbow oversaturation.
- The calibration card renders correctly in the BEAUTY view: neutral wedge ramps properly and the
  bottom chroma band shows clean pure R, G, B, C, M, Y, brown skin, grey 18%.

Compare the immediately preceding capture at 19:23 with RTGI Diffuse ENABLED: same camera, same
card, and the scene was violently oversaturated — neon-green foliage, glowing orange columns — while
the card's own patches were tinted by coloured bounce.

## What this appears to establish

The chroma explosion is produced by **RTGI's diffuse bounce**, not by the sidecar, not by the
transport, and not by the beauty pass. Everything upstream of RTGI renders correctly.

## Do NOT over-read it

Turning off diffuse bounce removes coloured bounce by definition, so this alone does not prove the
albedo is correct. It narrows WHERE, not WHY. The remaining candidates:

- **(a) Units/calibration (your H-B).** RTGI multiplies gathered light by albedo. Its tuning was
  established against Launchpad's ESTIMATE, which is back-derived from the tonemapped backbuffer and
  is therefore muted and desaturated compared to true reflectance. Feeding true saturated reflectance
  over-drives the bounce. Predicts exactly this.
- **(b) Scope (your Question 4 finding).** We supersede albedo at EVERY pixel, not only the
  forward-rendered pixels the feature existed to fix. So RTGI now gets true albedo across the whole
  frame rather than only where the incumbent was broken. Also predicts this, and predicts it would
  be worse than a forward-only fix.
- **(c) RTGI parameters.** Bounce Lighting was 2.750 and Ambient Occlusion 10.000 in an earlier
  capture — tuned for the muted incumbent. Possibly just needs retuning for true albedo.

(a), (b) and (c) are not mutually exclusive and all three predict this observation.

## QUESTIONS

1. Does this observation change your ranking of H-A / H-B / the Question-4 scoping error?
2. **Is your round-1 recommendation (the three-band raw-vs-accessor debug pass) still the right next
   step, or has it been demoted?** It costs a shader edit and a rebuild. Say plainly whether it is
   still worth it given this finding, or whether something cheaper now dominates.
3. Cheap discriminator between (a) and (c): if the problem is purely gain/tuning, reducing RTGI's
   Bounce Lighting should restore a sane image while KEEPING true albedo. If the problem is chroma
   (units), reducing bounce should dim the image but the HUE errors should persist. **Is that a valid
   test?** If yes, give the exact parameter and the values to try, and state what each outcome means.
4. Discriminator for (b): is there a way to restrict the sidecar to forward pixels TODAY, with
   existing controls only and no code — e.g. by raising SL_VISDIFF_K_THRESHOLD? You previously said
   NO, because deferred-opaque seed and forward-opaque both carry K=1 so no threshold can separate
   them. Confirm or revise. If it genuinely cannot be done without code, say so and specify the
   minimal code change for the forward-coverage mask.
5. Given all of it: what is the single cheapest next action? The user has spent hours, is out of
   patience, and every in-world trip is expensive. Prefer a runtime slider over a shader edit, and a
   shader edit over a viewer rebuild.

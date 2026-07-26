# ROUND 4 — clean measurement, and your proposed conditioning does not address it

Read `doc/RTGI_ALBEDO_COMPATIBILITY_BRIEF.md` (your round-3 compatibility analysis) and
`doc/ALBEDO_TEST_RESULTS_ROUND1.md` first. HEAD `a2bfceed82a`.

## The AO confounder is GONE — measurement is now clean

You said no conclusion about "weird bounces at low values" was clean until AO was tested at 0.
**It has been.** `Ambient Occlusion Intensity: 10.000 -> 0.000` had **NO impact on colour.**
RCAO/S-Log interaction is therefore eliminated as the primary cause, and AO has been left at 0.

## The observation, with a clean setup

```
Ambient Occlusion Intensity  0.000     <-- confounder removed
Bounce Lighting Intensity    0.466
Object Thickness             0.235
Temporal Responsiveness      0.200
Denoiser Medium, Smoothness  0.604
Ambient Level 0.300, Fade-Out Range 0.400
RCAO Blend 0.215, Unoccluded Renorm 1.100, Shadow Floor 0.000, Highlight Protect 0.600
RTGI Debug View Disabled
```

Night scene, exterior colonnade, many small RED candle flames at the column bases.
**At Bounce 0.466 the result is aggressively SATURATED** — hard red on the flames, red casts
spreading onto the column ornaments and nearby stone. It scales with Bounce Lighting.

User's read: *"raising the bounce lighting up to .466 causes aggressive saturation and that hue thing
is looking likely."* They are describing your hypothesis #2 (non-physical, near-1.0 saturated SL
albedo causing colour amplification).

## ⚠️ CORRECTION — your proposed conditioning does NOT address saturation

You proposed, for legacy content:

```
m = max(R,G,B)
conditioned = raw * min(1, ceiling / m)     // default ceiling 0.85
```

**That is a hue-PRESERVING magnitude scale. It cannot reduce saturation.** Pure red `(1,0,0)` maps to
`(0.85,0,0)` — 15% dimmer and *exactly as saturated*. Channel ratios are preserved by construction;
that was the stated design goal. But the observed symptom is **saturation**, not magnitude.

So the proposal is aimed at the wrong axis. **Address this directly.** If a chroma limit is the right
answer, define it precisely (e.g. bound `(max-min)/max`, or blend toward luminance above a chroma
threshold) and say what it breaks. You previously warned that desaturation "invents spectral
reflectance the viewer did not observe" — that warning is noted and correct for a *mandatory*
transform, but say whether an explicit, opt-in, clearly-labelled artistic compatibility control
changes your answer.

## ⚠️ THE QUESTION THAT MAY INVALIDATE THE WHOLE DIAGNOSIS

**Since the fullbright fix, candle flames publish ZERO diffuse** (`fullbrightF.glsl` now writes
`vec4(vec3(0.0), final_alpha)`). They therefore contribute **nothing as reflectors**. So the red at
Bounce 0.466 must be RTGI gathering their **EMISSION from the backbuffer** and bouncing it off
surrounding surfaces.

**Dozens of red flames producing red bounce is physically CORRECT.** So:

1. Is the observed result **correct GI that is merely too strong for taste** (=> pure tuning, no code),
   or **genuine amplification** (=> a real content/units incompatibility)?
2. **How would we tell the difference from in-world observation?** The proposed discriminator is:
   look at neutral grey stone lit by the candles — a mild warm tint means correct GI; going saturated
   red means amplification. **Is that sound?** Give the sharpest version of it, and predicted
   appearance for each outcome.
3. If the red comes from gathered EMISSION rather than from albedo, does that change your ranking?
   Specifically: is the receiving surface's albedo the amplifier, or is the emission itself being
   over-gathered because SL fullbright/glow content is far brighter in the backbuffer than the
   game content iMMERSE was tuned against?
4. **Does the fullbright fix create a NEW asymmetry?** Emitters now contribute light (gathered from
   the backbuffer) but absorb nothing and reflect nothing (zero diffuse). Is that self-consistent for
   a GI integrator, or does zero-diffuse-on-a-visible-surface cause a different artifact — e.g. black
   holes in the bounce, or missing occlusion? The fix is validated as an improvement, but say plainly
   if it has a downside we have not looked for.

## Correction to the record

An earlier note implicated the "RCAO S-Log Blend" patch. **That was Claude misreading which shader
the user meant** — they were referring to VirtualCinema 2, their colour grade, which is unrelated to
GI. That said, one fact established while chasing it stands and is useful:
`MartysMods/RCAO_AO_Tail.fxh:153` `RCAO_AO_Tail(float, float, float, float) -> float` is entirely
**scalar**, so the RCAO tail cannot introduce a hue shift under any settings. Also noted from that
file, line 34: **`AO_RCAO_MIX = 0` reproduces the stock RTGI tail bit-for-bit**, so `RCAO Blend = 0`
is available as a stock-behaviour A/B that keeps GI running — say whether that test is still worth
spending a trip on.

## What to produce

A decision, not a menu:
- Is this tuning, or is it a real incompatibility requiring code?
- If code: the exact conditioning operation, where it lives (viewer-side publication vs FX-side
  consumption), whether it is a new semantic or a modification of the existing one, and the default.
- The single sharpest in-world observation that confirms or refutes your answer before anything is
  written.

The user has spent an entire evening on this and every in-world trip is expensive. Be decisive, and
say plainly if the honest answer is "this is correct behaviour, lower the slider."

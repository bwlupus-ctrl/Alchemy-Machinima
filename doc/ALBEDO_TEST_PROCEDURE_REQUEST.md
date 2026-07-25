# WRITE THE IN-WORLD TEST PROCEDURE — Claude's directions keep being wrong

You already produced the albedo diagnosis (H-B dominant, H-A activation mechanism, H-C motion,
plus the consumer-side scoping error re Question 4). **Now write the actual step-by-step in-world
procedure.** Claude has given the user several bad instructions in a row and has lost the right to
author this. Read `doc/ALBEDO_CHROMA_EXPLOSION_DIAGNOSIS_BRIEF.md` for the raw observations and your
own prior findings.

## Claude's errors so far — do not repeat them

1. Told the user to enable `Motion flip Y` by reasoning from the ABI flag NAMES instead of deriving
   the algebra. You showed the current defaults (FLIP_X=true, FLIP_Y=false) are correct.
2. Told the user to turn `RenderVisibleDiffuseSidecar` OFF for an isolation test, while the
   SL_BridgeDebug "Visible diffuse" view — needed for the measurement — depends on that exact
   setting. The user got the magenta/black `sl_invalid_pattern` and lost a trip.
3. Conflated two different tests (the sidecar-off isolation test and the grey-card measurement) into
   one instruction. They require OPPOSITE settings.
4. Drip-fed one-click guesses across multiple in-world trips instead of one ordered plan.

## The instrument that now exists

A 2048x2048 calibration texture has been generated for the user to upload to Second Life:

- **Centre field, 768x768 clean, marked with orange corner ticks: sRGB byte 118 -> linear 0.181164**
  (18% photographic mid-grey). This is the primary reference.
- **Top band, 8 neutral steps:** sRGB bytes 0, 36, 73, 109, 146, 182, 219, 255 ->
  linear 0.000000, 0.017642, 0.066626, 0.152926, 0.287441, 0.467784, 0.708376, 1.000000.
- **Bottom band, 8 chroma patches:** pure R(255,0,0), G(0,255,0), B(0,0,255), C(0,255,255),
  M(255,0,255), Y(255,255,0), skin(186,138,110), and 18% grey(118,118,118).
  These exist because a neutral patch alone cannot distinguish a GAIN error from a CHROMA error.
- Delivered as uncompressed TGA (preferred, no J2C ringing / no gamma ambiguity) and PNG.

**The user can screenshot any debug view and Claude will sample the actual pixel values out of the
image.** So the procedure can and should produce READABLE NUMBERS, not just yes/no impressions.
Account for the fact that a screenshot of a ReShade debug view has passed through the swapchain's
sRGB encode, and that effects AFTER SL_BridgeDebug in the chain could modify the pixels — state
explicitly what must be disabled so the sampled numbers are trustworthy, and state what transfer
function Claude must invert when reading them.

## Every control in play (use exact names)

**Viewer (Alchemy, debug settings):** `RenderVisibleDiffuseSidecar` (currently the master gate for
the whole sidecar + the VISIBLE_DIFFUSE semantic), `RenderDeferredAtmospheric`,
`RenderEnableEmissiveBuffer`, `BDMergeVelocityBuffer`.

**SL_GBufferProvider.fx preprocessor:** `SL_PROVIDE_ALBEDO`, `SL_PROVIDE_MOTION`,
`SL_ALBEDO_TO_LINEAR` (must be 0 — user has flipped it back to 1 at least twice, call it out),
`SL_PROVIDER_MODE` (0 = COEXIST), `SL_ALBEDO_FLAG_COVERAGE_GATE`, `SL_ENABLE_DEBUG`,
`SL_NORMAL_FLIP_Z`, `SL_INPUT_IS_UPSIDE_DOWN`.

**SL_GBufferProvider.fx uniforms:** `DEBUG view` (Off / Normals iMMERSE receives / Motion iMMERSE
receives / Albedo iMMERSE receives), `SL_VISDIFF_K_THRESHOLD` (currently 0.55),
`Motion flip X` (true), `Motion flip Y` (false), `Motion scale` (1.00).

**SL_BridgeDebug.fx uniform:** view selector — Normals decoded / Normals raw / Albedo / ORM /
HDR color / Motion / Validity-tail / Visible diffuse / Exactness K. Also note its top-left
12-cell status strip (cells 0-9 = semantic valid bits, cell 9 = VISIBLE_DIFFUSE; cell 10 = mode,
blue COEXIST / orange OWNED / magenta MISMATCH; cell 11 = white flash on reset).

**Effect list order:** `iMMERSE: Launchpad` top, `SL_GBufferProvider` directly below, RTGI/MXAO/
Specular below that, VirtualCinema chain, `SL_BridgeDebug` last.

**Prim setup matters:** tint multiplies albedo, so no tint. Also specify shininess, glow, bump,
fullbright, and whether the test prim should be a plain opaque prim (deferred path) or fullbright
(forward path) — Claude gave contradictory guidance here. The feature's ORIGINAL purpose was to fix
forward-rendered pixels only, so both paths probably need separate measurement.

## What to produce

An ordered procedure the user can execute with the fewest possible in-world trips — ideally ONE
login. For EVERY step give:

1. The exact settings to change, by exact name, and their values.
2. Exactly what to look at (which effect, which view, where on screen).
3. What to capture (screenshot of what, framed how).
4. **What each possible outcome MEANS** — a decision tree, so the user is never left holding an
   observation nobody can interpret. Explicitly include the "unexpected / should not happen"
   outcomes and what they would imply.
5. Any step that can be skipped if an earlier step already settled the question.

Also state up front:
- The single most diagnostic measurement, if the user only does one thing.
- Which measurements definitively separate H-A (scoping), H-B (units/calibration), and H-C (motion),
  and the predicted NUMBERS for each hypothesis at the 18% grey patch and at the saturated chroma
  patches. Concrete predicted values, so the result is falsifiable rather than interpretable.
- Whether the correct end state for this feature is the forward-coverage mask you identified in
  Question 4, and if so, what measurement would confirm that before any code is written.

Write it so a tired, angry user can follow it without interpreting anything. No hedging, no
"try flipping X and see". If a step is a guess, label it a guess and say what it would prove.

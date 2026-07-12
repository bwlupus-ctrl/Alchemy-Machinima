# Projector Volumetrics — Next-Gen Brief (efficiency / robustness / cinematic quality)

**Status:** design brief only, written 2026-07-12 after the ghost-hunt rollback. The volumetric
shaders are byte-identical to the pre-session Batch-1-3 state (`d7878fab0a`); the S-Log levers,
TemporalReject and kill-switches were reverted but their designs were sound — this brief sequences
them back in behind the fixes that must land first. SMAA T2x was reverted independently
(`78f3837cf2`, it 50/50-blended with no velocity reprojection).

**Open blocker (do FIRST, before any of this):** the ghosting culprit is still unattributed.
Two-toggle bisection with everything else at baseline:
1. `BDMergeProjectorVolumetricsTemporal = FALSE` → ghost dies ⇒ the Batch-1A accumulator (fix = §R1).
2. Disable ReShade `MartysMods_Launchpad` + RTGI/SPECGI techniques → ghost dies ⇒ ReShade optical-flow
   temporal, and NO viewer-side change will fix it (mitigate by shot discipline / RTGI settings).
Everything below assumes the answer is known before code is written.

---

## Current cost model (what a pixel pays today)

Per cone, per half-res pixel inside the scissor: sphere-intersect [t0,t1], then `godray_res`
(4-64, adaptive) steps × (`clipProjectedLightVars` matrix math + cookie LOD fetch +
1-4 spot-shadow taps + HG phase + optional 3-octave fbm) — then temporal EMA, bilateral upsample,
optional bloom feed. The two dominant costs are **shadow taps** (up to 4× per step) and **wasted
steps**: the sphere of influence is a far looser bound than the actual light frustum, so for a
typical narrow spotlight most steps `continue` out of the cookie/frustum test after paying the
projection math.

---

## E. Efficiency (no visible change, pure headroom)

**E1. Tighten the march domain: ray∩frustum, not ray∩sphere.** Upload the projector's 4 side
planes (+ near/far) per cone (computed C++-side from `proj_mat`); slab-clip the view ray against
them ONCE per pixel to refine [t0,t1] before stepping. For narrow cones this cuts the marched
interval 2-5×, which either halves the cost or doubles the effective sample density for free.
Fall back to the sphere bounds if the plane set is degenerate. **Single biggest win; do first.**

**E2. One jittered shadow tap instead of N sub-taps.** `projvol_shadow_samples` multiplies the
most expensive fetch in the loop. Replace with a single tap whose position is jittered along the
step by the existing IGN dither (decorrelates exactly like sub-taps, and the temporal/dither
machinery already averages it). Keep the sub-tap path as a fallback lever for temporal-off purists.
Cost: up to 4× fewer shadow fetches at equal perceived quality.

**E3. Exponential step distribution.** Steps uniform in t today; distribute them denser near the
camera (where a step covers more screen area) — same count, less near-field banding, so
`godray_res` can drop a notch for the same look.

**E4. Luminance early-out.** Break the loop when accumulated in-scatter reaches ~`projvol_max`
(it will be clamped anyway). Bounds the pathological case (camera inside a dense bright cone).

**E5. Global step budget.** `cones_drawn × godray_res` is unbounded in a 6-projector club scene.
Add a per-frame step budget (setting, generous default) that the adaptive-res logic distributes
across visible cones by screen coverage — worst-case frame time becomes deterministic, which is
what matters when recording.

---

## R. Robustness (the ghost-saga lessons, institutionalized)

**R1. Reproject the BEAM, not the wall behind it.** Root defect of the Batch-1A temporal: history
is reprojected using the opaque SURFACE depth, but the shaft is airborne — under camera motion the
media and the wall move differently, so history lands wrong and only the 3×3 clamp stands between
that and a visible trail. Fix: the march writes its scatter-weighted mean sample depth
(Σ(w·t)/Σw, fallback t_surface when the shaft is empty) into the half-res target's alpha; the
temporal pass reprojects and disocclusion-tests with THAT. This is the *correct* fix if bisection
fingers the accumulator — land it before re-adding any beam contrast.

**R2. Re-land contrast-aware history rejection — default OFF.** The reverted TemporalReject math
(`w *= exp(-k · |lum(cur)-lum(hist)| / (lum(cur)+lum(hist)+ε))`) was sound: full denoise where the
beam is steady, snap-to-current where it changes. Re-land after R1 with default 0 so shipped
behavior is exactly legacy until dialed.

**R3. Temporal-off must be a first-class path.** Whatever the bisection says, a machinima tool
cannot *require* an accumulator. Target: dither=1 + E1/E3 at res ~24-32 should look clean with
`Temporal=FALSE`. Document that combination as the "deterministic mode".

**R4. Deterministic takes (flycam-recorder synergy).** Add a "lock media" toggle: freeze
`projvol_time` (noise scroll) and the dither frame-walk to a take-relative clock instead of
wall/frame time, so two recordings of the same camera path render identical beams. Cheap, and
uniquely valuable for multi-take machinima.

**R5. Bisection hygiene (process, not code).** When a temporal artifact appears, enumerate ACTIVE
temporal systems first (projvol temporal / any temporal AA / ReShade Launchpad-RTGI) and toggle
whole systems before tuning any component. This session burned hours violating that; the memory
file now says the same.

---

## C. Cinematic quality (in landing order, after E+R)

**C1. Re-land the S-Log pack** (reverted `40116f1770`, design unchanged, still gated/default-0):
Beer-Lambert extinction (keep `sigma_t` vec3 → spectral option later), soft-knee highlight
shoulder (replaces the flat-clipping hard `projvol_max`), contact fade + contact pool (beam
visibly lands on surfaces). These read dramatically better under an S-Log3 grade; they were
innocent of the ghosting.

**C2. Dual-lobe phase.** Blend a second, wide/backward HG lobe with the forward one
(`mix(HG(g_fwd), HG(g_back), lobe_mix)`) — beams get a soft ambient body instead of only a
view-dependent hotspot. ~3 ALU per sample; big perceived richness in a graded image.

**C3. Per-axis feather = barn doors.** `projvol_feather` is isotropic; make it a vec2 (x/y edge
softness in cookie space) and stage-light barn-door looks fall out of the existing edge math.

**C4. Anamorphic bloom feed.** The bloom-feed tent blur gains an aspect parameter (stretch the
tap pattern horizontally) → classic anamorphic streak on beam cores, essentially free.

**C5. Ambient air floor.** A tiny constant in-scatter term inside the cone (gated, default 0) so
beams read in "clean air" scenes without cranking `Density`/fbm — currently the only way to see a
beam in dry air is noise or fog, which changes the look elsewhere.

**C6. Spectral extinction knob** (after C1): vec3 extinction so a green gel deepens with beam
depth — the "neon through haze" look from the reference frame, physically motivated.

---

## Sequencing

0. **Bisection** (5 minutes in-world, no build) → decides whether R1 is the priority or ReShade is.
1. **E1 + E2** (pure perf, look-neutral, independently verifiable via GPU-zone timings).
2. **R1** then **R2** (temporal correctness; A/B against Temporal=FALSE as ground truth).
3. **C1** re-land, then **C2-C6** individually (each gated, each eval-able alone).
4. **E5 + R4** (production polish for multi-projector recording sessions).

Every item stays behind its own default-off/no-op gate per fork convention; every landing gets an
in-world A/B before the next lands — one variable at a time, which is the other thing this
session's ghost hunt re-taught.

---

# EXECUTION PLAN — Opus-tier subworker batches

Constraints this plan is built around (all learned the hard way):
- **Sequential builds** — every worker builds in the main tree; never two workers at once.
- **Agents cannot run the viewer** — every batch ends at a hard STOP for the user's in-world A/B.
  One batch = one variable set; nothing lands on top of an un-evaluated batch.
- **Orchestrator reviews before the user tests** — after each worker reports, the orchestrator
  verifies: enum/string lockstep (`llshadermgr.h` vs `.cpp` insertion position AND order),
  default-state no-op (every new branch guards on its gate; grep the diff), build linked, commit
  message format. Only then hand to the user.
- **Batch 0 gates nothing except priorities** — Batch A is bisection-independent; Batch B is the
  fix if toggle (1) kills the ghost, and still correct (just less urgent) if it's ReShade.

## Batch 0 — USER, in-world, no build (5 minutes)
1. `BDMergeProjectorVolumetricsTemporal = FALSE` → ghost dies? ⇒ accumulator (Batch B is the fix).
2. Re-enable temporal; disable ReShade Launchpad + RTGI/SPECGI techniques → ghost dies? ⇒ ReShade
   optical-flow temporal; viewer-side work proceeds on merit, ghost handled by shot discipline.
Report which toggle killed it before Batch B is dispatched (A can start immediately).

## Batch A — worker 1: "look-neutral performance" (E1, E2, E4)
- **E1 FrustumClip** (`BDMergeProjectorVolumetricsFrustumClip`, Bool, default OFF for A/B):
  C++ computes the projector's 4 side planes + near/far in view space from the same state
  `setupSpotLightVolumetric` uploads (`pipeline.cpp:9706` area) and uploads them per cone
  (new vec4[6] reserved uniform, or 6 vec4s). Shader (`class3/deferred/projectorVolumetricF.glsl`)
  slab-clips [t0,t1] against the planes once per pixel after the existing sphere clip
  (`lines ~232-253`), before the loop. Degenerate plane set ⇒ keep sphere bounds.
- **E2 ShadowJitterTap** (`...ShadowJitterTap`, Bool, default OFF): replaces the
  `projvol_shadow_samples` sub-tap loop (`glsl ~313-331`) with ONE `sampleSpotShadow` at a
  position offset along the step by the existing IGN value (`roffset` machinery, `~258-273`).
  When ON, ignores ShadowSamples>1.
- **E4 early-out** (no setting): `break` when `max(accum)*dt*PROJVOL_SCATTER*godray_multiplier`
  exceeds `projvol_max` — provably invisible because of the existing final clamp (`~483`).
- Deliverables: build clean, commit, and a perf-verification recipe for the user (GPU zone
  "projector volumetric cones" in the profiler HUD, before/after with a 3-cone scene).
- STOP → orchestrator review → user A/B: look identical with gates OFF and ON; frame-time delta.

## Batch B — worker 2: "temporal correctness" (R1, R2, R4)
- **R1 TemporalBeamDepth** (`...TemporalBeamDepth`, Bool, default OFF): march accumulates
  `depth_w += w·t; w_sum += w` (w = the sample's scalar in-scatter weight) and writes
  `frag_color.a = w_sum > eps ? depth_w/w_sum : t_surface` — CHECK `mProjVolHalf`'s format first
  (allocated near `pipeline.cpp:9551` block; if RGB-only, widen to RGBA16F). Temporal pass
  (`class1/deferred/projectorVolumetricTemporalF.glsl`): when the gate is on, reproject with the
  BEAM depth from `projectionMap.a` (current) and disocclusion-test against history alpha as now;
  when off, byte-identical legacy (surface depth). The upsample/bloom passes ignore alpha — verify.
- **R2 TemporalReject re-land** (`...TemporalReject`, F32, **default 0.0** this time): identical
  math to reverted `7c39d0b523` (scale-invariant luminance disagreement → `w *= exp(-k·change)`),
  re-inserted AFTER the neighborhood clamp. Enum/string: insert directly after
  `PROJVOL_TEMPORAL_BLEND` in BOTH files.
- **R4 LockMediaClock** (`...LockMediaClock`, Bool, default OFF): when ON, `projvol_time` and the
  dither frame counter derive from a resettable take clock (reset when the flycam recorder starts
  playback — hook `llflycamrecorder` start; worker locates it) instead of `gFrameTimeSeconds` /
  frame count, so identical camera paths render identical beams across takes.
- STOP → review → user A/B: ghost test with BeamDepth ON vs OFF (this is the decision the whole
  plan turns on if Batch 0 fingered the accumulator).

## Batch C — worker 3: "cinematic pack 1" (C1, C2, C3)
- **C1**: re-land the S-Log pack by cherry-picking `40116f1770` (expect small conflicts where the
  kill-switches were stripped: rim upload line, per-cone G upload; resolve to the plain uploads).
  Re-verify all four gates are 0.0-default no-ops.
- **C2 dual-lobe phase**: `...PhaseLobeMix` (F32 0=off) + `...PhaseLobeG2` (F32, default -0.15):
  `phase = mix(HG(g), HG(g2), lobe_mix)` in the loop (`glsl ~374-383`).
- **C3 barn doors**: `projvol_feather` scalar → keep the scalar setting for back-compat and add
  `...FeatherY` (F32, -1 = "use X" sentinel = default); shader takes vec2 feather and the edge
  test (`~299-308`) feathers x/y independently.
- STOP → review → user A/B under the S-Log3 grade.

## Batch D — worker 4: "cinematic pack 2 + production" (C4, C5, C6, E3, E5)
- **C4 anamorphic feed**: `...BloomFeedAnamorphic` (F32 1.0=off): scale the tent-blur tap X
  spacing in `projectorVolumetricBloomFeedF.glsl`.
- **C5 air floor**: `...AirFloor` (F32 0=off): constant in-scatter term added to `density`-scaled
  accumulation inside the cookie footprint only.
- **C6 spectral extinction**: `...ExtinctionR/G/B` or one color setting; fills C1's vec3 sigma_t.
- **E3 exponential steps**: `...ExpSteps` (Bool, default OFF): remap `t = t0 + (e^(k·u)-1)/(e^k-1)·len`.
- **E5 step budget**: `...StepBudget` (U32, 0=off): after the adaptive per-cone res calc
  (`pipeline.cpp:9794` area), scale all cones' res proportionally so Σ(res_i) ≤ budget.
- STOP → review → final user pass.

## Standard worker packet (paste into every dispatch)
Repo `I:\alchemy-machinima`, branch `develop`, work directly (no worktree). Build (PowerShell,
never Git Bash): `cmake --build "I:\alchemy-machinima\build-Windows-vs2026-os" --config Release
--target alchemy-bin`. Exe locked while the viewer runs — report a LNK1104, don't fight it.
Gotchas: NEW shader files need llviewershadermgr registration + a cmake reconfigure + staging
verification (prefer editing existing shaders); enum/string lockstep in llshadermgr.h/.cpp is
order-critical; every feature behind its own default-no-op gate (grep-verify every new branch
guards on its gate); settings.xml F32/Bool keys Persist=1 mirroring existing BDMergeProjector-
Volumetrics* blocks; match comment density/style; commit per feature-group ending with the
Co-Authored-By line; report: commit hash, build tail, per-setting name/default/range, in-world
eval checklist, uncertainties.

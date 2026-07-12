# Hybrid Froxel Volumetrics — Architecture Brief + Execution Plan

**Written 2026-07-12.** Goal: cinematic multi-light volumetrics that scale — a **hybrid** of
(a) a camera-frustum **froxel grid** carrying the shared atmosphere and every non-hero volumetric
light (O(1) integration cost in light count, world-space temporal that structurally cannot ghost),
and (b) the existing **per-cone march** kept for 1-2 flagged **hero** beams that need razor-sharp
gobo/shadow banding (the per-pixel occlusion froxels cannot preserve). This mirrors film practice:
key light sharp, atmosphere soft.

## Revert safety (three independent levels)
1. **Tag `pre-froxel-baseline`** (= `f685fccc3e`, per-cone Batches A+B working) — hard rollback:
   `git revert`/`checkout` against the tag.
2. **Master gate `BDMergeFroxelVolumetrics` (Bool, default OFF)** — with it off, zero froxel code
   executes and zero targets allocate; shipped behavior is the per-cone path, byte-identical.
3. **The per-cone march is NOT modified by this work** — it remains fully functional for ALL cones
   when the gate is off, and for hero cones when on. Worst case the froxel layer is abandoned and
   nothing of value is lost.

## Infrastructure decision (checked against the tree, do not re-litigate)
- There is **NO compute-shader support** in `llglslshader`/`llshadermgr` (grep confirms), and
  `LLRenderTarget` is 2D-only. Adding compute infra is its own project and is NOT needed at our
  grid sizes on the target GPU (RTX 5090).
- Therefore froxel storage = **2D Z-slice atlas** in ordinary `LLRenderTarget`s (RGBA16F), fragment
  shaders only, existing patterns throughout. A grid of X×Y×Z is stored as a 2D atlas of Z tiles
  (e.g. 240×136×64 → 8×8 tiles → 1920×1088 atlas). Sampling = manual trilinear (two bilinear tile
  taps + lerp in Z) in a small shared GLSL helper (`froxelUtil.glsl`-style include, registered like
  deferredUtil).
- Default grid for the target machine (machine-specific tuning is fair game): **240×136×64**,
  exponential Z to `BDMergeFroxelFar` (default 128 m). Settings for X/Y/Z/far so it scales down.

## The passes (all fragment, all half of them tiny)
Frame order, replacing nothing — inserted alongside the existing volumetric block in
`renderProjectorVolumetric`'s caller region (before `colorCorrect`, same composite point):
- **P1 media** (one atlas-sized pass): per froxel compute σs/σt from base density + height-fog +
  animated noise — the SEMANTICS of the current per-cone `projvol_density/fog/noise` levers move
  here so ALL lights share ONE atmosphere (a look upgrade: unified air). Writes atlas A
  (rgb = scattering albedo·σs, a = σt).
- **P2 light injection** (one atlas-sized pass): loop over ≤`BDMergeFroxelMaxLights` (default 8)
  non-hero volumetric projectors, uniform-array of per-light params (view-space pos/axis, cookie
  via the existing projector texture units per light is NOT feasible in one pass — sample cookie
  and that light's spot-shadow map per froxel center using the SAME `sampleSpotShadow` slot
  machinery; lights beyond the 6 shadowed slots inject unshadowed). Per froxel per light:
  cookie × shadow × atten × HG-phase(view dir) × σs → accumulate in-scatter rgb into atlas B.
  Jitter the froxel-center sample point per frame (IGN) for the temporal pass to resolve.
- **P3 temporal** (one atlas-sized pass): reproject each froxel's WORLD position into the previous
  frame's grid (prev view matrix → prev froxel coords → atlas sample), EMA blend with 3×3×1 clamp.
  No surface depth anywhere in the equation — the R1 wall-vs-beam mismatch cannot exist here.
  History = ping-pong atlas pair.
- **P4 integrate** (Z slice-ordered scan, 64 sequential tiny draws with read-previous-slice):
  front-to-back accumulate using **Hillaire's energy-conserving slice integral**
  `L += T · S·(1−e^(−σt·d))/σt; T *= e^(−σt·d)` (d = metric slice thickness, exponential
  distribution). Output atlas C: rgb = integrated in-scatter to this slice, a = transmittance.
  Scissor each slice draw to its tile (64 draws of ~240×136 px — trivial).
- **P5 apply** (fullscreen): per pixel, trilinear-sample atlas C at (uv, pixel view depth) →
  `scene = scene·T + L`. NOTE this multiplies scene by transmittance (fog occludes), unlike the
  additive-only per-cone path — this is correct participating-media compositing and is the second
  look upgrade. Hero cones composite additively after, as today.

## Hero split
- Extend the existing per-projector override system (`VolumetricShaftOverride`, right-click
  capture/clear UI) with a **Hero** flag. When froxel is ON: hero-flagged cones render via the
  existing per-cone march (sharp bars, Batch A/B optimizations); all other volumetric-flagged
  projectors inject into P2 and STOP rendering per-cone. When froxel is OFF: everything per-cone,
  exactly as shipped.
- The march's density should then use flat 1.0 (its fog/noise migrated to P1) — gate that change
  on froxel-on so froxel-off keeps the current in-cone media levers working.

## Math the implementer must get right (anchors)
1. Exponential slice depth: `z_i = near · (far/near)^(i/Z)`; slice thickness `d_i = z_{i+1}−z_i`.
2. Hillaire integral above — NOT point-sampled in-scatter × d (that bands).
3. Beer–Lambert front-to-back with per-slice `e^(−σt·d)`.
4. HG phase per froxel per light with view dir from camera to froxel center.
5. Froxel world-space temporal reprojection + neighborhood clamp.
6. Manual trilinear atlas sampling (clamp tile edges — half-texel inset to prevent tile bleed).

## Settings (all new, Persist 1; master default OFF)
`BDMergeFroxelVolumetrics` (Bool, OFF) · `BDMergeFroxelGridX/Y/Z` (U32 240/136/64) ·
`BDMergeFroxelFar` (F32 128) · `BDMergeFroxelMaxLights` (U32 8) · `BDMergeFroxelTemporalBlend`
(F32 0.9) · `BDMergeFroxelDensity/FogStrength/NoiseStrength/...` (migrated media levers) ·
`BDMergeFroxelAmbient` (F32 0, the air-floor idea lands here naturally).

## Worker batches (Opus-tier, sequential builds, user A/B between each)
- **F0 — plumbing + debug view:** atlas RT pair alloc/release (gated), `froxelUtil` helper
  (atlas↔froxel coords, trilinear, exp-Z), P1 media pass only, and a DEBUG fullscreen slice/fog
  visualizer (like velocityDebug) so the user can SEE the grid before any lighting exists.
  Checkpoint: debug view shows sane fog density in the grid; off = byte-identical.
- **F1 — integrate + apply:** P4 + P5 with media-only (ambient-lit fog): uniform gray in-scatter.
  Checkpoint: scene fogs correctly with distance, transmittance occludes, no tile seams.
- **F2 — light injection:** P2 with cookie+shadow+phase; per-light uniform arrays; hero flag
  excludes. Checkpoint: club scene, 6 projectors, one flat cost; beams visible in the medium.
- **F3 — temporal:** P3 reprojection+clamp+jitter. Checkpoint: no shimmer static, no ghost moving.
- **F4 — hero polish + migration:** hero-flag UI on the override panel, per-cone media flattening
  when froxel-on, perf pass (GPU zones per froxel pass), docs. Checkpoint: hero beam sharp bars +
  froxel fill in one shot — the reference-frame look.
Each batch: standard worker packet from `BDMERGE_PROJVOL_NEXTGEN_BRIEF.md` (build cmd, lockstep
warning, default-no-op proof, STAGING of changed shaders/settings + grep verification, report
format). New-shader gotcha applies HARD here (F0 adds files): register in llviewershadermgr,
cmake reconfigure, verify staged, or the viewer fatals on startup.

## Honest expectations
- Froxel beams are SOFT by design (grid resolution). That's what heroes are for.
- P2's per-froxel shadow sampling reuses the 6 shadowed spot slots; >6 shadow-casting volumetric
  lights fall back to unshadowed injection (same limit as today's lighting).
- The win condition: a 6-projector club scene at flat, deterministic cost + one hero shaft with
  crisp bars + one unified atmosphere — while recording.

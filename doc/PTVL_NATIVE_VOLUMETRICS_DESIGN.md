# Native Path-Traced Volumetric Light: Diagnosis and Design (v3.1)

**Status:** DESIGN ONLY. Written 2026-10-07 on branch `fix/animesh-clone-pose-polish`.
No source, shader or settings file has been changed.

**Goal:** get the look of Marty McFly's PTVL ReShade shader inside the viewer, then go
further where native data allows: off-screen lights, real shadow maps, exact motion
vectors, the pre-tonemap HDR scene, and the light rig.

**Reference:** `C:/Users/xianw/cxs/ref/MartysMods_PTVL_v4.fx` is proprietary. This doc
paraphrases its ideas only. No code, constant tables or assets from it may be copied.
The math here (HG CDF, equi-angular sampling, closed-form height fog) is public textbook
material.

**Review history:**
- v1 was reviewed adversarially by Codex (`C:/Users/xianw/cxs/vollight_review.txt`).
  Verdict: NO-GO; direction viable; froxel diagnosis confirmed.
- v2 was re-reviewed (`C:/Users/xianw/cxs/vollight_review2.txt`). Verdict: NO-GO, with
  most v1 findings resolved. The reviewer accepted the v2 disagreement on pre-water
  alpha, and confirmed there is no same-frame `mWaterDis` collision in the main-view
  order.
- v3 addresses every v2 re-review finding (see §V3). The v2 table is kept below it as
  history; rows superseded by v3 are marked.

**Line numbers:** `pipeline.cpp` is under concurrent edit. Lines shifted by +18 between v1
and v2 for everything after about line 7700. v2 and v3 cite **current** lines plus a
named anchor. Diagnosis citations (§1) use the v1 read and the same named anchors, so
search by name if a line moved.

---

## V3.1. Changes from v3 (Codex round 3)

Round-3 verdict: **GO-WITH-FIXES, no P0** (`C:/Users/xianw/cxs/vollight_review3.txt`). Every
finding is accepted; none is rejected.

| Finding | v3.1 resolution | Where |
|---|---|---|
| **P1-1** Split-velocity state contract | Both halves enter only via `mVelocityMap.bindTarget()` and leave only via `flush()`, so `LLRenderTarget` bookkeeping stays intact and raw FBO restores are forbidden. Canonical velocity state (blend, colour mask, depth, stencil, cull/front-face, polygon mode/offset, scissor, VAO/VBO via `setBuffer`) is set explicitly in the split halves **and** the unsplit path. Its values equal the state observed at today's call site, so the unsplit output is unchanged. The guard now covers the full cached and raw state list. `D_gen` copy and compare use `texelFetch` at identical integer coordinates on both textures, with no linearization and exact equality. V1 gains GL/`LLRenderTarget` state snapshots at four points, plus an unsplit-unchanged check. | §3.2, §6.0 V1 |
| **P1-2** Bisection count | Terminate only when the τ residual ≤ 1e-4 **and** half-bracket ≤ 1e-4·ℓ; cap 14 iterations (13 already gives ≈6.1e-5·ℓ). | §3.4 |
| **P1-3** Ambient coupled to Screen glow | Two accumulators: `incoming = ScreenAmount·screen_owned + AmbientAmount·probe_fill`, then `S_screen = event_weight·incoming`. The probe fill is never scaled by Screen glow. | §3.5(b), §3.6 step 7, §3.10 |
| **P1-4** Snapshot release list | Every VolLight-owned interactive allocation is enumerated and released before any snapshot target exists, with a registry assertion. The user-owned velocity target is never released. | §3.10 |
| **P2-1** Stale pass-graph ambient wording | `Gs` now reads "screen trace + remaining-sector probe fill (two accumulators); no separate ambient × sky-visibility term". | §3.12 |
| **P2-2** UI visibility formula | `3.912/(VolLightDensity·lum(c_rgb))`, shown as "∞ (no medium)" when the denominator is < 1e-6. | §5 |
| VRAM applicability | `BDMergeVelocityBuffer` is already on for ReShade, so **≈262 MB at 4K Cinema** applies. 295 MB applies only to installs without velocity or motion blur. | §4.4 |

---

## V3. Changes from v2 (one entry per re-review finding)

| Finding | v3 resolution | Where |
|---|---|---|
| **P0-N1** Moving velocity before `renderGeomPostDeferred` breaks its depth contract | **Accepted.** The v2 move is withdrawn. v3 uses a **split velocity pass**, which is Codex option (a) refined so the consumer buffer stays bit-identical (details below the table). Option (b) (camera-only plus previous-frame velocity) is kept as a fallback mode. | §3.2, §3.8.1, §6.0 V1 |
| **P1-N1** Chromatic coefficients inconsistent | **Accepted.**<br>• `σt_rgb = σ·(albedo_rgb + a·(1−tint_rgb))`, so `c_rgb = albedo_rgb + a·(1−tint_rgb)`.<br>• `ĉ = lum(c_rgb)`.<br>• The proposal and importance ratio are both derived from that same `σt`.<br>• Guard: `ĉ < 1e-6` means no medium (L = 0, T = 1).<br>• `σ` is defined as the extinction scale, and the visibility readout uses `σ·ĉ`. | §3.3, §3.4 |
| **P1-N2** Inverse Taylor branch too coarse for 1e-4 | **Accepted.**<br>• The `−log1p(−x)/(ku)` form is kept for all `|x| ≥ 1e-3`, where `x = τ*·k·u/A`.<br>• Below that, a third-order series: `Δs = (τ*/A)(1 + x/2 + x²/3 + x³/4)`.<br>• Every inverse is then validated by evaluating `τ(Δs)`, and bisection runs whenever the relative residual exceeds 1e-4. *(Iteration bound superseded by V3.1 P1-2.)* | §3.4, §6.0 |
| **P1-N3** `mWaterDis` is `GL_RGBA` without HDR | **Accepted.**<br>• VolLight **requires the HDR path** (`RenderHDREnabled` && GL > 4.05, the same predicate as `pipeline.cpp:1295`) and fails closed otherwise, with a UI note.<br>• At enable and after every screen-buffer reallocation, the composite target is checked for completeness and an RGBA16F internal format.<br>• If the check fails, a dedicated RGBA16F composite target is allocated through the ladder. | §3.9, §4.4 |
| **P1-N4** `gSnapshot` cannot delimit snapshot lifetime | **Accepted.**<br>• A new RAII `ALVolLightSnapshotScope` is built in `LLViewerWindow::rawSnapshot`, after the buffer-reallocation decision and **outside the subimage loops**.<br>• It receives the full output dimensions, `scale_factor` and buffer offsets.<br>• `setSubfield(subfield, tile origin)` is called before each `display()`.<br>• On exit it **always** releases transient targets and bumps the reset generation **exactly once**.<br>• `gSnapshot` stays an inner render gate only.<br>• The §3.8/§3.10 contradiction is removed. | §3.8.1, §3.10 |
| **P1-N5** Invalid acceptance references | **Accepted.**<br>• **S6:** horizontal in-slab rays, plus a per-pixel clipped-segment reference for every other ray.<br>• **S8:** checked against an offline angular-and-path integral with the same slab and ownership masks, plus a closed-form uniform-dome total check.<br>• **Tiles:** compared against an untiled deterministic reference at identical pixel coordinates.<br>• **S7/S8:** frames, crop statistic, run count and 95 % CI are specified. | §6.0 |
| **VRAM** Velocity and snapshot peak missing | **Accepted.**<br>• The forced RG16F velocity map (+33.2 MB) and the new full-res `D_gen` depth copy (+33.2 MB, needed by the split velocity and the composite) are added.<br>• Cinema at 4K is now **≈295 MB** worst case, or **≈262 MB** if velocity is already on for motion blur or ReShade.<br>• Snapshot peak is about **29 B per snapshot pixel** after releasing the interactive targets: ≈245 MB at 4K, ≈975 MB at 8K, with a ladder. | §4.4 |
| *Self-found while writing v3* | v2 counted ambient twice: a step-6 probe fill **and** `ambient × sky-visibility`. v3 keeps only the step-6 fill, so the S8a closed-form check can pass. | §3.5(b), §3.6 |

**Rejected findings:** none.

**How the split velocity pass works (P0-N1):**
1. `renderGeomVelocity(range)` gains a pool-type range.
2. **At hook G**, inside the pool loop at the `POOL_ALPHA_PRE_WATER` boundary, it runs the
   camera fallback plus the velocity passes of every pool with type < `POOL_ALPHA_PRE_WATER`.
   - At that boundary every depth-writing opaque-forward, fullbright and masked pool has
     already rendered.
   - It runs under a full GL state guard, at the same kind of boundary
     `doAtmospherics`/`doWaterHaze` already use.
   - It is not called from inside another pool's render.
3. The raw depth at hook G is copied to `D_gen` (R32F).
4. **At the original late site** (`pipeline.cpp:26302`), a fix-up pass rewrites
   camera-fallback velocity **only** where final depth ≠ `D_gen` (water surface,
   depth-writing alpha). Then it runs the velocity passes of the pools with type ≥
   `POOL_ALPHA_PRE_WATER`.

Each pool's velocity passes still run **exactly once**, in the same type order, depth-tested
against the same final depth wherever it matters. The buffer that motion blur and
ReShade read is therefore designed to be **bit-identical** to today's. That is the
blocking acceptance test V1; on any mismatch, `VolLightMotionSource` falls back to (b).

**Remaining assumption (verified by V1):** depth only moves nearer after hook G. Any
pass that writes depth farther would break bit-identity, and V1 would catch it.

---

## V2. Changes from v1 (history; rows superseded by V3 are marked)

| Finding | v2 resolution | Where |
|---|---|---|
| **P0-1** Atmospherics hook is not before all alpha | **Accepted.** VolLight is split in two:<br>• **Generation** (captures, all lighting, histories, `L_full`) runs at a new hook just before the first `POOL_ALPHA_PRE_WATER` draw, before `doWaterHaze()`.<br>• **Opaque composite** stays at the atmospherics hook, right after `doAtmospherics()`.<br>Water and pre-water-alpha emitter policy is stated. Pre-water alpha never calls the forward hook. The camera-underwater case is excluded in v1. | §3.2 |
| **P0-2** Velocity units ×2, and re-entrant hoist | **Accepted.** *(The relocation is superseded by V3 P0-N1's split velocity; the units fix stands.)*<br>• `prev_uv = uv − 0.5·velocity_ndc`.<br>• `renderGeomVelocity` moves to **immediately before** `renderGeomPostDeferred` (`pipeline.cpp:25894`), inside a full GL/target state guard.<br>• The original call (`pipeline.cpp:26302-26306`) is **skipped** that frame, so it never runs twice.<br>• VolLight forces the velocity buffer to be allocated. | §3.2, §3.8.1 |
| **P0-3** Bitfield update inverted; shift by 32 | **Accepted.** `new = visible & interval; visible &= ~interval`, with branch-safe 64-bit interval masks and no shift of 32 or more. | §3.6 |
| **P0-4** Infinite sun path in uniform fog | **Accepted.**<br>• The medium is now a **bounded slab**: flat below the floor, exponential above it, and zero above `VolLightTopZ`.<br>• Every light path is clipped to the slab top and to `VolLightMaxLightPath`.<br>• `k→0`, `u≤0`, near-horizontal rays and no-intersection cases are handled explicitly. | §3.3 |
| **P1-1** Boundary guards; Cine Haze `k` vs `H` | **Accepted.**<br>• `t_a = max(t_start, near)`, early-out when `D ≤ t_a`.<br>• Clamp `τ*` to `[0, τ(D)]`; use `log1p`/`expm1` forms; guard the log argument.<br>• `P` is renamed to "extinction-event probability under the scalar proposal".<br>• Import converts Cine Haze `k` (1/m) to `H = 1/k`. | §3.3, §3.4 |
| **P1-2** Probe and screen units | **Accepted.** Ambient is a **calibrated artistic approximation**: a two-sided irradiance/π blend weighted by the HG forward/back mass. The screen term is a **Lambertian-emitter approximation** with its own calibration test. No global radiometric-consistency claim is made. | §3.5(b), §3.6 |
| **P1-3** Energy overlap beyond the sun disc | **Accepted.** An explicit ownership policy, per-term debug views, and the S7/S8 calibration scenes with numeric tolerances. | §3.7 |
| **P1-4** Temporal metadata and resets | **Accepted.**<br>• Meta ping-pong target carries previous depth and normal; history length lives in history alpha.<br>• Ambient moves into the screen signal, so the analytic history no longer depends on noisy sky visibility.<br>• Light changes are reactive for **both** histories; medium changes hard-reset both.<br>• The Hero rotating 2×2 sample is **dropped**.<br>• One monotonic `mVolLightResetGen` feeds both histories. | §3.8.1 |
| **P1-5** Pass and tap budget understated | **Accepted.**<br>• Pass counts per tier are explicit after merging passes.<br>• Taps per traced pixel are explicit.<br>• Practical visibility is computed **once per light per pixel**, only for the top-4 (Cinema) or top-8 (Hero) lights.<br>• Per-pass ms figures are **provisional** until the WP2/WP3/WP5 microbenchmarks; tier gates freeze at a budget checkpoint. | §4 |
| **P1-6** VRAM | *(Figures superseded by V3.)* **Accepted, with a higher figure than suggested.** The full enumeration gives **≈230 MB at 4K Cinema** (≈240 Hero, ≈120 Draft). An allocation-failure ladder is added. | §4.4 |
| **P1-7** Texture units | **Accepted.** A dedicated `vollightSpotF` permutation has exactly **one** `sampler2DShadow` and one shadow matrix. The ten-way `SPOT_SHADOW` dispatch is not reused. | §3.5(c) |
| **P1-8** Chromatic composite, MRT, metering | **Accepted.**<br>• The composite is a scratch pass into **`mWaterDis`** (an existing RGBA16F scratch) reading the scene, then `copyColorToAttachment0` back. This is the Roto Ink/lens idiom (`pipeline.cpp:18152`, `18688`).<br>• Attachment 0 only, inside the existing sidecar guard (`pipeline.cpp:7914`).<br>• Exact RGB transmittance; its bandwidth is budgeted.<br>• The "Include in metering" toggle is **removed**: always metered, like a real camera. | §3.9, §5 |
| **P1-9** Snapshots | *(Lifetime superseded by V3 P1-N4.)* **Accepted.** An explicit snapshot render scope keyed on `gSnapshot`:<br>• never reads or mutates interactive history;<br>• full-image pixel seeding;<br>• deterministic N=16 in-frame accumulation;<br>• bumps the reset generation afterwards;<br>• numeric noise acceptance. | §3.10 |
| **P1-10** Depth and radiance reduction coupling | **Accepted.** Each pyramid level keeps the **lower-median source sample** (second-smallest view z of 4) and **carries that sample's radiance**. Min z is stored separately. Radiance is never averaged across depth layers. | §3.6 |
| **P2-1** ReShade sees VolLight | **Accepted.** Stated. The pre-fog option is costed: it needs a dedicated capture plus a bridge-selection bit, and RTGI G-buffer semantics stay opaque-only. | §3.11 |
| **P2-2** VCam parity | **Accepted.** An **explicit v1 limitation**, plus a user decision (§8). A history-free analytic variant is available as WP4b if parity is required. | §3.11, §6 |
| **P2-3** Actor-only coverage | **Accepted.** The registry test applies to spot passes, practical batches and screen-emitter suppression. Stated behaviour: actor-only lights produce **no haze anywhere, including over the actor**. | §3.5 |
| **P2-4** "No compute" wording | **Accepted.** Reworded. | §3.1 |
| **P2-5** Non-measurable acceptance | **Accepted.** Test scenes S1-S8, camera tracks T1-T5, numeric metrics, and the Tracy procedure. The subjective look sign-off is **separate** from engineering gates. A sub-4-px gobo bar is explicitly not guaranteed. | §6.0 |
| Verification notes on R3 / R12 | **Accepted.**<br>• R3 now says "17.5× coefficient ratio, directional evidence, not a pixel ratio".<br>• R12 notes that local fog volumes can tint. | §1 |

**Rejected findings:** none.

**Partial disagreements** (do not change the fix):
- **P0-1, water.** The review implies pre-water alpha would need VolLight data. With the
  camera above water, `POOL_ALPHA_PRE_WATER` holds objects on the **far side of the water
  plane** (`lldrawpoolalpha.cpp:172-175`: `water_sign = −1`). Their air path is owned by
  the opaque composite at the water-surface depth, exactly as the WindLight haze pass
  already handles it. So the earlier generation hook is adopted for robustness and for
  the emitter policy, not because those fragments consume `L_full`.
- **P1-6.** The suggested "180-200 MB" is itself low once every side target is counted.
  v2 uses ≈230 MB. The re-review confirmed this for the listed rows. v3 adds velocity and
  `D_gen`.

---

## 0. TL;DR

- **Why Voxel Air looks bad.**
  - The froxel air is lit by a **constant grey ambient (0.35)**. Sun and moon never light
    it.
  - It is composited **after** exposure and bloom.
  - Its medium coefficient is **17.5x lower** than the per-cone shafts. Beams fade
    unless density is raised, and raising density greys the frame, sky included.
  - Its temporal pass smears on zooms and cuts. Its medium noise aliases at froxel
    resolution.
- **Recommendation.** Retire the froxel grid. Build a **per-pixel forced-single-scatter
  estimator**, traced at half resolution. It has two signals:
  - analytic lights with real shadows: sun/moon CSM, single-shadow spot passes, Cine Rig,
    and practicals with cached screen-space visibility;
  - a PTVL-style **screen-radiance slice trace** with a 64-sector HG-CDF horizon bitfield.
  
  Both are accumulated with exact motion vectors, then depth-aware upsampled. Generation
  runs **before any alpha**, and the opaque composite at the atmospherics hook (**before
  metering and bloom**). Post-water transparents use a forward hook.
- **Budget** for the default Cinema tier on an RTX 5090: **≈0.75 ms at 1080p, ≈1.1 ms at
  1440p, ≈2.2 ms at 4K**, including the velocity fix-up. These are provisional until the
  microbenchmarks.
- **VRAM** is **≈262 MB at 4K for this user**, whose velocity buffer is already on for
  ReShade. It is ≈295 MB on installs without it.
  Snapshots peak at about 29 B per snapshot pixel.

---

## 1. Diagnosis: why the froxel system looks bad

The root causes are ranked. Codex confirmed them independently, with the caveats folded
in below.

### R1. The air is lit by a constant grey
- `froxelIntegrateF.glsl:117-121` sets the baseline source to
  `S_j = froxel_ambient * sigma_s`. It is scalar, uncoloured, has no phase, and ignores
  time of day.
- Default `BDMergeFroxelAmbient` = 0.35 (`settings.xml:19590-19599`).
- **Sees:** a milky veil, the same at noon and at midnight.

### R2. Sun, moon and most lights are absent from the medium
- Injection iterates only shadow-slot projectors that carry the volumetric flag.
  Actor-only and hero projectors are skipped (`renderFroxelVolumetrics`, `pipeline.cpp`
  ~20180-20262 in the v1 read).
- Sun shafts come from the unrelated, display-referred G3.2 pass (`renderVolumetric`
  call after `colorCorrect`). Its own comment calls them "craptacular rays"
  (`volumetricLightF.glsl:76`).

### R3. Beam visibility and the veil share one σs, at a 17.5× lower coefficient
- Per-cone shafts use `PROJVOL_SCATTER` 0.35 × density 1
  (`projectorVolumetricF.glsl:247,724`).
- The froxel path uses σs from density 0.02/m (`settings.xml:19467`,
  `froxelInjectF.glsl:237,244`).
- The coefficient ratio is 17.5. This is **directional evidence, not a guaranteed pixel
  ratio**: the two paths integrate differently. The shader comment concedes beams are
  "faint by design" (`froxelInjectF.glsl:38-42`).
- Raising density restores beams but greys everything through R1.

### R4. Sky and far field form a fog wall
- Sky and no-depth pixels clamp to `froxel_far` = 128 m (`froxelApplyF.glsl:61-70`).
- About 92 % of the sky becomes grey at the defaults, and everything beyond 128 m is
  fogged identically.

### R5. Composite after metering and bloom
- Luminance, exposure and bloom are generated before `renderFroxelVolumetrics`. Current
  lines: `generateLuminance` at `pipeline.cpp:22466`, froxel at `22572`.
- **Sees:** a lifted image, and bloom pasted over the fog.

### R6. Temporal smears on zoom and cut, and flickers at shadow edges
- The previous grid is reconstructed with the **current** `froxel_tan_half_fov`
  (`froxelTemporalF.glsl:117-132`). Invalidation checks only grid, near and far.
- There is no cut reset.
- Jitter spans the whole froxel, up to 13 m deep (`froxelInjectF.glsl:162-170`). The
  clamp is built from six noisy neighbours (`froxelTemporalF.glsl:156-165`).

### R7. 8×8-pixel cells with a non-depth-aware apply
- Grid 240×136 (`settings.xml:19414-19434`). Screen-XY trilinear sampling with no edge
  test (`froxelApplyF.glsl:72-79`).
- **Sees:** halos on silhouettes and stair-stepped gobos.

### R8. Medium noise aliases
- Noise is evaluated only at froxel centres, with three octaves and no medium temporal
  filter (`froxelMediaF.glsl:160-183`, `froxelUtil.glsl:148`).
- The mechanism is certain; how bad it looks is an inference.

### R9. Wasted near slices
- Exp-Z starts at the 0.25 m near plane (`llcamera.h:38`). About 14 of 64 slices sit
  below 1 m.

### R10. The fog floor follows the avatar every frame
- Default mode 1 (`settings.xml:19513-19522`) adds `gAgent.getPositionAgent().z` each
  frame.

### R11. Alpha is fogged at background depth
- The apply runs after forward geometry using opaque depth.

### R12. Turning Voxel Air on kills Depth Haze; global air has albedo 1
- Cine Haze is suppressed by the froxel master (`alcinehaze.cpp:78-87`).
- Global albedo is fixed at 1 with no absorption (`froxelMediaF.glsl:194-196`). Local fog
  volumes *can* add tint, so "grey only" applies to the global air.
- Hero beams are additive, have no transmittance, and use the per-cone coefficient.

**Not root causes:**
- The Hillaire slice integral, inset atlas sampling and first-frame NaN guard are
  correct.
- Cost is not the complaint.

---

## 2. What PTVL does, and where native beats it

**PTVL's ideas, paraphrased:**
- It uses a half-res HDR radiance copy with mips as the emitter set.
- It forces one scatter event per pixel, exponentially distributed in σs and truncated at
  the surface.
- It marches one screen slice against a depth mip pyramid. Occluder angular intervals
  [front, front+thickness] are mapped through an HG (g=0.75) CDF into 64 sectors. Newly
  occluded sectors add the tap's radiance. Leftover sectors are extrapolated from the
  energy already seen.
- Absorption is analytic and tints toward a colour.
- A per-8×8-tile learned slice-direction PDF is built with compute atomics.
- The final blend is `lerp(unscattered, scattered, p_scatter)`.
- It accumulates only while a key is held.

**Native advantages:**

| PTVL limit | Native answer |
|---|---|
| Only on-screen light | CSM-shadowed sun/moon; single-shadow spot passes with cookies (spots, projectors, Cine Rig); practicals with cached screen-space visibility |
| HDR guessed from LDR | Real pre-tonemap `mRT->screen` (RGBA16F) |
| No motion vectors | `mVelocityMap` NDC deltas (rigid, rigged, avatar, camera fallback), disocclusion rejection, one reset generation |
| Uniform medium | Bounded slab: flat ground fog, exponential falloff, top, distance start, tinted absorption, local volumes, world-anchored dust |
| Unknown sectors extrapolated | Filled from probe/sky (approximation; §3.5b) |
| Sun disc counted twice | Masked; ownership policy (§3.7) |
| Alpha fogged at background depth | Pre-alpha generation, opaque composite, forward hook |
| Full-res compute | Half-res fragment trace with bilateral upsample |

---

## 3. Architecture

**Names:**
- Module: `ALVolLight` (`alvollight.{h,cpp}`).
- Shaders: `class3/deferred/vollight*.glsl` and `class1/deferred/vollightUtil.glsl`.
- Settings: `VolLight*`.

### 3.1 Constraints, checked against the tree
- **Compute.** The viewer loads GL compute symbols, including `glDispatchCompute`
  (`llgl.cpp:2202`). It has **no implemented compute shader, program or resource path**:
  `llshadermgr` only classifies vertex and fragment objects. Adding one is out of scope,
  so every pass is a fragment pass into an `LLRenderTarget`.
- **GLSL version.** 400/420 is available (`llshadermgr.cpp:644-648`), so `uint`,
  `bitCount`, `findLSB` and `findMSB` are usable. 64-bit masks are `uvec2`.
- **Shadows.** Sun is 4 CSM cascades (`shadowMap0..3`); spot slots are `shadowMap4..13`
  (`shadowUtil.glsl:28-60`). All are `sampler2DShadow`.
- **Cine Rig lights** are viewer-side `LLVOVolume` projectors and omnis
  (`alcinelightrig.cpp:1600-1644,2065-2150`). They take spot-shadow slots via
  `updateAutoShadowSlots`.
- **Velocity map.** It is allocated only when `BDMergeVelocityBuffer || BDMergeMotionBlur`
  (`pipeline.cpp` ~1550). **VolLight-on must join that predicate** and trigger the
  buffer realloc. This costs +33.2 MB at 4K (§4.4).
- **HDR.** VolLight requires the HDR path: `RenderHDREnabled` && GL > 4.05, as at
  `pipeline.cpp:1295`. It fails closed otherwise.

### 3.2 Frame placement: two hooks and split velocity (fixes P0-1, P0-2, P0-N1)

**Pool order inside `renderGeomPostDeferred`, camera above water**
(`pipeline.cpp:7924-7936`, `lldrawpool.h:59-79`):
1. `POOL_WATEREXCLUSION`, which runs `doWaterExclusionMask`.
2. `POOL_WL_SKY`, then the opaque-forward pools, then `POOL_GLOW`.
3. **[hook G]**.
4. `doWaterHaze()` at `POOL_ALPHA_PRE_WATER`.
5. Pre-water alpha (far side of the water plane, `water_sign=−1`).
6. Void water, then `POOL_WATER`.
7. **[hook C]**.
8. `doAtmospherics()` at `POOL_ALPHA_POST_WATER`.
9. Post-water alpha.

**Split velocity (v3; replaces the v2 "move before `renderGeomPostDeferred`").**

*Why v2 was wrong.* `renderGeomVelocity` relies on the **final opaque depth**:
- its camera fallback reconstructs from depth;
- its geometry stamps depth-test without writing depth (`pipeline.cpp:7754-7816`, current
  ~7772-7834).

Opaque-forward, fullbright and masked pools render *inside* `renderGeomPostDeferred`, so
running velocity before that function sees incomplete depth.

*v3 design* (only when VolLight is active; otherwise today's single call is untouched):
1. **Pool-range parameter.** `renderGeomVelocity(U32 min_type, U32 max_type, bool
   camera_fallback, VelocityFixupMode)` iterates `mPools` filtered by type.
2. **Early part, at hook G,** inside the pool loop at the `POOL_ALPHA_PRE_WATER` boundary,
   before generation.
   - At this boundary every pool with type < `POOL_ALPHA_PRE_WATER` has rendered (sky,
     all opaque-forward, fullbright, masked, avatar, glow). The shared depth
     (`mRT->deferredScreen.shareDepthBuffer(mVelocityMap)`, `pipeline.cpp` ~1553) is
     therefore complete for every opaque surface.
   - Runs the camera fallback plus the velocity passes of pools with type <
     `POOL_ALPHA_PRE_WATER`.
   - **State contract (v3.1, P1-1).**
     - **Target bookkeeping.** Both halves (early and late) enter **only** through
       `mVelocityMap.bindTarget()` and leave **only** through `mVelocityMap.flush()`.
       That keeps `LLRenderTarget`'s `sBoundTarget`, `sCurFBO`, nested-target stack,
       read buffer and viewport consistent (`llrendertarget.cpp:534,620`). Raw
       `glBindFramebuffer` restores are forbidden. The outer `mRT->screen` binding of the
       pool loop is restored by that stack, not by hand.
     - **Canonical velocity state, set explicitly in both halves and in the unsplit
       path** rather than inherited from whichever pool ran last:
       - blend disabled;
       - colour mask RGBA on;
       - depth test as today (`LEQUAL`, no write; fallback pass depth-off);
       - stencil test disabled;
       - cull and front-face as today's call site;
       - polygon mode FILL;
       - polygon offset disabled;
       - scissor disabled;
       - VAO/VBO bound by `setBuffer` only.

       The canonical values are **the state observed at today's call site**, captured by
       the V1 instrumentation below. Setting them explicitly in the unsplit path is
       therefore a no-op for today's output, and V1 proves it.
     - **`LLVolLightGLStateGuard` (early half only)** saves and restores the pool loop's
       cached `LLGLState`/`gGL` state and the raw GL state:
       - read framebuffer and read buffer;
       - draw buffers;
       - viewport and scissor;
       - global and indexed colour masks (the loop's `LLScopedIndexedDrawBufferGuard`
         re-asserts attachment-1 masks; harmless on the single-attachment velocity FBO);
       - blend enable, func and equation;
       - depth test, func and mask;
       - stencil;
       - cull and front-face;
       - polygon mode and offset;
       - bound program;
       - bound VAO and array/element buffers;
       - `gGL` matrix modes and stacks, and `gGLLastMatrix`;
       - active texture unit and every unit's bindings.
   - This is the same kind of between-pools boundary `doAtmospherics`/`doWaterHaze` use.
     It is not inside any pool's render.
3. **`D_gen` copy (v3.1, exact-compare contract).** Right after the early part, a
   fullscreen pass copies the depth to `D_gen` (R32F, full-res):
   - the source depth texture is read with **`texelFetch(depthTex, ivec2(gl_FragCoord.xy), 0)`**,
     an integer coordinate with no filtering, no linearization and no reconstruction;
   - the value is stored unchanged.
   
   The late fix-up compares `texelFetch(depthTex, p, 0).r` with `texelFetch(D_gen, p, 0).r`
   at the **same integer coordinate `p`**, using exact float equality. Both reads return
   the same normalized value for an unchanged D24/D32F texel, so equality is exact
   wherever depth did not change. The composite (§3.9) reads the same `D_gen`.
4. **Late part, at the original site** (`pipeline.cpp:26302-26306`), when VolLight is
   active:
   - **(i) Fix-up.** A fullscreen pass writes camera-fallback velocity from the **final**
     depth **only** where `final_depth != D_gen` (water surface, depth-writing alpha).
   - **(ii)** Velocity passes of pools with type ≥ `POOL_ALPHA_PRE_WATER`, depth-tested
     against final depth, as today.
   - `gGLLast*` is still snapshotted after this site (`pipeline.cpp:26308-26319`), so both
     parts see the previous frame's matrices.

*Equivalence argument.*
- Where depth did not change after hook G, the early part does exactly what the original
  did for those pools, and the late pools do the same as before.
- Where depth moved nearer, the original's early-pool stamps would have failed against
  the nearer depth. The fix-up restores the camera value from final depth, then the late
  pools stamp as before.
- Pool order is preserved and each velocity pass runs once.
- This assumes depth only moves nearer after hook G (true for depth-test-LEQUAL writers).
  Test V1 verifies it: the consumer buffer must be **bit-identical** to the unsplit pass
  on all test tracks.

*Cost.* Same geometry work as today, plus one full-res fix-up pass and one depth copy
(≈0.05-0.1 ms at 4K).

*Fallback, option (b): `VolLightMotionSource = CameraOnly`.* The late velocity pass stays
completely untouched. VolLight reprojects with camera-only motion of its own sample
position, plus an object-motion estimate from the **previous** frame's velocity, sampled
by a 2-step fixed-point lookup (`v ≈ vel_prev(uv − 0.5v)`).
- Cost: zero extra passes.
- Correctness: exact for the static world; one-frame-lag extrapolation for actors, so
  more rejection and noise on fast limbs.
- Used automatically if V1 fails, or if a pool's velocity pass proves order-dependent.

*VolLight's own reprojection.* For water-clipped samples (§3.2 water policy), VolLight
uses the camera reprojection of its own clipped position rather than the texel velocity.

**Hook G: generation.** It is inserted at `cur_type >= POOL_ALPHA_PRE_WATER &&
!done_vollight_gen`, before `doWaterHaze()` (`pipeline.cpp:8068`), and runs every
VolLight pass except the composite:
- radiance and depth capture;
- the depth/radiance pyramid;
- analytic passes, practicals and the screen trace;
- prefilter and temporal;
- the upsample to `L_full`.

At that moment the frame contains opaque surfaces, sky, opaque-forward and glow, but **no
water, no WindLight haze and no alpha**. That is the right emitter set: the haze glow is
never re-emitted.

**Water policy at hook G.**
- The air ray is clipped analytically to the water plane:
  `D_air = min(D_opaque, t_waterplane)` when the camera is above water. Pixels flagged by
  the water-exclusion mask are not clipped.
- Below-water pixels in the capture get depth = water-plane hit and radiance = 0. They
  are **non-emitting occluders**.
- So the current-frame **water surface and pre-water alpha are not screen emitters**.
  This is a stated v1 look limitation: there is no screen glow from lakes. A later option
  is to use the previous frame's pre-composite capture for those pixels.

**Hook C: opaque composite.** It is inserted right after `doAtmospherics()`
(`pipeline.cpp:8062`).
- It writes `scene·T_rgb(D) + L_full` using `D` from the depth buffer, which now includes
  the water surface. This is consistent with the generation clip.
- Where `D_now < D_gen` (any surface added between the hooks that writes depth), `L` is
  scaled by `P(D_now)/P(D_gen)`. `D_gen` is the full-res R32F copy taken at hook G.

**Forward hook.**
- Post-water alpha calls `volLightApplyForward` (§3.9).
- Pre-water alpha **never** calls it. Its air path is the composite at water-surface
  depth, as with the WindLight haze.

**Camera under water** (`sUnderWaterRender`; atmospherics moves to `POOL_WATER`): VolLight
is **off** in v1. Water fog owns the look. History resets on the transition.

**Gating:**
- Main view only: `mRT == &mMainRT && !gCubeSnapshot && !sPrismLensRender &&
  !sUnderWaterRender && !sRenderingHUDs`.
- The snapshot scope is handled separately (§3.10).
- A Prism/aux state guard follows the `ProjVolAuxStateGuard` pattern.

### 3.3 Medium model: bounded slab (fixes P0-4, P1-1)

**Height** is `h = world z`. The floor `h_f` comes from `VolLightFloorMode`:
- World Z, the default; or
- latched avatar feet, set by the "Set floor here" button and never updated per frame.

The top is `h_top = h_f + VolLightTopHeight` (default 40 m). The UI field is "Falloff
height" `H` (m), with `k = 1/H`; `H = 0` means uniform, i.e. `k = 0`.

**Density:**

| Height band | Density | Description |
|---|---|---|
| `h ≤ h_f` | `σ(h) = σ0` | flat ground fog; no unbounded growth below the floor |
| `h_f < h < h_top` | `σ(h) = σ0·e^(−k(h−h_f))` | exponential falloff |
| `h ≥ h_top` | `σ(h) = 0` | above the slab |

**Coefficients (v3, one consistent parameterization).** `σ(h)` is the **extinction scale**
and `τ` is its optical depth.
- `σs_rgb = σ·albedo_rgb`.
- `σa_rgb = σ·a·(1−tint_rgb)`.
- `σt_rgb = σs_rgb + σa_rgb = σ·c_rgb`, with **`c_rgb = albedo_rgb + a·(1−tint_rgb)`**.
- Every optical depth is a per-channel scale `c_rgb·τ` of the one scalar `τ`.
- **Guards:** `albedo_rgb, tint_rgb ∈ [0,1]`, `a ∈ [0,1]`. If `ĉ = lum(c_rgb) < 1e-6`, the
  medium is treated as absent (`L = 0`, `T = 1`). There is no division by albedo.
- **Visibility readout:** `3.912/(σ0·ĉ)` m.
- Default white albedo with `a = 0` gives `c = 1`, which keeps the v1/v2 meaning of
  density.

**Optical depth of a segment.** For any segment `[s0, s1]` along unit direction `w` with
world-up component `u = w·ẑ` (used for camera, sun and local-light paths alike):
1. Clip the segment to the slab. If `u > 0`, `s ≤ (h_top−h0)/u`. If `u ≤ 0` and
   `h0 ≥ h_top`, the segment either never enters (τ=0) or enters at `(h_top−h0)/u`.
2. Clip to `VolLightMaxLightPath` (light paths; default 128 m) or `VolLightMaxDist`
   (view; default 256 m). **Every segment is finite.**
3. Split at the floor crossing `s_f = (h_f−h0)/u`, if it lies inside the segment.
4. On the flat piece, `τ = σ0·len`.
5. On the exponential piece, from height `h_a` over length `ℓ`:
   `τ = σ0·e^(−k(h_a−h_f)) · ℓ · E(−k·u·ℓ)`, with `E(x) = expm1(x)/x`.
   - Use the Taylor branch when `|x| < 0.02` (the cineHaze form).
   - This covers `k→0` and near-horizontal rays exactly (`u = 0` gives `E = 1`).
   - Clamp exponents to ±60 as in `cineHazeF.glsl:52`.
6. **Sun path from a scatter point:**
   - `u_sun ≤ ε`: the sun is at or below the horizon. Use the full `MaxLightPath`
     segment, which is finite. The moon takes over via `sun_up_factor`.
   - Point already above `h_top`: `τ = 0`.

**Transmittance** is `T_rgb = exp(−c_rgb·τ)`.

**Dust.** The 64³ dust volume (`dust_volume_64_rgba8.ktx`) modulates σs **at scatter
points only**. Transmittance stays analytic. Drift uses the shared wind direction
(`bdmerge_dust_wind_dir`) on `LLPresentationTime`.

**Local volumes** (Atmos Volumes, WP8) are constant-density box/ellipsoid segments added
to `τ` and to the sampling mixture.

**Cine Haze import:**
- `H = 1/max(CineHazeHeightFalloff, 1e-4)` (Cine Haze stores `k` in 1/m;
  `alcinehaze.cpp:128,158`).
- Height disabled → `H = 0`.
- `h_f = CineHazeReferenceHeight`; density, start and colour copy directly.
- `MaxOpacity` and `Strength` have no physical analogue and are **not imported** (a UI
  note says so).
- Cine Haze's density *increases* below the reference; VolLight's flat floor is a
  deliberate difference.

### 3.4 Scatter-event sampling (fixes P1-1, P1-N1, P1-N2)

For the view ray:
- Set `t_a = max(VolLightStart, near)`. If `D_air ≤ t_a`, or `ĉ < 1e-6`, there is no
  in-scatter and `L = 0`.
- `ĉ = lum(c_rgb)`, using the §3.3 consistent `c_rgb`. `τD = τ(t_a, D_air)`, evaluated
  piecewise.
- **`P = −expm1(−ĉ·τD)`**: the extinction-event probability under the scalar proposal.
  It is not strictly the scattering probability.
- Draw `ξ`, from in-repo blue noise rotated per frame by R2.
- Compute `τ* = −log1p(−ξ·P)/ĉ`, then **clamp it to `[0, τD]`**. Find the piece containing
  `τ*` from the cumulative τ at the floor crossing.
- **Invert piecewise** (v3, P1-N2):
  - Flat piece: `Δs = τ*_local/σ0`, which is exact.
  - Exponential piece: let `A = σ0·e^(−k(h_a−h_f))` and `x = τ*_local·k·u/A`.
    - `|x| ≥ 1e-3`: `Δs = −log1p(−x)/(k·u)`. Clamp `−x` to `≥ −1 + 1e-7`, which matters
      for upward rays (`k·u > 0`) where `x → 1` near total optical depth.
    - `|x| < 1e-3` (includes `k → 0`, `u → 0`): use the third-order series
      `Δs = (τ*_local/A)·(1 + x/2 + x²/3 + x³/4)`. Truncation error is ≤ `x⁴/5 < 2e-13`
      relative.
  - **Validate every inverse.** Compute `r = |τ(t_a + Δs) − τ*|/max(τ*, 1e-6)`. If
    `r > 1e-4`, or `Δs` leaves the piece, bisect on `τ(s) − τ*` over the piece.
    - **Termination (v3.1):** stop only when **both** `r ≤ 1e-4` **and** half the
      remaining bracket is `≤ 1e-4·ℓ`. Return the bracket midpoint.
    - Hard cap of **14 iterations**. Half-bracket after 13 is `ℓ/2¹⁴ ≈ 6.1e-5·ℓ`, which
      satisfies the distance gate; the 14th is margin.
  - In practice the validation costs one extra `τ` evaluation per sample (ALU only).
- **Estimator.** Derived from the **same** `σt_rgb = σ·c_rgb`:
  - The proposal is `p(t) = ĉ·σ(t)·e^(−ĉτ(t))/P`.
  - The contribution is `σs_rgb(t)·T_rgb(t)·Ls = σ(t)·albedo_rgb·e^(−c_rgb·τ(t))·Ls`.
  - Weight = contribution/p = **`(P/ĉ)·albedo_rgb·e^(−(c_rgb − ĉ)·τ(t_s))`**, multiplied
    by `Ls(x_s)`.
  - When `c_rgb` is grey, the ratio term is 1.
- **Samples per pixel:** K stratified points for the analytic terms (Draft 2, Cinema 4,
  Hero 8); 1 point for the screen trace.

### 3.5 Analytic terms → `S_analytic` (direct delta lights only)

All radiances use the **surface chain**:
- **Sun:** `sunlit` from `calcAtmosphericVarsLinear` (as in `hazeF.glsl:74`) times the
  ALEnvIntensity Sun/Moon EV.
- **Local lights:** `getLightLinearColor() × AlchemyGlobalLightScale ×
  localLightEVScale`, as at `pipeline.cpp` ~20290-20298.

**Firefly clamp** per sample: `VolLightFireflyEV` stops over mid-grey, using **last
frame's** `mExposureMap`.

**(a) Base pass: sun/moon.** One half-res pass, MRT. It also writes the meta target
(`z`, oct-normal).
- Per sample: one CSM tap through `sampleDirectionalShadow`'s cascade select (hardware
  2×2 PCF), the sun-path `T` from §3.3, the HG phase with g plus `VolLightBackLobe`, and
  one dust tap.

**(b) Ambient, an artistic approximation.**
- `sampleProbeAmbient` returns **cosine-convolved irradiance** (`reflectionProbeF.glsl:661-691`),
  not radiance.
- v2 defines `L_amb ≈ [m_f·E(−v) + m_b·E(+v)]/π`. Here `E(n)` is probe irradiance at
  the pixel's mid-ray point and `m_f/m_b` are the HG mass in the forward/back
  hemispheres for the current g.
- Cost is 2 evaluations per **pixel**, not per sample. It is scaled by "Sky fill"
  (`VolLightAmbientAmount`) **only**, and never by "Screen glow" (v3.1; see §3.6 step 7).
- No radiometric-consistency claim is made; calibration test S8 bounds it.
- With the screen term on, ambient is evaluated **only** as the §3.6 step-6 fill of the
  unoccluded sectors: `Σ_runs (bits_in_run/64)·E(dir_run)/π`. The analytic signal
  therefore never depends on the noisy trace.
  - **v3 self-correction:** v2 applied *both* a step-6 probe fill and a separate
    `ambient × sky-visibility` term, which counted the unoccluded sectors twice. v3
    keeps one.
  - In the uniform dome (S8a) this makes screen plus ambient sum exactly to `L`.
- In Draft (no screen pass) it moves to the base pass with visibility 1, gated by "Leak
  guard".

**(c) Shadowed spots, projectors and Cine Rig.** One pass per light, scissored to the
light's screen rect and depth-bounded. Dedicated `vollightSpotF.glsl`:
- **Exactly one** `sampler2DShadow` (that light's slot target), one `mat4
  vollight_shadow_matrix`, and the spot bias/offset uniforms. Hardware PCF tap.
- One cookie sampler (`projectionMap`, via `setupSpotLightVolumetric`).
- Depth, normal, dust and blue noise.
- Total ≤ 7 units, well under the guaranteed 16.
- **Equi-angular sampling** on the ray segment clipped to the light's cone/sphere:
  - `Δ = (c−o)·v`, `d = max(|c−o−Δv|, 1e-3)`, `θ_{a,b} = atan((t_{a,b}−Δ)/d)`.
  - `t = Δ + d·tan(mix(θa, θb, ξ))`, `pdf = d/((θb−θa)(d²+(t−Δ)²))`.
- Per-projector `VolumetricShaftOverride` values (multiplier, g, feather, tint, max
  distance) apply.

**(d) Practicals.** One batched pass over unshadowed point and spot lights, uniform arrays
(Cinema ≤8, Hero ≤16), sorted on the CPU by projected contribution.
- **Visibility is computed once per light per pixel**, not per sample:
  - It is taken at the ray point of closest approach to the light, clamped to the
    segment.
  - It is a screen-space march of V taps toward the light against depth pyramid mip 1
    (Cinema V=6, Hero V=8).
  - Only the **top-4 (Cinema) or top-8 (Hero)** lights by unoccluded contribution at
    that pixel are traced. The rest use the "Leak guard" factor.
- Per-sample work is ALU only.

**Actor-only lights** (`ALCineLightRigManager::actorOnlyBits`) are excluded from (c), (d)
and screen-emitter suppression. Stated behaviour: they produce **no haze anywhere, even
over the actor**, consistent with "do not light the set".

### 3.6 Screen term → `S_screen` (emissive and bounce glow, plus ambient)

**Capture and pyramid** (fixes P1-10). Level 0 at half res comes from full res:
- The source is each 2×2 quad's **lower-median sample**: the second-smallest view z of
  the 4, with ties going to the lower index.
- Store its z (R), the quad's min z (G, for conservative tests) and **its own radiance**
  (RGBA16F). The same sample is the half-res trace sample.
- Levels 1-4 apply the same rule to the 4 children's representative z. Radiance is
  carried from the chosen child, never averaged, so background emitters cannot leak
  through a foreground depth.
- Masks applied to the captured radiance:
  - **sun/moon disc mask** (sky pixels within 2× the disc angular radius → 0);
  - below-water → 0 (§3.2);
  - optional prim-glow boost from the scene glow alpha.
- PTVL's N·V division is **off by default**; when enabled it is clamped to `1/0.25`.

**Trace, per half-res pixel, one slice per frame:**
1. Pick a scatter point `x_s` with one sample (§3.4).
2. Pick φ: blue noise plus a frame rotation, or the learned PDF (§3.6b).
3. March N taps (Cinema 32, Hero 64) along φ with exponential spacing, choosing the mip
   by distance.
4. For each tap, the occluder interval runs from `p` to `p+p·thick`, with a metric floor
   of 0.05 m:
   - `μ = dot(v, normalize(q−x_s))` for each end.
   - **HG CDF:** `F(μ) = (1−g²)/(2g)·[(1+g²−2gμ)^(−1/2) − 1/(1+g)]`, with `F(−1)=0` and
     `F(1)=1`; use `F = (μ+1)/2` when `|g| < 1e-3`.
   - Bin indices: `lo = clamp(int(floor(64·min(F0,F1))),0,64)`,
     `hi = clamp(int(ceil(64·max(F0,F1))),lo,64)`.
5. **Safe interval masks (P0-3).** Never shift by 32 or more:
   ```glsl
   uint lowBits(int n) { return n <= 0 ? 0u : (n >= 32 ? 0xFFFFFFFFu : ((1u << uint(n)) - 1u)); }
   uvec2 mask64(int n) { return uvec2(lowBits(n), lowBits(n - 32)); }   // bits [0, n)
   uvec2 interval = mask64(hi) & ~mask64(lo);                           // bits [lo, hi)
   uvec2 newBits  = visible & interval;                                 // newly covered
   visible       &= ~interval;
   int   count    = bitCount(newBits.x) + bitCount(newBits.y);
   ```
   `visible` starts as `uvec2(0xFFFFFFFFu)`. If `count > 0`, fetch the tap's carried
   radiance and add `count/64 · radiance · exp(−ĉ·τ(x_s→p))` to the **`screen_owned`**
   accumulator.
6. **Remaining `visible` bits** (off-screen or unknown) are the **ambient-owned sectors**.
   Each remaining run (found with `findLSB`/`findMSB`; at most 2 runs, with any further
   runs merged into the second) contributes `(bits_in_run/64)·E(dir_run)/π` to the
   separate **`probe_fill`** accumulator.
   - `dir_run` is the run's mid-direction.
   - `E` is probe irradiance, the §3.5(b) approximation.
   - This is the **only** ambient term when the screen pass runs.
   - `bitCount(visible)/64` is exported as the sky-visibility debug view.
7. **Combine (v3.1, P1-3).** The two accumulators are scaled independently:
   ```text
   incoming = VolLightScreenAmount  * screen_owned
            + VolLightAmbientAmount * probe_fill
   S_screen = event_weight(§3.4) * incoming
   ```
   - The probe fill is **never** multiplied by `VolLightScreenAmount`. Screen glow 0 leaves
     Sky fill intact, and vice versa.
   - The S8a closed-form check runs with both amounts at 1.
   - The screen-only and ambient-only debug views output each scaled term separately.

**Calibration.** The screen term assumes each captured pixel is a **Lambertian emitter
whose radiance toward `x_s` equals its radiance toward the camera**. It is an artistic
one-bounce/emissive layer, bounded by test S8. No global consistency is claimed.

**(b) Learned direction PDF** (optional; Hero; WP7 A/B-gated):
- A fragment-only 4-ary tree per 8×8 tile: 1 + 4 + 16 RGBA16F texels for 64 bins.
- Sampling costs 3 fetches.
- Update = 1 gather pass (16 reads per traced pixel equivalent) plus 1 tree-rebuild pass.
- EMA 0.05, with 25 % uniform mixed in. Reset on generation change.

### 3.7 Energy ownership policy (fixes P1-3)

| Term | Owns | Does not own |
|---|---|---|
| `S_analytic` | Direct in-scatter from delta lights: sun/moon, spots/projectors/rig, practicals | Anything reflected off surfaces |
| `S_screen` | Emissive + one-bounce surface radiance as seen on screen (artistic), and ambient sky/probe fill | Sun/moon disc (masked), water and pre-water alpha (v1) |
| WindLight `doAtmospherics` | Far-field aerial perspective (km scale), applied per surface | The local medium |
| VolLight medium | Finite local slab (≤ `MaxDist`, ≤ `h_top`) | Anything beyond |

**Known, accepted overlaps** (art-controlled through separate amounts, visible in debug
views):
- Sky pixels whose radiance came from the same sun.
- Emitter prims of analytic lights.
- WindLight solar haze plus the local solar in-scatter.

**Debug views** (`VolLightDebug`): analytic, screen, ambient, sky-visibility,
scatter-distance, transmittance, history-length, reactive, reset-generation flash,
learned-PDF bins.

**Calibration scenes:** S7 (g=0 analytic) and S8 (screen/ambient), with tolerances in
§6.0.

### 3.8 Denoise, temporal and upsample (generation hook)

#### 3.8.1 Temporal (fixes P0-2, P1-4)

**Targets** (all half-res):
- `S_analytic`, `S_screen`: RGBA16F (rgb + spare).
- `Meta[2]` ping-pong, RGBA16F: R linear view z, GB oct-normal, A object-motion flag.
- `H_analytic[2]`, `H_screen[2]`: RGBA16F (rgb + history length n, capped at 64).

**Passes:**
- **Prefilter** (`S_screen` only): one edge-aware 3×3 à-trous step with log-z and normal
  weights.
- **Temporal:** one MRT pass updates both histories:
  - `prev_uv = uv − 0.5·velocity_ndc(fullres_px_of_sample)`. The velocity comes from the
    early split part at hook G (§3.2); in fallback mode (b) it is the camera-plus-previous
    estimate. The sample position is fixed (the lower-median pixel), so no sub-pixel
    phase enters the reprojection. Water-clipped samples use camera reprojection of their
    own clipped position.
  - Catmull-Rom 5-tap history fetch.
  - **Reject** (n → 0) when any of these holds:
    - `prev_uv` is off-screen;
    - `|z_prev − z_expected|/z_expected > VolLightDepthReject` (default 0.03), where
      `z_expected` is this sample reprojected through the previous view;
    - `dot(n_prev, n_cur) < 0.8`, using `Meta[prev]`.
  - **Variance clip** in YCoCg from a 3×3 neighbourhood: γ = 1.0 analytic, 1.25 screen
    (prefiltered input).
  - **Blend:** `α = max(1/(n+1), α_min)`; analytic `α_min` = 0.2, screen 0.06.

**Reactive** (raises `α_min` toward 1; **both histories**):
- Object motion: `|v − v_camera_only| > ε`, written to `Meta.A` by the base pass.
- A CPU lighting delta per light: moved, rotated, or intensity/colour change > 2 %,
  including rig flicker FX and cues.
  - Applied inside that light's screen rect: analytic `α_min → 1`, screen `α_min → 0.3`.
  - The screen history reacts because the captured surfaces change illumination.
- A luminance spike above 3σ after the clip.

**One reset generation (`LLPipeline::mVolLightResetGen`, U64).** The temporal pass resets
all histories when it differs from the stored value. It increments on:
- a projection discontinuity: a new `mProjectionCutThisFrame` flag, set beside
  `noteProjectionChange()` at `pipeline.cpp:7742`;
- a change of `ALDirectorSwitcher::cutSerial()` (`aldirectorswitcher.h:79`, same idiom as
  `pipeline.h:1686`);
- a camera jump above `VolLightCutJumpM` (default 2 m) or 30° in one frame;
- teleport or region change (`gAgent` `mRegionChangedSignal`, `llagent.cpp:1189`);
- resolution, tier or VolLight toggle;
- **any medium change**: density, colour, falloff, floor, top, start, max distance,
  local-volume set. While a slider is dragged this resets every frame. That is accepted:
  the user sees the prefiltered 1-frame estimate while dragging, and it re-converges on
  release;
- **snapshot scope exit**: exactly once, unconditionally, from the RAII scope destructor
  (§3.10). Entry does not increment; it releases the interactive targets instead;
- underwater transitions;
- `VolLightMotionSource` change.

**Still refine.** When the camera and scene have been static for 30 frames, in Director
FreezeTime, or with the toggle on, `α_min → 0`.

**Removed in v2:** the Hero checkerboard / rotating 2×2 sample.

#### 3.8.2 Upsample → `L_full` (end of hook G)
- One full-res pass writes `L_full` (R11G11B10F).
- Joint bilateral over the 4 nearest half-res samples. Weight = bilinear × `exp(−|Δlog z|/σz)` × normal similarity.
- The post-blur radius grows when n < 4 (SVGF-lite folded in).
- If all weights collapse, use the nearest same-depth sample; otherwise use 0.

### 3.9 Composite (hook C) and forward hook (fixes P1-8, P1-N3)

**Opaque composite.** It reads `mRT->screen` attachment 0 and depth, computes
`T_rgb(D)` analytically at full res plus `L_full` (with the `P(D_now)/P(D_gen)` ratio of
§3.2), and writes `scene·T_rgb + L` into **`mWaterDis`**.
- **Precision gate (v3, P1-N3).**
  - `mWaterDis` is allocated with `screenFormat = hdr ? GL_RGBA16F : GL_RGBA`
    (`pipeline.cpp:1367,1567`), and that allocation is unchecked today.
  - VolLight therefore **requires the HDR path** and fails closed without it: the UI
    shows "Volumetric Light needs HDR rendering", and nothing allocates.
  - At enable, and after every `allocateScreenBuffer` (including snapshot reallocations),
    `ALVolLight` verifies `mWaterDis.isComplete()` and an RGBA16F internal format, via a
    stored format or `glGetTexLevelParameteriv(GL_TEXTURE_INTERNAL_FORMAT)`.
  - If either check fails, the composite uses a **dedicated RGBA16F `mVolLightComposite`**
    target. It is allocated through the ladder (§4.4), and its own completeness is checked.
- `mWaterDis` is an RGBA16F scratch (in HDR) with its own depth. Codex's re-review
  confirmed there is **no same-frame collision** in the main-view order:
  - Roto/Tron scene use it before hook G.
  - Water copies and consumes before hook C.
  - `doAtmospherics` copies depth only.
  - Later Roto, Tron and lens users come after the copy-back.
  - Probe-pack scratch routing (`getWaterDisTarget`, `pipeline.cpp` ~1840) only affects
    aux/probe targets, and VolLight is `mMainRT`-only.
- Then `mRT->screen.copyColorToAttachment0(mWaterDis, …)`, the Roto Ink idiom at
  `pipeline.cpp:18152`.
- The shader writes **attachment 0 only**. It runs inside `renderGeomPostDeferred`'s
  `LLScopedIndexedDrawBufferGuard` (`pipeline.cpp:7914`), which keeps the sidecar and
  coverage attachments write-disabled.
- No dual-source blending and no monochrome approximation: the transmittance is an exact
  chromatic medium.
- **Ordering regression check (WP1):** a debug assertion that no `mWaterDis` user runs
  between C1 and C2. If a future change breaks this, switch to `mVolLightComposite`
  (+66 MB at 4K; §4.4 contingency).
- Silhouette handling mirrors `froxelApplyF.glsl:82-92`.

**Forward hook.** `volLightApplyForward(pos_eye, color)` sits next to `cineHazeApply` in
`applySkyAndWaterFog`:
- `T_rgb(d_frag)` is analytic.
- `L ≈ L_full(uv)·P(d_frag)/P(D_gen)`.
- Additive draws get T only.
- It is skipped when `water_sign < 0`, i.e. for pre-water alpha.
- Known approximation: light between a hair strand and the wall is under-counted for that
  strand.

**Metering.** `generateLuminance` (`pipeline.cpp:22466`) reads `mRT->screen`, which is now
fogged. **Always metered, like a real camera.** The v1 toggle is removed; a pre-fog
metering source is future work. Manual exposure is unaffected.

### 3.10 Snapshot render scope (fixes P1-9, P1-N4)

**Why not `gSnapshot`.** `gSnapshot` is set inside every `display(…, for_snapshot)` call
(`llviewerdisplay.cpp:457`), once per subimage (`llviewerwindow.cpp:5772-5794`). It is not
cleared when `rawSnapshot` returns. It therefore cannot own resource lifetime. It stays
only as an inner render gate.

**Scope.** RAII `ALVolLightSnapshotScope`, constructed in `LLViewerWindow::rawSnapshot`
**after** the screen-buffer reallocation decision (`llviewerwindow.cpp` ~5676-5715) and
**before** the subimage loops (~5772). Its destructor covers every early return.
- **Constructor arguments:** full output size (`image_buffer_x/y`), `scale_factor`,
  `buffer_x/y_offset`, `window_width/height`, and whether the main RT was reallocated
  (`reset_deferred`).
- **On construction:**
  - **release, before any snapshot target is created, every VolLight-owned interactive
    allocation (v3.1, P1-4)**, explicitly:
    - radiance pyramid (RGBA16F with mips);
    - depth pyramid (RG32F with mips);
    - `S_analytic`;
    - `S_screen`;
    - `Meta[0]`, `Meta[1]`;
    - `H_analytic[0..1]`, `H_screen[0..1]`;
    - prefilter scratch;
    - `L_full`;
    - learned-PDF tree and side RT (Hero);
    - interactive `D_gen`;
    - `mVolLightComposite`, if allocated.
    
    Blue noise (2 MB, immutable) is kept. A debug assertion checks that VolLight's
    allocation registry reports 0 bytes of interactive targets before the first snapshot
    allocation.
  - **The velocity target is never released by VolLight.** It is owned by the main RT
    (`mVelocityMap`, allocated in `allocateScreenBuffer`), and `BDMergeVelocityBuffer` is
    independently on here for ReShade. If the snapshot reallocates the main RT, the viewer
    reallocates it as today.
  - set `ALVolLight::sSnapshotActive`.
- **Per subimage:** `scope.setSubfield(subfield, tile_origin_px)` is called immediately
  before each `display()` call, with
  `tile_origin_px = (subimage_x·window_width − buffer_x_offset, subimage_y·window_height − buffer_y_offset)`
  in full-output coordinates.
- **Inside snapshot frames** (gated by `sSnapshotActive` && `gSnapshot`):
  - transient targets sized to the **current render target**: the reallocated main RT, or
    the window for tiles;
  - **seeding from full-output pixel coordinates** (`tile_origin_px + local_px`), so
    analytic samples are tile-independent;
  - a deterministic, history-free policy: full-res trace with no upsample; analytic K=16;
    screen 4 slices × 64 taps over a half-res coupled pyramid; practical V=8 for all
    lights;
  - **N=16 in-frame iterations** with decorrelated seeds, blended additively (weight 1/N)
    straight into one **RGBA32F accumulator**. There are no separate signal targets.
    Because the §3.6 step-7 combination is linear, each pass adds its term already scaled
    by its own amount (`ScreenAmount` or `AmbientAmount`), so one accumulator suffices.
- **Destructor (always):**
  - release all transient targets;
  - clear `sSnapshotActive`;
  - **increment `mVolLightResetGen` exactly once**;
  - interactive targets reallocate lazily on the next interactive frame.
- **Known limitation (tiles only).** Tiling happens only when the image exceeds the GL max
  texture size or the main-RT reallocation fails. In that case the screen term and
  practical visibility at tile borders cannot see the neighbouring tile's content. The
  analytic term is exact.
- **Peak memory:** §4.4.

### 3.11 Coexistence

| Existing | Decision |
|---|---|
| Froxel "Voxel Air" | **Replace.** Hidden after WP8; deleted one release later. Mutually exclusive at runtime (VolLight wins). |
| `ALLocalFogManager` (Atmos Volumes, froxel-only today) | **Reuse** the data and UI; analytic segments in VolLight (WP8) before froxel deletion. |
| Dust wind `BDMergeFroxelWind*` | **Reuse** unchanged. |
| Per-cone shafts | Byte-identical when VolLight is off. When on, participating projectors skip per-cone; a "Per-cone hero" flag keeps the march *and* excludes the light from VolLight. |
| G3.2 sun godrays | Suppressed while VolLight sun shafts > 0; the UI says so. |
| WindLight haze | Kept: far field. |
| Cine Haze / Depth Haze | Suppressed while on (UI note). Shared medium math; import per §3.3. |
| ReShade bridge | Both the raw `color_hdr` copy (`pipeline.cpp` ~14133) and the `mRT->screen` fallback (`llreshadebridge.cpp:252`) now include VolLight. A pre-fog option would need a dedicated pre-composite capture plus a bridge-selection bit (+33-66 MB, +1 copy). RTGI's depth and G-buffer still describe opaque surfaces only. Turn off ReShade PTVL when VolLight is on. |
| Prism VCam aux, probes, water reflection | **Excluded in v1: an explicit limitation.** VCam monitor/feed outputs show no VolLight (and no froxel/G3.2 suppression differences) while the main camera does. If feed parity is required, WP4b adds a history-free analytic-only variant: K=8, no screen term, own transient targets. |
| VRAM governor | Lazy allocation; release on off; allocation ladder in §4.4. |

### 3.12 Pass graph (v3)

```
[renderGeomPostDeferred]
  ... sky, opaque forward, fullbright, masked, avatar, glow ...
  == hook G (before doWaterHaze / first POOL_ALPHA_PRE_WATER draw) ==
  V1  renderGeomVelocity EARLY (camera fallback + pools < PRE_WATER), state-guarded
  V2  copy raw depth -> D_gen (R32F, full-res)
  G1  capture L0: lower-median sample -> radiance RGBA16F + depth RG32F (MRT), masks
  G2-G5 pyramid levels 1-4 (coupled select, MRT)            [Cinema/Hero]
  G6  base: sun/moon (+ambient in Draft) -> S_analytic, Meta[cur]
  G7..  per spot/rig light, single-shadow shader, scissored -> S_analytic (additive)
  Gp  practicals batch (cached per-light visibility)        [Cinema/Hero]
  Gs  screen trace + remaining-sector probe fill (two accumulators:
      ScreenAmount*screen_owned + AmbientAmount*probe_fill; no separate
      ambient x sky-visibility term) -> S_screen (+PDF side RT in Hero)
  Gl  [learned-PDF gather + tree rebuild]                   [Hero, optional]
  Gf  prefilter(S_screen)                                    [Cinema/Hero]
  Gt  temporal MRT (both histories)
  Gu  bilateral upsample -> L_full
  ... doWaterHaze, pre-water alpha, water ...
  == hook C (after doAtmospherics) ==
  C1  composite scene*T_rgb + L -> mWaterDis ; C2 copyColorToAttachment0 -> mRT->screen
  ... post-water alpha: volLightApplyForward() ...
[renderDeferredLighting, original velocity site]
  V3  velocity LATE: fix-up (camera fallback where depth != D_gen) + pools >= PRE_WATER
  ... gGLLast* snapshot (unchanged) ...
[renderFinalize: luminance/exposure/bloom see the fog]
```

---

## 4. Budget (RTX 5090-class)

### 4.1 Pass counts (new draws per frame)

Velocity geometry is **split, not duplicated**. Each pool's velocity pass runs once. The
split adds two cheap full-res passes, V2 (`D_gen` copy) and V3 (fix-up), counted below.
If the velocity buffer was not already enabled for motion blur or ReShade, VolLight also
causes today's full velocity geometry cost.

| Tier | Passes | Composition |
|---|---|---|
| **Draft** | 2 + 1 + 1 + ≤4 + 1 + 1 + 2 = **≤12** | V2/V3 + G1 (depth/meta only) + G6 + ≤4 spots + Gt + Gu + C1/C2. No pyramid, practicals, screen, prefilter. |
| **Cinema** | 2 + 5 + 1 + ≤8 + 1 + 1 + 1 + 1 + 1 + 2 = **≤23** | V2/V3 + G1-G5 + G6 + ≤8 spots + Gp + Gs + Gf + Gt + Gu + C1/C2 |
| **Hero** | 2 + 5 + 1 + ≤10 + 1 + 1 + 2 + 1 + 1 + 1 + 2 = **≤27** | as Cinema, with ≤10 spots and the 2 learned-PDF passes |

### 4.2 Texture taps per traced (half-res) pixel, worst case (every spot overlaps)

| Pass | Draft | Cinema | Hero |
|---|---|---|---|
| Pyramid (amortised) | 0 | ≈8 | ≈8 |
| Base: 3 + 2K (+ ambient 2×≤4 probes in Draft) | 7 + 8 | 11 | 19 |
| Spots: per light 1 + 3K | 4×7 = 28 | 8×13 = 104 | 10×25 = 250 |
| Practicals: top-L × V, once per pixel | — | 4×6 = 24 | 8×8 = 64 |
| Screen: N + ≤N/2 radiance + 2 fill + 2×≤4 ambient | — | 32+16+2+8 = 58 | 64+32+2+8+3 = 109 |
| Prefilter | — | 18 | 18 |
| Temporal: 3 + 2×5 + 2×9 + 1 | 32 (one history ≈ 18) | 32 | 32 |
| **Total** | **≈61** | **≈255** | **≈500** |

Plus per **full-res** pixel: `D_gen` copy 1, velocity fix-up 2, upsample ≈10, and
composite ≈3 + blit.

Spot counts are worst case. Scissoring makes typical overlap 1-3 lights. The practical
term, which was the review's main concern, is now bounded at 24/64 taps.

### 4.3 Time targets

These are **provisional**. They include the composite scratch and blit, the meta traffic,
and (v3) the `D_gen` copy plus velocity fix-up. They exclude the velocity geometry pass
itself, which is an existing feature cost. Gates freeze at the **budget checkpoint after
WP5**, from microbenchmarks taken in WP2 (base and velocity split), WP3 (spots) and WP5
(trace).

| Tier | 1080p | 1440p | 4K |
|---|---|---|---|
| Draft | 0.40 ms | 0.60 ms | 1.2 ms |
| **Cinema** | **0.75 ms** | **1.10 ms** | **2.2 ms** |
| Hero | 1.15 ms | 1.75 ms | 3.5 ms |

- **Gate rule (after the checkpoint):** median ≤ cap and p95 ≤ 1.25× cap, using the §6.0
  Tracy procedure.
- If a tier misses its cap, its parameters are cut (spots, N, K) rather than raising the
  cap. The user agrees the cap at the checkpoint.
- **Offsets:** froxel, G3.2 and per-cone march for participating lights all go away, as
  does ReShade PTVL if the user runs it.

### 4.4 VRAM at 4K (half = 1920×1080, full = 3840×2160)

**Cinema:**

| Target | Size |
|---|---|
| Radiance pyramid RGBA16F + mips | 22.1 MB |
| Depth pyramid RG32F + mips | 22.1 MB |
| `S_analytic` RGBA16F | 16.6 MB |
| `S_screen` RGBA16F | 16.6 MB |
| `Meta[2]` RGBA16F | 33.2 MB |
| `H_analytic[2]` | 33.2 MB |
| `H_screen[2]` | 33.2 MB |
| Prefilter scratch | 16.6 MB |
| `L_full` R11G11B10F | 33.2 MB |
| Blue noise | 2 MB |
| **VolLight-owned subtotal** (v2 rows, confirmed by the re-review) | **≈229 MB** |
| `D_gen` R32F full-res (v3: split velocity + composite ratio) | 33.2 MB |
| **VolLight targets** | **≈262 MB** |
| Velocity map RG16F full-res, **forced** by VolLight (`pipeline.cpp` ~1551); 0 if motion blur or the velocity buffer is already on | 33.2 MB |
| **Worst-case incremental** | **≈295 MB** |

**Applicable figure for this user: ≈262 MB at 4K Cinema** (v3.1). `BDMergeVelocityBuffer`
is already on for the ReShade bridge, so the velocity map is not a VolLight-induced
allocation, and the velocity geometry pass is not a new cost. The 295 MB row applies
only to installs where neither the velocity buffer nor motion blur is enabled.

These figures exclude FBO and driver overhead. Other tiers and resolutions, as VolLight
targets / worst case including velocity:

| Configuration | VolLight targets | Worst case |
|---|---|---|
| Hero, 4K | 274 MB (+ learned PDF 4.1 MB + side RT 8.3 MB) | 308 MB |
| Draft, 4K | 151 MB (`S_analytic` + `Meta[2]` + `H_analytic[2]` + `L_full` + blue noise + `D_gen`) | 185 MB |
| Cinema, 1440p | 116 MB | 131 MB |
| Cinema, 1080p | 66 MB | 74 MB |

**Contingency:** a dedicated `mVolLightComposite` RGBA16F, used only if the §3.9
format/completeness check fails: +66 MB at 4K.

**Snapshot peak (v3, transient, at the snapshot render-target resolution).** The
interactive targets are **released** on scope entry, so the peak replaces them rather than
adding to them.

| Snapshot item | Bytes per pixel |
|---|---|
| RGBA32F accumulator | 16 |
| Half-res coupled pyramid (radiance RGBA16F + depth RG32F, with mips, at ¼ pixel count) | ≈5.3 |
| `D_gen` | 4 |
| Velocity, re-allocated with the main RT; VolLight-induced only if not otherwise enabled | 4 |
| **Total** | **≈29.3** |

| Snapshot size | Pixels | Peak |
|---|---|---|
| 4K | 8.3 MP | **≈245 MB** (the accumulator alone is 133 MB, as the re-review noted) |
| 6K | 21.2 MP | ≈620 MB |
| 8K | 33.2 MP | **≈975 MB** |

The main RT's own reallocation at snapshot size is the viewer's existing behaviour and is
not counted here.

**Snapshot ladder:**
1. RGBA16F accumulator (≈21 B/px).
2. Half-res snapshot trace with bilateral upsample (≈17 B/px).
3. Render the snapshot without VolLight and show a toast: "Snapshot rendered without
   Volumetric Light (GPU memory)".

**Allocation-failure ladder.** `LLRenderTarget::allocate` returns false; check every call.
1. Release partial allocations and drop the learned PDF (Hero → Cinema features).
2. Drop the screen term: pyramids, `S_screen`, `H_screen`, prefilter. Move ambient to the
   base pass.
3. Draft with `L_full` replaced by in-composite bilateral upsample. This removes 33 MB at
   +0.1 ms. The forward hook then falls back to transmittance plus the nearest half-res
   sample.
4. If velocity or `D_gen` allocation fails, switch `VolLightMotionSource` to CameraOnly
   (option b), which drops the split and `D_gen` use for velocity. `D_gen` stays required
   for the composite; if it fails, go to step 5.
5. Disable VolLight for the session. Show the UI notice "Volumetric Light paused: GPU
   memory". Retry only on resize or tier change. Never assert.

---

## 5. UI (Lightbox tab "Vol Light")

**Placement and gate:**
- The tab sits in `floater_lightbox_settings.xml` and replaces "Froxel Air" (lines
  5800-5805) after WP8.
- It uses the scroll_container pattern. Every slider has `show_text`, `can_edit_text`, a
  reset button and `wheel_adjust`.
- **Gate:** `python scripts/code_tools/xui_layout_check.py --changed <rev>` must report
  zero findings.

| Group | Control | Setting | Range / default |
|---|---|---|---|
| Master | Enable volumetric light | `VolLightEnabled` | off |
| | Quality | `VolLightTier` | Draft / **Cinema** / Hero |
| | Look preset | combo | Clear Air, Morning Mist, Hazer Light, Club Smoke, Forest Shafts, Neon Rain, Firelight Smoke, Underworld |
| Air | Density (shows "visibility ≈ `3.912/(VolLightDensity·lum(c_rgb))` m", or "∞ (no medium)" when the denominator is < 1e-6) | `VolLightDensity` | 0-0.5 /m, 0.02 |
| | Distance start (m) | `VolLightStart` | 0-50, 0 |
| | Max distance (m) | `VolLightMaxDist` | 16-1024, 256 |
| | Falloff height (m; 0 = uniform) | `VolLightHeightFalloff` | 0-100, 4 |
| | Ceiling above floor (m) | `VolLightTopHeight` | 2-500, 40 |
| | Floor reference | `VolLightFloorMode` + "Set floor here" | World Z / Latched avatar |
| | Floor height | `VolLightFloorZ` | world m |
| | Haze colour | `VolLightAlbedo` | white |
| | Absorption | `VolLightAbsorption` | 0-1, 0 |
| | Absorption colour | `VolLightAbsorbTint` | neutral |
| Light | Anisotropy | `VolLightAnisotropy` | −0.5-0.95, 0.6 |
| | Back-scatter | `VolLightBackLobe` | 0-1, 0 |
| | Light shafts (sun/moon) | `VolLightSunAmount` | 0-4, 1 |
| | Lamp & rig shafts | `VolLightLocalAmount` | 0-4, 1 |
| | Practical glow | `VolLightPracticalAmount` | 0-4, 0.5 |
| | Screen glow | `VolLightScreenAmount` | 0-4, 1 |
| | Sky fill | `VolLightAmbientAmount` | 0-2, 1 |
| | Only flagged projectors | `VolLightFlaggedOnly` | off |
| Texture | Dust amount / size / drift | `VolLightNoise` / `VolLightNoiseScale` / `VolLightDrift` | 0.25 / 3 m / 0.1 |
| | Wind from / tilt / invert | **shared** `BDMergeFroxelWind*` | — |
| Motion | Smoothness / Responsiveness | `VolLightHistory` / `VolLightReactive` | — |
| | Refine stills; Reset history | `VolLightStillRefine`; button | on |
| Advanced | Object thickness | `VolLightThickness` | 0.25 |
| | Leak guard | `VolLightLeakGuard` | 0.75 |
| | Firefly clamp (stops) | `VolLightFireflyEV` | 8 |
| | Max light path (m) | `VolLightMaxLightPath` | 128 |
| | Depth reject | `VolLightDepthReject` | 0.03 |
| | Motion source | `VolLightMotionSource` | **Split** (exact) / CameraOnly (fallback) |
| | Debug view | `VolLightDebug` | §3.7 list |
| | Import from Depth Haze | button | (note: MaxOpacity/Strength not imported) |

- **Removed in v2:** "Include in exposure metering". It is always metered.
- **Status lines:**
  - "Depth Haze and Sun Rays (legacy) are paused while Volumetric Light is on".
  - "Off under water (v1)".
  - "Not shown in VCam feeds (v1)".
  - "Volumetric Light needs HDR rendering" (fail-closed, v3).

---

## 6. Phased plan

The process follows the repo's `CLAUDE.md`:
- Codex implements via prompt file, then Opus adversarial review, then the Codex loop
  until there are no majors.
- Fable owns §3.3-3.6 and §3.8.1.
- Checkpoint commit, then **one fresh build per package** (`cxs\fresh_build.ps1`).

Every WP also does shader registration, cmake reconfigure and staging verification;
proves the off path is byte-identical; and passes the prebuild guard and
`xui_layout_check`.

### 6.0 Test assets and metrics (fixes P2-5)

**Scenes** (saved Director scenes, checked in under `doc/vollight_tests/` as scene
LLSD plus notes):

| ID | Scene |
|---|---|
| S1 | Window shafts, **sun behind camera** |
| S2 | Club: 6 projectors + rig, gobo with 8-px and 3-px bars at 1440p |
| S3 | Backlit alpha-hair close-up vs bright window |
| S4 | Neon/emissive night street |
| S5 | Indoor room, bright sky outside (leak) |
| S6 | Fog ramp: flat plane to 1 km, density 0.02, `k = 0.25`, top 40 m, camera at `h_f + 1.5 m` looking horizontally |
| S7 | Energy, analytic: uniform medium (`k = 0`), g=0 and g=0.6, one unoccluded spot and the sun, no geometry in the crops |
| S8 | Energy, screen/ambient: camera inside a debug uniform-radiance dome (sky override `L`, probe-irradiance override `πL`), uniform medium, plus an S8b variant with one emissive wall filling half the view |

**Camera tracks** (Director keyframes; 60 fps capture):

| ID | Track |
|---|---|
| T1 | Pan 90° in 2 s |
| T2 | Dolly 3 m/s |
| T3 | Zoom 60°→15° in 3 s |
| T4 | Hard cut A→B |
| T5 | Static camera, actor walks across a shaft at 1.5 m/s |

**Reference:** a debug "reference mode" renders each frame with the snapshot policy
(history-free, full-res, N=64). It is deterministic per frame.

**Metrics:**
- Errors are measured in exposed log2 luminance (pre-tonemap × exposure), on fixed
  256×256 crops listed per scene. The temporal-error heatmap is a debug view.
- **Ghost/trail (T4, T5):**
  - After a cut, frame N+1: mean |Δ| vs reference ≤ 0.10 stop, and no connected region
    > 64 px² with |Δ| > 0.5 stop.
  - Trail: the |Δ| > 0.25-stop region behind the actor clears within **3 frames**.
- **Smear (T1-T3):** mean |Δ| ≤ 0.15 stop and p99 ≤ 0.6 stop, per frame over the track.
- **Noise:**
  - Still-refine, static, after 120 frames: temporal σ over 30 frames ≤ 1 % of the crop
    mean.
  - Cinema on T2: RMS relative error ≤ 4 % on fog-dominated crops.
- **Gobo edges (S2):** 10-90 % edge width ≤ 3 full-res px for bars ≥ 8 px at 1440p.
  **Bars < 4 px are not guaranteed from a half-res signal**; this is documented, and the
  hero per-cone flag is the remedy.
- **Silhouette leak (S2/S3):** mean luminance in the 4-px band at the actor edge ≤ 5 %
  above reference.
- **Sky/fog ramp (S6)**, v3 with a clipped-segment reference:
  - For **every** pixel, the composite transmittance (debug view, per channel) equals
    `exp(−c·τ_ref)` within 1e-3 absolute.
  - `τ_ref` is evaluated on the CPU in double over that pixel's **explicitly clipped**
    segment: `[t_a, min(D, MaxDist)]`, clipped to the slab top and floor pieces. Sky
    pixels on upward rays therefore exit at `h_top`, not at 256 m.
  - On the horizon scanline, whose rays stay inside the slab, luminance is monotone
    non-decreasing in distance up to `MaxDist` and constant beyond it.
- **Energy statistics (S7, S8)**, v3:
  - **Procedure:** static camera, Still refine on.
    - Discard the first 256 frames.
    - Over the next 30 frames, record the crop-mean in-scatter (`L` debug view, linear,
      per channel) on the listed 256×256 crops.
    - Repeat for **8 independent runs**, each with a different global seed offset.
    - Per run, the statistic is the 30-frame mean.
    - Across runs: the mean and a **95 % CI** (Student t, 7 dof).
  - **S7 (analytic).** The reference is an offline double-precision march (2048 steps
    over the same clipped segment, analytic visibility, same phase and same radiometric
    chain).
    - **Pass:** |mean/ref − 1| ≤ 3 % at g=0 and g=0.6.
    - **And:** CI half-width ≤ 1 % of ref.
  - **S8 (screen + ambient).**
    - **S8a, closed form.** In the uniform dome with `k = 0`, the total of screen term
      plus ambient fill must equal `L·albedo_rgb/c_rgb·(1 − e^(−c_rgb·σ0·ℓ))`, where `ℓ`
      is the clipped view length. This holds because every direction carries radiance L
      and the phase function integrates to 1.
      **Pass:** ±5 %, CI ≤ 2 %.
    - **S8b, ownership-masked integral.** The reference is an offline angular-and-path
      integral computed on a frame dump (depth, captured radiance, probe-irradiance
      override):
      - 512 path steps over the same clipped segment, 4096 stratified directions per
        step;
      - each direction is classified with the **same ownership masks**: on-screen and
        depth-consistent means screen-owned with the captured radiance; otherwise
        ambient-owned with the E/π approximation;
      - same HG, same transmittance.
      - Screen-only and ambient-only debug views are compared to their respective
        partial integrals.
      **Pass:** ±10 % each, CI ≤ 3 %.
- **Math unit tests** (light-rig test-harness style, C++ fp32 mirror of the GLSL plus a
  double reference):
  - Inverse: after validation and bisection, relative τ residual ≤ 1e-4 **and** distance
    error ≤ 1e-4·ℓ against the double reference. Over 10⁶ random rays covering
    `|x|` ∈ [0, 1), u ∈ {0, ±1e-6, random}, k ∈ {0, 1e-6, random}, floor and top
    crossings, and `τ*` at 0 and at `τD`.
  - Chromatic: grey `c_rgb` gives an importance ratio of exactly 1; black albedo with
    `a = 0` gives `L = 0`, `T = 1`.
  - HG CDF endpoints and monotonicity.
  - Interval masks for every (lo, hi) in 0..64 against a reference bit loop.
- **Velocity split parity (V1)**, v3, **blocking**:
  - The consumer `mVelocityMap` with VolLight on (split) must be **bit-identical** (0 ULP)
    to VolLight off (unsplit) on T1-T5.
  - Scenes must include water, depth-writing forward alpha, rigged alpha attachments,
    animesh, and fullbright and masked forward geometry.
  - Motion-blur output hash and ReShade-bridge velocity bytes must also be identical.
  - **Any mismatch** means `VolLightMotionSource = CameraOnly` ships as the default, and
    the split is fixed or dropped.
  - **GL-state snapshots (v3.1).** A debug build dumps a full GL plus `LLRenderTarget`
    state snapshot at four points:
    - (a) immediately before hook G;
    - (b) immediately after the early half's guard restores;
    - (c) at entry to today's velocity call site, unsplit;
    - (d) at entry to the late half, split.

    Pass conditions:
    - (a) == (b), field by field. Fields: `sBoundTarget`, `sCurFBO`, target stack depth,
      draw/read FBO, read buffer, draw buffers, viewport, scissor, colour masks
      (indexed), blend, depth, stencil, cull/front-face, polygon mode/offset, program,
      VAO, array/element buffers, active unit and all unit bindings, `gGL` matrices.
    - The canonical velocity state actually in effect during every velocity draw is
      identical between (c) and (d).
  - **Unsplit path unchanged.** With VolLight off, the velocity buffer must be
    bit-identical to the pre-change build. This proves that setting the canonical state
    explicitly is a no-op for today's output.
- **Off path:** golden-frame hash identical with VolLight off.
- **Perf (Tracy procedure):**
  - 600 frames on T2 per resolution and tier, after a 120-frame warmup.
  - Report median and p95 per GPU zone; gate per §4.3.
  - Captures saved under `Desktop\Tracy\vollight\`.
- **Snapshot (S6-snap)**, v3:
  - **Noise.** Two untiled snapshots of S3 and S4 at 2× window resolution with different
    global seed offsets: RMS relative difference ≤ 1.5 % on the crops.
  - **Tiles vs untiled reference.** The same snapshot is rendered (a) untiled, through
    the main-RT reallocation path, and (b) with forced 2×2 tiling (debug setting
    `VolLightDebugForceSnapshotTiles`). Both use identical full-output seeds and are
    compared **at identical pixel coordinates**:
    - analytic-only mode: max |Δ| ≤ 0.01 stop everywhere;
    - full mode, interior (≥ 64 px from any tile border): mean |Δ| ≤ 0.02 stop;
    - full mode, border band: mean |Δ| ≤ 0.15 stop. This is the documented screen-term
      tile limitation (§3.10).
  - **Lifetime.** After the snapshot, `mVolLightResetGen` has advanced by exactly 1, all
    snapshot transient targets are released (verified by the allocation log), and the
    next interactive frame reallocates its targets.

**Look sign-off is separate.** The user compares side by side with ReShade PTVL on S3/S4
and approves. This is **not** an engineering gate.

### 6.1 Work packages

**WP0: Scaffolding and math.**
- `ALVolLight`, settings, lazy allocation and the allocation ladder.
- `vollightUtil.glsl`: slab τ, inverse, HG/CDF, interval masks, equi-angular.
- In-repo seeded blue-noise generator.
- Reset-generation plumbing (projection flag, cut serial, region signal).
- **Accept:** math unit tests; off-path hash.

**WP1: Two hooks, medium and composite.**
- Hook G (fog-only base with ambient) and hook C composite via `mWaterDis`, with the HDR
  fail-closed gate, the format/completeness check and the `mVolLightComposite` fallback.
- `D_gen` copy.
- Forward hook, water policy, underwater off.
- Cine Haze and G3.2 suppression, floor modes.
- **Accept:** S6, S3 silhouette and hair, S7 transmittance; off-path hash.

**WP2: Sun/moon, temporal and upsample.**
- Split velocity: range parameter, early part at hook G, late fix-up plus late pools,
  state guard, `VolLightMotionSource` fallback.
- **V1 must pass before anything else in WP2 is accepted.**
- K-sample sun with the bounded sun path.
- Meta, histories, reject/clip/reactive, reset generation, upsample.
- Base-pass microbenchmark.
- **Accept:** S1, T1-T5 metrics, V1, S7 sun.

**WP3: Spots, projectors and rig.**
- Single-shadow `vollightSpotF`, equi-angular sampling, overrides, actor-only, per-cone
  skip and hero.
- CPU light-delta reactive (both histories).
- Spot microbenchmark.
- **Accept:** S2 metrics, S7 spot, flicker phase lag ≤ 2 frames, actor-only repro
  (`5ba438b1348`).

**WP4: Practicals.** Batched lights with per-light-per-pixel cached visibility, top-L
selection and leak guard.
- **Accept:** S5 leak crop ≤ 5 % above reference; practical tap count verified with a
  shader counter in debug builds.

**WP4b (optional, if VCam parity is required): VCam variant.** History-free,
analytic-only.

**WP5: Screen term.**
- Coupled pyramid, masks, trace with safe bitfield, sky fill and ambient × sky-vis.
- Trace microbenchmark.
- **→ Budget checkpoint:** freeze the §4.3 caps with the user.
- **Accept:** S4, S8, silhouette leak; look sign-off scheduled.

**WP6: Denoise polish, still refine and snapshot scope.**
- RAII `ALVolLightSnapshotScope` in `rawSnapshot`, `setSubfield` per subimage, snapshot
  ladder.
- **Accept:** noise metrics, S6-snap.

**WP7: Learned direction PDF (optional).** Keep only if variance drops ≥ 25 % at equal
ms on S3/S4.

**WP8: Local volumes, froxel retirement and import.**
- **Accept:** each Atmos Volume renders with the froxel master forced off. The import
  round-trips the density/colour profile within S6 tolerances, with the `H=1/k`
  conversion checked.

**WP9: UI, presets, rig and Director hooks.**
- **Accept:** `xui_layout_check` reports zero findings; wheel works on every slider;
  presets round-trip.

**WP10: Perf lock.** The Tracy procedure on all tiers and resolutions; results in
`doc/perf_baseline_*.md`; off-path frame time unchanged.

---

## 7. Risks

1. **Velocity split.** A pool's velocity pass may depend on state that changes between
   hook G and the late site, or a pass may write depth farther after hook G. Either
   breaks bit-identity.
   *Mitigations:* the bit-exact V1 gate; the CameraOnly fallback (option b, exact for the
   static world, lagged for actors); VolLight-only gating.
2. **`mWaterDis` precision and reuse.**
   *Mitigations:* the HDR fail-closed gate; format and completeness checks; the debug
   ordering assertion; `mVolLightComposite` contingency (+66 MB). The re-review confirmed
   there is no current collision.
3. **Generation before water.**
   *Mitigations:* the analytic water-plane clip plus the composite ratio. Water-exclusion
   regions follow the mask. There is no lake screen glow in v1.
4. **Screen-term noise and edges.**
   *Mitigations:* coupled pyramid, prefilter, clip, post-blur; S4/S8 gates.
5. **Medium-slider drags reset history every frame**, which is noisy while dragging.
   This is accepted; still refine re-converges.
6. **Approximations.**
   - Ambient (irradiance/π blend) and the screen term (Lambertian) are art layers.
   - The forward-hook ratio under-counts light between hair and wall.
   *Mitigations:* S8 tolerance; debug views.
7. **Perf.** Spot overlap worst case, and composite bandwidth at 4K.
   *Mitigations:* the budget checkpoint; tier parameter cuts.
8. **VRAM.**
   - Interactive: ≈262 MB at 4K for VolLight targets, ≈295 MB including the forced
     velocity map.
   - Snapshots peak at ≈29 B/px: ≈975 MB at 8K.
   *Mitigations:* interactive and snapshot ladders; interactive targets released during
   snapshots; governor visibility.
8b. **HDR-off users** get no VolLight (fail closed). This is stated in the UI.
9. **ReShade/RTGI** sees a fogged colour buffer with opaque G-buffer semantics.
10. **VCam parity gap** in v1 (explicit).
11. **Underwater off** in v1 (explicit).
12. **Proprietary reference.** Reviewers reject verbatim structure, tables or the iMMERSE
    assets.

## 8. Decisions needed from the user
1. Replace Voxel Air in the UI at WP1, or after WP8? *Recommendation:* after WP8.
2. Should ReShade see fog (default), or pay for a pre-fog capture?
3. Should all shadowed lights scatter by default? *Recommendation:* yes, with "Only
   flagged" available.
4. Should still refine engage automatically when static? *Recommendation:* yes.
5. **Is VCam feed parity required for v1?** If so, schedule WP4b.
6. **Is underwater VolLight needed?** It is out of v1 scope.
7. ~~Is the velocity buffer already on?~~ **Answered (v3.1):** yes, `BDMergeVelocityBuffer`
   is on for ReShade. That gives ≈262 MB at 4K and no new velocity geometry cost.

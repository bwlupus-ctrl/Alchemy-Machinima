# Native XeGTAO framework + visibility-bitmask AO: SL integration plan

## Revision 4 (10/22)

Four peer reviews of Revision 3 (Opus, Codex, GLM, DeepSeek) found **no true P0**. Two
items labelled P0 by one reviewer were adjudicated as a P1 (screen-fixed noise may swim
under camera motion) and a P2 (wording of the OFF byte-identity promise). All four reviews
agreed on the same structural problem: the visible payoff sat behind the heaviest framework
work, and several gates were synthetic-only. Revision 4 therefore **re-phases** the work
for an early, cheap A/B against the user's reference look, and re-estimates it honestly.

**Numbering.** Phase names in this revision (P0-P4, L1-L5) are Revision 4 numbering.
Revisions 3 and 2 below are historical and use the older P1-P6 numbering; where they
differ, Revision 4 governs. The old "P1-A/P1-B" findings are now cited as
"Decision F-A" and "F-B" (framework decisions); the old "P1-C" variance-sizing finding is
now section 9.2.

### Revision 4 phase list and effort

| Phase | Deliverable | Effort (engineer-days) |
|---|---|---|
| **P0** | Reference-look capture protocol executed; BLEND/alpha coverage survey in the user's scenes; CPU oracle for **bias, variance and motion** on synthetic fixtures plus captured G-buffers | 2-3 (oracle refinements continue inside P1) |
| **P1** | **Early A/B checkpoint**: native bitmask gather + spatial denoise published into the *existing* light-map green with stock SSAO plumbing; selectable native (linear) vs stock-curve response; user A/B in sunlit + interior scenes. **User go/no-go before P2** | 6-9 |
| **P2** | Framework: Prepare, allocation predicate, dispatch gate, white init and ownership, first-use warm-up, Actor FX replay AO input, main-view admission, OFF identity, licences, shader cache | 14-20 |
| **P3** | Calibration against the reference, Director UI, persistence, machine ceilings/profiles | 6-9 |
| | **Core win = P0-P3** (main view, scalar AO, no feeds/stills/bent) | **28-41** |
| **P4** | Bent normals + probe specular occlusion (hero-mirror/SSR policy stated explicitly) | 12-16 |
| L1 | Alpha (BLEND) receivers - indirect terms only, never direct; water stays neutral | 5-8 |
| L2 | VCam feeds + combined AO/Relight/VCam ledger + capture-governor pin/unpin | 12-16 |
| L3 | High-resolution stills: snapshot preparation transaction (single target); Cinematic preset | 4-5 |
| L4 | Sun-blur dispatch cleanup (shrunk: green is already published after the blur) | 1-2 |
| shared | Cross-scene/hardware qualification allowance for the later items | 5-8 |
| | **Full scope = core + P4 + L1-L4 + shared** | **67-96** |
| L5 | Bitmask SSGI (separate approval, not in either total) | 8-15 |

Reviewers ranged 25-35 days (reduced scope) to 60-90 (full scope); the two totals above
are the corresponding honest figures for this plan's reduced and full scopes. Estimates
exclude user screenshot turnaround and hardware availability. Padded tiled stills and
ReShade AO export remain unscoped additions.

### Revision 4 review-item ledger

Sources: **O** = Opus, **C** = Codex, **G** = GLM, **D** = DeepSeek. Every item below is
adopted unless marked otherwise. "Sec." gives where the resolution lives.

| ID | Sev. | Finding (short) | Resolution | Sec. |
|---|---|---|---|---|
| O1 | P1 | Sparse bitmask under-occludes (porous walls/torsos); corner gate High-only | Bias gates (corner, wall, arm-against-torso) at **every** preset; sample-gap linking, thickness floor >= step spacing when linked; thickness sweep to 0.30 m; Low conditional / Horizon-equivalent | 6.1, 9.2, P0-P2 |
| O2 | P1 | More denoise passes trade away detail; iid model undervalues stratified noise | Filter support capped at +-4 full-res px; per-preset contact-width/crease gate; budget moved from passes to slices (High T=72 to 128, passes 7 to 4); oracle uses the real stratified sequence | 9.2, 9.3 |
| O3 | P1 | Sigma and pan gates contradict on smooth fixtures | **Primary gate = pan/swim on real captured scenes**; sigma secondary; joint feasibility from cross-offset covariance in P0 | 9.2 |
| O4 | P1 | Visible win hidden behind stock response (`softenLightF.glsl:137`) | P1 ships a selectable native linear response vs the stock curve; sunlit + interior A/B **before** funding P2 | 3.3, P1 |
| O5 | P1 | BLEND coverage gap missing (`RenderAutoMaskAlphaDeferred` default 0) | P0 survey of BLEND pixel fraction; auto-mask / per-object Alpha Mode documented as the widening path; L1 | 7, P0 |
| O6 | P1 | Caps/table vs RTX 5090; "bind identically" vs "Cinematic may use more" | RTX 5090 per-preset thresholds; ceilings are machine settings with a 5090 capture profile and a conservative distributed default | 9.3, 9.4 |
| O7 | P2 | 24-bit depth step (~3 cm at ~220 m) bands distant terrain | Bias and thickness floor tied to the depth step; distance fade | 6.1 |
| O8 | P2 | Light map is 8-bit GL_RGBA in non-HDR; "+1/1024" is below a code step | Dither (non-HDR) or HDR-only; gates stated in output code values | 8.2, B12 |
| O9 | P2 | Old sun-blur phase mostly redundant (green published after blur) | Shrunk to L4, 1-2 days | L4 |
| O10 | P2 | Main-only P1 needs only AO + Relight main admission | P2 implements the main-view subset; full ledger moves to L2 | 9.4, P2, L2 |
| O11 | P2 | 40-60 days optimistic | Re-estimated: core 28-41, full 67-96 | 10 |
| O12 | - | Simplifications (one bitmask gather, drop Low, main only, keep Prepare/white init/OFF) | Adopted: one gather (thickness to infinity = Horizon diagnostic); Low conditional on P0; feeds/governor/tiles/BLEND/sun-blur deferred | 8, 9.3 |
| O13 | - | Recommended order | Adopted as P0-P3, P4, later | 10 |
| C1 | P1 | Shared Actor FX replay has no geometry at AO gather time (consumer companions alone cannot give parity) | AO-owned input overlay for replay OPAQUE/MASK actors (depth + normal), budgeted, with Option B fallback | 8.1, P2 |
| C2 | P1 | Relight plane churn breaks "zero allocation" under the combined plan | Fix is owned by the Relight stack-fixes job (plane 1 retained while cap > 4, allocation from the admitted plan); AO admission consumes the admitted plan read-only; dependency stated | 9.4, P2 |
| C3 | P1 | Zero compile/alloc calls does not prove "no hitch" | Warm-up draws for every program state during Prepare; cold/warm first-transition CPU and present-interval ceilings | 8.5, P2 |
| C4 | P2 | Variance sizing does not size for motion | P0 oracle reports cross-offset covariance and signed reference-residual deltas; P1's first deliverable is a production-shader GPU feasibility spike; pass counts conditional | 9.2, P0, P1 |
| C5 | P2 | Water is not covered by listing BLEND shader families | Water stays **neutral and unchanged**; any future water receiver is its own family | 7, L1 |
| C6 | P2 | Single-target snapshots need a preparation transaction | L3; interim whole-shot stock fallback and re-prepare on exit in P2 | 7, P2, L3 |
| C7 | P2 | Conflicting 3070 table vs 5090 appendix | One 5090 table; 3-4x factor is planning guidance only; mid-range INCONCLUSIVE without tester captures | 9.3 |
| G1 | P1 | Gates synthetic-fixture only | P0 captures a real-content corpus; preset freeze requires real-content ROI and pan results | P0, 9.2 |
| G2 | P1 | Denoise achievability; Low/Balanced may fail own gates; 3-pass floor asserted; sequence High first | 3-pass floor **withdrawn** (support cap + crease gate instead); Low conditional; P1 builds the High-class configuration only | 9.2, P1 |
| G3 | P1 | Riskiest framework bundled with the visible payoff | Re-phased: P1 early A/B on stock plumbing; framework is P2 | 10 |
| G4 | P1 | Section 13 sun contact contradicts "never darken direct light" | Removed from this plan; separate feature with its own decision record if ever wanted | 13 |
| G5 | P1 | Effort 60-90 realistic | Full scope 67-96; core 28-41 | 10 |
| G6 | P2 | Variance constants unfitted; Low 0.025 visible grain; portability INCONCLUSIVE | Constants fitted in P0 on real depth; Low conditional; INCONCLUSIVE rule retained; frozen-noise hook retained | 9.2, 10 |
| D1 | P0 to **P1** | Screen-fixed noise swims under pans | Motion acceptance (pan/track/zoom at 1x/2x/4x); surface-anchored deterministic noise evaluated against screen-anchored by measurement; Low/Balanced withheld until motion passes; stock `aoUtil.glsl:60` reflect convention not copied | 9.1, 9.2 |
| D2 | P0 to **P2** | Prepared OFF byte-identity contradicts the reservation cost | Defined under frozen texture streaming and guaranteed headroom; otherwise a disclosed cost of Prepare ON | 1, 8 |
| D3 | P1 | Hero-mirror/SSR replacement stays unoccluded; goal gap | Policy stated explicitly; goal narrowed; P4 gate for partial replacement weights | 5, P4 |
| D4 | P1 | Alpha receiver "before direct" ambiguous | L1 multiplies **indirect terms only**, never the whole colour | 7, L1 |
| D5 | P1 | T/p per preset not stated | Table states slices x steps, T, passes, support, E and sigma for every preset | 9.3 |
| D6 | P2 | Effort optimistic | See O11/G5 | 10 |
| D7 | P2 | Early A/B consumer step | See G3 | P1 |

**Not adopted verbatim (reason):** DeepSeek's "require either surface-anchored noise or
materially larger T/p" is not mandated: both anchorings are candidates and P0 measurement
chooses (decision D4 below). Opus's "drop Low" is adopted conditionally: P0 decides, because
a Horizon-equivalent Low may still be a useful cheap live option. Deferred, with reason:
padded tiled stills and ReShade AO export (unscoped, cost without payoff); hero/SSR
occlusion (needs separate evidence); water receivers (separate shader family, separate
budget); sun-contact shadows (violates this plan's hard rule, separate decision record).

### Revision 4 contradictions removed

(1) RTX-3070 timing table versus the 5090 decision: one 5090 table. (2) "Bind identically"
versus "Cinematic may use more": caps are identical per *profile*; the profile is a machine
setting. (3) Section 13 sun contact versus "never darken direct light": section removed.
(4) "Byte-identical Prepared OFF" versus the 8N-byte reservation: defined under frozen
streaming and guaranteed headroom. (5) Sigma 0.025 versus pan p95 0.005: motion is primary.
(6) Preview/stills "P1 support single-target high-res" versus the snapshot reallocation
route: stills move to L3.

## Revision 3 (10/21) - historical (old P1-P6 numbering)

- Re-review found **no P0**. This revision addresses P1-A/B/C and the P2 findings;
  Revision 2 below is historical and is superseded where these policies differ.
- Native capability uses static GL/extent inputs only. Added machine-level **Prepare
  native AO**, default OFF, a separate latched preparation/admission sequence, and
  zero native allocation/compilation for stock users who leave preparation OFF.
- Defined white initialization and per-view light-map content validity, including
  reserved-but-unwritten OFF, debug consumers and DOF scratch reuse.
- Sized candidate presets from an explicit CPU variance model before shader tuning;
  Low/Balanced have disclosed relaxed plane gates, High/Cinematic retain sigma ≤0.005.
  Revised gather/filter costs, budgets and contact-pan excess-delta measurement.
- Use capture slot order only; give the existing capture governor sole ownership of
  feed resolution/cadence, pinned during a take. Raster counts are report-only;
  content changes cannot freeze a scheduled feed. Clarified the gather-only 160 M cap
  and denoise accounting under the combined 2,000 M tap-equivalent ceiling.
- Corrected legacy gloss/environment citations to `softenLightF.glsl:341` / `:351`.

## Revision 2 (10/21) - historical (old P1-P6 numbering)

- P1 now reserves the existing `deferredLight` whenever native AO is capable (option b),
  repairs both allocation/admission gates and the sun dispatch gate, and explicitly
  excludes preview/cube views. Prepared runtime toggles allocate nothing and reload no shaders.
- Added a three-pass spatial denoise minimum without TAA, revised Low/Balanced costs,
  and hard flat-plane variance and sub-pixel camera-pan gates for hardware recording.
- Defined classic-sky direct equality before nonlinear recombination; designed separate
  legacy gloss/environment probe hooks before SSR/hero replacement.
- Added combined AO + Relight + VCam resident/peak-memory and work admission, with
  main reservations, fixed feed allowances and deterministic feed-first degradation.
- Added HUD, hero mirror, impostor, water-reflection and DOF scratch policies/tests;
  made the stock-noise test hook production-inert and corrected shipped MIT notice paths.
- Counted startup shader companions/cache impact, corrected the two source citations,
  and raised P1–P5 effort to **40–60 engineer-days**, including P1 light-map work.

Date: 2026-10-22 (Revision 4; Revision 3 dated 2026-10-21). **Planning only; no implementation, build, commit or stash.**
Audited branch: `feature/director-kit-p0`, HEAD `e0cc289b108f9acd3fe625fd7540ba6f6c79eb80` for the Revision 3 citations;
the review spot-checks and every citation first added in Revision 4 were verified against `git show HEAD:<path>` at
`d4f4ec1192ffe96d8fd24d948d1207022b1a8f2d`.
Source references below describe this checkout, not the older line numbers in the Relight brief.
Paths starting with `class1/`, `class2/` or `class3/` are relative to
`indra/newview/app_settings/shaders/`; other shortened viewer filenames are relative to
`indra/newview/`. Line references identify the start of the relevant implementation.

## 1. Recommendation and scope

Implement a native current-frame ambient visibility pass: XeGTAO-style slice setup and
depth preparation, finite-thickness visibility-bitmask sectors (one gather; thickness
tending to infinity is the Horizon diagnostic) and a short spatial denoise. Revision 4
reaches an A/B against the user's reference look *first* (P1, on the existing stock SSAO
plumbing), and only then funds the lifecycle framework (P2). Bent-normal probe occlusion
follows scalar AO (P4). Keep the stock SSAO branch available throughout rollout.

**Scope tiers.** *Core win* (P0-P3, about 28-41 days): main view, scalar AO, deferred
OPAQUE/MASK plus shared Actor FX replay OPAQUE/MASK, selectable response, Director
controls. *Full scope* (about 67-96 days) adds bent normals/probe specular, BLEND
receivers, VCam feeds with the combined ledger, high-resolution stills and the sun-blur
cleanup. Section 10 gives the phase table.

The user-visible goal is stronger, cleaner contact and cavity definition throughout the
world, especially around railings, fingers, lace and foliage. This cannot supply every
kind of game shadow: AO controls nearby **indirect** visibility; sun and local-light
visibility remain separate. Direct light, light-rig emitters, emission and graphic overlays
must never be multiplied by this AO. Short-range bounce is L5, not a hidden part of any core phase.

**Three independent meanings of OFF:**

1. Native master OFF: execute this revision's original SSAO/shadow path, respecting the
   user's original `RenderDeferredSSAO` setting. Require render-pipeline byte identity
   with baseline under the conditions defined below.
2. Native ON with amount zero: ambient visibility is one and specular visibility is one;
   direct lighting remains unchanged. This is an identity/composition test, not permission
   to run native passes while the native master is OFF.
3. AO disabled altogether: a proposed native mode, separate from stock SSAO OFF and its
   historical shadow-blur side effect. Its shadow semantics are defined in L4.

Do not change existing saved settings merely to enter/leave native mode. A user who has
stock SSAO disabled must still be able to use native AO without triggering its listener
(from P2; the P1 checkpoint deliberately requires stock SSAO ON, see P1).

**Byte-identity definition (Revision 4).** Every "byte-identical OFF" statement in this
plan means *render-pipeline* identity: same inputs, same GPU/driver, pre-tonemap readbacks,
frozen stock noise, **with texture streaming frozen** (identical resident textures and
discard levels in the baseline and candidate runs, verified from the bound-texture discard
list) **and guaranteed headroom** (preparation admission refuses a plan whose reserve
would leave less than a proposed 256 MiB margin under the existing texture budget, using
the accounting at `llviewertexture.cpp:654`/`:673`, so the reservation cannot itself change
streamed discard levels in the test scene). Outside those conditions the Prepare-ON
reserve (an 8N-byte light map when none existed, plus the AO bundle) shares texture
accounting and **can** lower streamed-texture quality. That is a disclosed, user-visible
cost of Prepare ON shown in the Prepare status; it is not an OFF-identity failure. A viewer
with Prepare OFF carries no native allocation at all and is stock.

## 2. Source and licensing boundary

No `.tmp/ref_private/` content or ReShade/iMMERSE shader source was opened for this plan.
In particular, the release `reshade-shaders` directory and the user's ReShade staging
directory are outside the research boundary. Existing viewer bridge code is viewer
integration code and was inspected; proprietary effect implementation is not an input.

Permitted technical sources, consulted directly:

- [Jimenez, Wu, Pesce and Jarabo, Practical Realtime Strategies for Accurate Indirect Occlusion (2016)](https://www.iryoku.com/downloads/Practical-Realtime-Strategies-for-Accurate-Indirect-Occlusion.pdf):
  §4 defines the slice integral; §6 describes bent-normal/cone specular visibility. Use
  the mathematics independently; do not assume the paper licenses third-party code.
- [Therrien, Levesque and Gilet, Screen Space Indirect Lighting with Visibility Bitmask (2023)](https://arxiv.org/html/2301.11376v2):
  §3.1 replaces horizons with sectors, covers front/back angles and thickness, and
  explains leakage from unknown back surfaces. §3.2 motivates directional ambient
  visibility; §3.3 is the later bounce extension. The paper's temporal sampling is excluded.
- [Intel XeGTAO](https://github.com/GameTechDev/XeGTAO), especially
  [XeGTAO.h](https://github.com/GameTechDev/XeGTAO/blob/master/Source/Rendering/Shaders/XeGTAO.h)
  and [XeGTAO.hlsli](https://github.com/GameTechDev/XeGTAO/blob/master/Source/Rendering/Shaders/XeGTAO.hlsli):
  MIT source suitable for an attributed GLSL port. Its framework prepares view depth/mips,
  gathers AO/edges, and denoises spatially; bent normals are optional. It is HLSL/DX-oriented,
  not a drop-in viewer shader. Intel has discontinued maintenance.

At implementation time, pin the exact XeGTAO upstream commit and record which functions
were ported and their modifications. Preserve `Copyright (C) 2016-2021, Intel Corporation`
and `SPDX-License-Identifier: MIT` in derived files, credit Filip Strugar and Steve Mccalla,
and include the complete upstream [MIT LICENSE](https://github.com/GameTechDev/XeGTAO/blob/master/LICENSE)
in `indra/newview/licenses-win32.txt`, `indra/newview/licenses-linux.txt` and
`indra/newview/licenses-mac.txt` (all three exist). `doc/licenses/` does not exist and is
not the shipped-notice destination. The platform manifests already copy these files to
`licenses.txt` (`viewer_manifest.py:543`, `:852`, `:1121`); inspect each packaged notice in
P2 acceptance (the P1 checkpoint is a developer build and is not distributed). The licence requires copyright and permission
notices in copies/substantial portions; retain the warranty disclaimer with the complete
text. Keep existing viewer LGPL notices in existing files. Do not relabel a port as wholly
independent or import unrelated sample dependencies/assets. The bitmask implementation
will be independently written from the public paper. No licence files are added in this
planning task.

## 3. Current SSAO, end to end

### 3.1 Producer, inputs and storage

| Current behavior | Evidence |
|---|---|
| `calcAmbientOcclusion` reconstructs view position from GL depth using `inv_proj`, NDC z = `2*depth-1`, and homogeneous division. Despite the local name `pos_world`, these are eye-space positions. | `class1/deferred/aoUtil.glsl:49` |
| Eight fixed 2D offsets have squared radial spacing; divide offsets by `screen_res`. A sampled 128-pixel noise pattern reflects/rotates the kernel. Radius = `min(ssao_radius / -z, ssao_max_radius)`. | `class1/deferred/aoUtil.glsl:60`, `:77` |
| Each sample contributes an inverse-distance-squared obscurance with a 0.05 normal offset and `ssao_factor_inv` cap. The depth-difference rule changes the denominator; output is clamped visibility, one = open. This is not a horizon integrator. | `class1/deferred/aoUtil.glsl:89` |
| The 128×128 RGB16F noise texture is generated with `ll_frand()` when created. It is static during its lifetime, but not a stable cross-process seed. | `pipeline.cpp:3324` |
| `hasAmbientOcclusion` attaches `aoUtil.glsl`. Shader creation chooses `sunLightSSAOF` versus `sunLightF` from the saved SSAO flag. | `indra/llrender/llshadermgr.cpp:303`; `llviewershadermgr.cpp:3066` |
| Class-2 sun SSAO output packs **R = sun visibility, G = AO visibility, B/A = two spot shadow samples**, then clamps all channels. AO is computed here, not in the later lighting compositor. | `class2/deferred/sunLightSSAOF.glsl:52` |
| `deferredLight` is allocated at the active deferred extent, with `screenFormat` RGBA16F in HDR, GL_RGBA otherwise. Screen resolution divisor/multiplier is applied before allocation; this is not an AO-specific half-resolution buffer. | `pipeline.cpp:1349`, `:1388`, `:1448` |
| Sun pass selects the active view light target, sets its dimensions and writes the fullscreen triangle. Cube captures use a separate sun-probe program with no SSAO feature. Preview captures skip the producer. | `pipeline.cpp:27310`; `llviewershadermgr.cpp:3092` |
| A light map is not guaranteed: main allocation requires shadows, stock SSAO or DOF; Prism completeness/allocation requires stock SSAO or shadows (DOF is absent from this Prism predicate). Producer dispatch separately requires stock SSAO outside cubes, shadows or lighting debug, and excludes previews. Preview quality forces the cached SSAO flag false. | `pipeline.cpp:1448`, `:1770`, `:1830`, `:27310`; `llprismpreview.cpp:1453` |
| Uniform upload scales stock radius and maximum pixel radius for snapshots, uploads factor/inverse and builds a value/saturation matrix. | `pipeline.cpp:25149` |

### 3.2 Shared blur and ShadowDist (existing phase-1 controls)

`pipeline.cpp:27345` runs horizontal `deferredLight -> screen` then vertical
`screen -> deferredLight` only while SSAO is enabled, outside cube/preview captures
and raw/cascade diagnostics. It uses `RenderShadowGaussian`, `RenderShadowBlurSize`
and `RenderShadowBlurDistFactor`; snapshot autoscale multiplies the blur size
(`pipeline.cpp:27359`, `:27384`, `:27425`). The temporary `screen` content is not final beauty.

`class1/deferred/blurLightF.glsl:54` reads normal, position and light channels. It scales
offsets with normal/depth, perturbs the origin with a fixed pixel parity pattern, and
accepts neighbors by squared point-to-plane distance (`:67`). Each axis uses the center
plus six taps on either side (`:78`, `:88`, `:103`). Channels share locations and geometry
rejection, but `.xyxx` weights give green its own kernel weight. Thus stock AO toggling
also changes sun softening; it is not just an AO quality switch.

The existing ShadowDist phase-1 controls change **red only after the old blur** (`class1/deferred/blurLightF.glsl:121`):

- 0: stock shared kernel;
- 1: dedicated Gaussian sigma in pixels;
- 2: Gaussian sigma in world metres, projected using depth and pixels/metre;
- 3: preserve center red, no sun blur. Green still receives stock blur in every mode.

CPU clamps mode to 0..3, scales pixel sigma for snapshots, converts millimetres to metres,
and derives pixels/metre from the bound height and live projection (`pipeline.cpp:27389`).
The shader caps its sampling radius/budget and uses integer-grid red taps
(`class1/deferred/blurLightF.glsl:128`). These controls are runtime uniforms, not shader reload switches.
Their defaults are mode 0, 1 px and 2.5 mm (`app_settings/settings.xml:20817`).
They remain ineffective when the SSAO blur dispatch gate is false. L4 must preserve
their useful sun-quality choices even if it retires the coupling workaround. Until L4,
native AO never changes sun softness: with stock SSAO off the sun channel is exactly the
stock no-blur result for that configuration, with stock SSAO on it is the stock blurred
result, and native green is published *after* that blur.

`RenderDeferredBlurLight` is declared at `app_settings/settings.xml:20700`, but the audited C++ has
no matching dispatch read/listener; do not mistake it for a functioning decouple switch.
Actual blur controls are cached/listened at `pipeline.cpp:931`, `:944` and read at `:2889`,
`:2902`.

### 3.3 Composition: what gets darkened today

`class3/deferred/softenLightF.glsl:180` reads light-map R/G separately. `scol` is red,
`ambocc` is green. `adjustIrradiance` (`:137`) mixes the original irradiance with an
occluded endpoint: value/saturation transform of irradiance × scale, limited by an
absolute ceiling. Ambient tint also colors the ceiling. This is not simply `irradiance *= AO`.

| Path | Current composition and consequence |
|---|---|
| Deferred PBR | `class3/deferred/softenLightF.glsl:222`: ORM red is **material-authored AO**; green/blue are roughness/metalness. Probe irradiance and radiance are sampled at `:234`; only irradiance goes through SSAO at `:236`, followed by Shadow Lift. `pbrBaseLightKeep` at `:244` evaluates IBL and sun separately. Screen SSAO does not currently attenuate probe radiance. |
| PBR IBL | `class1/deferred/deferredUtil.glsl:718`: authored AO multiplies diffuse IBL and drives `computeSpecularAO` for specular IBL. It is distinct from screen visibility. `:1742` separates IBL, direct sun and emission. Preserve that separation. |
| Deferred legacy/Blinn-Phong | `class3/deferred/softenLightF.glsl:272`: probe ambient goes through `adjustIrradiance` at `:286`. Sun is added separately (`:294`); gloss/environment reflections are applied later (`:341`, `:351`) without screen AO. Base-color alpha fullbright mixing (`:346`) must retain its semantics. |
| Classic skies | Legacy sums ambient and sun in sRGB before converting to linear (`class3/deferred/softenLightF.glsl:299`); PBR also recombines nonlinearly (`class1/deferred/deferredUtil.glsl:1763`). Gate the unchanged direct input before this sum. The legacy `debug_direct` subtraction at `softenLightF.glsl:309` depends on ambient and is not the oracle. Changing ambient can change the apparent final sun difference; stock SSAO behaves the same. Exact final-image direct equality is impossible in this compatibility path. |
| Forward legacy alpha | `class2/deferred/alphaF.glsl:435`: probes/ambient, Shadow Lift and direct sun are evaluated, with an explicit **no SSAO** comment at `:439`. Local lights are separate (`:373`). |
| Forward legacy material BLEND | `class3/deferred/materialF.glsl:568` has its own lit forward branch: probe ambient at `:598` and explicitly no SSAO at `:599`, separate sun at `:603`, gloss/environment at `:646`/`:657`, local lights at `:692`. Non-BLEND writes the deferred G-buffer (`:768`). Include this additional alpha path in any native receiver/probe changes; changing only `alphaF` misses legacy materials. |
| Forward PBR alpha | `class2/deferred/pbralphaF.glsl:369`: probes then Shadow Lift, explicitly no SSAO; `:384` calls PBR base lighting with material AO. Local lights use the forward helper (`:332`). |
| Shared Actor FX PBR replay | `class2/deferred/sharedActorFxPbrF.glsl:597`: OPAQUE/MASK use opaque probe policy, BLEND uses transparent policy; all currently bypass screen SSAO (`:610`). PBR beauty, authored emission and creative treatment have distinct ownership (`:621`, `:632`). An OPAQUE replay can therefore differ from the deferred original unless native AO is integrated here too. |

The key advantage over final-image AO already exists in the viewer's irradiance path.
Improve its visibility and add probe-only specular occlusion; do not introduce a beauty
multiply. **Revision 4:** because the visible win otherwise sits behind this curve,
P1 provides a *selectable* ambient response: 0 = stock `adjustIrradiance` curve
(default, interpretable A/B), 1 = native linear, `irradiance *= mix(1, visibility,
amount)` applied to the ambient/probe irradiance only, before Shadow Lift. Exactly one
response applies; never both stacked. Both leave sun, local lights, emission and rim
untouched.

**Why the response decides the visible win.** At full occlusion the stock curve
(`softenLightF.glsl:137`) returns only `ssao_effect_mat * min(irradiance * scale,
ceiling)`, a bounded darkening of the *ambient term alone* (defaults: scale 0.6, ceiling
0.18, value 0.8). In a sunlit scene ambient is a minor share of the final pixel, so native
AO behind the stock curve can look almost unchanged next to a reference that darkens the
final image. P1 measures this honestly with sunlit and interior A/B before P2 is funded;
if ambient-only AO is not enough in sun, that is a user decision (D1 below), not a reason
to multiply direct light.

### 3.4 Complete SSAO setting and UI inventory

These are the six actual `RenderSSAO*` settings plus the deferred master. All line
references in the declaration column are `app_settings/settings.xml`.

| Setting | Type/default; declaration | Live plumbing / persistence |
|---|---|---|
| `RenderDeferredSSAO` | bool true; `:20688` | Persist 1. Cache signal `pipeline.cpp:896`, read `:2854`; **shader reload listener** `llviewercontrol.cpp:1237`. Handler refreshes caches and calls `setShaders()` (`:216`). Changes producer selection and `HAS_SSAO` compositor permutation (`llviewershadermgr.cpp:3066`, `:3661`). |
| `RenderSSAOScale` | F32 500; `:18778` | Persist 1. Runtime cache signal `pipeline.cpp:932`, read `:2890`, radius upload `:25155`. Pixels at 1 metre, not a physical radius in metres. |
| `RenderSSAOMaxScale` | U32 200; `:18811` | Persist 1. Signal `:933`, read `:2891`, pixel-cap upload `:25156`. |
| `RenderSSAOFactor` | F32 0.30; `:18822` | Persist 1. Signal `:934`, read `:2892`, factor/inverse upload `:25158`. |
| `RenderSSAOEffect` | Vector3 (0.8, 1, 0); `:18833` | Persist 1. Value/saturation, unused third component. Signal `:935`, read `:2893`, matrix upload `:25162`. |
| `RenderSSAOIrradianceScale` | F32 0.6; `:18789` | Persist 0. `LLCachedControl` in lighting pass `pipeline.cpp:27709`, uploaded every bind. No shader reload listener found. |
| `RenderSSAOIrradianceMax` | F32 0.18; `:18800` | Persist 0. Same live path; ceiling additionally scaled with EnvIntensity probe-diffuse factor at `pipeline.cpp:27725`; tint occurs in shader. |

Only the master has the SSAO shader-reload listener; scalar/vector controls refresh
cached state or are cached directly. Reserved uniform enum/string order is
`indra/llrender/llshadermgr.h:186` / `indra/llrender/llshadermgr.cpp:1479`.

UI/preset surfaces:

- Preferences Advanced has `UseSSAO`, bound to the master
  (`skins/default/xui/en/floater_preferences_graphics_advanced.xml:793`). Capability
  gating is in `llfloaterpreferencesgraphicsadvanced.cpp:393`, `:427`, `:465`.
  Feature tables enable it for higher graphics tiers and can disable availability
  (`featuretable.txt:65`, `:253`, `:404`, with Linux/Mac counterparts).
- Director World > Shadows has the advanced SSAO toggle and the explicit warning about
  shadow smoothing (`skins/default/xui/en/panel_dpage_world_shadows_probes.xml:20`).
  Softening modes are at `:37`, pixel/mm sliders at `:52`/`:57`, shared blur size at
  `:199`, Lightbox link at `:209`. Search entries are in
  `skins/default/xui/en/director_kit_index.xml:2135` and `:2138`.
- Lightbox SSAO controls occupy `skins/default/xui/en/floater_lightbox_settings.xml:2891` through the scale
  controls at `:3160`; indexed at `skins/default/xui/en/director_kit_index.xml:2370`. Its
  `RenderSSAOEffectValue` / `RenderSSAOEffectSat` bindings (`skins/default/xui/en/floater_lightbox_settings.xml:2916`,
  `:2973`) **do not have matching declarations or C++ adapters in this audited tree**.
  Treat these as unresolved legacy UI bindings, not additional working settings.
  P3 must bind native appearance values to declared controls and test them end to end.
- Phototools links to Lightbox for full SSAO control
  (`skins/default/xui/en/floater_phototools.xml:353`); it is not a separate native AO panel.
- Graphics presets explicitly include the old master (`llpresetsmanager.cpp:315`, `:324`).
  Native quality needs deliberate inclusion; a new setting is not saved automatically
  everywhere just because `Persist=1`.

## 4. G-buffer and view inputs

| Input | Available representation; integration rule |
|---|---|
| Depth | `deferredScreen.allocate(..., true)` at `pipeline.cpp:1385`; default `DEPTH_FMT_24` in `indra/llrender/llrendertarget.h:92`, allocated as `GL_DEPTH_COMPONENT24` by `indra/llrender/llrendertarget.cpp:130`, `:430`. Ordinary GL depth, clear/far = 1, not reverse-Z. Shared with scene screen at `pipeline.cpp:1443`. |
| Reconstruction | `class1/deferred/deferredUtil.glsl:656` reconstructs with `inv_proj`, NDC `2*d-1`. Linear distance is `-view.z`. Use active view projection including asymmetric tile/lens projections; do not copy DX depth constants/signs into GL. Test near/far, off-axis, divisor and odd extents. |
| Base color | Attachment 0 `GL_SRGB8_ALPHA8` (`pipeline.cpp:1385`); legacy alpha carries lighting/fullbright semantics. Never repurpose it for AO. |
| Materials | Attachment 1 GL_RGBA (`pipeline.cpp:689`): legacy specular/gloss or PBR ORM. `class1/deferred/gbufferUtil.glsl:36` decodes the layout; `class1/deferred/pbropaqueF.glsl:146`, `:213` writes ORM. No per-object UUID/material-thickness ID exists in this G-buffer. |
| Normals | Attachment 2 is **GL_RGBA16 normalized**, not RGBA16F, in HDR; non-HDR `GL_RGB10_A2` (`pipeline.cpp:689`). Octahedral **view-space shading normal** in XY, environment/AvatarLightResponse carrier in B, flags in A (`class1/deferred/globalF.glsl:55`; `class1/deferred/gbufferUtil.glsl:52`). `decodeNormal` does not initialize W (`class1/deferred/globalF.glsl:80`); fetch raw flags separately. |
| Geometric normal | Not stored separately. PBR normal maps are applied through TBN before writing (`class1/deferred/pbropaqueF.glsl:135`, `:144`), with front/back flip at `:190`. Reconstruct an edge-aware macro normal from depth for bias/validity; use shading normal for the lighting hemisphere, with a bounded correction policy. Do not interpret a shading normal map as physical occluder relief. |
| Flags / actor identity | Flag values and optional tag decoding are injected at `indra/llrender/llshadermgr.cpp:697`, `:718`. Avatar tag is HDR-dependent (`class1/deferred/globalF.glsl:63`); not a universal actor mask or material ID. Use helpers, never hard-coded comparisons to packed alpha. |
| Emissive | Optional attachment 3, RGBA16F HDR / GL_RGBA otherwise (`pipeline.cpp:689`). Alpha can carry silhouette metadata. Do not steal it for bent normals. |
| Velocity | `mVelocityMap` RG16F, separate geometry pass sharing opaque depth (`pipeline.cpp:1585`, `:8483`; `pipeline.h:1698`). It is not an always-valid MRT, not owned by every feed. AO must not allocate or sample it; no history/reprojection. |
| Half resolution | No reusable half-res AO buffer exists. Relight has per-pack `areaGather/areaScratch/areaVisibility` (`pipeline.h:1361`) and ceil-half extent logic (`pipeline.cpp:26960`). Projector shafts have separate half/history targets (`pipeline.h:1591`, `:1612`). Reuse allocation/math patterns, **never alias live targets or temporal histories**. |

Opaque and alpha-tested fences/foliage appear in deferred depth/normals. Alpha mask
discard and normal write can be seen in `class1/deferred/diffuseAlphaMaskF.glsl:42`.
Blended hair/lace do not become full opaque occluders by virtue of appearing in beauty.
An AO bitmask fixes height-field over-occlusion where depth exists; it cannot recover
missing transparent layers, backfaces or offscreen geometry.

## 5. Ambient, reflection probes and double-counting

Current class-3 probe sampling separates diffuse and glossy directions:
`class3/deferred/reflectionProbeF.glsl:763` samples ambient with the normal, radiance
with reflection direction and roughness LOD, then mixes SSR (`:783`) and hero probes
(`:802`). Legacy has separate gloss/environment outputs (`:879`) and the same replacement
ordering (`:911`). Probe diffuse includes `sky_gi_scale`/ambient tint and ambiance mixing
(`:563`); specular EV is applied before SSR/hero replacement (`:778`). Class-2 fallback
uses ambient blending and an environment cube (`class2/deferred/reflectionProbeF.glsl:46`,
`:77`); test it explicitly when real reflection probes are disabled.

Do not conflate these existing mechanisms with native local AO:

- Authored ORM AO and its `computeSpecularAO` response are material-scale data
  (`class1/deferred/deferredUtil.glsl:736`). They remain active exactly once.
- Probe `refParams` contain ambiance, radiance scale, fade and near-z
  (`llreflectionmapmanager.cpp:3362`). The `.z` weight is fade-in
  (`class3/deferred/reflectionProbeF.glsl:495`), **not a hidden screen-AO term**.
- `LLReflectionMap::doOcclusion` is a bounding-volume GPU occlusion-culling query
  (`llreflectionmap.cpp:316`), not per-receiver specular visibility. Do not disable it.
- Captured probe light already contains scene-scale visibility. Additional AO must be
  short-range; screen AO stays excluded from probe baking to prevent feedback/double bake.

P4 hook design:

1. Carry **shading N, diffuse sampling bent N and visibility-cone data separately**.
   Never pass bent N as the one normal argument to the existing probe routine: that
   would also bend the specular reflection vector and modify SSR geometry.
2. Native probe overload samples irradiance using bent N, keeping existing influence,
   tint, ambiance and EV logic. Apply scalar native visibility once on this irradiance.
   Select either stock-compatible response or native response, not both.
3. Calculate a bounded roughness/view/cone specular factor from bent direction + open
   visibility. Multiply **probe/environment radiance before SSR/hero replacement**.
   Leave punctual BRDF normals, sun visibility and local-light data untouched.

   **Hero-mirror / SSR policy (explicit, Revision 4).** Native AO attenuates (i) ambient
   irradiance and (ii) the *non-replaced probe radiance* only. Radiance supplied by an
   SSR hit or a hero-probe replacement is **not** attenuated: an SSR hit is real beauty
   that already carries its scene occlusion, and a hero probe is a separate render of the
   scene. The goal "AO applies to reflection probes" therefore means ordinary
   parallax-corrected probes plus the probe share of a partially replaced pixel:
   `mix(probe * specFactor, replacement, weight)`, so the factor drops out as the
   replacement weight reaches one. P4 gates partial weights 0/0.25/0.5/1 and the status
   text states the limitation. Extending occlusion to hero/SSR needs separate evidence and
   is not scheduled; never multiply the already mixed radiance.
4. Preserve authored material diffuse/specular AO. Native specular visibility applies
   once independently; do not also feed `materialAO * nativeAO` into
   `computeSpecularAO`, which would count native AO twice. Correlated baked cavities
   can still over-darken; compare authored AO=1 versus cavity maps and keep native radius
   short. A combined-visibility heuristic would be an explicit later look decision.
5. With visibility=1, bent direction must equal shading N and native specular factor=1.
   With near-zero open moment, use N and zero/bounded visibility, never normalize zero.
   A single bent cone cannot represent multiple separate openings perfectly; do not
   promise the complete directional lighting result of a stored per-sector distribution.

**Separate legacy-path design:** `sampleReflectionProbesLegacy` returns `glossenv` and
`legacyenv` after SSR/hero mixing; they are later applied to lit color at
`softenLightF.glsl:341` and `:351`. Do not multiply these returned mixed values by AO.
The native overload applies the cone factor independently to the original linear
gloss probe sample after its EV scale (`class3/deferred/reflectionProbeF.glsl:895`),
and to the original linear environment sample after its EV scale (`:904`), before
the two SSR mixes (`:926`, `:927`) and hero replacements (`:933`, `:934`). Preserve
the shading-normal reflection vector and the original replacement weights. Bent N
changes only the ambient sample. Retain separate factors for gloss LOD and the
legacy sharp environment lookup; specular visibility is not diffuse AO copied twice.

Keep `applyGlossEnv`'s Fresnel, specular color and existing color-dependent energy
heuristic (`:940`) and `applyLegacyEnvKeep`'s authored `envIntensity` mixing weight
(`:954`) unchanged. The class-2 fallback stores legacy environment in sRGB
(`class2/deferred/reflectionProbeF.glsl:92`): in its native overload decode the probe
sample, attenuate in linear space, then re-encode before the original legacy mix.
Factor=1 bypasses the new conversion for exact identity. Cover deferred legacy,
forward legacy alpha/material receivers if enabled, and class-2 probes-disabled skies.
P4 must isolate probe-only tests with ambient held equal: the legacy color-dependent
heuristic and nonlinear environment mix mean a final-beauty subtraction cannot prove
that an SSR/direct input changed. Direct BRDF inputs and SSR/hero replacement inputs
must be unchanged independently.

Shadow Lift currently follows SSAO (`pipeline.cpp:27709`; `class1/deferred/deferredUtil.glsl:1728`). Keep
that order initially, including EnvIntensity ceiling scaling. Do not darken the whole
post-Shadow-Lift beauty or rig rim to compensate for an exposure mismatch.

## 6. Relight reuse and thickness

The Relight brief is historical design; this checkout **already implements** area
shadow gathering/filtering. Follow actual source and the Step 6 audit, not its original
"not built" statement.

Reusable concepts:

- `class1/deferred/screenSpaceRayUtil.glsl:10` snaps to the full-depth texel actually read,
  including half-res/odd-extent ties; `:24` gives current-view linear depth; `:33` computes
  receiver footprint at receiver depth. Port/generalize these concepts for AO without
  pulling in unnecessary light-specific uniforms.
- Its tracer validates finite penetration, source exclusion and a refined surface hit
  (`:44`, `:69`). AO is a slice gather, not a per-light trace; do not run this entire
  tracer for every sector.
- `class3/deferred/areaShadowFilterF.glsl:12` has depth/normal bilateral rejection;
  `:44` upsamples using actual full-res sample positions; `:61` handles zero weights.
  Useful validation patterns, but XeGTAO's denoiser and edges need their own implementation.
- `alareashadowmath.h:16` centralizes view policy; `:65` defines quality; `:78` starts a
  fixed-work plan, reducing work deterministically; `:118` counts actual feed passes;
  `:134` reserves main work before assigning feed allowances. Use an AO-specific pure
  policy/math header instead of coupling AO quality to area-light channel count.
- `pipeline.cpp:26987` polls asynchronous timestamps without blocking. Copy its
  nonblocking principle, not the main-view timer/query ownership.

**Thickness must be separate.** `RenderAreaShadowThickness` defaults to 0.35 m
(`pipeline.cpp:26784`); in the ray helper it tolerates a front-depth crossing plus step
length and validates a plane hit. Bitmask thickness models the **back extent of each
sample** and determines an occupied angular interval. Same units do not imply the same
meaning. Reusing 35 cm would make fingers, railings and hair cards behave like thick slabs.
Sharing a setting would also let an AO look unexpectedly alter light shadows.

Propose `RenderNativeAOThicknessM = 0.03 m` as a **calibration starting point**, range
0.001..0.5 m, finite/clamped, with a separate receiver bias. Sweep 0.01/0.03/0.10/**0.30** m
across SL avatar and architectural scales before selecting a shipping default (Revision 4
extends the sweep to 0.30 m because sparse samples under-occlude at small thickness, see
6.1). Start constant
in metres; distance-scaled thickness is an optional separately named parameter, default
zero, because it changes the look as the camera moves. A single front depth cannot
identify the true thickness of an arbitrary mesh.

### 6.1 Bias, sparse-sample porosity and depth precision (Revision 4)

**Porosity is bias, not noise.** With 2-8 samples per side per slice, each sample marks
only the sectors its finite thickness subtends. At 3 cm thickness and sample spacing of
centimetres to decimetres, a continuous wall, torso or floor is represented by isolated
slivers; the OR-union leaves most of the hemisphere unmarked, so AO is *systematically too
weak* regardless of how well the variance is controlled. The sigma model cannot see this.

Mitigations, each selectable and each judged by P0/P1 evidence rather than assumed:

- **Sample-gap linking.** When consecutive samples on one slice side lie on one continuous
  surface (depth difference consistent with the predicted slope of the macro normal, within
  a tolerance of one depth step plus the thickness), fill the sectors between the first
  sample's front angle and the second's before applying thickness. Discontinuities (depth
  jump larger than the tolerance) do not link, so railings and finger gaps stay open.
- **Thickness floor.** For *linked* runs, `t_eff = max(t, step spacing in metres at that
  depth)`. A universal floor is rejected: it would turn railings and hair cards into slabs.
- Horizon-equivalent mode (thickness to infinity) is a diagnostic for comparison and for
  the conditional Low preset only.

**Per-preset bias gates (every advertised preset, not only High).** Against a numerically
integrated reference, at 1/3/10 m, full and half resolution, 1080p and 4K:
a corner (two perpendicular planes) mean absolute AO error <= 0.05; a floor beside a wall
at distances 0.1-1 m <= 0.05; an *arm against torso* fixture (4.5 cm cylinder beside a
15 cm cylinder, gap 0-3 cm) mean absolute error <= 0.07 and darkness (1 - AO) at touch >= 70%
of the reference; thickness sweep 0.01/0.03/0.10/0.30 m reported for each. A preset that
fails a bias gate is relabelled, reworked or withheld; it is never excused by its noise gate.

**Depth precision.** The deferred depth is GL non-reverse-Z `DEPTH_COMPONENT24`
(`indra/llrender/llrendertarget.cpp:130`). The linear step at distance `z` is
`dz ~= z^2 / (near * 2^24)` for `far >> near`; with a 0.1 m near plane this is ~0.6 cm at
100 m and ~3 cm near 220 m. Use the live near/far from the projection, and:

- `bias_eff = max(bias, 2 * dz(z))` and `t_eff >= dz(z)` (a sample cannot be thinner than
  the depth quantum);
- **distance fade:** multiply the resolved visibility toward 1 with
  `w = 1 - smoothstep(0.5, 2.0, dz(z) / t)`; at the 3 cm default AO begins to fade near
  ~160 m and is fully off near ~320 m, preventing banded terrain;
- gate: terrain/plane fixtures at 50/100/220/400 m show no banding (AO standard deviation
  within the open-plane gate) and a monotonic fade.

## 7. Fork interaction policy

| Feature | Current hook / evidence | Native AO policy |
|---|---|---|
| Cine Light Rig and Rig Rim | Rig/ActorMask lighting dispatch in `pipeline.cpp:27818`, `:28127`; rim data/helpers `class1/deferred/deferredUtil.glsl:1064`; `alcinelightrigmanager.cpp:879` identifies projector/bounce emitters for rim. `alcinerigrim.cpp:149` gates rim in HUD, probe and alpha passes. | Do not attenuate direct key/fill/rim/background, their synthetic bounce lights, or rim overlays with AO. Ambient/probe illumination on their receivers can change. A light called "bounce" is still evaluated as a local light here. `CineRigRimIncludeProbes` controls whether rim is rendered into probe captures; it is not an extra probe-lighting input. Keep those captures AO-free. Their captured rim contribution can subsequently receive ordinary indirect probe occlusion at a receiver. |
| Actor Mask | Per-view classification/validity in `pipeline.cpp:27194`, `:27234`, actor-only receiver routing at `:28127`; separate depth-sharing mask lifecycle at `:1371`, `:1444`. | Default world AO applies to opaque avatars as ordinary receivers/occluders. The light-recipient mask is not a blocker-depth mask. A future actor exemption can return visibility=1 for recipients, while retaining their occluder depth. Never remove world depth based on actor bits. HDR tag alone is insufficient for animesh/custom targets. |
| Actor FX | Deferred PBR changes material normal/ORM/emission (`class1/deferred/pbropaqueF.glsl:137`, `:164`, `:192`); shared replay path `class2/deferred/sharedActorFxPbrF.glsl:597`. | Physical shaded surfaces receive indirect AO. Cover/sensor/flat/emissive treatments retain their authored overrides; do not AO-multiply creative post-light color or emission. Replay OPAQUE/MASK actors enter the AO **input** (depth + normal overlay, 8.1) and consume AO in P2; BLEND follows the alpha policy below. |
| Tron | Scene-layer dispatch and depth/state rules in `pipeline.cpp:8854`, `:8886`, `:8914`; avatar flag encoding `class1/deferred/globalF.glsl:63`. | AO may change the physical surface behind Tron. Do not attenuate neon/emissive grid, trails, rim tint/pulse, or screen-space graphics. Keep raw normal flags/sidecars intact. |
| Roto Ink / silhouette | Outline Scene layer at `pipeline.cpp:8834`, `:8919`; other layer handling `:15187`; silhouette carrier decode `class3/deferred/softenLightF.glsl:168`. | Do not alter outline extraction inputs or post outline colors. Shade physical fill before its creative overlay. Preserve silhouette K/R, fullbright and emissive policy; zero-emission and flat-fill fixtures must retain their promised treatment. |
| VCam / Prism camera feeds | Exact-size pool at `pipeline.cpp:1746`; per-view save/invalidate at `:11479`, restore at `:11668`. Relight Step 6 documentation `doc/director_kit/RELIGHT_SL_INTEGRATION.md:788`. | The core phases (P0-P4) are main view only; feeds keep their existing stock AO policy and the status says so. L2 adds native feed opt-in with pack-local AO resources, view state and fixed allowances. Never sample main AO in a feed. Pool storage is per size bucket, reused sequentially, not permanently one allocation per camera ID. |
| Preview atlas / media / Gate | Preview quality forces SSAO/shadows off and restores them (`llprismpreview.cpp:1443`); raw G is neutralized in `pipeline.cpp:27293`. | Preview remains AO-neutral. Media has no new depth. Gate camera sources run AO only when actually rendered in an eligible scratch view, not again at display composition. |
| HUD | Deferred lighting asserts `!sRenderingHUDs` (`pipeline.cpp:27246`); Relight policy rejects HUD (`alareashadowmath.h:16`). | Explicitly reject `sRenderingHUDs` at AO allocation admission, dispatch and consumer validity. HUD gets visibility 1 and cannot consume retained world AO even if it shares a target. |
| Hero-mirror pass / cube captures | Hero swaps to `mHeroProbeRT` (`llheroprobemanager.cpp:309`); mirror identity is `isMirrorPass()` (`llheroprobemanager.h:88`); Actor Mask separately recognizes it (`pipeline.cpp:27199`). | Reject `gCubeSnapshot`, hero pack identity and `isMirrorPass()` independently; hero mirrors are not opted-in VCam feeds. No native AO/bent sampling in captures, no feedback baking. Retain the stock capture sun path. Main-view hero replacement inputs stay unoccluded by probe-only AO. |
| Impostors | `generateImpostor` sets `sImpostorRender` (`pipeline.cpp:32753`) and renders into avatar-owned targets (`:32900`). | Reject generation/update passes; no native allocation, gather, publication or consumer lookup. An impostor drawn into the ordinary world may receive the world depth-based policy, with its billboard-depth limitation disclosed; do not bake native AO into its cached image. |
| Water reflection renders | `sReflectionRender` exists at `pipeline.cpp:658`; it is also used by forward impostors (`:32750`). Relight rejects reflection views (`alareashadowmath.h:16`). | Reject reflection flag and any water reflection camera/target route, never infer main view from a false flag alone. This checkout has no separate active water-reflection flag setter beyond the impostor route; use a synthetic reflection-view dispatch fixture as well as real available routes. Main-view water has a dedicated post-deferred pool (`lldrawpoolwater.cpp:128`) and dedicated probe consumers (`class3/environment/waterF.glsl:311`, `:313`); it does not inherit changes to `alphaF`, `pbralphaF` or `materialF`. It stays **neutral and unchanged** in every phase of this plan (a later water receiver is its own family, see the alpha policy). Reflection captures never reuse main AO. |
| DOF scratch reuse | CoF overwrites `deferredLight` at `pipeline.cpp:23727`; DOF samples it at `:23780`, `:23802`. | Finish all light-map consumers before CoF; invalidate published-green lifetime before that overwrite. Keep `aoResolved` separate and valid only for its owning lighting view. Reinitialize R/G/B/A on every later native lighting dispatch; DOF scratch must never be mistaken for last-frame visibility. Do not allocate/rebuild the screen pack for an AO toggle. |
| High-resolution stills | Snapshot autoscale set/reset `llfloatersnapshot.cpp:778`, `:1343`; projection/pixel kernel handling `pipeline.cpp:27364`, `:27407`. | The single-target route reallocates the screen pack to the still's extent (`llviewerwindow.cpp:6340`), renders immediately (`:6377`) and reallocates the original extent (`:6413`). Core phases force **stock AO for the whole shot** with an explicit status and, on exit, re-prepare the interactive generation outside any take (a snapshot is already a hitch). L3 adds the snapshot preparation transaction (admit and prepare the actual still extent, count snapshot scratch and restoration overlap in the peak, whole-shot stock fallback on failure, reject incompatible requests during a pinned take). Physical AO radius/thickness are never multiplied by snapshot autoscale; only a reference-pixel cap/filter width uses full output dimensions if needed. Test 1×/2×/4× framing in L3. |
| Tiled snapshots | `llviewerwindow.cpp:6003`, `:6143` loops subimages through `display`; Relight explicitly rejects zoomed tiles (`pipeline.cpp:26619`). | Initially fall back to stock for the **entire tiled shot**, with explicit status. Later use padded tiles with overlap ≥ projected AO reach + all filter support, crop interiors, use global output-pixel noise coordinates. No per-tile unpadded native AO: neighboring off-tile occluders are absent. |
| ReShade bridge | Depth and preserved normal/albedo sidecars published in `llreshadebridge.cpp:355`; preserve-copy reason `:376`. ABI 1.2 fields/encoding at `llreshadebridgeabi.h:76`, `:162`, `:179`; generation tracking `llreshadebridge.cpp:469`. | Preserve sidecars and layout. Recommend native AO with external MXAO/RTGI AO disabled by the user to avoid stacking. Native AO export is optional later ABI work; do not overwrite ORM red or encoded normal channels. No automatic manipulation of proprietary effects/settings. |

### Alpha policy: coverage is a real limitation

- **Core phases (P1-P4):** deferred OPAQUE and MASK (alpha-tested) surfaces, and shared
  Actor FX replay OPAQUE/MASK actors, cast and receive AO. A MASK hole stays a hole; test
  both sides of two-sided cards.
- **BLEND neither casts nor receives** and stays on its current unoccluded-indirect path,
  disclosed in status/help. In this fork `RenderAutoMaskAlphaDeferred` defaults to **0**
  (`app_settings/settings.xml:20961`; its comment says textures keep their authored alpha
  mode and the per-object Alpha Mode override masks manually). So every texture with an
  alpha channel in blend mode stays BLEND: much SL hair, lashes, lace, clothing and
  foliage. That undercuts a "thin content" pitch, so **P0 measures the BLEND pixel
  fraction** in the user's scenes, and the documented ways to widen coverage are the
  auto-mask setting and per-object Alpha Mode (masking) where the look allows.
- **L1 (later) BLEND receivers, indirect terms only.** Reuse resolved visibility only when
  the fragment's depth/normal match the opaque receiver within a footprint-scaled tolerance,
  else visibility = 1 (so wall AO never paints foreground hair). Visibility multiplies
  **only the indirect terms** - ambient/probe irradiance and probe specular - *before* they
  are summed with direct terms. It never multiplies the final colour, direct sun, local
  lights, projector, rig, emission or rim (`softenLightF.glsl:294` keeps sun separate and
  forward paths must keep that separation). Hook points, enumerated per family:
  `class2/deferred/alphaF.glsl:435` region, `class2/deferred/pbralphaF.glsl:369`,
  `class3/deferred/materialF.glsl:568`-`:599` (legacy material BLEND), and the four shared
  Actor FX BLEND replay companions. Independent opt-in until parity passes.
- **Water** is out of scope and unchanged. If a future experiment includes it, enumerate its
  shader variants, match receivers at the water fragment's *geometric* coordinates (its
  distorted probe-lookup coordinates must not become AO lookup coordinates), and budget
  and test that family separately.
- True alpha occlusion needs a coverage/depth representation (possibly another layer);
  defer it with its own bandwidth and transparency policy rather than silently changing
  depth writes, alpha cutoff or blending.

## 8. Proposed native passes and resources

Use fullscreen raster GLSL passes first, fitting existing render-target/shader binding
patterns and avoiding an additional compute-only hardware requirement. XeGTAO's math
and denoiser can be MIT-ported; its DX UAV dispatch and coordinate conventions cannot.
Keep uint bit operations at the supported GLSL level, with a bounded-loop fallback for
population count if required by the viewer's shader version. A compute backend is later
optimization, requiring identical output gates.

Order: deferred geometry -> existing sun/shadow producer and its required blur -> native
depth prepare -> slice gather -> spatial filter/resolve -> green-channel publication ->
ambient/sun compositor -> local lights -> forward/replay consumers -> existing post.
Depth prep may execute earlier after geometry if dependencies permit; do not move beauty
or bridge-preservation ownership accidentally.

| Resource / pass | Proposed format, size and contract |
|---|---|
| `aoDepth` prepare | R32F linear **positive -view.z**, full extent, five levels initially. Write per-mip explicitly using depth-aware reduction; do not call generic averaged `glGenerateMipmap` for geometry depth. Background sentinel is explicit and never a real blocker. Use base-level depth for finest samples/edge tests. From P2, when replay proxies are active the prepare pass takes the nearer of the deferred depth and the AO input overlay (8.1). |
| Gather (all phases) | One gather program selected by uniform. Inputs: active depth/mips, shading normal (or overlay normal at replay pixels) and depth-derived macro normal, projection/extent, radius/thickness/bias, noise anchoring (9.1). Per slice a register-local 32-bit occupancy mask of the normal-centered angular hemisphere; the finite front/back interval comes from the sample position and assumed thickness behind it along the camera ray, with sample-gap linking and the thickness floor of 6.1. Clip intervals, convert cosine to angular space, classify sector overlap and OR occupancy. Do not reinterpret cosine directly as uniformly spaced angle. Thickness tending to infinity reproduces horizon behaviour (diagnostic, conditional Low). Outputs visibility in [0,1] plus packed edge confidence. |
| `aoRaw`, `aoScratch` | R16F each for scalar AO at full or ceil-half dimensions. Edge metadata R8 at gather extent. Denoise raw -> scratch (-> raw -> scratch) for the **preset's pass count (2-4)**; resolve reads the final ping-pong target. The Revision 3 three-pass floor is withdrawn (9.2); total filter support is capped at +-4 full-res px. Never sample and render the same image. Retain bilateral/edge validity at each step. |
| `aoResolved` | R16F full extent. Joint depth/normal upsample when gather is half resolution. At an edge with no valid source, return neutral visibility instead of spreading foreground occlusion across the background. An optional full-res refinement at depth edges must be separately budgeted. |
| AO input overlay (P2) | Native-only, only when shared-replay proxies are planned: R32F linear depth + RG16F octahedral view normal + private depth buffer, full extent (8.1). Not aliased with any live target. |
| Bent variant (P4) | Replace raw/scratch/resolved scalar formats with RGBA16F: XYZ unnormalized open-direction moment in view space, A scalar visibility. Filter moments, normalize after resolve; do not blur oct-encoded XY directly. Full-resolution resolved result is consumed by native probe shaders. Edge texture remains R8. |
| Publish | Green-only draw into the active `deferredLight`, sampling the separate resolved target, **after** the stock shared blur. Save/restore indexed/global color masks and all other GL state. R/B/A are not read-modify-written or rounded again. Non-HDR applies the deterministic dither of 8.2. This preserves the established AO slot without a new G-buffer layout. |
| Consumer (P1 checkpoint) | Existing soften program with a uniform selecting the ambient response (stock curve / native linear, 3.3). Requires stock SSAO ON so `HAS_SSAO` and the stock green exist. |
| Consumer (P2) | Precompiled native soften companion handles configurations where stock `HAS_SSAO` is absent; the original compositor is selected whenever native is OFF. Replay OPAQUE/MASK consumer companions (8 programs). |
| Consumer (P4) | Separate native probe entry points and precompiled native lighting programs. Material normal, authored AO and direct paths retain original roles. Never attach bent data to existing material/bridge channels. |

Use the receiver's view-ray direction for the assumed back extent, with GL eye-space
signs verified against a constructed slab; a fixed camera -Z offset is not equivalent
at the image edges. Thickness is a reconstruction assumption, not actual mesh volume.

Depth mip safety is important for the bitmask: a coarse filtered depth cannot represent
all narrow gaps in its footprint. Use conservative sample-LOD limits near discontinuities,
nearest source association and fine-level fallback/refinement. Validate thin fixtures at
each LOD; XeGTAO horizon heuristics cannot be assumed correct for finite interval masks.
The P1 checkpoint should first pass with LOD0, then re-enable mip sampling behind those gates.

Integrate slice contributions with the appropriate projected-normal/cosine weights.
Use analytic angular-sector weights (or a separately verified approximation), not an
unexamined `1 - bitCount(mask)/32` transplanted into the GTAO integral. A CPU numerical
hemisphere oracle establishes the convention: one means unoccluded. Compute bent moments
from **open** sectors using the same measure. Do not reuse the closed-form two-horizon
bent-normal formula after switching to a disconnected mask.

All shifts must be unsigned and defined for empty/full intervals: handle a 32-sector
span explicitly rather than shifting by 32. Clamp/validate inputs, acos arguments,
projection division, radius and normals. Empty/sky/outside-view samples are unknown/open,
not black borders or wraparound blockers. Missing information can leak; it must not NaN.

### 8.1 AO input geometry for shared-replay actors (Revision 4, from review C1)

**Problem at HEAD.** When shared Actor FX replay owns an actor, native PBR beauty is
suppressed in the deferred pass (`lldrawpool.cpp:1123`, `:1159`), so the deferred
depth/normal the AO gather reads shows the *background* where the actor stands. The replay
path primes depth only later, while draining alpha (`lldrawpoolalpha.cpp:1265` ->
`LLActorMover::renderSharedActorStyleDepthPrepass`, `llactormover.cpp:17692`), after the
proposed gather, and its shader `class1/deferred/sharedActorFxPbrDepthF.glsl:86` carries
coverage/depth only, not a shading normal. Adding eight replay *consumer* companions
therefore cannot give casting/receiving parity: the gather would still see through the
actor. Stock SSAO has the same blindness, so the P1 checkpoint is not a regression, but
P2 must not claim parity without this strategy.

**Strategy A (P2, preferred): AO-owned input overlay.** Before AO depth prepare, if
`hasSharedActorStyleProxies()` (zero cost otherwise), render the same replay OPAQUE/MASK
proxy draws, with the same rigging palette, MASK/dissolve discard rule and two-sided
normal flip as the depth prepass, using four AO-input companion programs (alpha OPAQUE/MASK
x unrigged/rigged, forked from `sharedActorFxPbrDepthF/V`) into the overlay: R32F linear
depth, RG16F octahedral view-space normal (geometric normal; a replay normal map is not
reproduced, so the hemisphere uses the geometric normal at those pixels) and a private
depth buffer for actor-vs-actor ordering. Depth prepare then takes the nearer of scene and
overlay depth; the gather and the replay consumers read overlay normal where the overlay is
nearer. World pixels beside an actor see the actor as an occluder and vice versa
(mutual occlusion). Existing depth-prime ownership and ordering are untouched.

**Strategy B (fallback if A exceeds its budget): depth-only overlay** (R32F + private depth;
8N bytes) with the macro normal reconstructed from depth at replay pixels; no new normal
output and two fewer programs.

**Budget (proposed, RTX 5090):** overlay pass <= 0.15 ms at 1080p with four typical
avatar-class replay actors, and <= 0.3 ms p95 including the prepare-pass change in the
actor-heavy fixture; zero GPU work and zero draw calls when no proxy is active; draw count
<= that of the existing depth prepass. Extra memory 12N bytes (A: 23.7 MiB at 1080p / 94.9
MiB at 4K) or 8N bytes (B: 15.8 / 63.3 MiB), charged in the admission ledger and prepared
at Prepare time (never allocated when an actor first appears).

**Gates:** overlay depth within one depth step and normal within 2 degrees of the same actor
rendered through the deferred path (geometric-normal comparison for normal-mapped
materials); MASK holes and dissolve identical to the depth prepass; two-sided and rigged
cases; background sentinel never visible through an actor; mutual actor/world occlusion at a
wall corner next to the actor; replay turning on/off mid-view causes zero allocations and no
stale-validity frame. **Mutations:** skip the overlay; order it after the gather; ignore
dissolve; drop the rigging palette; each must fail its named fixture.

### 8.2 Light-map precision (Revision 4, from review O8)

`deferredLight` is `GL_RGBA` (8-bit) unless HDR is on, where it is RGBA16F
(`pipeline.cpp:1388`, allocation `:1450`). In non-HDR the published green is quantised to
1/255, so AO within 0.4% of open maps to exactly 1 and smooth gradients can band, and the
Revision 3 pan comparison "stock max +1/1024" is below one code step. Policy: HDR publishes
unquantised FP16. Non-HDR applies a **deterministic ordered dither** (fixed 4x4 Bayer on
integer output-pixel coordinates, +-0.5 LSB, pure function, applied once after the final
resolve and before the green write); non-HDR gates are expressed in output code values
(a one-step delta is 0.0039, inside the 0.005 p95 pan limit). Alternative, decision B12:
make native AO HDR-only. Recommendation: the capture profile uses HDR; non-HDR is allowed
with dither.

### 8.3 Lifetime, runtime selection and failure policy

**Decision F-A: option (b), reserve `deferredLight` only after explicit preparation.**
Separate static support, preparation intent, admission and active execution:

- `nativeStaticSupported(extent)` uses only the required GL version/extensions,
  texture/render-target format support, limits and positive supported extent. No live
  free-VRAM value, texture discard level, timing, governor state or master setting is
  an input. Require the viewer's raster GLSL path with defined 32-bit uint operations
  (bounded-loop sector counting may replace missing popcount), sampled GL depth,
  renderable R32F/R16F/R8 and the current light-map format; RGBA16F is additionally
  required for a bent plan. Each extent/mip must fit static texture/renderbuffer limits
  and checked integer dimensions. Compile/link success is a separate preparation gate.
  Pack/view eligibility is the explicit policy in §7: main, plus opt-in real
  feed packs after L2; never preview, aux/cube, hero, HUD, impostor or reflection views.
- Proposed `RenderNativeAOPrepare` (**Prepare native AO**) is machine-level, default
  **false**; exclude it from scenes, creative presets and Quick. Define reservation
  capability as `nativeStaticSupported && RenderNativeAOPrepare && eligiblePack`.
  Capability alone is not allocation success, memory admission or permission to draw.
  `nativeReserveRequestedForPack` is a latched reservation intent: propose it only in
  that explicit preparation transaction, commit it with the prepared generation, and
  retain it until explicit teardown/reprepare. Failed admission leaves the prior latch
  unchanged; it is never recomputed from current free VRAM in allocation/completeness.
- A requested plan passes the separate §9 preflight once at explicit preparation.
  Success latches `nativePreparedForPack` with extent/format/plan generation and owns
  its resident resources. Failure leaves preparation unavailable, stock running, and
  no partial reservation. No automatic retry or capability change follows VRAM pressure.
- Extend main allocation `pipeline.cpp:1448` with the explicit reservation request
  (`shadow_detail > 0 || ssao || RenderDepthOfField || nativeReserveRequestedForPack`);
  extend Prism completeness `:1770` and allocation `:1830` identically. Evaluate changed
  reservation requests only at the controlled preparation/teardown or ordinary extent
  lifecycle boundary, never from a per-frame memory predicate. Keep DOF out of the feed
  predicate; preview's pinned map at `:1863` retains its existing lifetime.

**Exact preparation sequence, before a take:**

1. With Prepare OFF, use stock allocation and startup programs only: zero native bundle,
   additional light-map reservation, native program links or native cache allocations.
   Ordinary stock shadows/SSAO/DOF costs remain the baseline. A native-master request
   while unprepared reports "Prepare native AO before enabling" and stays on stock.
2. User turns Prepare ON outside capture. Freeze the requested eligible extents/formats,
   presets and scalar/bent modes; check static support, then run the main-view
   memory/work admission (AO + Relight main, read from the Relight *admitted* plan, 9.4),
   including existing allocations, the 256 MiB streamed-texture headroom margin of the
   byte-identity definition (1) and the transition peak.
   Display the additional resident bytes and any reduced plan before marking it ready.
3. Link/load native companions during this explicit prepare step (or startup if Prepare
   was previously saved ON); never reload stock shaders through `setShaders`. Allocate
   each missing/changed native target and required light map **once**, reuse existing
   stock maps, initialize contents/validity as below, run the **warm-up draws of 8.5**,
   then atomically publish Prepared. Prefer a targeted pack preparation helper; never call
   `allocateScreenBuffer` merely to add the map. Roll back new resources on failure,
   leaving the old valid path intact. A snapshot's own `allocateScreenBuffer` calls
   (7) invalidate Prepared; the interactive generation is re-prepared on snapshot exit.
4. Pin the admitted plan and capture-governor snapshot before a take. Native execution
   requires `Prepare && nativePreparedForPack && matchingGeneration && eligibleView &&
   RenderNativeAOEnabled` plus valid current-view inputs/programs. Prepared OFF/ON toggles
   select existing programs/storage only: zero allocation/release, shader reload/link,
   screen-pack rebuild or legacy-setting writes. Toggle OFF retains prepared resources.
5. Prepare OFF is an explicit teardown outside a take: first select stock, invalidate
   native publication, release native-only storage/programs and the reservation-only map
   when no stock/DOF owner needs it. During a take, preparation, teardown and changed
   extent/format/quality requests stay pending until capture ends. GL loss aborts the
   take with fallback; restoration requires preparation again. Pressure alone never
   flips the allocation gate or silently rebuilds a pack mid-session.

This retains the established R/G/B/A contract, existing bind/unbind consumers and DOF's
scratch target, with less parallel routing than option (a), a native-owned resolved
RGBA light map. Option (a) would need independent sun-channel initialization, blur,
all-consumer rebinding and DOF lifetime isolation; it also duplicates a full-size RGBA
target when stock shadows already allocate one. Option (b)'s explicit cost when the
old gate was false is 8N bytes HDR (15.8 MiB at 1080p / 63.3 MiB at 4K), nominally 4N
non-HDR; account the actual allocated format. Add the AO bundle (scalar approximately
17–98 MiB across the listed 1080p/4K modes), not just this map. These bytes share texture
accounting and can reduce streamed-texture quality (`llviewertexture.cpp:654`, `:673`);
admission enforces the headroom margin and the Prepare status shows the cost. Only users
who explicitly enable Prepare pay this retained OFF cost; stock users with Prepare OFF
pay no native overhead. Prepared OFF requires render-pipeline byte identity **under the
conditions of the byte-identity definition in section 1 (frozen streaming, guaranteed
headroom)** and zero native execution; it does not promise unchanged texture residency
when those conditions do not hold.

**Decision F-B: reserved storage is not valid lighting content.** `bindDeferredShader`
(`pipeline.cpp:25082`) substitutes white only for an incomplete target; a complete
unwritten reserve otherwise binds uninitialized data. Clear a newly allocated native-
reserved light map to **RGBA=(1,1,1,1)** once, with framebuffer/state restoration, before
exposing it. Also track pack-local content ownership/generation: Neutral, current-view
Lighting, or DOF scratch. Allocation completeness never implies Lighting validity.

For OFF, preserve the old producer gate. If it would not have written this view's map,
bind the original white fallback in **every lighting consumer**, including soften's
debug path, even if reserved storage is complete or retains prior native/DOF contents.
If the old gate runs, allow its completed current-view stock lighting map normally.
Audit explicit light-target bindings as well as `bindDeferredShader`; no native OFF
uniform/pass is needed to select this CPU-side binding. DOF's `dofCombineF` binding is
separate: accept only its current post-lighting scratch generation after CoF/required
beauty writes have completed, preserving the stock path, never a neutral or stale AO map.
At each view begin invalidate lighting ownership; after CoF overwrite mark DOF scratch.
White allocation clear plus these ownership gates cover first OFF frame and ON->OFF
after DOF without clearing the reserved map every frame.

Repair dispatch independently of allocation: at `pipeline.cpp:27310`, preserve the
entire old predicate for stock/OFF, but OR in `nativeActiveEligibleView` under
`!sPrismPreviewRender`. Native activation requires `!gCubeSnapshot` plus the complete
view policy above; merely having a complete light map never makes a view eligible.
For native ON with stock SSAO=false, shadows=0, DOF=false and debug=0, clear all four
channels to 1, run the precompiled no-stock-AO sun producer (R/B/A remain neutral), then
publish native G. When shadows/debug need real channels, run their proper producer
before green publication. Preserve preview's white clear and `llprismpreview.cpp:1453`
override; do not let the new OR bypass it. Cube captures retain only their old sun-probe
dispatch. No shadow/SSAO listener or `allocateScreenBuffer` is called by native toggles:
that function releases Actor Mask, silCover and area targets (`:1371`, `:1379`).

- Add an AO resource bundle to `LLPipeline::RenderTargetPack` beside existing per-pack
  resources (`pipeline.h:1361`). Main/aux/probe packs must not share AO validity. Prefer
  validity/extent/view-generation in the owning bundle to another unscoped global bool.
- Native master defaults false. Proposed controls: `RenderNativeAOEnabled`, ambient response
  (stock curve / native linear), radius in metres, thickness in metres (Horizon-equivalent
  at the limit), amount, quality, half/full, denoise, probe-occlusion enable (P4), feed
  enable (L2) and debug mode. Names are proposals, not
  settings that exist today. Creative and quality ownership is separated in P3.
- Compile stock programs normally; precompile native consumer/pass programs only in
  explicit preparation (startup if Prepare is saved ON), after common permutation
  clearing; link failure disables native only. Runtime
  master/response/amount/debug/probe/feed changes **never call `setShaders`**,
  relink or attach shaders. Do not route through `RenderDeferredSSAO` or rewrite it.
- OFF executes original draw programs, uniforms and dispatch gates. Extra native shader
  objects and admitted resident target reserves are allowed; no native pass or native
  uniform traffic in steady OFF. Do not bind/sample a newly reserved light map in stock
  configurations that originally had none. OFF retains prepared storage so OFF->ON->OFF
  has zero allocations/releases and cannot hitch through screen-pack reconstruction.
- Prepare native-only bundles and required light maps only with Prepare ON, at startup
  or explicit pre-capture extent/format/quality preparation, with scalar and bent modes using the same planned storage.
  Pin the admitted plan before capture; include any scalar/bent formats needed by runtime
  probe toggles. Toggle only among prepared resident states. An unprepared request reports
  unavailable/pending preparation and keeps the current valid path; never allocate or
  compile to satisfy a runtime toggle. Startup Prepare OFF reserves nothing native.
  Release on extent/format re-preparation, pool eviction outside a take,
  explicit teardown or GL loss, not on master OFF or temporary unused feed allowance.
- Update screen/Prism allocation, resize, release, eviction, last-close and GL restore
  paths together (`pipeline.cpp:1379`, `:1725`, `:1966`, `:3143`, `:3227`).
  Account all mip bytes, including R32F levels: current format accounting is at
  `indra/llrender/llrendertarget.cpp:89`, allocation at `:378`. An LLRenderTarget extension
  may be needed for explicit mip ownership/accounting; list it in implementation scope.
- Before any view renders, invalidate its AO; validate only after every required pass
  succeeds. An allocation/program/admission failure selects the stock fallback for that
  whole view, never stale main/feed AO. Log requested/effective state once on change.
- Guard framebuffer, viewport/scissor, program, texture bindings/active unit, depth/stencil,
  culling, blending, draw buffers, color masks and relevant sRGB state. Restore on all
  exits. Do not let AO sampling affect sidecar writes, velocity or Actor Mask state.

The P1 checkpoint always retains the original sun producer and shared blur (including the
now-unused stock green) and publishes native green after them. P2 may keep that arrangement
until the optimized variant is qualified. That pays redundant AO during bring-up but
preserves an easy shadow oracle. A separately precompiled native sun variant with no stock AO evaluation is the
optimization target, gated by identical R/B/A and direct-term results. Do not count the
redundant version as meeting final performance acceptance. L4 (shrunk) only removes the
remaining blur-dispatch dependency on the old SSAO flag; because green is published after
the blur, native AO is already independent of sun softness.

### 8.4 Startup shader variants and cache gate

`llviewershadermgr.cpp:3638` creates one stock soften program for the current common
configuration. Classic sky is a runtime `classic_mode` uniform and PBR/legacy selection
is a G-buffer flag branch in the same fragment shader (`softenLightF.glsl:214`, `:222`);
HDR is a render/attachment configuration, not three independent soften programs.
Keep those runtime branches. Add **one native soften companion per existing loaded
configuration when Prepare is ON**, supporting scalar/bent, amount/debug and probe enable through uniforms.
Do not create a Horizon × Bitmask × HDR × classic × PBR × preset permutation product.
Across HDR on/off qualification budget at most two native soften cache configurations
with other settings fixed, each covering classic/PBR skies and materials; identical
source/permutation keys may reuse one binary. Do not add an HDR define just to force
two entries. This adds one linked companion per prepared startup/explicit preparation,
rather than eight programs; Prepare OFF adds none.
Existing common keys (shadows/probes/SSR/etc.) still produce their own cache entries.

P2's planned additional linked program count per loaded configuration is **19**:
five depth/gather/denoise/resolve/publish programs, one no-stock-AO sun producer, one
soften companion, eight OPAQUE/MASK Actor FX beauty companions (2 alpha modes ×
scalar/slot families × unrigged/rigged; loader at `llviewershadermgr.cpp:2691`) and four
AO-input overlay companions (8.1; two fewer and no normal output under Strategy B). The P1
checkpoint adds only the depth/gather/denoise/resolve/publish set (5) because it reuses the
stock sun producer and soften program. Leave glow and BLEND unchanged in P2. One gather program (bitmask; thickness tending to infinity reproduces horizon behaviour) serves all presets;
denoise count is CPU draw count, not a shader permutation. P4 bent output can use a
runtime output mode with RGBA target binding; budget **zero extra soften companions**
for bent normals. L1's optional BLEND experiment can add four replay beauty companions
plus its explicitly enumerated forward consumer families; record those counts before
enabling the experiment, rather than silently doubling all forward shaders.

These counts imply up to 19 additional cold links and binary-cache records, not an
honest prediction of seconds or bytes without driver measurements. Binary cache loading
and saving already exist (`indra/llrender/llglslshader.cpp:457`, `:1071`). In P2 measure
cold startup and warm-cache startup versus stock, with program/stage counts, cache hits,
disk bytes and resident program memory. Provisional pass/fail ceilings (scaled with the
19/15 program count; if 19 programs cannot meet them, fall back to Strategy B's two fewer
programs before loosening any ceiling): native additions
≤4 s cold, ≤0.65 s warm, ≤40 MiB extra cache per loaded configuration during preparation;
measure startup delta when saved Prepare is ON. With Prepare OFF require zero additional
native links, cache records and resident program bytes. No compile/link after preparation
on a native-master toggle.
Report failures and optimize/fail preparation; do not move first-use compilation into
a take. Source edits can invalidate existing
shared-helper cache entries as well; measure total startup delta, not just new links.
L4 may add one shadow-only filter companion; include that link/cache record in its
delta. Every later optional forward family must publish its count and repeat the gate.

### 8.5 First-use transitions: no hitch on first enable (Revision 4, from review C3)

Zero compile/link/allocation calls is necessary but not sufficient. The driver can still
defer program finalisation, framebuffer validation and state creation to the first draw
(the cached-binary load or compile starts in `indra/llrender/llglslshader.cpp:457`), and
the 120-second aggregate pacing gate can hide an isolated stall. Therefore:

1. **Warm-up during Prepare.** Before publishing Prepared, issue one representative draw
   (full extent or a tile, as the state requires) through *every* prepared program and
   attachment/state combination with the real FBO attachments, viewport, color masks and
   blend state: depth prepare, gather (scalar; bent when planned), each denoise
   ping-pong direction, resolve, publish (masked green state), the no-stock-AO sun
   producer, the soften companion for classic and PBR flag paths, the AO-input overlay
   companions (OPAQUE/MASK x unrigged/rigged) and the replay consumer companions.
   Discard results and restore the light map to white.
2. Prepared is published only after the warm-up completes without GL error.
3. **Gates (RTX 5090, settled scene, cold and warm program cache).** For the *first*
   transition OFF -> ON into each execution state - bitmask scalar, bent (P4), replay active
   (first replay actor appearing), feeds (L2) - the first frame's CPU frame time must be
   <= the steady native-frame CPU time + 1.0 ms (proposed), no present interval may exceed
   1.5x the period, and p99 of the following 120 frames must stay <= 1.1x the period.
   Report cold and warm separately; a cold-cache failure that warm passes blocks only
   the "no first-run hitch" claim, and is reported rather than hidden.
4. Reject "exceptional first-use spike relative to steady cost" as a failure even when the
   aggregate pacing gate passes.

## 9. Determinism and live-capture performance

### 9.1 Deterministic inputs and repeatability

`doc/CINE_RELIGHT_QUALITY_BRIEF.md:299` (§6) requires current-frame sampling, fixed pixel
seeds and no temporal accumulation; `:318` requires OFF image diffs and inert resources.
For AO there is no light ID. The noise seed is a **pure function of current-frame inputs
and an explicit fixed seed**: never a frame number, clock, channel number, camera UUID,
`noiseSine`, `ll_frand` texture regeneration or velocity. Filtering is spatial only.
Stock's reflected 128-pixel noise lookup (`class1/deferred/aoUtil.glsl:60`) is not copied.

Two candidate anchorings are compared by **measured pan/swim in P0**, not by preference
(review D1, O2):

- **(S) Screen-anchored.** XeGTAO's spatial low-discrepancy sequence (R2/Hilbert index of
  the integer output-pixel coordinate) at a fixed noise index, with the matched 2x2/4x4
  stratified footprint. The natural baseline; its residual noise stays fixed on the screen
  while surfaces slide beneath it, which is what can read as swimming. An arbitrary pixel
  hash is a diagnostic only.
- **(W) Surface-anchored.** The same sequence indexed by an integer cell of the
  *reconstructed world position* (camera-relative float plus an integer camera-cell
  offset uniform to avoid precision loss; cell size about 2-4 projected pixels at that
  depth, quantised per log-depth bucket so cells do not resize continuously; a normal-facet
  salt so adjacent faces do not share noise). Still deterministic and history-free, so within
  these rules; its grain sticks to surfaces during pans. Costs measured in P0: flicker at
  cell boundaries, depth-bucket seams, grazing-angle stretching.

If neither passes the motion gate for a preset (9.2), that preset gets more samples or is
withheld; temporal history, TAA or capture post-processing are never used to hide swimming.

For half resolution define exact source texel centers; for tiles use global final-output
coordinates. Same current inputs/settings/extent/seed on the **same GPU/driver** must
produce bit-identical AO readbacks, including after camera cuts, feed scheduling changes
and process restart. No AO warmup history is required. This does not imply bit-identical
whole-viewer beauty across GPU vendors or identical camera alone: texture streaming,
animated surfaces, native probe fades, sky changes, AA jitter and governor state can alter
the G-buffer. Record/freeze those inputs and disable temporal AA/exposure/noisy post
effects for the test oracle. Native AO must not depend on or enable TAA to reach its
quality claim. Compare raw AO as well as final images.

### 9.2 CPU oracle, quality gates and motion (Revision 4)

**One oracle sizes bias, variance and motion before any preset is frozen** (P0; proposed
`ao_oracle.py`, the successor of `ao_variance_check.py`). Inputs: (i) analytic fixtures -
plane, corner, sphere, railing, finger gap, arm-against-torso cylinders, 1-4 px creases and
gaps; (ii) **captured G-buffers**: depth and normal dumps through a debug-only readback
(never shipped) from at least eight protocol scenes (section 11) at the base camera and at
0.125/0.25/0.5 px camera offsets along horizontal, vertical and diagonal directions. It
runs the *real stratified sequence* of 9.1 (both anchorings), the exact proposed bilateral
filter and the sample-gap-linking sampler of 6.1, against a converged reference (>= 4096
samples) of the same G-buffer. Outputs per candidate preset: bias per fixture; resolved
variance `w^T C w` with composed overlap/rejection weights (neither `9 * passes` nor depth
taps equals independent samples); **cross-offset error covariance** `C(delta)` for the pan
increments; the signed reference-residual delta distribution
`(AO[t]-AO[t-1]) - (Ref[t]-Ref[t-1])` (p95 and maximum); contact-width retention; and
PASS / FAIL / INCONCLUSIVE. A failed model blocks that preset. Record inputs and
reproducible output.

**Planning model (unfitted constants, disclosed).** Raw sigma `0.09 * sqrt(8 / T)`, T depth
gathers/pixel, covariance inflation 1.25, residual correlated floor 0.002. For a uniform
plane interior convolve normalised `[1,1,1]/3` p times per axis; the 2D effective sample
count is `E = 1 / (sum(k_i^2))^2`: E = 9.00 / 18.17 / 26.73 / 35.13 for p = 1 / 2 / 3 / 4.
Predict `sigma_resolved = sqrt(0.002^2 + 1.25 * 0.09^2 * (8/T) / E)`. This is a sizing
assumption, **not a fitted SL covariance model and not a GPU result**; equal 3x3 weights
approximate the bilateral filter, and edge rejection, half-res interpolation and shared
depth samples reduce E. P0 replaces the constants with conservative bounds fitted on the
real-depth corpus; do not credit upsampling as extra independent samples.

**Why motion needs its own sizing (review C4, O3, D1).** If the error difference between
adjacent sub-pixel camera positions has standard deviation `sigma * sqrt(2*(1-rho))`,
a p95 of `1.96 * sigma * sqrt(2*(1-rho)) <= 0.005` needs `rho >= 0.994` at Low's modeled
sigma 0.0237, `rho >= 0.958` at Balanced (0.0088) and `rho >= 0.853` at High (0.0047)
(Gaussian approximation, a sizing estimate only). On smooth synthetic fixtures the
screen-fixed error stays strongly correlated and the gates look compatible; textured SL
geometry decorrelates it. That is why the real-depth corpus, not smooth fixtures, decides.
Stock itself uses a spatial noise texture, so determinism alone proves repeatability, not
spatial stability.

**Filter support is capped (review O2, G2).** Total filter support is at most **+-4 full-res
px**: full-res gather <= 4 passes, half-res gather <= 2 passes. Revision 3's "at least
three complete passes" floor is **withdrawn**: it was inferred from an analogy to Intel's
TAA-supplemented denoising notes and never validated against contact detail, and the
depth/normal bilateral does not stop blur across creases where depth is continuous
(finger/palm, cloth folds). The budget goes to gather samples instead. Never use temporal
history to compensate.

**Gate hierarchy.**

1. **Primary - pan/swim on real captured scenes.** (a) Offline pan corpora from the
   protocol scenes: for the same screen pixel on valid overlapping receiver interiors,
   `abs(AO[t](p)-AO[t-1](p))` maximum <= 0.02, p95 <= 0.005, and maximum no larger than
   stock SSAO's maximum on the identical sweep (+1/1024 in HDR; +1 code step in non-HDR,
   8.2). For **all contact-crease/rail/finger/corner ROIs**, including continuously
   visible receivers, use the reference-residual excess delta above: maximum <= 0.02, p95
   <= 0.005 (this permits genuine reference contact changes above 0.02). Newly revealed
   pixels are labelled separately. (b) Hardware-captured 120-second pans on the RTX 5090 at
   1080p and 4K: horizontal, vertical, diagonal, orbit/track and zoom at 1x/2x/4x speed,
   reviewed offline and by the user; a perceptible screen-locked pattern, crawl or
   swimming **fails even if the statistics pass**. A world-aligned diagnostic difference
   may be computed offline, never used by runtime AO. Repeat after bitmask and bent changes
   and for every degraded preset eligible during a take.
2. **Bias and contact (per preset, 6.1).** Corner/wall/arm-torso bias, thin-gap visibility
   (>= 0.90 in reference-open gaps, no halo > 1 full-res px beyond the reference
   boundary) and a **contact-width gate**: for 1/2/3/4 px creases and lit gaps at 1080p the
   filtered half-maximum width is within +-1 px of the reference, and peak crease darkness
   is >= 80% of the reference. Failure reduces passes or support; it is never fixed by
   adding passes.
3. **Secondary - static sigma** (fixture based, kept): on an unoccluded plane at
   front-facing/30°/60° slopes and 1/3/10 m, 256x256 interior ROI, 256 sub-pixel
   positions, full/half, 1080p/4K, odd extents: mean >= 0.99, standard deviation <= 0.002,
   peak-to-peak <= 0.01; on a uniformly partially occluded plane the standard deviation of
   error from its reference is <= 0.025 Low / <= 0.010 Balanced / <= 0.005 High and
   Cinematic, with absolute mean error <= 0.005 for every preset. Store raw and resolved
   distributions, modeled covariance and measured E. **A sigma pass never rescues a
   motion, bias or contact-width failure** (the Revision 3 sigma/pan contradiction).
4. **Preset freeze (review G1).** A preset is frozen only after items 1-3 pass on the
   real-content corpus (alpha cards, foliage, avatars, 100 m+ depth ranges), not only on
   fixtures. Portability beyond the RTX 5090 is reported INCONCLUSIVE, never PASS.

### 9.3 Presets, hardware and budgets (Revision 4)

These are **CPU-model-sized candidates and proposed validation targets, not measured
GPU results**. T = slices x steps per side x 2 depth gathers per pixel; "passes" = 3x3
bilateral ping-pong draws in the *gather* domain (each adds +-1 gather-domain px, so +-2
full-res px for a half-res gather); sector count is 32 for every preset; all use fixed
deterministic noise (9.1) and a depth-aware resolve. The CPU sizing gate precedes GPU
cost qualification; a timing budget never overrides the declared quality gates.

| Preset | Gather | Slices x steps/side; T | Passes; full-res support; E | Modeled sigma / partial-plane gate | Availability |
|---|---|---|---|---|---|
| Live Low (**conditional**) | ceil-half | 2 x 2; 8 | 2; +-4; 18.17 | 0.0237 / <= 0.025 | Ships only if P0 shows it passes bias, contact-width and motion gates (default expectation: dropped). If kept it runs the gather with thickness forced to infinity (Horizon-equivalent) and makes no thin-gap claim. |
| Live Balanced | ceil-half | 6 x 5; 60 | 2; +-4; 18.17 | 0.0088 / <= 0.010 | Distributed default; capture profile. |
| Detail High | full | 8 x 8; 128 | 4; +-4; 35.13 | 0.0047 / <= 0.005 | Capture-5090 profile only (its gather exceeds the distributed cap). |
| Still / Cinematic | full | 12 x 8; 192 | 4; +-4; 35.13 | 0.0040 / <= 0.005 | Stills only (L3); explicit still budget. |

Changes from Revision 3, for audit: High 6x6 with 7 passes (+-7 px) became 8x8 with 4
passes; Balanced 4x4 half with 4 passes (+-8 full-res px) became 6x5 half with 2 passes;
Cinematic 8x8 with 8 passes became 12x8 with 4; Low 3 passes became 2. The capped support
spends budget on samples, not blur. Sigma uses the 9.2 model with its unfitted constants;
P0 re-issues this table with fitted values before the P1 configuration (the High-class row)
is frozen. In reply to review D5: the reviewer's estimate that Balanced needs about T=32 at
p=3 and High about T=128 at p=4 is consistent with these rows.

**Cost and structural admission.** Gather reads = gather-domain pixels x T; denoise
neighbor reads = 9 per pass per gather-domain pixel (a **lower bound**, not total
tap-equivalents: add edge/depth/normal guidance fetches, each target component write,
mip preparation, gather metadata writes, full-res resolve and publication; bent writes four
components; publish exact coefficients from the final loops).

| Preset | Gather reads, M, 1080p / 4K | Denoise neighbor reads, M, 1080p / 4K |
|---|---|---|
| Low | 4.15 / 16.59 | 9.33 / 37.32 |
| Balanced | 31.10 / 124.42 | 9.33 / 37.32 |
| High | 265.42 / 1,061.68 | 74.65 / 298.60 |
| Cinematic | 398.13 / 1,592.52 | 74.65 / 298.60 |

**RTX 5090 thresholds (replace the Revision 3 RTX-3070-class table; review C7, O6).**
The only qualification machine is the RTX 5090 (user decision 10/21, folded in here).
Milliseconds are GPU-timestamp p95 of depth prepare + gather + denoise + resolve + publish
at the protocol scenes, with the AO-input overlay reported separately (<= 0.3 ms, 8.1).
Log GPU/driver, clocks, scene, AA, divisor, feeds and bent on/off.

| Preset | p95 at 1080p | p95 at 4K | Fits distributed default (live gather <= 160 M)? | Fits Capture-5090 profile? |
|---|---|---|---|---|
| Low (conditional) | <= 0.2 ms | <= 0.5 ms | yes | yes |
| Balanced | <= 0.4 ms | <= 1.2 ms | yes (31 / 124 M) | yes |
| Detail High | <= 1.0 ms | <= 3.5 ms | **no** (265 / 1,062 M) | yes (live gather <= 1,200 M) |
| Still / Cinematic | <= 2.0 ms | <= 6.5 ms | 1080p only (still <= 1,200 M); 4K no | yes (still gather <= 1,700 M) |

Balanced 0.4 ms at 1080p is the user's 10/21 planning figure. All thresholds are
provisional: first measured by the P1 production-shader spike; changed only by a recorded
decision, never loosened silently to pass. Bent mode gets a provisional +35% budget;
reject a larger increase until profiling explains it. Intel's published example timings
(full-resolution high about 0.56 ms at 1080p on RTX 2060 and 1.4 ms at 4K on RTX 3070,
directional output about +25%; [Intel XeGTAO measurements](https://github.com/GameTechDev/XeGTAO))
are only a plausibility check: different hardware/API tests, not an SL guarantee.

**Ceilings are machine settings with two named profiles.** Work ceilings are deterministic
(taps and iterations per frame) and bind identically on every GPU *for a given profile*;
what differs between machines is which profile the machine setting selects. Scenes and
looks can never raise a profile.

| Ceiling | Distributed default (conservative) | Capture-5090 profile (machine opt-in, qualified by 5090 captures) |
|---|---|---|
| Live gather subceiling (gather depth reads only; excludes denoise, depth prep, resolve/publish) | 160 M | 1,200 M |
| Qualified still gather subceiling | 1,200 M | 1,700 M |
| Combined tap-equivalents per frame (AO + Relight, plus VCam after L2) | 2,000 M | 4,000 M |
| Relight own iteration subceiling | 1,500 M | 1,500 M |
| Combined resident and transition peak / feed AO subset | 768 MiB / 256 MiB | 768 MiB / 256 MiB |

Denoise is charged only under the combined ceiling, never hidden in the gather subceiling.
Use 64-bit arithmetic and ceil-half extents. Only prequalified presets meeting the spatial
gates may be admitted; do not cut denoise below its qualified pass count or improvise a
sparse sample count to fit. Apply the degradation order of 9.4 before capture, never feed
GPU timing into pixel quality during a take. Preserve requested/effective values.

**Other GPUs.** The 3-4x slower assumption for mid-range cards (Balanced about 0.4 ms on
the 5090 means about 1.5 ms on a 3060-class card) is *planning guidance only*. This plan
qualifies no other GPU: mid-range performance is **INCONCLUSIVE** until beta-tester
captures exist - at least two testers on distinct GPU families (one mid-range NVIDIA and one
AMD or Intel), each running the section 11 scenes at Balanced with a Tracy capture and the
logged work plan (requested/effective preset, resident bytes, tap-equivalents, p95 GPU
times). Only then may a mid-range claim be made, against the 5090 thresholds scaled by the
measured factor. The distributed default stays conservative until then.

Main reservation comes first. Feeds receive fixed allowances from the remainder, bounded
by eight **actual AO view passes**, including repeated Gate renders, as Step 6 does
(`alareashadowmath.h:118`, `pipeline.cpp:26956`). Frame identity may reset accounting only.
No redistribution based on scheduling/visible content. Keep the AO and area-shadow
subceilings and enforce the combined budget below as an additional hard gate.

Approximate additional target storage, excluding existing G-buffer/light buffers,
allocator overhead and old/new overlap during transitions (N = full pixel count):

| Variant | Approximate bytes | 1080p / 4K |
|---|---|---|
| Scalar full | R32F depth chain ≈5.33N + R8 edges N + raw/scratch/resolved 6N | 24.4 / 97.6 MiB |
| Scalar half | depth ≈5.33N + edges 0.25N + raw/scratch N + resolved 2N | 17.0 / 67.9 MiB |
| Bent full | depth ≈5.33N + edges N + three RGBA16F targets 24N | 60.0 / 239.9 MiB |
| Bent half | depth ≈5.33N + edges 0.25N + two half RGBA16F 4N + resolved 8N | 34.8 / 139.1 MiB |

Five levels are slightly below the infinite 4/3 mip-chain estimate above. Odd extents
must use the actual level sums. Eight resident 2048² scalar-full bundles would cost
approximately **394.7 MiB**, already failing the **256 MiB feed-AO subceiling**, before
reserved light maps, Relight or feed beauty targets. Eight identical-size cameras actually
reuse one exact-size scratch bucket in this checkout; this example is a conservative
resident-bundle stress case, not a claim that camera IDs each own a pack. Charge each
real resident target once for memory, but every actual rendered view pass for work.
Include pinned preview/capture targets that coexist, not just the reusable scratch pool.

### 9.4 Combined admission: AO + Relight soft shadows (+ VCam after L2)

P2 establishes one pure, allocation-free preflight ledger for the **main view** (AO + Relight
main work and bytes); L2 extends it to native feeds and all VCam targets, which is why the
core phases do not need the full combined ledger or governor integration (review O10).
Use `alareashadowmath.h:78`, `:118`, `:134` as the pattern: 64-bit integer bounds, reserve
main requested-cap work first, fixed remaining feed allowances, count repeated Gate
renders, and reject a view's admission before the take when its minimum cannot fit.
Do not borrow an idle
main reservation or another feed's allowance based on timing, cadence or visible lights.
Use requested Relight light cap for reservations, not the momentarily selected set.
Divide remaining work into eight fixed pass-slot allowances (integer remainder unused),
with AO + Relight + feed rendering charged together to each allowance. Map prepared
passes to slots by the stable capture-slot ordering below; each repeat render consumes
its own pass slot. Memory is admitted by actual resident size buckets, not divided by
camera count; reserve its main share before admitting feed buckets.

**Relight interplay (Revision 4, from review C2).** At HEAD the Relight area-shadow pass
derives its plane count from the *currently selected* light count (`const U32 planes =
(selected.size+3)/4;`, `pipeline.cpp:26961`), releases the surplus main-view plane when
the selected set shrinks (`:26973`) and allocates missing targets on demand. With a
requested cap above four, ordinary light-visibility changes can cross the
one-plane/two-plane boundary during a take, which defeats "zero allocation" however the
capture resolution is pinned. **This is being fixed in the Relight stack-fixes job**: plane 1
is retained while the requested cap exceeds four and Relight consumes the admitted plan
rather than the selected set, including its OFF cleanup ownership (`pipeline.cpp:26742`)
and its 0/1/4/5/8 selected-light and mapped-light-eligibility allocation-counter tests.
AO does not edit Relight code. AO admission *consumes the admitted Relight plan read-only*
(bytes and tap-equivalents at the requested cap). Dependency for P2 acceptance: until that
job lands, AO admission conservatively charges two planes whenever the requested cap
exceeds four, and any combined zero-allocation claim is reported INCONCLUSIVE rather than
PASS; AO's own allocation counters must still stay flat under selected-light transitions.

The following are proposed release ceilings, not current enforcement or measurements. The
values shown are the **distributed default**; machine settings may lower them, and the
Capture-5090 profile of 9.3 raises specific ceilings only as listed there.

| Gate or diagnostic | Accounting / consequence |
|---|---|
| Combined resident and transition peak ≤768 MiB; feed AO subset ≤256 MiB | Sum **main + all resident feeds** AO depth/mips/edges/ping-pong/resolve, Relight gather/scratch/visibility for every channel, native-required `deferredLight` reserves, and all VCam G-buffer/depth/beauty/light/water/capture/preview targets. Include post attachments, retained OFF/prepared bundles, alignment/driver allocation estimates and simultaneous old/new resources. Count shared depth/targets once. Existing main beauty/G-buffer is a baseline outside this incremental ledger; it still counts in total VRAM/headroom. Target success alone is not admission. |
| Combined scheduled texture work ≤2,000 million tap-equivalents/frame (4,000 M in the Capture-5090 profile) | For AO **and** Relight main/feeds, sum worst-case depth gathers (including mip prep and hit refinement), all filter/resolve/publish reads/writes; add every scheduled VCam fullscreen/post/copy pass at its reserved dimensions. One texture read or render-target component write is one equivalent; dynamic-loop passes use their maximum bound. Publish the per-pass coefficient table from production shader loops; an unknown coefficient is a failed admission. Retain the live AO gather subceiling (160 M distributed default, 1,200 M Capture-5090 profile; stills per 9.3) and Relight's own ≤1,500 M iteration subceiling as additional limits, not substitutes for this sum. Denoise is excluded from the gather subceiling and included here in full. |
| Feed raster work: **report-only** thresholds 4,096 draw calls / 8 M submitted triangles per frame; hard ≤8 actual AO/Relight feed passes | Geometry cost depends on content and is not bounded by texture taps. Record counts, threshold overruns and frame-time impact for qualification, including repeated Gate renders. They may fail a pre-take workload qualification but never reject, freeze, resize or reschedule a feed mid-take. Do not make per-frame admission depend on draw-list contents. The eight-pass cap is a static schedule bound; reject a ninth scheduled pass before the take, with a runtime guard against schedule corruption before drawing/allocating. |
| Measured combined GPU delta p95 ≤4 ms at 1080p60 / ≤6 ms at 4K30 (RTX 5090) | Compare AO + Relight soft shadows + all admitted VCam work enabled versus the same main scene with those costs disabled. Also pass the full-frame 2 ms headroom and present-interval gates below. The structural limits do not predict milliseconds; a plan that fails either timing or structural limits fails qualification. 4K60 needs its own measured qualification. |

**Deterministic degradation, before preparation/take (feed rules apply from L2; the core
phases have only the main view, whose own ladder is High -> Balanced -> Low if shipped ->
stock):** reserve the requested main AO
and main Relight plans first. **Use capture slot order only**: lower numbered occupied
slots have precedence, higher numbered slots degrade first. There is no explicit capture
priority field in `llprismlens.h`, and this plan adds none. Pin the slot map; repeated
Gate-source renders use their prepared source-slot order and fixed occurrence order,
not runtime arrival order or object IDs. For each highest numbered admitted feed:
(1) bent -> scalar AO, (2) step down through qualified
AO presets to Low, (3) reduce feed Relight samples, then steps, then half resolution
using its existing pure workload planner, (4) disable native feed AO (charge any stock
fallback work too), (5) disable feed Relight, (6) if capture cost still prevents admission,
return an explicit **Needs capture-governor reprepare** result, (7) reject that feed's
admission before the take if no governor plan fits. AO does not change feed resolution
or cadence. Recompute memory and worst scheduled-frame work after each step; cadence
never reduces resident bytes or the peak where all feeds coincide. Skip ineligible
sizes/presets; keep requested values in status.

**Capture-governor precedence:** the existing capture governor is the sole owner of
resolution scale and cadence (`llprismlens.h:353–385`, `mEffectiveResolutionScale`,
`mCadenceEntitlementHz`, `mUserResolutionScaleCeiling`, `mAdaptiveEnabled`). Outside a
take it may choose a new plan under the user's capture ceilings; AO/Relight preflight
then admits resources/work for that snapshot. Step 6 is a handoff, not a second governor
or an AO override of capture settings. Iterate reprepare outside capture until both
plans fit, or reject capture admission with a reason. Freeze the final resolution,
cadence entitlements, slot map and worst-frame schedule at take start; suspend adaptive
governor changes until the take ends. Status may continue reporting timing/pressure.
This requires explicit governor pin/unpin integration in L2, not an assumption that
today's runtime fields already freeze it. Neither planner may overwrite the other's
plan mid-take; catastrophic GL/resource loss ends the qualified take with a logged
fallback, rather than silently changing quality or freezing selected feeds.

Only after all feed reductions/rejections may main lose bent output, step down qualified
AO presets, then reduce main Relight samples/steps/half using its existing pattern; if
still over, reject the native/Relight capture plan and report baseline fallback. Memory
pressure also releases excluded bundles **before** capture; scalar/half transitions that
would exceed the peak limit must free old storage first. No quality or allocation churn
during a take. All retained residents, including an AO-disabled but prepared feed, still
count: disabling execution alone cannot solve a byte overrun.

L2 acceptance must cover 1/2/4/8 feeds with AO + Relight together, mixed real pool sizes,
2048² stress accounting, HDR/non-HDR, main 4K bent, odd extents, pinned preview/outputs,
repeat Gate renders and a deliberately over-cap plan. Log requested/effective plans,
per-owner bytes/peak bytes and scheduled work; every ledger entry and every ceiling
must PASS, or the deterministic reduced plan must fit and be visibly reported. Reorder
the view schedule: effective quality and main reservation remain identical. Mutations
that omit Relight/feed beauty bytes, count IDs rather than passes, hide transition
overlap or redistribute unused reservations must fail. Hardware recording is qualified
against this combined plan, not against an AO-only wall benchmark.

### 9.5 Frame limiter and VRAM governor

The limiter waits at frame end and counts VSync/background time already elapsed;
slow frames skip waiting (`llappviewer.cpp:1811`). It cannot recover GPU time spent
on AO. Its sleep is tagged `RENDER_SLEEP` so AutoTune does not treat intentional
waiting as slow rendering (`:1816`). Never use that sleep or a displayed capped FPS
as AO cost. Measure GPU timestamps and CPU/frame intervals independently.

For hardware recording, qualify 60 fps (16.67 ms) and 30 fps (33.33 ms) explicitly.
On settled representative scenes, require at least 2 ms measured frame headroom at
the selected rate, p99 inter-present interval ≤1.1× period and fewer than 0.1% intervals
>1.5× period over a 120-second run. If the AO-off scene already fails, report baseline
failure and AO delta rather than labeling AO a stable-frame-rate solution. Run an
uncapped performance sample as well as capped recording; include populated event/crowd
scenes and multiple feeds, not just a wall fixture.

The VRAM governor treats render targets as fixed allocations; render-target bytes are
already inside texture accounting (`llviewertexture.cpp:654`). It can raise the effective
cap for the fixed-allocation floor (`:673`), and capture texture protection yields under
pressure (`:501`, `:524`). New AO buffers can therefore lower streamed-texture quality
or cause pressure even if their own allocations succeed. Use existing accounting exactly
once, add pre-capture memory admission/status, and qualify pressure/recovery. Do not
silently change thickness/radius or oscillate quality based on governor timing. Pin the
effective plan during a take; catastrophic allocation loss ends the qualified take
with a logged whole-view fallback and capture-quality warning, never stale data.

## 10. Phased implementation with measurable gates

Effort is experienced graphics-engineer days, including automated fixtures and review
but excluding user screenshot turnaround/hardware availability. Phases are independently
reviewable; the order is deliberate: **the cheapest evidence comes first and each phase
ends in a decision point** (P0 exit gate, P1 user go/no-go, P2/P3 acceptance).

| Phase | Effort | Cumulative | Decision at the end |
|---|---|---|---|
| P0 reference look, BLEND survey, oracle | 2-3 | 2-3 | Does at least one High-class preset jointly pass bias, variance, contact and motion on real depth? |
| P1 early A/B checkpoint | 6-9 | 8-12 | **User go/no-go (D1)** before any framework spend |
| P2 framework | 14-20 | 22-32 | Shippable main-view native AO, default OFF |
| P3 calibration + UI + persistence | 6-9 | **28-41 core win** | Ship core |
| P4 bent normals + probe specular | 12-16 | 40-57 | Separate approval |
| L1 alpha receivers | 5-8 | | Only if the P0 survey justifies it |
| L2 feeds + combined ledger + governor | 12-16 | | Only if VCam feeds need native AO |
| L3 stills / snapshot transaction | 4-5 | | Only if stills need native AO |
| L4 sun-blur cleanup | 1-2 | | Optional |
| shared qualification allowance | 5-8 | **67-96 full scope** | |

L5 (bitmask SSGI, 8-15) is separate and not in either total. Gates below are future work, **not tests run by this
planning task**. No check may silently pass when a GL capability or image oracle is absent:
report INCONCLUSIVE and do not ship the corresponding claim.

### Common harness contract (all phases)

Extend the style of `scripts/code_tools/relight_area_check.py:1`: a real GL context linking
production shaders (`:9`), controlled depth/normal/material inputs, framebuffer readbacks,
and current-camera negative controls (`:510`). Add proposed `ao_oracle.py`
(CPU reference sizing of bias, variance and motion before preset freeze, 9.2), `ao_bitmask_check.py`,
`ao_dispatch_check.py` and pure math TUT cases. Dispatch/ownership tests must exercise
production routing as `scripts/code_tools/relight_vcam_dispatch_check.py:14` does, not a copied mock that
publishes validity manually. Its executable mutation pattern (`:581`) is the model.

Every phase inherits the clauses below, except that **P0 and P1 are exempt from the
preparation, zero-allocation and OFF-matrix clauses by design** (P1 allocates lazily on
first enable, requires stock SSAO ON and is a developer checkpoint, not a shippable
feature). They are *not* exempt from determinism, direct-light, bias, contact-width or
motion gates.

- Native OFF versus clean baseline at this HEAD (under the section 1 byte-identity
  conditions: frozen streaming, guaranteed headroom): byte-equal raw light-map and stable
  pre-post beauty readbacks, SSAO true/false × shadows 0/sun/local × HDR/non-HDR ×
  classic/PBR skies. Also ON->OFF restoration, water/alpha/replay, and resize/camera cuts.
  For configurations with no baseline light map, compare the original neutral inputs
  and beauty; the new reserved texture must not introduce a stock read/dispatch.
  Freeze baseline noise for equal-input comparisons with the production-inert hook below;
  native reproducibility also tests across restarts. Compare pre-tonemap values.
- Master/amount/response/debug/probe/feed runtime toggles (from P2): **zero shader reloads,
  compiles, links, allocations/releases, screen-pack rebuilds and legacy-setting writes**
  for a prepared resident plan. Count calls, do not infer from UI smoothness. Resource
  preparation happens only with Prepare ON, at startup or an explicit extent/format/plan
  transition before capture; Prepare OFF stock startup allocates/links nothing native.
- Settled identical extent/plan for 300 frames and a 120-second take: **0 target/texture
  allocations and releases per frame**. Master OFF: zero native passes and native uniform
  uploads; reserved resident bytes stay unchanged, budgeted and disclosed. Explicit native
  teardown releases native-only bundles; shared light-map lifetime follows explicit reservation
  and its stock/DOF users. Check shader objects separately from targets.
- Repeat same GPU inputs 100 times and after view/cut/schedule permutations: byte-equal
  AO output, finite/bounded values, no history/velocity reads. No CPU/GPU synchronous
  query waits in the pass. Full GL state and main-view sentinel restored after all exits.
- Native ON direct sun and local-light inputs **before** classic compatibility recombination
  are unchanged; emissive-only pixels and graphic overlay inputs are unchanged. For legacy
  classic skies capture `linear_to_srgb(sun_contrib) * sunlit_linear * 0.7` immediately
  before `softenLightF.glsl:299`; for PBR capture `finalSun` before the nonlinear sum in
  `deferredUtil.glsl:1763`. Verify sun visibility, shading N, BRDF and emitter parameters
  independently. Neither final-beauty subtraction nor legacy `debug_direct` is a direct
  equality oracle; stock SSAO has the same nonlinear recombination dependency. GPU term
  readbacks use exact equality where programs are identical; native variant arithmetic
  differences must remain within 1e-6 relative/absolute FP32 tolerance and be explained.
  No tolerance permits an AO-dependent direct-light reduction. Test sun-only, point-only,
  projector-only, rig-only, emission-only and indirect-only scenes independently.

**Stock noise freeze isolation:** `pipeline.cpp:3324` uses `ll_frand()` to build the stock
noise texture. Never seed/replace global `ll_frand`, change call count/order or install a
shipping debug setting for this test. Prefer same-process baseline/OFF draws sharing the
actual captured noise bytes. If cross-process fixtures need capture/replay, guard the
texture-upload interception with a dedicated test-target-only compile definition, absent
from every shipping configuration; it defaults to pass-through and is activated only by
the harness. Even in that target execute the original RNG draws before test upload so
downstream RNG state is unaffected. Use the identical captured bytes in clean-baseline
and candidate harness runs. Prove the inactive hook uploads the original bytes unchanged,
and inspect release compile definitions/preprocessed path to prove it is compiled out.
The primary OFF byte-identity gate also runs without any hook active. Fixing the native
seed never changes stock noise generation or invalidates the OFF oracle.

### P0 - Reference look, coverage survey and CPU oracle (2-3 days; no viewer code)

**Effect:** none visible. It buys the evidence every later decision depends on and can end
the project cheaply.

**Deliverables:**

1. **Reference-look capture protocol executed** (section 11): the user's iMMERSE ON/OFF
   pairs for the named scenes, including at least two sunlit exteriors with an avatar and a
   nearby railing/wall, three interiors with corners and furniture, close avatar
   fingers/face/hair, and a foliage/fence scene; camera bookmarks and frozen settings so
   the same views are re-shot natively in P1.
2. **BLEND/alpha coverage survey** in those scenes (review O5): the fraction of visible
   pixels whose final surface is OPAQUE, MASK or BLEND, split by pool (hair, lashes, lace,
   clothing, foliage, water, other), using a throwaway debug coverage count (not shipped).
   Record `RenderAutoMaskAlphaDeferred` = 0 (`settings.xml:20961`) and re-measure with
   auto-mask / per-object Alpha Mode masking to show how much coverage can be widened.
   The result sets L1's priority and the honest "thin content" claim.
3. **CPU oracle** (9.2): bias + variance + **motion** (cross-offset covariance, signed
   reference-residual deltas) on analytic fixtures *and* captured G-buffers, using the real
   stratified sequence and both noise anchorings; per-preset PASS/FAIL/INCONCLUSIVE; fitted
   model constants; thickness default candidates; verdict on conditional Low; the choice of
   noise anchoring (S) or (W).

**Exit gate:** at least one full-resolution High-class configuration is predicted to pass
bias (6.1), variance, contact-width and motion gates on the real-depth corpus. If none does,
**stop and report** with the options: raise T, change the anchoring, or accept a visibly
noisier look with the user's sign-off. Do not start P1 on a failed oracle.

**Risk / effort:** low, **2-3 days** for the protocol, survey and a first oracle on synthetic
fixtures plus captured G-buffers; oracle refinement continues inside P1 (counted there).

### P1 - Early A/B checkpoint: native bitmask on the existing plumbing (6-9 days)

**Effect:** the first visible native AO, cheaply, so the user can compare it with the
reference before more is funded (reviews G3, D7, O4).

**Scope.** Main view only. One bitmask gather (6.1: linking, thickness floor, depth
precision) in the **P0-selected High-class configuration only** (GLM: sequence High first),
the capped bilateral denoise, resolve, and a **green-only publish into the existing
`deferredLight`** *after* the stock shared blur (so the stock sun/blur is untouched and
native AO cannot change sun softness). Native is active only when the user's existing
stock SSAO setting is **ON**: the allocation gate (`pipeline.cpp:1448`), dispatch gate
(`:27310`) and blur (`:27345`) are already true and are **not modified**; the soften
program already has `HAS_SSAO` and reads green. Stock SSAO is still computed and
overwritten - a bring-up cost accepted at the checkpoint. Native working targets are
allocated lazily on first enable (a hitch is accepted; developer/debug only; no Prepare,
no ledger). Native OFF leaves every stock path untouched.

**Selectable response (debug setting):** `0` = stock `adjustIrradiance` curve
(`softenLightF.glsl:137`), `1` = native linear `irradiance *= mix(1, visibility, amount)`
on ambient/probe irradiance only, before Shadow Lift; direct sun, local lights, emission
and rim unchanged. For the checkpoint the selector is one uniform inside the existing
soften program; the companion-program decision is made in P2.

**Excluded views and known limits.** Preview, cube, hero, HUD, impostor and reflection views
and snapshots (native forced to stock) skip native entirely. Shared-replay OPAQUE/MASK
actors are not in the AO input - exactly as with stock SSAO, so this is not a regression -
fixed by 8.1 in P2. BLEND is unchanged. No feeds.

**First deliverable inside P1 (review C4): a production-shader GPU feasibility spike** -
the real gather + denoise at 1080p/4K on the RTX 5090 with GPU timestamps and a pan
capture - before the surrounding wiring is finished. Compare with the P0 prediction; a
large mismatch stops the phase and re-sizes the presets.

**Files:** `pipeline.cpp/.h` (publish draw, lazy targets, debug settings), proposed
`alnativeaomath.h`, `class1/deferred/nativeAOUtil.glsl`, `nativeAODepthF.glsl`,
`nativeAOGatherF.glsl`, `nativeAODenoiseF.glsl`, `nativeAOResolveF.glsl`,
`nativeAOPublishF.glsl`, the soften response uniform in `class3/deferred/softenLightF.glsl`,
`llviewershadermgr.cpp/.h`, `app_settings/settings.xml` (debug settings), harness files.
No Director UI, no licences (developer build only), no Prepare.

**Acceptance (checkpoint, High-class configuration):**

- Plane AO >= 0.99, sky exactly 1, no NaN; corner measurably darker than the plane
  (>= 0.10 in the specified geometry); CPU hemisphere reference mean absolute error <= 0.05
  on plane/corner/sphere; **bias gates of 6.1** (corner, floor-wall, arm-against-torso)
  and the thickness sweep 0.01/0.03/0.10/0.30 m with sample-gap linking on and off.
- Thin fixtures: railing/finger-gap reference-open gaps >= 0.90, no halo > 1 full-res px,
  contact junction still loses >= 0.10; contact-width gate; LOD0 first, then mips.
- Depth-precision fade fixtures at 50/100/220/400 m (6.1).
- **Primary motion gate** (9.2) on the protocol scenes, then the secondary sigma gate; the
  chosen noise anchoring (S or W) is confirmed on hardware captures.
- Determinism: 100 repeat byte-equal AO readbacks, finite/bounded, no history/velocity reads,
  no synchronous queries; native direct-light inputs unchanged (the common clause);
  stock R/B/A byte-identical after publish.
- Native OFF byte identity in the default configurations (SSAO on/off x HDR) under the
  section 1 conditions - trivially, because native code is unreachable.
- Timing on the RTX 5090 reported against the 9.3 thresholds (not a P1 pass/fail).
- **Mutations that must fail:** reverse GL z convention; publish into R as well as G;
  filter the native green through the stock blur (AO filtered twice); disable sample-gap
  linking silently; AND instead of OR; shift by 32; cosine treated as angle; multiply beauty
  instead of irradiance.

**User A/B and decision D1.** Re-shoot the P0 scenes natively and deliver, per scene, four
images: stock + reference OFF, stock + reference ON, native with the stock curve, native
with the linear response (plus the debug visibility image). The user chooses:
(a) **GO** - fund P2 and P3; (b) **ADJUST** - re-tune radius/thickness/response, up to 2
extra days, then re-decide; (c) **NARROW** - ambient-only AO is visibly too weak in sunlit
scenes: either accept that, or open a *separate* sun-contact-shadow feature with its own
decision record (section 13) - this plan will not darken direct light; (d) **STOP**.

**Risk / effort:** medium (GL conventions, bitmask math, bias), **6-9 days**. The gather,
denoise and math header, the oracle and the harness are reused by P2; the lazy-allocation
and stock-SSAO-required glue is discarded.

### P2 - Native AO framework (14-20 days)

**Effect:** shippable main-view native AO (default OFF) with no hitch on first use, no
resource churn on toggles, stock restored exactly when OFF, and replay actors participating.
Funded only after D1.

**Contents:** (1) **Prepare native AO** (machine setting, default OFF), static support,
latched reservation intent, main-view admission (AO + Relight main from the *admitted*
plan, 9.4, headroom margin of section 1), atomic Prepared. (2) Allocation predicates
(`pipeline.cpp:1448`; Prism completeness `:1770` and allocation `:1830` only for pack
fixtures), dispatch repair (`:27310`) with the no-stock-AO sun producer, white
initialization and lighting/DOF ownership (Decision F-B). (3) **First-use warm-up and
cold/warm ceilings** (8.5). (4) **Shared-replay AO input overlay** and the eight replay
consumer companions (8.1). (5) The OFF identity matrix. (6) Presets Balanced and High (and
Low only if P0 kept it) with their 5090 gates and the two ceiling profiles (9.3). (7) Dither
or HDR-only policy (8.2). (8) Snapshot interim: stock whole shot and re-prepare on exit (7).
(9) MIT notices and the shader-cache gate (19 programs, 8.4). (10) Relight dependency (9.4).

**Files:** as Revision 3's framework list - `pipeline.cpp/.h` (allocation `:1448`, Prism
`:1770`/`:1830`, dispatch `:27310`, preview `:27295`, `bindDeferredShader :25082`,
preparation/lifecycle, main-view ledger); `llprismpreview.cpp` (preview override at
`:1453`, preserved); `llviewershadermgr.cpp/.h`; the P1 shaders plus
`nativeAOReplayInputF/V.glsl` (four variants); native permutations of
`class3/deferred/softenLightF.glsl` and `class2/deferred/sharedActorFxPbrF.glsl`;
`llactormover.cpp/.h` (AO-input draw of the proxies); `app_settings/settings.xml`;
`indra/llrender/llshadermgr.cpp/.h` if reserved uniforms are needed;
`indra/llrender/llrendertarget.cpp/.h` if mip ownership requires it;
`indra/newview/licenses-win32.txt`, `licenses-linux.txt`, `licenses-mac.txt` with the
`viewer_manifest.py` mappings verified; harness files and the test-target noise capture
plumbing. Prefer a shared native-only utility over changing stock helper expressions.

**Risk / effort:** high, **14-20 days** (lifecycle and light-map work 6-8; replay AO input
and consumers 4-6; first-use warm-up and shader cache 2-3; OFF matrix and harness 2-3).

**Acceptance:**

- Everything the P1 acceptance requires for the preset(s) shipped, repeated per preset
  (Balanced, High): per-preset bias and contact-width gates, primary motion gate on the real
  corpus, secondary sigma, frozen only after the real-content ROI (9.2 item 4).
- **Preparation gates:** fresh stock startup with Prepare OFF has zero additional native
  targets, native links/cache records and reserve-map bytes. Unsupported GL/extent rejects
  preparation without partial resources. Prepare ON allocates each missing target once,
  clears new maps white, runs the warm-up and publishes Prepared only on complete success;
  repeating the request allocates nothing. Failed admission/link rolls back. Sweep live
  VRAM/governor pressure after preparation: static support, reservation and pack IDs never
  flip; no `allocateScreenBuffer`, automatic retry or mid-take teardown. Prepare OFF tears
  down outside capture only; scenes/presets cannot enable Prepare.
- **First-use gates** (8.5): cold and warm first-transition CPU and present-interval
  ceilings for bitmask scalar and for replay activation.
- **OFF identity** under the section 1 byte-identity conditions, including the
  Prepared-OFF case with the 8N reserve and the frozen-streaming / headroom proofs; the
  complete matrix SSAO x shadow x HDR x sky x DOF x resize x restart x DOF-scratch ownership
  x ON->OFF.
- **Reserved OFF content, allocation/dispatch hole, excluded views, DOF** - the Revision 3
  gates are retained: with no stock producer the first prepared OFF binds white through
  soften beauty/debug and explicit lighting bindings, ON->OFF with dirty RGBA sentinels
  and DOF scratch equals the white-fallback baseline; with stock SSAO=false x shadows=0 x
  DOF=false x debug=0, native ON produces a complete correctly sized light map and
  non-neutral G while R/B/A stay exactly neutral, 100 prepared OFF/ON toggles do zero
  allocation/release/`allocateScreenBuffer`/reload/link/legacy-setting calls, and Actor
  Mask, silCover and area targets are unchanged; each of the two allocation gates and the
  dispatch gate fails independently when disabled; preview stays neutral with stale
  sentinels; `gCubeSnapshot`, hero mirror, HUD, impostor and reflection execute no native
  passes; DOF CoF scratch overwrites `deferredLight` only after lighting consumers and the
  next native view reinitialises it; no `aoResolved` alias with DOF or area targets.
- **Replay parity (8.1):** AO-input depth/normal readbacks with replay active; background
  sentinels never visible through an actor; mutual actor/world occlusion; dissolve, MASK
  holes, two-sided and rigged cases; the deferred versus shared OPAQUE/MASK replay
  indirect result within one RGBA8 quantisation step (or the FP tolerance); overlay budget
  <= 0.3 ms; no allocation when a replay actor appears.
- **Main-view admission:** the preflight rejects over-cap bytes/work before allocation and
  draw, including reserved OFF targets and the overlay; AO admission consumes the admitted
  Relight plan (9.4); the Relight dependency is landed or reported INCONCLUSIVE.
- **Snapshots:** a native-on snapshot is a whole-shot stock fallback with a status; after it
  the interactive generation is re-prepared once, zero leaks, valid generation ID.
- **Reconstruction:** full/half and odd 63x47, 1920x1080, 3840x2160 have <= 1 full-res
  pixel edge alignment error; near/far/FOV/off-axis reconstruct a known plane within the
  derived depth-format tolerance, with no screen-radius sign flip.
- **Performance:** RTX 5090 thresholds of 9.3 for the shipped presets after replacing the
  redundant stock AO computation; combined delta p95 and the 9.5 frame-pacing gates;
  the notices in `licenses.txt`; the 19-program startup gate of 8.4.
- **Mutations that must fail:** use the main projection in another view; remove gather
  dispatch; force validity before a failed allocation; call `setShaders` on toggle;
  allocate the scratch target each frame; skip the warm-up; skip the replay overlay or order
  it after the gather; bypass the headroom check; Relight plan ignored by admission.

### P3 - Calibration, Director UI, persistence, ceilings (6-9 days)

**Effect:** the core win is controllable and matches the reference as closely as the
ambient-only design allows: a searchable/favoritable native AO control with honest
quality/view status, saved creative looks, scene restoration and calibrated look presets.

**Calibration (2-3 days):** using the P0 captures, freeze the thickness default, radius
and the Natural / Defined / Reference-match looks and the final response choice
(section 11 targets: contact-ROI ratio within +-10%, contact width within +-2 px at
1080p); record image IDs and settings. Add the machine settings for the two ceiling
profiles of 9.3 (Capture-5090 is opt-in and never loaded from a scene or look).

**UI, persistence (4-6 days).** As Revision 3's Director phase: files `llfloaterdirector.cpp`,
`alkitpresets.cpp`, `alpresetlook.h`, `llpresetsmanager.cpp`; generator
`scripts/code_tools/director_kit_gen.py` and `scripts/code_tools/director_kit/kit_pages.json`
with its page data providers; generated World Shadows XUI/index; Preferences and Lightbox
XUI/adapters; Director manifests/fixtures and take-start metadata. Do not hand-edit only
generated XUI/index and leave sources stale.

Current search parses the v3/22-field generated index (`alkitsearch.cpp:67`, `:136`);
Quick proxies refer back to actual source rows (`alkitshell.cpp:2121`). Header Quick is
default deny, with listener tripwires, explicit per-setting review reasons and uniqueness
checks (`scripts/code_tools/director_kit_gen.py:1356`, `:1471`, `:1522`, `:1533`). The new
master is **not Quick-safe just because it is a bool**: prove its listeners do not reload
shaders/recreate global buffers and that prepared master toggles allocate nothing.
Unprepared requests report pending preparation instead of allocating from the toggle. Keep
it denied until reviewed. Existing `RenderDeferredSSAO` stays unsafe. Update generator
count/fixtures only for real additions; do not rename a nonexistent widget to make indexing
pass.

Scene persistence is an explicit allowlist (`llfloaterdirector.cpp:3603`, `:4098`,
`:4360`), not every `Render*` setting. Existing Relight creative settings are included,
while quality is machine-wide (`:3734`). Look ownership intentionally excludes masters,
quality, targeting and shader-compile settings (`alkitpresets.cpp:61`). Boundaries:

- The AO look family owns radius, amount, thickness, chosen response and optional bent
  appearance controls; no master, feed enable, budget or quality changes on look load.
  Keep quality preset and appearance preset visibly distinct.
- Scenes explicitly save native master and creative AO parameters. A scene with no AO block
  selects stock compatibility, avoiding inherited native AO from the previously loaded
  scene; show this policy in release notes; do not reset unrelated legacy SSAO/preferences
  or camera placement.
- Graphics preferences save quality/full-half/denoise and memory/work limits deliberately.
  Prepare is a separate machine preference default OFF; scenes, looks and graphics quality
  preset loads cannot turn it on or trigger preparation. Expose pre-take cost/status and
  teardown. Scene load never raises machine ceilings or the profile; a scene requesting
  native while unprepared stays on stock and reports pending preparation. Take-start
  metadata records Prepare/prepared generation, profile, requested and effective quality,
  per-preset gates, seed and anchoring, radius/thickness, view policy and fallback reasons,
  following `pipeline.cpp:26651` and `alrelighttakerecord.h:56`.
- The legacy Lightbox `RenderSSAOEffectValue`/`RenderSSAOEffectSat` bindings (3.4) stay
  unresolved legacy UI and are not reused.

**Acceptance:**

- Search finds "AO", "ambient occlusion", "GTAO", "bitmask", "thin objects" and (after P4)
  "probe occlusion" and lands on a real widget; row/header, reset, undo, changed-only
  state, Quick proxy and status agree; generator/parity/functional checks pass.
- Save/load a creative preset without changing master, quality, budgets, subjects or feed
  enable; scene roundtrip restores creative state; an old sparse scene after an AO-heavy
  scene returns to the documented stock state; unknown keys are ignored; custom numeric
  knobs roundtrip without preset listeners overwriting them.
- Toggling master/response/appearance through Preferences, Lightbox, Director and Quick after
  explicit preparation: reload count zero and zero allocation/release; Prepare and the
  ceiling profile remain excluded from Quick/scenes/looks; the default-OFF startup incurs no
  native allocation/link/cache cost; pre-take quality preparation allocates at most once
  per affected target and 300 updates allocate zero; reset works on declared settings.
- Match-look captures and 120-second frame-pacing/VRAM runs pass 9.5 and 11; publish
  requested versus effective state and the excluded alpha/tile/feed/water modes, not a
  misleading "Ultra".
- **Mutations:** shader-reload listener on an allowlisted native header; an index entry for
  a nonexistent widget; master/quality written from look load; omitted scene key; an old
  scene retaining the previous native enable; status reporting requested quality as
  effective after a budget clamp; scene load switching the ceiling profile.

**Risk / effort:** medium, **6-9 days** (UI/persistence 4-6, calibration 2-3), excluding
user screenshot turnaround.

### P4 - Bent normals, probe specular occlusion (12-16 days; separate approval)

**Effect:** indirect light follows open directions; ordinary probe reflections lose
implausible brightness inside close creases while direct highlights remain. Hero-mirror/SSR
replacement radiance is unoccluded by policy (section 5).

**Files:** native gather/filter/resolve formats and math; native permutations/entry points
in `class3/deferred/reflectionProbeF.glsl`, `class2/deferred/reflectionProbeF.glsl`,
`class1/deferred/deferredUtil.glsl`, `class3/deferred/softenLightF.glsl`, replay consumers;
the separate legacy gloss/environment overloads and class-2 conversion policy of section 5;
settings and harnesses. If texture-unit/link limits are exceeded, reduce native bindings or
fail native only, not stock shaders.

**Risk / effort:** high (probe/SSR separation, four consumer families at
`class3/deferred/reflectionProbeF.glsl:763`-`:934`, class-2 sRGB round-trip, artistic
compatibility), **12-16 days**; reviewers warn the lower end holds only without regression
surprises.

**Acceptance:**

- Open plane bent N dot shading N >= 0.999; asymmetric blocker bends toward the known
  opening; unit length error <= 1e-3 after resolve; zero moment never yields NaN; symmetric
  blockers introduce no preferred screen direction; byte-identical repeats.
- Roughness 0.05/0.25/0.5/1.0 x view-angle sweep: probe specular factor in [0,1], open
  case = 1; a blocked-cone fixture reduces only its expected indirect reflection. Compare
  materialAO 1/0.5 and native visibility 1/0.5 against a composition oracle to catch squaring.
- Direct sun glint and local-light glint unchanged. SSR-only and hero-only fixtures hold
  ambient equal and are unchanged by probe-only occlusion; **partial replacement weights
  0/0.25/0.5/1** show `mix(probe * factor, replacement, weight)` exactly; `glossenv` and
  `legacyenv` tested independently across spec.a = 0/>0, envIntensity = 0/0.5/1, SSR on/off
  and hero replacement; a post-mix AO multiply must fail; the class-2 sRGB round-trip at
  factor = 1 is bypassed. Probe capture buffers are unchanged; classic sky/probes-disabled
  fallback is finite and ambient-only in scope.
- Shared replay parity; Tron/rim/emissive and silhouette invariants; every receiver policy
  under HDR and non-HDR without misreading flags/carriers. Cost within the +35% bent budget.
- **Mutations:** bent N used as direct BRDF normal; AO applied twice through ORM; specular
  occlusion after the SSR mix; hero replacement multiplied by AO; replay consumers skipped.

### Later items (each requires its own approval and effort confirmation)

**L1 - Alpha (BLEND) receivers, indirect only (5-8 days).** The opt-in receiver of the alpha
policy (section 7): visibility multiplies ambient/probe irradiance and probe specular terms
before they are summed, never the final colour or any direct term. Files:
`class2/deferred/alphaF.glsl`, `pbralphaF.glsl`, `class3/deferred/materialF.glsl` (BLEND
branch), `sharedActorFxPbrF.glsl` BLEND variants (four companions). Gate: foreground hair
over an occluded wall uses neutral AO when depth mismatches; matching replay surfaces agree;
legacy/PBR hair, lashes and lace tested; **direct sun, local light and emission inputs
readback-equal** (D4); if the gate fails ship it disabled and label the limitation. Water is
excluded; a water receiver would be a separate, separately budgeted family (7). Priority is
set by the P0 BLEND survey.

**L2 - VCam feeds, combined ledger, capture-governor pin (12-16 days).** Native AO in real
camera feeds using each feed's own depth/projection: pack-local AO resources, the full
combined AO + Relight + VCam ledger of 9.4, capture-slot-order degradation, governor
pin/unpin (`llprismlens.h:353`-`:385`), Prepare-time feed preparation predicate (Prism
`:1770`/`:1830`), and the feed acceptance of Revision 3 P3 (1/2/4/8 feeds, mixed sizes, Gate
repeats, allocation failure at every target, resize, eviction, last close, ninth pass
rejected, report-only raster thresholds, no mid-take changes). Mutations: feed aliases main
target; budget counts IDs not passes; partial allocation publishes validity.

**L3 - High-resolution stills, snapshot transaction, Cinematic preset (4-5 days).** Single-
target snapshots: admit and prepare the actual still extent before `allocateScreenBuffer`
(`llviewerwindow.cpp:6340`), include snapshot scratch and restoration overlap in peak
accounting, render (`:6377`), restore and re-prepare the interactive generation
(`:6413`), whole-shot stock fallback on failure, reject incompatible requests during a
pinned take; 1x/2x/4x framing; Cinematic preset against the still ceilings. Tiled stills
stay a whole-shot stock fallback; padded tiles (overlap >= projected AO reach + all filter
support, global output-pixel noise coordinates) are a separately estimated item.

**L4 - Sun-blur dispatch cleanup (1-2 days; shrunk).** Because green is published after
the shared blur, native AO is already independent of sun softness. What remains is that the
blur dispatch is still gated on the stock SSAO flag, so the existing sun-softening modes
0-3 are unreachable when native runs with stock SSAO off. Add a dispatch condition (and, if
wanted, a shadow-only filter companion) preserving modes 0/1/2/3 and saved values. Gate: exact
channel readbacks show AO changes do not alter sun and sun-soften changes do not alter native
AO; mutation: the SSAO flag as the only gate; millimetres autoscaled twice.

**L5 - Later optional bitmask SSGI (8-15 additional days, separate approval).** Short-range
diffuse bounce using visible newly covered sectors. Separate master and strength, default
OFF; AO remains usable without it. Files: future source-radiance target and gather, bitmask
utilities, diffuse-only composition, target lifecycle, settings/UI/persistence and
harnesses; coordinate with existing SSGI designs and do not reuse the final
toned/graded/ReShade beauty as a light source. Needs a current-frame scene-linear source
with explicit direct/emissive ownership, first-hit/sector ownership, albedo/energy controls
and an account of existing probe irradiance. No previous-frame feedback, temporal history,
double-counted probes or new direct-light AO modulation. Acceptance: colored-wall bounce
reaches only reference-visible nearby surfaces; a wall-occluded receiver stays at baseline
within one RGBA8 step; duplicate samples cannot add energy twice; source intensity sweep
bounded and monotonic; SSGI OFF byte-identical to AO-only. Mutations: previous-frame
source, already-composited bounce as source, double sector ownership, final-beauty AO
multiply, unoccluded colour leaking through a solid wall. A detailed probe/SSGI energy
oracle is required before coding.

### Effort summary (Revision 4)

Core win: P0 2-3 + P1 6-9 + P2 14-20 + P3 6-9 = **28-41 engineer-days** (about 6-8 working
weeks for one engineer), main view, scalar AO, deferred and replay OPAQUE/MASK, response
choice, Director controls. Full scope: core + P4 12-16 + L1 5-8 + L2 12-16 + L3 4-5 + L4 1-2
+ shared qualification allowance 5-8 = **67-96 engineer-days**. Reviewers' ranges were 25-35
(reduced scope) to 60-90 (full scope). L5 adds 8-15 and is excluded from both totals. This
is a planning estimate, not a commitment; each later item is re-estimated before approval.

## 11. Screenshot-only "match the look" protocol

The user supplies captures of their modded iMMERSE **ON and OFF**, with the same camera,
scene, resolution, exposure, sky, materials and light-rig state. No proprietary file,
shader text, disassembly or implementation description is requested or inspected.
Reference images are user-provided visual targets only.

**Use by phase (Revision 4).** P0 executes this protocol and fixes the camera bookmarks and
frozen settings; P1 re-shoots a four-scene subset (sunlit wall + railing, interior corner,
avatar fingers/face, foliage/fence) as the A/B the user decides on (D1); P3 runs the
calibration targets below. The A/B in sunlit scenes is deliberately the first thing
shown, because ambient-only AO may under-deliver there (section 3.3).

For each scene retain four labeled images: (A) viewer stock + external AO OFF,
(B) same viewer stock + user's reference effect ON, (C) native AO + external AO OFF,
(D) native AO debug visibility/bent normal. Also capture native master OFF to verify
baseline restoration. External non-AO grading may stay active only if identical in all
beauty comparisons. If the user's RTGI switch couples bounce and AO, record that and
compare the AO/contact component separately; P1–P4 cannot match bounce color changes.

Required scenes: indoor corner/under furniture; two sunlit exteriors (high and low sun), each with a white wall, a nearby railing and an avatar;
close avatar fingers/face/eyelashes/hair; alpha-mask fence and foliage; glossy PBR clothing
beside legacy glossy prims; nighttime rig with point/projector highlights; emission/Tron/
Roto silhouette; large populated region; 1/4/8 camera feeds and high-res/tiled stills (reference only; native feeds and stills are L2/L3).
Settle textures/probes and freeze world animation where feasible; take an ungraded
scene-linear diagnostic alongside display-referred screenshots.

Register image pairs and agree named ROIs: open wall, contact strip, finger gap,
background behind rail, hair edge, probe glint and direct-light highlight. Record mean
linear luminance ratios, contact width in pixels/metres, halo width and gap visibility.
Use a first calibration target of reference contact-ROI ratio within ±10%, contact width
within ±2 pixels at 1080p, without violating the stronger lit-gap/direct/emissive gates.
Do not optimize whole-image similarity by darkening sun or crushing the grade. Let the
user rank the final Natural / Defined / Reference-match appearance variants from paired
screenshots; quality remains an independent control. Freeze accepted values with image
IDs and settings so later shader changes can repeat the comparison.

## 12. Tier B decisions for the user

These choices are recorded for implementation, **not permission to make changes now**.
Recommendations supply a concrete starting design; unapproved alternatives are not
silently treated as agreed requirements. Rows changed in Revision 4 are marked (R4).

| ID | Decision | Recommendation / consequence |
|---|---|---|
| B1 (R4) | Replace stock SSAO permanently or keep it selectable? | Keep Stock / Native during rollout, native master default OFF. There is one bitmask gather; thickness tending to infinity is the Horizon diagnostic, not a separate algorithm. Once qualified, native can become the preferred choice with explicit migration. |
| B2 (R4) | Default thickness and intended scene scale? | Start 3 cm; sweep 1/3/10/30 cm in P0-P1 and freeze in P3. Keep Relight thickness separate. Depth-step floor and distance fade apply (6.1). No automatic distance scaling of the physical thickness. |
| B3 | Should avatars under Actor Mask receive AO? | Yes, like other physical surfaces; Actor Mask remains local-light recipient selection. Offer a later receiver exemption only if desired; avatars/world still occlude each other. Flat Cover/silhouette/emission retain creative policy. |
| B4 | Expose native AO to ReShade? | Initially no new ABI texture; user disables external MXAO/RTGI AO while native is enabled. Later optional append-only ABI AO visibility/bent-normal semantics need generation/validity, own lifetime and consumer agreement. Never store it in material AO. |
| B5 (R4) | VCam coverage versus live performance? | Core phases are main view only. Native feeds are L2, an explicit opt-in after the combined ledger and governor pin exist. Preview/media remain neutral; feed status reports stock fallback rather than sampling main AO. |
| B6 (R4) | Blended hair and alpha? | Keep the BLEND baseline. P0 measures how much of the user's content is BLEND (default `RenderAutoMaskAlphaDeferred` = 0). L1 is an optional indirect-terms-only receiver. Water stays neutral. Alpha-tested geometry is included from P1. |
| B7 (R4) | Native ambient response: stock-compatible floor/HSV or linear visibility multiply? | **Selectable in P1** (default stock curve); the user compares both against the reference; final default chosen in P3. Exactly one response applies and neither touches direct light or emission. |
| B8 | Old scene without AO keys: inherit current native AO or stock? | Select stock compatibility for old sparse scenes. Creative presets exclude master/quality; scenes own the master, machine owns cost ceilings. |
| B9 (R4) | Hi-res and tiled stills? | Core phases: explicit whole-shot stock fallback for any snapshot. L3 adds native single-target stills via a preparation transaction. Padded tiles only with seam-free capture gates and a separate estimate. |
| B10 (R4) | Hardware and recording target? | The only qualification machine is the RTX 5090 (10/21). Use the 9.3 thresholds. Recommended capture settings: High at 1080p, Balanced at 4K until measured, under the opt-in Capture-5090 profile. The distributed default offers Balanced only. Mid-range is INCONCLUSIVE without tester captures. |
| B11 (R4) | Relaxed Low/Balanced noise gates, or sigma <= 0.005 everywhere? | Low is conditional (<= 0.025, Horizon-equivalent only); Balanced <= 0.010; High and Cinematic <= 0.005. Sigma is *secondary*: pan/swim on real captured scenes, bias and contact-width gates decide. If every preset must reach 0.005, withdraw Low and Balanced. |
| B12 (new) | Non-HDR 8-bit light map: dither or HDR-only? | Recommend HDR for the capture profile; allow non-HDR with the deterministic dither of 8.2. HDR-only is the simpler, stricter alternative. |
| B13 (new) | Ceilings: one conservative profile or an opt-in 5090 profile? | Both: distributed default (160 M live gather, 2,000 M combined) and a machine-setting Capture-5090 profile (1,200 M live, 4,000 M combined). Scenes and looks can never select it. |
| B14 (new) | Hero-mirror/SSR radiance stays unoccluded? | Yes (section 5): SSR hits and hero probes already carry their own scene occlusion; only ordinary probe radiance and the probe share of partial replacement are attenuated. |

No choice here permits proprietary code reuse, temporal AO accumulation or final-image
darkening. Those are fixed constraints.

## 13. Sun contact shadows: removed from this plan (Revision 4)

Revision 3's section 13 proposed a current-frame sun/moon contact-visibility pass written
into light-map **red**, before the sun filter. Red feeds `scol` and so `sun_contrib`
(`softenLightF.glsl:294`): that machinery darkens **direct** sun, which contradicts this
plan's fixed rule that direct light, light-rig emitters, emission and graphic overlays are
never multiplied by AO-derived visibility (reviews G4, and the DeepSeek/Codex direct-light
checks). It was labelled "adjacent", yet it was specified here. It is therefore removed:

- Nothing in P0-P4 or L1-L4 reads or writes light-map red, traces toward `light_dir`, or
  alters sun/moon visibility. The native publish writes **green only**; "publish into R as
  well as G" is a standing mutation that must fail.
- If, after the P1 A/B, the user wants sunlit contact definition that ambient-only AO cannot
  supply (decision D1, outcome NARROW), it becomes a **separate feature with its own
  decision record**: what it darkens, light-map red ownership, interaction with the
  ShadowDist modes, offscreen fallback, camera-motion alignment, no double edge, contact
  gates analogous to Relight's, budget and effort - and no dependency on this plan's
  phases. The concepts in `screenSpaceRayUtil.glsl` (snapped depth texel, receiver
  footprint, refined hit) are reference material for that record, not commitments.

## 14. Principal risks and release conditions

| Risk | Specific mitigation / release condition |
|---|---|
| Halos / foreground AO bleeding | Correct finite intervals, edge/normal-aware filtering, actual sample coordinates, neutral invalid-neighbor resolve. Thin-gap and halo-width gates at full/half/mips. |
| Sparse-sample porosity (systematic under-occlusion) | Sample-gap linking, thickness floor for linked runs, thickness sweep to 0.30 m, corner/wall/arm-torso bias gates at **every** preset (6.1); Low conditional. |
| Thick-object light leaks | Fixed thickness is an approximation, not geometry knowledge. Thick-wall reference fixtures and bounded radius; document leakage rather than hiding it with a final-image multiply. |
| Hair, alpha and missing backfaces | Separate OPAQUE/MASK/BLEND expectations; P0 measures the BLEND fraction (default `RenderAutoMaskAlphaDeferred` = 0); L1 receivers indirect-only; water neutral; no transparent caster claim without an additional depth representation. |
| Replay actors invisible to the AO input | AO-owned input overlay (8.1), budgeted, with a depth-only fallback; stock SSAO has the same blindness so P1 is not a regression. |
| Normal-map self-occlusion / bent artifacts | Separate macro and shading normals, bounded bias/hemisphere correction, cosine-weighted moment oracle and zero-moment fallback. |
| Probe/material/SSGI double counting; hero/SSR | Keep authored AO, native visibility, probe influence and later bounce distinct. Probe-only specular hook before SSR replacement; hero/SSR radiance explicitly unoccluded; never bake screen AO into probes. |
| Cost in large scenes / crowds | Pixel/sample ceiling bounds gather but not whole rendering/texture misses. Measure dense SL regions, alpha overdraw, probes, Relight and feeds together; ceilings as machine settings with two profiles; stable capture plans. |
| Hardware generality | Only the RTX 5090 is qualified; other GPUs INCONCLUSIVE until tester captures; conservative distributed default. |
| Large radius / depth mip loss; distant banding | Physical short-range radius, projected cap, edge-LOD refinement, depth-step bias floor and distance fade; padded tiles or explicit fallback. |
| Shader portability / texture units | Raster-first GLSL, safe unsigned masks, native-only capability fallback, real links on NVIDIA/AMD/Intel as available; no unsupported-device "PASS" from a CPU-only model. |
| Toggle hitches / resource churn / first-use hitch | Default-OFF explicit Prepare; static GL/extent support and latched admitted resources, never live governor-driven allocation predicates; warm-up draws for every program state and cold/warm first-transition ceilings (8.5); prepared light map/bundles retained across native OFF; white initialization and lighting/DOF ownership gates; zero allocation/reload toggles, 300-frame/120-second evidence. |
| Prepared OFF is not byte-identical under streaming | Identity is defined under frozen texture streaming and guaranteed headroom (section 1); otherwise the reserve is a disclosed cost shown in the Prepare status. |
| Screen-locked noise / motion swimming without TAA | Primary gate = pan/swim on real captured scenes plus hardware pans; P0 oracle with cross-offset covariance and the real stratified sequence; surface-anchored candidate; filter support <= +-4 px with contact-width gate; Low/Balanced withheld until motion passes. |
| Relight plane churn breaks zero-allocation | Fixed in the Relight stack-fixes job; AO consumes the admitted plan read-only; combined claim INCONCLUSIVE until it lands (9.4). |
| Combined AO / Relight / VCam overcommit | Main-view ledger in P2, full ledger with main-first reservations and feed-first deterministic reduction in L2; no timing-driven quality changes or per-frame eviction churn. |
| Ambient-only AO too weak in sunlit scenes | P1 A/B before P2 funding; user decision D1; no direct-light darkening under this plan (section 13). |
| Creative looks regress | Direct/emissive/rim/Tron/silhouette invariants, shared Actor FX parity, screenshot ROIs, versioned scene migration and preserved stock path. |
| Overclaimed determinism | Equal-input AO readbacks across runs, frozen seed, no history; record whole-viewer input differences and never imply camera-only or cross-driver bit identity. |

Shipping evidence must include the named positive fixtures, rejected mutations, raw
readbacks, OFF baseline diffs, shader-reload/allocation counters, per-pass p50/p95/p99
timings and the user's paired look captures. This plan establishes that work; it does
not claim those implementation gates have already passed.

## Decisions the user needs to make (Revision 4)

Folded-in 10/21 decision: the only test machine is an RTX 5090; hard caps are
hardware-independent deterministic work budgets per profile; millisecond targets are
measured on the 5090 (9.3); other GPUs are checked from beta-tester Tracy captures and the
logged work plan, and are INCONCLUSIVE until then.

| ID | Decision | When | Recommendation |
|---|---|---|---|
| D0 | Approve P0 (2-3 days, no viewer code) and supply the reference ON/OFF captures of section 11 | Now | Approve; it is the cheapest evidence and can stop the project. |
| D1 | After the P1 A/B: GO (fund P2-P3), ADJUST, NARROW or STOP | After P1 | The key decision. Judge the sunlit scenes first. NARROW means accept ambient-only AO or commission the separate sun-contact feature of section 13. |
| D2 | Low preset: drop, or keep as Horizon-equivalent only | After P0 | Drop unless the oracle shows it passes bias, contact-width and motion. |
| D3 | Noise anchoring: screen-anchored or surface-anchored | After P0 | Let the measured pan/swim result decide; the user reviews the paired pan captures. |
| D4 | Adopt the opt-in Capture-5090 ceiling profile and the 9.3 5090 thresholds | Before P2 | Adopt; keep the distributed default conservative. |
| D5 | Non-HDR light map: dither or HDR-only | Before P2 | HDR for the capture profile; dither otherwise. |
| D6 | Scope after the core win: P4 bent/probe, L1 alpha receivers, L2 feeds, L3 stills | After P3 | P4 first; L1 priority from the P0 BLEND survey; L2/L3 only on demand. |
| D7 | Accept hero-mirror/SSR radiance unoccluded | Before P4 | Accept (B14). |
| D8 | Confirm the Relight stack-fixes job lands before P2 acceptance | Before P2 | Confirm; otherwise the combined zero-allocation claim stays INCONCLUSIVE. |

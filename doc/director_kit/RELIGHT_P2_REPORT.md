# Relight Phase 2 report

2026-10-20. Implemented on `feature/director-kit-p0`, against unchanged HEAD
`8b033a0360a`. The user's decision supersedes brief D5: Phase 1 and Phase 2 will
be judged together. No Phase 3 work was added. No commit, push, staging, stash
operation, viewer build or viewer launch was performed. Existing unrelated
untracked work and the dirty `vcpkg` submodule were preserved.

## Delivered against brief section 4

- **4.1 selection:** CPU selection starts at `mLights`, with a read-only on,
  colour, range and frustum filter. A read-only intersection reproduces the
  current deferred collector's actor/attachment/world toggles and its existing
  count-before-colour/range/frustum budget, without modifying that collector.
  Actor Mask's independent uncapped draws and F0/F3 routing are included.
  `mNearbyLights` supplies draw membership only; its faded weights, distances,
  order and pointers do not supply ranking scores or ties. Rank is unshadowed
  first, projected radius times luminance times camera attenuation second,
  UUID last. There is no rig-first rule. The fixed eight-entry insertion list
  allocates no per-frame storage. Cap verdicts are logged, never silently cut.
- **4.1 visibility:** one fullscreen RGBA8 gather plane per four selected
  lights, at half width/height by default. Fixed equal-area disk strata and
  golden-angle azimuths face the receiver. Radius comes from Phase 1's source
  metadata, including fixture source size / 2, even if Light shape is disabled.
  Spatial integer hashing uses pixel coordinates and the selected light's own
  UUID, never channel, frame, clock, previous frame or `noiseSine`.
  `screenSpaceRayUtil.glsl` is a current-frame subset of the repository's public
  SSR projection/linear-depth helpers. Rays march linearly with the SSGI design
  thickness test; off-screen/behind-camera unsupported segments count as lit.
  The world-space normal/start biases are 0.02/0.05 m.
- **4.1 denoise:** depth/normal bilateral horizontal and vertical spatial
  passes, followed by a full-resolution four-tap bilateral upsample. No history
  or temporal accumulation. Denoise radius zero skips the two blur draws.
  Denoise footprint and spatial jitter cells scale relative to 1080p, accounting
  for half resolution. March jitter itself is a dimensionless fraction of the
  world-space march step, not an unscaled screen-pixel offset.
- **4.1 lighting:** deferred box/batched points, box/fullscreen projectors and
  Actor Mask wrappers consume explicit-LOD visibility reads. Selected channels
  live in `light_shape.w`; unselected, disabled and excluded-view CPU data have
  `w=-1`. Forward/alpha continue to use the original shape helper and `w=-1`.
- **4.2 projectors:** the existing projector shadow variable becomes
  `min(existing shadow-map visibility, screen visibility)` for selected lights.
  Existing cookie/cone, ambiance, Rig Rim controls, environment-reflection
  behavior and PCSS-lite code remain in place. Shadow maps still supply
  off-screen occlusion; point lights retain the screen-space limitation.
- **4.3 views/lifecycle:** `areaShadowsAllowedThisView()` rejects cube captures
  (including hero mirrors), Prism/VCam, HUDs, impostors, water reflection and
  tiled snapshots (`getZoomFactor()>1`, with an explicit log). Targets belong
  to the active render-target pack, allocate lazily in an allowed view, resize
  from actual deferred target dimensions, and invalidate/release beside `mRT`,
  including resolution divisor changes. Turning the master off releases them
  lazily, including the null-cull path. Steady-state OFF allocates/runs nothing.

`RenderAreaShadows` and every dependent setting are runtime values. The existing
`relight_enabled` upload now carries shape in bit 0 and area visibility in bit 1.
Phase 1 readers test bit 0 explicitly. This adds **no master uniform upload call**
on the OFF path and no compile define, shader reload listener or `setShaders()`
on any area-shadow toggle. Nonzero stale GPU channel storage is deliberately
left untouched while OFF and ignored by bit 1; clearing GPU storage would itself
violate the no-extra-upload rule. CPU shape values are always the -1 sentinel
while OFF. Phase 1's live floor/boost and forward light hashing are preserved.

Mirror rig emitters (projectors, bounce omnis and catchlights) are explicitly
excluded by their own IDs through `isMirrorLight()`. They retain derived,
clone-only lighting and cast no area shadows. Copies never borrow a source
actor's channel. Actor Mask recipient bits and the source metadata used for
Phase 1 remain unchanged.

The fullscreen pass saves/restores texture wrappers/bindings and selectors,
shader, colour masks (including indexed masks), indexed blend enables, draw/read
FBO/buffers, viewport and scissor. Scoped guards restore blend, depth, cull and
stencil enables. The RT bind/flush stack is balanced on every dispatch.
No G-buffer format/layout, ReShade ABI, `llprimitive` clamp, stock light-budget
ordering, shadow-map allocation or `BDMergeMaxSpotShadows` policy was changed.
The proprietary ReLight files were never read, opened, ported or paraphrased.

## Settings and Director click path

**Machinima > Director Console... (F2) > Light > Rim > Advanced > Soft shadows.**
The real card is `kc_area_soft_shadows`, next to `kc_light_shape`. Its generated
header switch `area_shadows_enable` binds `RenderAreaShadows`. All eight real
controls have `enabled_control="RenderAreaShadows"`, so they are disabled while
OFF; this is explicit because the generic card switch only dims its body.
The Rim page master leaves independent card controls reachable.

| Setting | Default | Bounds / behavior |
|---|---:|---|
| RenderAreaShadows | false | master |
| RenderAreaShadowLights | 4 | 1-8 |
| RenderAreaShadowQuality | 1 / Medium | Low, Medium, High, Cinematic, Custom |
| RenderAreaShadowSamples | 4 | 1-32 |
| RenderAreaShadowSteps | 12 | 1-64 |
| RenderAreaShadowMaxDist | 8 m | 0.1-64 m |
| RenderAreaShadowThickness | 0.35 m | 0.001-4 m |
| RenderAreaShadowHalfRes | true | half width and height |
| RenderAreaShadowDenoise | 2 | 0-8 reference pixels; effective kernel capped at 8 |

The brief leaves distance/thickness/denoise defaults unspecified; 8 m, 0.35 m
and radius 2 reuse the SSGI design defaults. Presets set S/steps/resolution
together: Low 2/8/half, Medium 4/12/half, High 8/20/half, Cinematic 16/32/full.
Editing one of those three knobs marks the preset Custom, making reselecting
the previous named preset reliably reapply it. Other advanced settings stay live.
Tooltips describe cost, blocker tolerance, screen-space limits and resolution.

Source: `scripts/code_tools/director_kit/director_mockup_data.json`. Row pin is
2154, with a Phase 2 comment (+8 rows; master is a header switch). All three
requested generators were regenerated after the final C++ edits and checked.
Generated page hashes account for changes across the other generated panels.
JSON source, page sources and functional manifest retain LF in index/worktree.

## Re-anchoring

Brief references describe `057e558`; implementation was traced against HEAD
`8b033a0360a`, including its committed Phase 1 runtime fix and latest in-world
report corrections. These are exact function/setting anchors, not the brief's
old offsets. Working-tree lines are before future review edits.

| File / symbol | HEAD | Phase 2 |
|---|---:|---:|
| `indra/newview/pipeline.cpp` / `bool LLPipeline::allocateScreenBufferInternal` | 1302 | 1304 |
| `indra/newview/pipeline.cpp` / `void LLPipeline::releaseScreenBuffers` | 3180 | 3187 |
| `indra/newview/pipeline.cpp` / `void LLPipeline::bindDeferredShader(` | 24891 | 24919 |
| `indra/newview/pipeline.cpp` / `void LLPipeline::renderDeferredLighting` | 26506 | 26862 |
| `indra/newview/pipeline.cpp` / `F32 LLPipeline::setupSpotLight(` | 28527 | 28888 |
| `indra/newview/llviewershadermgr.cpp` / `gDeferredLightProgram.mName` | 2858 | 2863 |
| `indra/newview/llviewershadermgr.cpp` / `gDeferredSoftenProgram.mName` | 3615 | 3638 |
| `indra/newview/alcinerigrim.cpp` / `void ALCineRigRim::bindAnimated` | 251 | 251 |
| `indra/llrender/llshadermgr.h` / `RELIGHT_ENABLED` | 946 | 949 |

New pipeline anchors: `areaShadowsAllowedThisView` at 26545,
`renderAreaShadows` at 26567, dispatch before local lighting at 27508.
The Soft shadows card begins at `panel_dpage_light_rim.xml:231`; Light shape
remains at 215. The source count-before-filter code was left in place.

## Gates and evidence

| Gate | Verdict / evidence |
|---|---|
| Shader validation + extended relight_shader_check.py | PASS: 31 production GLSL units/permutations, 44 production program links, original helper/live-master probes, 8 rig fixture programs and 126 material/geometry rows with zero failures. Adds production visibility/filter links and runtime checks. NVIDIA RTX 5090 / OpenGL 4.6 / driver 610.88. `.tmp/relight_p2_shaders_final.log`. |
| Area-light runtime | PASS: production multiPoint/point/spot main bodies; all 8 channels in both RGBA planes; legacy/PBR; shape ON/OFF; stale channel ignored while master OFF; exact unselected identity. Same final log. |
| GPU determinism / spatial filtering | PASS: actual gather/filter programs, fixed 32x32 RGBA8 repeated byte equality, occluder and penumbra positive controls, off-screen lit, ID/channel permutation. Half-resolution source upsample and discriminating depth/normal edge tests also PASS in `.tmp/relight_p2_area_gpu.log`. This is a controlled fixture, not a full viewer frame. |
| alrelightmath + new pure CPU tests | PASS 12/12. Existing 9 tests plus all 120 ranking-input permutations, leaking-first order, UUID ties, caps, all 63 excluded-view flag combinations, contribution literals, presets, integer hash and equal-area disk strata. Standalone test only recompiled/relinked using existing runner/libraries. `.tmp/relight_p2_math{_compile,_link,}.log`. Existing nonfatal vswhere message remains. |
| OFF / Phase 1 preservation | PASS: all eight disabled source-expression streams equal HEAD after removing new disabled area blocks. All setting/reload guards PASS (`.tmp/relight_p2_off.log`). GPU comparison against HEAD: 1890 OFF and 1890 Phase 1 ON float32 component readbacks byte-identical (`.tmp/relight_p2_identity.log`, `relight_p2_baseline.json`, `relight_p2_after.json`). Full-scene OFF-vs-baseline image diff remains INCONCLUSIVE pending in-world testing. |
| Separate zs_check calls | PASS: llrender/llshadermgr.cpp; newview/alcinelightrigmanager.cpp, alcinerigrim.cpp, llviewercontrol.cpp, llviewershadermgr.cpp, pipeline.cpp, tests/alrelightmath_test.cpp. One file per call, `.tmp/relight_p2_zs_*.log`. These also cover every touched header, including alareashadowmath.h through the test. Supported CachePath relocated into workspace. |
| Director generation/checks | PASS all three generators and all three --check modes; 2154-row pin, eight real enabled-control bindings verified. Manifest retains its five existing warnings. `.tmp/relight_p2_{kit,sources,manifest}*.log`. |
| prebuild_guard.ps1 -ChecksOnly | PASS all 11 gates, exit 0. Installed guard unchanged via `.tmp/reflect_prebuild_checks.ps1`, with GIT_CONFIG_COUNT=1, GIT_CONFIG_KEY_0=core.safecrlf, GIT_CONFIG_VALUE_0=false and supported compiler cache relocated into the workspace. `.tmp/relight_p2_prebuild_final.log`. ChecksOnly exited before any object purge. |
| git diff --check / LF | PASS `git diff --check`; new source/shader/test/report artifacts also directly checked for whitespace/conflict markers. `git ls-files --eol` confirms source JSON, page_sources.json and functional_manifest.json are i/lf w/lf. |

A first gate run caught the new card colliding with an existing World card's
widget name; it was renamed `area_soft_shadows` and regenerated. The first C++
syntax checks caught matrix/vector constructor conversions; those were corrected
and the affected TUs passed again. No viewer build was used to resolve them.
No independent Opus review was performed in this implementation session.

Reproduction: `python scripts/code_tools/relight_shader_check.py --rig-json
.tmp/relight_p2_after.json`; `--area-only` is the short production area-program
fixture. `--rig-only --shader-ref HEAD --rig-json .tmp/relight_p2_baseline.json`
reads committed shader sources without checkout/stash. Byte comparisons in
`.tmp/relight_p2_compare.py` compare both components of each recorded pair.

## Instrumentation and expected performance

`RelightQ` debug logs record allowed-view and every capped selection verdict.
The once-per-second enabled summary records selected/candidate counts, cap,
S, steps, trace resolution and GPU milliseconds. Two lazily created timestamp
queries are reused; results are polled without waiting, and the log explicitly
says INCONCLUSIVE while the first result is pending. Queries are released on
OFF and with screen buffers. Tracy also has an `area shadows` GPU zone.
No frame timestamp feeds selection, disk samples, march jitter or filtering.
The standalone GPU test logs `[RelightQ] determinism PASS/FAIL` through assertions.

Upper bound on depth reads is `trace_pixels * K * S * steps`, before background,
blocker and off-screen early-outs. Costs below are estimates, not viewer timings:

| 1920x1080, K=4 | S / steps | Trace pixels | Maximum march depth reads |
|---|---:|---:|---:|
| Low | 2 / 8 | 518400 | 33.2 million |
| Medium (default) | 4 / 12 | 518400 | 99.5 million |
| High | 8 / 20 | 518400 | 331.8 million |
| Cinematic | 16 / 32 | 2073600 | 4.25 billion |

K=8 doubles march work. Doubling S or steps doubles it; full resolution is
approximately four times half-resolution march work. Cinematic is expensive
and intended for measured capture use. Blur is two separable kernels up to
17 taps, plus four upsample neighbours per plane. At default 1080p half-res,
the effective blur radius is 1 trace pixel. CPU selection is O(mLights * nearby)
for a filtered draw-membership scan and O(8) insertion, avoiding a temporary
membership allocation. Large light populations should be profiled.

One plane needs gather + scratch + full visibility: about 11.9 MiB at 1080p
half-res, 23.7 MiB at full-res. K>4 doubles it. These targets allocate only on
activation/resolution/plane-count transitions; selection, state snapshots and
steady-state dispatch allocate no per-frame containers or GL objects.
Actual viewer GPU milliseconds, visual quality and frame-time impact remain
unmeasured because no viewer build/launch was authorized.

## Combined Phase 1 + Phase 2 in-world checklist (brief section 9)

Run after independent review and the separately authorized viewer build. Keep
camera, resolution, exposure, pose, environment, materials and rig FX fixed.
Use a wall and floor, avatar 0.5 m from the wall, fixture-mode rig key with
source size 0.6 m, and a world point light behind the wall.

- [ ] Capture committed baseline and both masters OFF at fixed resolution;
  require zero full-image difference. Toggle each master ON/OFF/ON without a
  shader reload or disconnect. Change sub-settings live, including while OFF.
- [ ] Phase 1: sweep source size 0.01 -> 0.6 m and larger; highlights broaden,
  diffuse wraps at the horizon, and frontal diffuse remains stable. Check
  legacy gloss and PBR roughness, preserving the latest Phase 1 corrections.
- [ ] Physical falloff: reference brightness agrees within 1% while uncapped;
  farther wall darkens, close objects brighten. Inspect calibration verdicts;
  move lights within 5 cm/through the source and require finite bounded output.
- [ ] Phase 2: the on-screen blocker stops the behind-wall point leaking onto
  opaque avatar surfaces. Shadows sharpen near contact and soften with distance.
  Give the world light a nonzero default source radius to judge its soft edge;
  radius zero should give a hard point-source shadow.
- [ ] Try all presets and K=1/4/8; inspect selected/candidate/cap/GPUms logs.
  Add >K lights, including shadowed rig projectors and leaking world points;
  leaking lights win. Repeat exact-score ties and channel reorderings.
- [ ] Check legacy/PBR, box/fullscreen volume boundary, projector cookies,
  projector min(map,screen), PCSS-lite, ambiance and environment reflections.
  Off-screen geometry still requires a shadow map; screen-space point shadows
  cannot supply it. Sweep thickness/distance to inspect thin blockers/halos.
- [ ] Check half/full resolution, denoise zero/nonzero and silhouette edges;
  resize and change RenderResolutionDivisor. Tiled snapshots log disabled;
  untiled higher-resolution captures retain the spatial footprint.
- [ ] Verify alpha hair/sheer cloth retain Phase 1 lighting without borrowing
  wall visibility. Alpha soft shadows are deferred by D6.
- [ ] Check Actor Mask, F0/F3, Rig Rim normal/debug/silhouette, bounce on/off,
  and Reflect/True Mirror rigs. Copies remain clone-only, unshadowed, and do
  not consume a source actor's visibility channel or shadow-map slots.
- [ ] Confirm probes, hero mirrors, Prism/VCam, HUDs, impostors and water
  reflection retain prior lighting; verify ReShade bridge output as before.
- [ ] Render the identical fixed frame twice with both masters ON; pixel-diff
  the entire output and require `[RelightQ] determinism PASS`, recording camera,
  resolution and settings. The standalone fixture does not replace this test.
- [ ] Visual reference A/B: engine features OFF for the user's authored ReShade
  setup, then that effect OFF for the engine version at comparable positions.
  Compare highlight shape, falloff, softness and contact hardening. Expect
  screen-space blind spots and more spatial noise at equal sample counts.

## In-world tuning 10/20

Applied to HEAD `358b6fbadf8` following the night-alley comparison. No commit,
push, viewer build/launch or stash operation. Unrelated workspace files and
the dirty `vcpkg` submodule were preserved.

### Wall-lamp self-shadow fix and live tuning

The old gather marched to the light sample itself. A light embedded in its
fixture could therefore lose its own wall/door pool. The shared screen-space
march now stops `max(source radius, 0.1 m) + effective light bias` before each
disk sample. Hits with linear camera depth within the effective bias of the
light centre are ignored. Background depth is explicitly unsupported/lit.
The dimensionless spatial march jitter and light-UUID sample seeds remain
unchanged; there is no frame/history input.

New `RenderAreaShadowLightBias` defaults to **0.25 m**, with CPU finite-value
validation and bounds 0-4 m. Its effective value is
`max(clamped setting, 0.5 * clamped thickness)`. At the reported 0.35 m
thickness this is 0.25 m; a point source therefore has a 0.35 m end gap.
Medium remains K=4, 4 rays, 12 steps, 8 m, 0.35 m, half resolution, denoise 2.
The housing regression supports retaining these cost/quality defaults with
the new exclusion. The world-light source radius default remains zero.
Blockers inside the source exclusion, or at camera depth within the bias of
the source, can intentionally be missed; use the smallest bias that keeps the
fixture lit. This does not remove the existing off-screen limitation.

Receiver starts now use a normal offset of `max(0.02 m, 1.5 * world-space
trace-pixel footprint)`. Adjacent positions are reconstructed at the receiver's
own depth, so the offset follows distance, projection and actual trace
resolution without expanding across depth silhouettes. The old fixed 5 cm
march addition was removed; samples stay inside the shortened segment.

**Director > Light > Rim > Advanced > Soft shadows** now includes **Light end
bias (m)** and **Visibility view**. The latter binds the new nonpersistent
`RenderAreaShadowDebugView`: Off or selected light 1-8. It displays the current
full-resolution visibility channel as grayscale after all image/present effects
(white=lit, black=shadow). Invalid/unselected channels show the normal image.
Debug is independent of the Rim master and cannot read stale/excluded-view
targets. Normal filtering explicitly resets its debug selector to zero.
Both new settings are live, with no shader-manager dependency/reload listener.
The master-OFF branch adds no gather/filter/debug draw or uniform upload.

### Synthetic evidence

`python scripts/code_tools/relight_area_check.py` now runs directly through the
existing hidden WGL harness. Its production gather receives a wall at Z=-4 m,
a point light at Z=-3.95 m (5 cm in front), and a 0.30 x 0.30 x 0.45 m box
housing. The visible front face and surrounding wall form the depth fixture.
At Medium settings the near-wall minimum visibility is **1.000 (>0.9)**; adding
a separate intervening opaque slab gives minimum visibility **0.000**. Both
full and half trace viewports pass. Bias zero reproduces fixture shadows;
restoring 0.25 produces byte-identical lit output on the same linked program.
The updated fixture fails against HEAD's original gather at the wall-lit
assertion, demonstrating that it detects this regression.

Additional checks cover a shallow housing at fine thickness (light-depth
rejection), eight repeated byte-identical wall-shadow frames, all four local
grayscale channel selectors used by either RGBA plane, the existing blocker/
penumbra/channel-permutation/filter/upsample controls, and the production
receiver-offset helper under perspective projection. The latter measures the
2 cm floor and literal 0.0625/0.125/0.03125 m offsets as distance/resolution
change. Evidence: `.tmp/relight_tuning_area.log` and
`.tmp/relight_tuning_area_head_negative.log`. These are controlled GPU fixtures;
the user's actual alley still needs an in-world visual comparison.

### Card layout and why the earlier gate missed it

Source kit data now sets `full_width=true` on Diagnostics, Light shape and
Soft shadows. Each occupies its own main-column row instead of placing Light
shape beside Diagnostics. ALFlowGrid gives a spanning card the available
viewport width, including at the 1160 px default and 850 px minimum floater
widths; card body controls retain their normal responsive layout.

The old checker deliberately bypassed runtime kit interiors. Its kit geometry
rules rejected authored coordinates, and font checks measured row labels, but
neither enforced where the runtime grid placed these cards. Generator
`--check` modes verified generated-data freshness rather than this layout.
This allowed valid rows inside an incorrectly column-packed card to pass.

`xui_layout_check.py` now enforces the Rim Advanced main-column contract even
under zero-size generated page roots. It requires all three card span flags
and pins their actual C++ consumption through ALKitCard/ALFlowGrid. Its selftest
uses the real generated page and removes each relight span in turn; both
mutations must fail with `kit-card-columns`. This is an explicit source-layout
contract, not a claim of a viewer screenshot measurement. No existing rule,
baseline or finding allowance was relaxed.

### Gates

| Gate | Result / evidence |
|---|---|
| `relight_shader_check.py` | PASS: 31 production units/permutations, 44 production program links, live uniforms/runtime lighting, 126 rig rows with zero failures, production area gather/filter and receiver probe. `.tmp/relight_tuning_shader.log`. |
| `relight_area_check.py` | PASS: housed-lamp, separate blocker, half/full, live bias, grayscale channels, receiver scaling, denoise/upsample and deterministic readbacks. `.tmp/relight_tuning_area.log`. |
| `alrelightmath` and area math | PASS 13/13, including default/thickness/live/nonfinite/clamped light bias. Standalone compile/link only, using the existing test runner/libraries. `.tmp/relight_tuning_math{_compile,_link,}.log`. |
| OFF/runtime identity | PASS: eight disabled expression streams unchanged; both new settings have no reload dependency; 1,890 OFF and 1,890 Phase 1 ON float32 component readbacks byte-identical to HEAD with area shadows disabled. `.tmp/relight_tuning_off.log`, `.tmp/relight_tuning_identity.log`. Full viewer-frame equality was not measured. |
| Separate `zs_check -Files <one file>` calls | PASS: `pipeline.cpp` and `tests/alrelightmath_test.cpp`, one call per file. `.tmp/relight_tuning_zs_pipeline.log`, `.tmp/relight_tuning_zs_test.log`. The test also covers the changed pure math header. An initial signed comparison warning was fixed and the final TU passed again. |
| XUI layout build gate | PASS: `--changed HEAD`, **40 files, zero findings**; direct Rim page zero; `--selftest` PASS including the new negative regression. `.tmp/relight_tuning_layout_changed.log`, `.tmp/relight_tuning_layout_selftest.log`. Additional `--all` inventory reports 2,515 findings in 318 untouched files; none is in a file changed by this task (`.tmp/relight_tuning_layout_all.log`). |
| Director outputs | PASS: regenerated `director_kit_gen.py`, `director_page_sources.py`, `director_functional_manifest.py` after the final C++ edit, then each `--check`. Row pin 2156; manifest retains five existing warnings. `.tmp/relight_tuning_{kit,sources,manifest}*.log`. Source JSON, page sources and functional manifest are LF. |
| `prebuild_guard.ps1 -ChecksOnly` | PASS all 11 gates, exit 0; `.tmp/relight_tuning_prebuild.log`. Uses the installed guard unchanged through the existing workspace cache wrapper, with `GIT_CONFIG_COUNT=1`, `GIT_CONFIG_KEY_0=core.safecrlf`, `GIT_CONFIG_VALUE_0=false`. Exited before object purge. |
| `git diff --check` | PASS, including the appended report; JSON LF checked directly. |

## Camera-motion alignment

2026-10-20, against `358b6fbadf8` plus the existing uncommitted tuning round.
Preserved its source-end/receiver biases, live controls, visibility debug view
and full-width card layout. No commit, push, viewer build/launch or stash
operation. The alignment changes affect only the enabled gather/filter shaders;
no setting change requires a shader reload and no temporal state was added.

### Causes and fixes

1. **Depth columns rather than surface crossings.** The committed gather at
   `indra/newview/app_settings/shaders/class3/deferred/areaShadowF.glsl:37`
   accepted every `rayDepth > sceneDepth + thickness`, with no upper bound or
   hit-position validation. A foreground silhouette thus represented an
   infinitely deep column along the camera direction. Rotating/translating the
   camera moved this false occluder relative to the real geometry. This same
   predicate was carried into the uncommitted shared marcher. Fixed at
   `indra/newview/app_settings/shaders/class1/deferred/screenSpaceRayUtil.glsl:66`:
   accept a finite depth candidate, extended by one march interval to avoid
   skipping a surface, refine against its sampled normal/position plane, and
   validate the surface at the **hit's** projection. Background, out-of-view,
   parallel, out-of-segment and source-excluded hits remain unsupported/lit.
   Thickness now means finite blocker tolerance, rather than the minimum
   separation behind an unbounded column. Its setting comment and generated
   tooltip describe this behavior; its default/bounds and bias scaling remain.
2. **Camera-facing source exclusion.** The tuning round's
   `abs(occluder.z-light.z) <= light_bias` rejected an entire camera-depth slab,
   including unrelated blockers far from the emitter. Its orientation changed
   with the camera. The exclusion at
   `indra/newview/app_settings/shaders/class1/deferred/screenSpaceRayUtil.glsl:65`
   now uses Euclidean source distance, also checked at the refined hit. Rigid
   camera transforms preserve that distance. The shortened endpoint and live
   `max(setting, thickness/2)` bias remain. A separate GPU probe keeps a real
   blocker about 36 cm from the emitter shadowed in both camera views even
   though their camera-depth separation is within the 25 cm bias.
3. **Half-res depth/position mismatch and upsample phase.** At committed
   `areaShadowF.glsl:43`/`:47`, point-sampled full-resolution depth was
   reconstructed at the nominal half-resolution UV. For even dimensions that
   UV lies between full-resolution texel centres; for odd dimensions the phase
   varies. Committed `areaShadowFilterF.glsl:38` then interpolated using nominal
   low-resolution centres. Fixed gather entry at
   `indra/newview/app_settings/shaders/class3/deferred/areaShadowF.glsl:27`, shared
   texel-centre reconstruction at
   `indra/newview/app_settings/shaders/class1/deferred/screenSpaceRayUtil.glsl:11`,
   and upsample neighbours/weights at
   `indra/newview/app_settings/shaders/class3/deferred/areaShadowFilterF.glsl:45`.
   Gather, march and bilateral guides use the actual fetched full-res texel;
   upsample weights use those same representative positions. Exact boundary
   ties have a fixed <0.001-full-res-pixel roundoff allowance. Blur offsets
   remain integer source-texel steps. No window dimensions or temporal offsets
   enter these calculations.

### Requested audit (current worktree file:line)

| Investigation | Evidence / result |
|---|---|
| 1. Current frame matrices, G-buffer and eye-space lights | Camera projection/view publication: `indra/newview/llviewercamera.cpp:391` and `:421`. Area selection stores IDs, then reads `get_current_modelview()` immediately before dispatch at `indra/newview/pipeline.cpp:26808`; light centres use that view at `:26822`. `get_current_modelview()` reads `gGLModelView`, not history (`indra/llrender/llrender.cpp:2292`). The bind reads current active-pack normal attachment 2 and depth at `pipeline.cpp:24961` and `:24975`. Draw-time `LLVertexBuffer::drawArrays()` calls `gGL.syncMatrices()` (`indra/llrender/llvertexbuffer.cpp:956`); `indra/llrender/llrender.cpp:1086` uploads projection and its inverse from the live projection stack. No stale/last-frame matrix or cached selection-time light transform was found in this pass. |
| 2. Same-frame pass ordering | Main G-buffer geometry at `indra/newview/llviewerdisplay.cpp:1107`, ghost opaque/masked geometry immediately afterward, target flush at `:1143`, then deferred lighting at `:1147`. Area dispatch at `indra/newview/pipeline.cpp:27531` precedes local light collection/draws (`:27585` onward), including Actor Mask consumers. It follows the ambient/sun soften draw, which does not consume local-light area visibility. No ordering change was needed. |
| 3. Half-res, resize/divisor, viewport/scissor | Fixed UV/upsample phase as above. Dimensions already come from `mRT->deferredScreen`, with ceil-half allocation and size checks every dispatch (`indra/newview/pipeline.cpp:26762`, `:26777`); divisor scaling is applied during screen-buffer allocation (`:1350`). `indra/llrender/llrendertarget.cpp:608` binds viewport `(0,0,mResX,mResY)`. The area guard disables scissor at `pipeline.cpp:26736`, restores it and the viewport, and balances target bind/flush. Thus UI/world-view offsets do not enter offscreen UVs. No target-size or viewport bug was found. |
| 4. Projection jitter / velocity / bridge | `indra/newview/llviewercamera.cpp:380` documents reverted SMAA T2x jitter; the velocity snapshot currently equals the scene projection. Area shaders use live projection/inverse projection, not the unjittered velocity uniform. Velocity runs later (`indra/newview/pipeline.cpp:28230`), before history publication at `:28241`. ReShade gathering occurs after finalization (`indra/newview/llviewerdisplay.cpp:1834`); `indra/newview/llreshadebridge.cpp:349` publishes camera matrices and does not perturb the scene projection. No FSR scene-projection jitter path was found. The new fixture includes asymmetric sub-pixel projection offsets anyway. |
| 5. Selection / channel identity | Comparator is unshadowed-first, contribution, UUID (`indra/newview/alareashadowmath.h:30`); it has no previous-frame state or hysteresis. Camera movement can legitimately change rank/membership. Every gather stores this frame's channel UUID (`indra/newview/pipeline.cpp:26816`), and each light draw resolves its own UUID via `deferredLightShape()` (`:26575`), including box, batch, projector and Actor Mask paths. Validity resets at entry (`:26589`, `:26890`) and is published only after all planes/upsamples (`:26871`). No channel/draw mismatch was found; existing UUID/channel permutation readbacks still pass. |
| 6. Reconstruction precision / depth convention | `indra/newview/app_settings/shaders/class1/deferred/deferredUtil.glsl:662` reconstructs NDC depth as `2*depth-1`, multiplies by `inv_proj` and divides by W (`:656`). Scene projection uses `glm::perspective` (`indra/newview/llviewercamera.cpp:378`); depth is conventional OpenGL 24-bit (`indra/llrender/llrendertarget.cpp:144`, `indra/llrender/llglstates.h:39`), not reversed Z. No independent near/far approximation was added. The real precision/mapping defect was reconstructing at a different texel position, fixed above. |

### Regression and validation

`scripts/code_tools/relight_area_check.py:201` now constructs two distinct
camera views of the same wall, blocker and light, with each frame's own depth,
encoded view normals, light transform and asymmetric projection. It executes
the production gather/filter programs without relinking between frames.
At 64x48, odd 63x47, and reduced 32x24, assertions compare both full/half
visibility edges against an independent analytic ray/plane oracle evaluated
against **that frame's point-sampled G-buffer**, and compare the half-res
upsample to a full-density reference: error **<=1 full-resolution pixel**.
The reference uses the same receiver bias to isolate the upsample error.
Reusing the preceding frame's image fails the alignment criterion; stale
projection/light uniforms produce a different image. Repeated current-frame
readbacks remain byte-identical. A separate production probe checks full-res
texel-centre placement, including odd sizes.

The old housing fixture included a false column hit: its front face did not
intersect the grazing ray, so zero bias must now also leave that pool lit.
The separate-blocker positive control moves the source forward so the visible
slab front really crosses the receiver-to-source segment. This corrects the
fixture geometry while retaining the Medium settings, live bias on/off/on,
half/full views and lit-pool/opaque-blocker assertions. The shallow foreground
slab now explicitly rejects a non-crossing column. These changes supersede
the earlier report's camera-depth-slab and bias-zero-darkening expectations.

Runtime mutation controls in `.tmp/relight_camera_negative.py` separately
restore an unbounded column, the camera-depth source slab, and unsnapped UVs.
All three fail the corresponding GPU assertions; no tracked source or stash
is changed by these negative runs.

| Gate | Result / evidence |
|---|---|
| `relight_area_check.py` | PASS: camera-motion, texel centres, <=1 px geometry/upsample, lag/stale controls, source-local exclusion, Medium lamp/blocker, channel/debug/filter and determinism. `.tmp/relight_camera_area.log`. |
| `relight_shader_check.py` | PASS 31 production GLSL units/permutations, 44 production links, live runtime controls, eight-channel lighting consumers, 126 rig rows with zero failures, and all area/motion probes. `.tmp/relight_camera_shaders.log`. |
| Tests | PASS 13/13 `alrelightmath` tests using the existing standalone runner; no test source changed in this alignment round. `.tmp/relight_camera_math.log`. |
| OFF / runtime identity | PASS eight disabled source-expression streams and no reload/temporal dependencies (`.tmp/relight_camera_off.log`); 1,890 OFF and 1,890 Phase 1 ON float32 readbacks byte-identical to `358b6fbadf8` with area shadows disabled (`.tmp/relight_camera_identity.log`). |
| Separate `zs_check -Files <one file>` calls | PASS `pipeline.cpp` and `tests/alrelightmath_test.cpp`, covering both pre-existing tuning C++ modifications, one call per file. `.tmp/relight_camera_zs_pipeline.log`, `.tmp/relight_camera_zs_test.log`. Alignment needed no additional C++ edits. |
| Generated content / prebuild | PASS all 11 gates, exit 0. Regenerated all three Director outputs after the tooltip correction. Installed `prebuild_guard.ps1 -ChecksOnly` ran via the existing workspace cache wrapper with `GIT_CONFIG_COUNT=1`, `GIT_CONFIG_KEY_0=core.safecrlf`, `GIT_CONFIG_VALUE_0=false`; `.tmp/relight_camera_prebuild.log`. ChecksOnly exited before object purge. |
| Whitespace | PASS `git diff --check` after the final report edit; source JSON and generated JSON remain LF. `.tmp/relight_camera_diff_check.log`. |

These are controlled GPU fixtures and a source/order audit, not an in-world
alley capture. The screen-space offscreen/hidden-surface limitation remains.
Candidate refinement adds two normal reads and one depth read for candidates
that reach validation; viewer GPU time and the user's moving-camera scene
still need in-world measurement after a separately authorized build.


## Bias decoupling and contact hardening

2026-10-20, on `358b6fbadf8` plus the uncommitted tuning and camera-alignment
work. This section supersedes the earlier thickness-scaled light-bias policy
and preset counts. No commit, push, stash, viewer build or launch.

`ALAreaShadow::lightBias` now accepts only the housing bias. It sanitizes that
setting independently to 0–4 m (nonfinite fallback 0.25 m); Thickness cannot
enter the calculation. Defaults remain **0.25 m housing bias / 0.35 m blocker
thickness**. The shortened ray endpoint still includes the source allowance,
and the source-local 3D exclusion remains intact. Thickness still controls
finite blocker-depth acceptance. The setting comment and generated Light end
bias help now describe independent controls.

The gather uses the existing Phase 1 `area_source_radius(volume)` upload:
rig fixture `mSourceSizeM * 0.5` through `lightShapeSourceFor`, otherwise
`RenderLocalLightDefaultRadius` (including legacy world lights and rig
catchlights without fixture size). Positive radii are preserved exactly.
A **zero radius uses a 0.10 m shadow-only disk radius** (0.20 m diameter), so
area-shadow mode can produce contact hardening for default point-sized world
lights. This fallback does not change Phase 1 illumination range, attenuation,
source metadata or specular size. It is independent of housing bias and
blocker thickness; explicit small positive sources remain small.

Equal-area radial strata, fixed golden-angle azimuths and the existing
pixel-plus-light-UUID seed provide repeatable current-frame samples. There is
no frame seed, history, reprojection or temporal blending. Increasing rays
rather than blur supplies the additional visibility directions. Presets and
Medium persisted-setting defaults now use:

| Preset | Samples S | Steps per ray | Resolution | Maximum nominal march taps per light/trace pixel |
|---|---:|---:|---|---:|
| Low | 8 | 16 | Half | 128 |
| Medium | 16 | 24 | Half | 384 |
| High | 24 | 32 | Half | 768 |
| Cinematic | 32 | 48 | Full | 1,536 |

Custom bounds remain 1–32 samples and 1–64 steps. Existing saved values remain
live; selecting a quality preset applies its new counts. These budgets cost
more than the previous presets; viewer GPU timing has not been measured.

The production GPU harness now sweeps Thickness 0.001/0.1/0.35/1/4 m at fixed
housing bias, requiring identical housed-lamp pool pixels at full and half
resolution. At default thickness, Medium's full-resolution pool minimum is
**0.937**, while the separate real blocker's minimum is **0.000**. No visibility
floor was added. Bias on/off/on edits use the same linked executable; with the
new disk, some directions really cross the housing when bias is zero. The
point-ray shallow-surface and moving-camera geometry diagnostics use an
explicit tiny positive radius to isolate reconstruction from the disk fallback.

An additional **unfiltered production gather** fixture compares a half-plane
blocker near the receiver against one separated from it. All presets produce
more partially visible pixels with separation: Low **6/698**, Medium
**36/727**, High **46/749**, Cinematic **222/3,198** (contact/separated).
It also requires larger explicit source radii to widen the penumbra, zero
radius to match an explicit 0.10 m disk byte-for-byte, and repeated draws to
match exactly. These counts demonstrate contact hardening in controlled GPU
fixtures, not an in-world visual or performance measurement.

| Gate | Result / evidence |
|---|---|
| `relight_shader_check.py` | PASS: 31 production GLSL units/permutations, 44 production links, runtime lighting/uniform checks, 126 rig rows with zero failures, area and contact-hardening probes. `.tmp/relight_small_shaders.log`. |
| `relight_area_check.py` | PASS: thickness-independent housed lamp, separate blocker, full/half, all four quality presets, zero-radius fallback, live radius/bias, deterministic gather/filter and moving-camera regressions at 64x48, odd 63x47 and 32x24 (<=1 px alignment). `.tmp/relight_small_area.log`. |
| Tests | PASS 13/13 `alrelightmath`, updated presets and independent bias sanitization. Standalone test compile/link using existing libraries; `.tmp/relight_small_math{_compile,_link,}.log`. |
| OFF / Phase 1 identity | PASS eight disabled expression streams and runtime/no-temporal source checks (`.tmp/relight_small_off.log`); 1,890 OFF and 1,890 Phase 1 ON float32 component readbacks byte-identical to HEAD with area shadows disabled (`.tmp/relight_small_identity.log`, `.tmp/relight_small_identity_gpu.log`). Whole viewer-frame equality was not measured. |
| `zs_check`, one TU per call | PASS `pipeline.cpp` and `tests/alrelightmath_test.cpp`, covering the changed math header; `.tmp/relight_small_zs_pipeline.log`, `.tmp/relight_small_zs_test.log`. |
| Generated outputs | Regenerated kit, page sources and functional manifest after C++/tooltip edits; all three `--check` gates PASS. `.tmp/relight_small_{kit,sources,manifest}{,_check}.log`. Five existing manifest warnings remain. |
| `prebuild_guard.ps1 -ChecksOnly` | PASS all 11 gates, exit 0, via the existing workspace cache wrapper with `GIT_CONFIG_COUNT=1`, `GIT_CONFIG_KEY_0=core.safecrlf`, `GIT_CONFIG_VALUE_0=false`; `.tmp/relight_small_prebuild.log`. ChecksOnly exited before object purge. |
| `git diff --check` | PASS after the final report append; `.tmp/relight_small_diff_check.log`. |

The screen-space hidden/offscreen geometry limitation remains. The existing
camera transform, texel-centre reconstruction, spatial filtering and UUID
channel selection fixes are preserved. All controls remain runtime uniforms;
no reload listener or shader permutation was added.

## Workload budget

2026-10-20. Fixes the P0 in `.tmp/review_codex_relight_tune.md` and supersedes
all earlier preset/cost tables. No commit, push, stash, viewer build or launch.
`.tmp/ref_private/` was not opened.

`RenderAreaShadowBudget` is a live S32 setting in **millions of march
iterations/frame**, default **1500**, sanitized to **1–1500**. It is under
Soft shadows > More options (`area_shadow_budget`, enabled by the master).
The setting is a lowerable hard safety ceiling, not an uncapped quality knob.
Each gather draw is limited to **half the frame budget**, default **750M**.
The exact admission calculation is `ceil(width/2)*ceil(height/2)*K*S*steps`
for half resolution or `width*height*K*S*steps` for full resolution, using
64-bit integer arithmetic and the actual selected K and deferred target size.
It also requires one channel to fit the draw cap. The pure planner decrements
S to 1 first, then steps to 1, then forces half resolution if necessary; if the
minimum cannot fit, the view gets no area visibility. Zero-sized/empty or
pathologically oversized inputs dispatch nothing. It does not change persisted
quality settings. Cinematic can retain full resolution only when its effective
sample/step counts fit both ceilings. No frame time or query result enters the
plan, and no setting triggers shader reloads.

Requested presets are now **Low 4/8/half**, **Medium 8/12/half**,
**High 12/20/half**, **Cinematic 24/32/full**. Low and Medium have exactly
2x their pre-tuning nominal march work (2/8 and 4/12 respectively); High is
1.5x the former 8/20. Custom remains 1–32 rays / 1–64 steps and uses the same
budget. Persisted older high counts are immediately constrained by admission.
Medium setting defaults and the Director reset value are updated consistently.

The 1500M ceiling removes the reviewed 101,921M 4K/K=8 worst case: the largest
admitted work in this table is 1493M/frame and 746.5M/draw. Default Medium at
4K/K=8 is 1393.5M/frame, 696.7M/draw. The default is therefore below the
requested 1500M/750M limits even at 4K, while Low stays cheap and Cinematic
alone requests full resolution. This is a conservative work bound, **not a
measured GPU-time or TDR guarantee**: each validated crossing can additionally
read normals/depth, and reconstruction/filtering have separate costs. The
budget can be lowered for slower hardware; viewer GPU timings remain unmeasured.

RGBA planes retain their original channel identities. Each plane's channels
are split into contiguous batches of `min(4, floor(drawCap / perChannelWork))`.
`area_first`/`area_count` bound the production shader loop; colour write masks
preserve other channels across draws. All selected channels are written exactly
once before denoise/upsample. Partial planes and UUID seeding stay intact.
`[RelightQ] area-shadow budget clamp ...` logs effective work, requested/effective
quality, dimensions, K and channels/draw once when the input/plan changes,
including release of a previous clamp. The periodic summary reports effective
S/steps/resolution. Existing asynchronous timing remains diagnostic only.

Worst-case admitted march iterations with the default budget, **millions**;
no early-outs assumed. H = half width/height; F = full. Draw is the largest
actual channel batch (the final partial batch can be smaller).

| Resolution | K | Preset | Effective S/steps/res | Channels/draw | Frame M | Largest draw M |
|---|---:|---|---|---:|---:|---:|
| 1080p | 4 | Low | 4/8/H | 4 | 66.4 | 66.4 |
| 1080p | 4 | Medium | 8/12/H | 4 | 199.1 | 199.1 |
| 1080p | 4 | High | 12/20/H | 4 | 497.7 | 497.7 |
| 1080p | 4 | Cinematic | 5/32/F | 2 | 1,327.1 | 663.6 |
| 1080p | 8 | Low | 4/8/H | 4 | 132.7 | 66.4 |
| 1080p | 8 | Medium | 8/12/H | 4 | 398.1 | 199.1 |
| 1080p | 8 | High | 12/20/H | 4 | 995.3 | 497.7 |
| 1080p | 8 | Cinematic | 2/32/F | 4 | 1,061.7 | 530.8 |
| 1440p | 4 | Low | 4/8/H | 4 | 118.0 | 118.0 |
| 1440p | 4 | Medium | 8/12/H | 4 | 353.9 | 353.9 |
| 1440p | 4 | High | 12/20/H | 3 | 884.7 | 663.6 |
| 1440p | 4 | Cinematic | 3/32/F | 2 | 1,415.6 | 707.8 |
| 1440p | 8 | Low | 4/8/H | 4 | 235.9 | 118.0 |
| 1440p | 8 | Medium | 8/12/H | 4 | 707.8 | 353.9 |
| 1440p | 8 | High | 10/20/H | 4 | 1,474.6 | 737.3 |
| 1440p | 8 | Cinematic | 1/32/F | 4 | 943.7 | 471.9 |
| 4K | 4 | Low | 4/8/H | 4 | 265.4 | 265.4 |
| 4K | 4 | Medium | 8/12/H | 3 | 796.3 | 597.2 |
| 4K | 4 | High | 9/20/H | 2 | 1,493.0 | 746.5 |
| 4K | 4 | Cinematic | 1/32/F | 2 | 1,061.7 | 530.8 |
| 4K | 8 | Low | 4/8/H | 4 | 530.8 | 265.4 |
| 4K | 8 | Medium | 7/12/H | 4 | 1,393.5 | 696.7 |
| 4K | 8 | High | 4/20/H | 4 | 1,327.1 | 663.6 |
| 4K | 8 | Cinematic | 1/22/F | 4 | 1,459.8 | 729.9 |

Contact hardening is preserved: the source-radius disk and 0.10 m zero-radius
fallback, equal-area strata, UUID/pixel seed, independent housing bias,
finite crossing refinement and camera/texel alignment are unchanged. The raw
GPU fixture with retuned presets still widens the penumbra with separation
(contact/separated partial pixels: Low 11/453, Medium 32/632, High 12/738,
Cinematic 245/3088) and larger source radius. Repeated readbacks and zero-radius
fallback equivalence pass. At very tight budgets a single admitted ray cannot
provide a smooth multi-ray penumbra. Medium's housed-lamp minimum is 0.875;
the harness now permits one of its eight rays to hit housing (plus RGBA8
rounding), retaining the positive separate-blocker minimum 0.000, live bias
on/off/on and thickness-invariance assertions at full and half resolution.

Validation for this fix:

| Gate | Result / evidence |
|---|---|
| `relight_shader_check.py` | PASS, production compilation/links, runtime lighting, 126 rig rows with zero failures and all area probes. `.tmp/relight_budget_shaders.log`. |
| `relight_area_check.py` | PASS, split 1/2/3/4-channel production draws byte-identical to unsplit output (including housed lamp), contact hardening, thickness/bias, filtering and moving-camera regressions at 64x48, odd 63x47 and 32x24 with <=1px alignment. `.tmp/relight_budget_area.log`. |
| Pure tests | PASS 14/14, standalone compile/link with existing libraries. Every preset at 1080p/1440p/4K, K=4/8, budgets 1/100/1500/9999: deterministic plan, exact total, hard ceiling, every split draw bounded and every channel dispatched once, or fail closed. Explicit degradation-order, necessary-split, default-4K, empty/impossible/oversized probes. `.tmp/relight_budget_math{_compile,_link,}.log`. |
| `zs_check`, separate calls | PASS `pipeline.cpp` and `tests/alrelightmath_test.cpp`, covering the pure header. `.tmp/relight_budget_zs_pipeline.log`, `.tmp/relight_budget_zs_test.log`. |
| OFF/runtime source preservation | PASS existing identity and no-reload/no-temporal checks, including the new setting. `.tmp/relight_budget_off.log`. |
| Director outputs | Regenerated kit, page sources, functional manifest after C++ changes; all three check modes PASS, row pin 2157, five existing manifest warnings. `.tmp/relight_budget_{kit,sources,manifest}*.log`. |
| `prebuild_guard.ps1 -ChecksOnly` | PASS all 11 gates, exit 0; installed guard unchanged via the existing workspace-cache wrapper, with GIT_CONFIG_COUNT=1, GIT_CONFIG_KEY_0=core.safecrlf, GIT_CONFIG_VALUE_0=false. `.tmp/relight_budget_prebuild.log`. Exited before object purge. The first run caught generated headers becoming stale after the generator row-pin comment edit; all three outputs were regenerated and the final complete run passed. |
| `git diff --check` | PASS after the final report append. `.tmp/relight_budget_diff_check.log`. Source/generated JSON retain LF. |

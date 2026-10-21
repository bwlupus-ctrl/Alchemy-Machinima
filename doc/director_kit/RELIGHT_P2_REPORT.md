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

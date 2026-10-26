# Slipstream weather port plan

## 1. Shipping user workflow

Everything ships in **Director v2 > World > Weather**. Weather starts off; **Existing rain** remains the default. Existing tuning, saved looks and shelter retain their behavior. Choose Weather library, enable Local weather and apply rain/snow presets for physical rates, wind and transitions. Persistent wet surfaces, puddles, snowfall and precipitation visibility have separate controls. Stop precipitation allows wetness to dry; Clear surfaces clears wetness. Reset All restores built-in defaults and turns new expensive gates off.

**Advanced shelter** starts off. Near/Far detail, Near/Far coverage, Follow weather wind, Update rate, Bias, Softness, Show shelter and Update / coverage status are searchable. It improves windward doorways, overhangs and roof edges for existing/library rain, wet surfaces/puddles, snowfall and veil. Missing/failed data remains unknown. Main and VCam/Prism share world coverage; remote views cannot manufacture coverage. Blended glass has no reliable weather-blocking semantic and is excluded.

## 2. Ownership and compatibility

Baseline defaults and mode-0 images are pinned to `330d012e1612a3f4779a01dcc0614a91939ead0f`. WP7 starts at immutable `80abb6c19d6b0e601a56ebf28ca05d6e6058c74e`. Work stays on wip/weather-port, uncommitted for the lead. No viewer build, push, HEAD movement or other worktree edits. Excluded private/reference and external shader trees must never be opened.

The bridge freezes CPU weather once in idle; every view shares its time/seed/mean wind. Material weather runs after authored/Actor FX decoding and before native lighting/packing, preserving PBR/legacy conventions and exclusions. Default rain retains its original shader/clock. Auxiliary views explicitly consume their own view/depth/target and frozen weather.

The weather owner keeps WP2 basic cover and WP7 dedicated pairs. A shared native geometry traversal preserves authored alpha cutoff and never borrows sun/spot/probe targets. Origin, place, frame and transforms publish together. Unknown surfaces suppress new wet deposition. Every success/failure/auxiliary exit restores framebuffer, viewport, draw buffers, blend/depth/cull, samplers and matrices. glDrawBuffers exposes only written attachments. No attached depth texture is sampled; no production GPU readback/fence wait is added.

## 3. Shippable phases, settings, Director rows and budgets

Every existing default is retained. New expensive gates start off. Old looks reset newly absent gates off. Samplers append after existing slots. Resources/programs prepare before views and persist for the session across effect toggles; GL teardown releases them. Capability queries cache per context generation and attribute locations cache at preparation. Off paths allocate/do no GL work, including library-on/WP7-off.

| WP | Feature | Extra GPU p95 at 3840×2160 | Additional GPU memory | Default |
|---|---|---:|---:|---|
| WP1 | CPU rain core and Director | 0.05 ms | 4 MiB | Mode 0 |
| WP2 | Wet materials/puddles and basic cover | 0.60 ms | 4 MiB | Off |
| WP3 | Snowfall and precipitation veil | 0.65 ms | 16 MiB | Off |
| WP7 | Near/far tilted shelter | 0.60 ms above basic cover | 16 MiB | Off |

Budgets are enforced acceptance ceilings. Log GPU/driver/context, measure preparation, whole cull/draw, dirty/moving-roof/angular updates and consumers. Report a second view separately. Count shared allocations once and both read/write sides. Standalone fixtures cannot certify complete-scene FPS or native integration; the lead builds once after review and the user performs in-world acceptance.

### WP1 — CPU weather core, presets, and Director (M)

**Deliverable:** usable physical rain rates, smooth changes, weather wind and explicit weather-timed lightning, through the current rain renderer. Snow is supplied by WP3. The imported CPU carries unused fields without UI/render consumers in this release.

**Files:** copy `slweather.{cpp,h}` with headers; add `alweatherbridge`, initial `alweatherclock`, revisioned `alweatheruimodel`; register in `indra/newview/CMakeLists.txt`; idle hookup in `llappviewer.cpp`, successful-teleport events in `llviewermessage.cpp`; rain/strike uniform adapter in `pipeline.cpp`; narrowly extend `alweathermodel` to accept an externally scheduled event without altering its existing scheduler. Add settings, `ALPanelWeatherSettings` commands/preset schema, `llfloaterdirector` bindings, generator input, generated page, ownership docs, and `doc/contributions.txt` credit in the implementation. Tests: new `tests/slweather_test.cpp`, `alweatherbridge_test.cpp`, `alweatherclock_test.cpp`, `alweatheruimodel_test.cpp` wired like `alweathermodel_test.cpp`, plus standalone production-source CPU tests and `scripts/code_tools/weather_rain_gl_check.py`. WP1 includes the gated, view-safe library rain/lightning hook in `llprismlens.cpp` after auxiliary deferred lighting (near 11233), because that path never calls main `renderFinalize` (near 24599). Auxiliary draws consume the frozen frame, their own camera/depth/target, and restore state; no shared main rain scratch/cover/shelter history or scheduler mutation. Real GL proves mode-0 byte identity against 330d012e161 and library rain in main and an auxiliary fixture, with mutation controls.

| Proposed setting | Type; default; range | Director row |
|---|---|---|
| `AlchemyWeatherControlMode` | U32; 0; 0 Existing / 1 library | Weather controls |
| `AlchemyWeatherLocalEnabled` | Boolean; false | Local weather |
| `AlchemyWeatherRainRate` | F32; 5; 0–200 mm/h | Rain rate |
| `AlchemyWeatherPrecipVariation` | F32; 0.35; 0–1 | Rain bands |
| `AlchemyWeatherWindSource` | U32; 0; Simulator / Weather | Wind source |
| `AlchemyWeatherWindDirection`, `AlchemyWeatherWindSpeed` | F32; 225 degrees / 3 m/s; 0–360 / 0–40 | From direction; Speed |
| `AlchemyWeatherWindVariation`, `AlchemyWeatherWindGustiness`, `AlchemyWeatherWindDirVariation` | F32; 0.3 / 0.5 / 0; 0–1 / 0–1 / 0–180 degrees | Speed variation; Gustiness; Direction swing |
| `AlchemyWeatherTransitionSeconds` | F32; 20; 0–600 s | Transition |
| `AlchemyWeatherSeed` | U32; 1597463007; full U32 | Weather seed |
| `AlchemyWeatherStorm` | F32; 0; 0–1 | Storm amount |
| `AlchemyWeatherLightningTiming` | U32; 0; Existing / Weather storm | Lightning timing |
| Existing `AlchemyWeatherRainColor`, `AlchemyWeatherLightningColor` | Color3; retain (0.55, 0.68, 0.78) / (0.72, 0.82, 1.0) | Rain colour; Lightning colour, using existing themed colour control support |

New preset picker/Apply, source/current-rate/transition readouts, Stop precipitation, and existing Save/Load/Reset/Strike are command rows. Initially source is local/clear; no pretend parcel status. Preserve all current artistic rain rows and explain mode-dependent behavior, including legacy-only procedural gust/cloud link. Library rain uses a monotonic calibrated rate-to-density/strength bridge; physical accumulation uses the physical rate, not artistic brightness. The bridge must not persistently rewrite the user's old tuning. When mode 0 with new effects off, do not execute new shader uniforms, change permutations, or alter old clock calculations.

**WP1 scope:** Clear surfaces now is deferred to WP2, where persistent surfaces have an observable consumer. Snow rows remain deferred to WP3. Each row must change its bound setting/command and produce an observable production-consumer effect in a binding test. No inert future controls.

**CPU corrections authorized at import:** replace direct settings/time/agent dependencies with injected configuration, fixed 120 Hz time and explicit successful-place events. Correct upstream slweather.cpp:537: snap depth below 0.0001 m only while draining (never while accumulating). A 600 s production fixed-step test starts at zero and checks settled 5 cm/h reaches 0.005833… m. Every successful teleport, including short same-region teleports, resets ground patterns and applies canonical starting surfaces while retaining storm targets; override the distance exception near 709 and local-weather carryover near 745. Crossings and origin rebases preserve patterns. Test successful versus cancelled teleports, short teleports, crossings, rebases and retained target/serial semantics. WP1 canonical surfaces are zero because wet surface controls are supplied by WP2.

**Cost/risk:** ≤0.05 ms GPU and ≤0.10 ms main-thread p95 added CPU, no new full-frame pass. Biggest risks are double cloud/gust/lightning modulation, default migration, per-view double updates, direct wall-clock reads, and renderer-coupled source/reset behavior. Preserve reset/place serial meanings; expose the configuration/time seams rather than rewriting proven CPU math.

### WP2 — Persistent wet surfaces and puddles (L)

**Deliverable:** weather-off identity plus persistent outdoor wet material response, puddle masks and ripples with either rain mode. A legacy-to-CPU precipitation adapter feeds accumulation when new surfaces are enabled under Existing rain; it never changes the legacy streaks or their settings. Master off clears; Stop precipitation dries naturally.

**Files:** new material helper and shared `alweatherresources`; basic surface-cover owner in `alweathershelter`; `pipeline.{cpp,h}` map preparation/lifetime, material/terrain/opaque/masked/forward shaders from section 2, shader attachment/uniform plumbing; WP1 settings/UI/preset files; `scripts/code_tools/weather_material_gl_check.py` and fixtures. No full-screen material rewrite or extra G-buffer is introduced.

| Proposed setting | Type; default; range | Director row |
|---|---|---|
| `AlchemyWeatherPersistentWetEnabled`, `AlchemyWeatherPuddlesEnabled` | Boolean; false / false | Persistent wet surfaces; Puddles |
| `AlchemyWeatherWetAmount`, `AlchemyWeatherPuddleAmount`, `AlchemyWeatherRippleStrength` | F32; 0.5 / 0.35 / 0.25; each 0–1 | Wet amount; Puddle amount; Ripple strength |
| `AlchemyWeatherWetSeconds`, `AlchemyWeatherDrySeconds` | F32; 120 / 300; 1–3600 / 1–7200 s | Soak time; Dry time |
| `AlchemyWeatherStartWetness` | F32; 0; 0–1 | Starting wetness |
| `AlchemyWeatherSurfaceCoverResolution`, `AlchemyWeatherSurfaceCoverExtent` | U32 512 / F32 128 m; 256–1024 power-of-two / 32–256 m | Surface cover detail; Coverage |

Apply starting surfaces, Clear surfaces now and coverage/validity readouts are commands/readouts. Basic cover is required automatically *inside* the enabled new material effect, even if legacy rain occlusion is off; it does not turn on that old option or change legacy sampling. Publish explicit COMPLETE_EMPTY / COMPLETE_COVERED / FAILED / INCOMPLETE generation states: successful empty coverage is valid open sky. Native legacy validity is caster_batches > 0 (pipeline.cpp near 31407), with terrain not counted near 31316; retain that legacy predicate separately. New-map validity requires successful allocation, clear, terrain/caster traversal and coverage publication, not a nonzero caster count. Test terrain-only, no-roof and empty-map fixtures plus failed/incomplete passes. Initially vertical cover is sufficient, with a world-stable puddle mask. No unbounded cavity simulation, displaced water, guaranteed mirror reflection, or camera-ray-based indoor material mask. Suppress the old additive wet sheen and its additive wet-glint contribution only while the replacement is active; restore it unchanged otherwise. Existing splashes/mist/lens remain independent.

**Probe invalidation (WP2):** add bounded weather revisions/signatures to `llreflectionmapmanager.{cpp,h}` for on-demand refresh (near 2124) and cinematic On-change signatures (near 1094). Quantize material state, coalesce updates, cap refresh work and invalidate on reset/place/config revisions. Pure rain drawing alone does not dirty probes. Test stationary wetting/drying with on-demand and cinematic On-change.

**Cost/risk:** ≤0.60 ms and ≤4 MiB, including a double-buffered 512² D32F basic cover map (~2 MiB) plus bounded metadata. Shader ALU/samplers rather than another full-frame image. Keep roughness above the native minimum, dielectric darkening/film response bounded, metallic response conservative, ripple normals normalized/filtered. Risks: specular energy gain, patch shimmer, alpha/PBR/legacy mismatch, surface map use after rain stops, map validity in auxiliary views, and accidental modification of emission/flags. Sky/rain alone must not wet covered interiors.

### WP3 — Snowfall particles and optional visibility (M)

**Deliverable:** snow and mixed presets/rates with wind-driven, lit, depth-aware flakes. No depth blanket is required to enjoy snowfall. Physical precipitation visibility is a separately enabled bounded veil, never a hidden fog cost in a preset.

**Files:** `alweatherparticles`, `weatherSnowV/F`, `weatherSnowSim` and optional `weatherPrecipVeilF`; extend WP1 rain/lightning hooks in `pipeline` and `llprismlens.cpp` with snowfall; shader manager/CMake; settings/Director/preset sources; `scripts/code_tools/weather_particles_gl_check.py`. Prerequisite BEFORE WP3: make the production loader compute-aware in `indra/llrender/llshadermgr.{cpp,h}` and `llglslshader.{cpp,h}`. Current llshadermgr.cpp:639 emits at most #version 420, and :686 labels every non-fragment stage VERTEX. Implement capability-gated GLSL 430+ compute preamble, explicit COMPUTE stage classification, compute-only linking without graphics features/attributes, stage/version/permutation-aware shader/program cache keys and reload/failure cleanup. Require a production-loader compile/link/dispatch/readback test and unsupported-capability negative test before particle work; a harness-only #version edit cannot pass this prerequisite. Compute implementation must validate GL capability/entry points. Use a bounded instanced draw with deterministic IDs; unsupported GL gets explicit unavailable status, never a silent CPU particle storm.

| Proposed setting | Type; default; range | Director row |
|---|---|---|
| `AlchemyWeatherSnowEnabled` | Boolean; false | Falling snow |
| `AlchemyWeatherPrecipType` | U32; 0; Rain / Snow / Mixed | Precipitation |
| `AlchemyWeatherSnowRate` | F32; 1.5; 0–360 cm/h fresh snowfall | Snowfall rate |
| `AlchemyWeatherSnowFlakeSize`, `AlchemyWeatherSnowFallSpeed`, `AlchemyWeatherSnowRange` | F32; 0.015 m / 1 m/s / 64 m; 0.002–0.08 / 0.2–4 / 16–128 | Flake size; Fall speed; Draw distance |
| `AlchemyWeatherSnowParticleLimit` | U32; 32768; 4096–131072 | Flake limit |
| `AlchemyWeatherPrecipVisibilityEnabled`, `AlchemyWeatherPrecipVisibilityAmount` | Boolean false / F32 1; 0–1 | Precipitation visibility; Visibility amount |

Preset application does not enable ground snow/relief or the visibility veil. Shared cell hashes/time create repeatable flakes with camera culling; avoid screen-fixed sparkle and reseeding on pans. Track separate snow/rain components without adding snow to the legacy rain intensity. Apply extinction once, with open-sky cover and correct depth, without double counting existing mist.

**Cost/risk:** ≤0.65 ms combined simulation/draw/optional veil, ≤16 MiB at default cap, no default full-resolution history image. Dense high-cap stress cases get separate budgets/status, not assumed included. Risks: translucent overdraw, tiny flake aliasing, depth halos, crossing cell boundaries, wind changes, mixed-weather normalization, and auxiliary cameras seeing an empty particle volume.

### WP7 — Near/far tilted shelter maps (M)

**Deliverable:** better windward cover and doorway behavior for existing/library rain, wet surfaces/puddles, snowfall and precipitation veil without replacing the current shelter choice by default.

**Files:** expand `alweathershelter`, `alweatherresources`, `pipeline` depth/cull/map helpers and publication; GLSL exposure helper; native material/particle/rain consumers; shader manager/UI; `scripts/code_tools/weather_shelter_gl_check.py`. New map owner must not commandeer sun/spot/probe shadow targets.

| Proposed setting | Type; default; range | Director row |
|---|---|---|
| `AlchemyWeatherAdvancedShelterEnabled` | Boolean; false | Advanced shelter |
| `AlchemyWeatherShelterNearResolution`, `AlchemyWeatherShelterFarResolution` | U32; 1024 / 512; power-of-two 256–2048 / 256–1024 | Near detail; Far detail |
| `AlchemyWeatherShelterNearExtent`, `AlchemyWeatherShelterFarExtent` | F32; 32 / 128 m; 16–128 / 64–512 | Near coverage; Far coverage |
| `AlchemyWeatherShelterTiltEnabled` | Boolean; true under advanced gate | Follow weather wind |
| `AlchemyWeatherShelterUpdateHz` | U32; 30; 5–60 | Update rate |
| `AlchemyWeatherShelterBias`, `AlchemyWeatherShelterSoftness` | F32; 0.12 / 0.35 m; 0–1 / 0.01–2 | Bias; Softness |
| `AlchemyWeatherShelterDebug` | Boolean; false | Show shelter |

Maps follow slow mean wind with bounded fall-angle shear, never individual gusts. World-space snapped precipitation cells and a 70–95% near-border blend preserve world anchoring. WP2's existing basic map remains the untilted height source, reused when valid; tilted depth is never interpreted as ground height. New maps belong exclusively to weather and use native unrigged opaque/alpha-mask traversal with authored cutoff rules. Blended glass and worn/avatar shelter are excluded: blended roofs may admit precipitation. The existing camera fallback remains available for legacy rain/lens when advanced data fails.

Idle prepares the resources and advanced rain program. Draws never compile or allocate WP7 resources. Dedicated D32F near/far read/write pairs publish after both traversals and a framebuffer/texture-fetch barrier. Place, origin, frame, each matrix and depth range publish together. Failed allocation, unsupported requests, incomplete traversal or non-finite camera data remain explicitly unknown/fallback. Allocation/program failures retry after a detail/context change; transient traversal failures recover at the bounded update cadence without an allocation/compile retry loop. Static-object geometry revisions coalesce at the selected rate, with at most two new traversals per update. No audio, snow-depth, runoff, avatar or listener-ring consumer ships.

Both sides count toward the 16 MiB cap. An over-budget requested detail combination reports unknown coverage and asks for reduced detail; it never silently changes quality. Default map storage is 10 MiB; the enforced allocation budget includes a 256 KiB program/FBO bookkeeping reserve (10.25 MiB above the shared basic map). Main, VCam and Prism use the same world publication. Remote views outside coverage remain unknown and need the main camera to prepare their area; a view never overwrites another view's matrices. Probe materials consume the frozen publication, without map preparation.

**Cost/risk:** ≤0.60 ms extra p95 over basic cover, ≤16 MiB added. Measure full scene cull/draw/dirty costs and angular refresh spikes, including a moving roof. Risks: snapping/swimming, tilted/vertical coordinate mismatch, stale movers, bias leaks, far-camera coverage, invalid publication, render-state contamination, and GPU readback stalls. Map failures retain explicit fallback/unknown status, not a false valid stamp.

## 4. Validation and deliberate failures

Real-GL fixtures execute production CPU/resources/scheduling/shader/publication code from the selected immutable ref or WORKTREE. Identify geometry/viewer services as fixtures. Extract production declarations and fail if missing; do not manufacture missing symbols. Forward --source-ref to mutation subprocesses and use explicit timeouts. Mutations use disposable directories and must fail named behavior assertions, not just compilation. New behavior must fail at pre-WP7 HEAD.

### Phase-specific tests and deliberate failures

| WP | Production tests and assertions | Runnable mutations which must fail |
|---|---|---|
| WP1 | Existing `alweathermodel_test.cpp` unchanged cases plus pure `SLWeather` config/time bridge: amount↔rate endpoints/monotonicity, finite clamps/invalid config, local/none/master source rules, exponential 95%-settled transition semantics, vector wind wrap, deterministic bands/strike slots, reset/place serials, no source snow/runoff side effects when their consumers are off. GL execute old rain chain with new bridge off and library uniform uploads from production helper; light/medium/heavy rate response monotonic under fixed art settings, old presets byte-identical. Test two-scheduler exclusion and one event per strike. | Enable library adapter in default mode; double cloud/gust modulation; read wall time per view; leave both lightning schedulers active; remove master clear; drop the actual uniform upload/call site. |
| WP2 | Rasterize roof, sloped roof, terrain, vertical wall, masked canopy and doorway; inspect actual exposure/material output and PBR/legacy lighting. Test rain-start soak, rain-stop dry, puddle coverage stability, ripple range filtering, post-toggle exact restoration, emissive/HUD/water exclusion, forward alpha consistency, probe material response and stationary exposure under camera motion. Baseline native sheen remains exact when new gates off. | Replace roughness with 0; add water to emission; apply wetness in sRGB; use camera shelter for all surfaces; force map valid with wrong matrix; omit legacy gloss conversion; leave additive wet sheen active; disconnect helper from indexed/terrain variant. |
| WP3 | Real depth occluders/soft edges with fixed flake IDs: silhouettes occlude, rates/caps stay bounded, wind follows weather clock, no snow-to-rain leakage. Compile all instanced/compute/mixed/Prism permutations, test odd and 4K framebuffer sizes, camera cell crossing, unsupported-capability status and zero alloc while off. Veil extinction is monotonic, ≥0, physically scaled and applied once only to the relevant open depth. | Disable depth test; seed from view/frame count; omit a compute→draw barrier; exceed cap by one workgroup; clip flakes to main camera for a remote VCam; apply veil twice; let Snow preset enable relief. |

| WP7 | GPU-rendered near/far vertical and tilted maps, native masked roofs and moving blockers. Known directional rain reaches windward walls and is blocked leeward; near/far seams and mean-wind changes stay continuous; snow retains vertical height and uses tilted precipitation reach. Verify map generation/matrix/coverage stamps, origin rebase and allocation failure; main/probe/Prism/VCam state-save/restore sentinel draws after every early exit. No listener ring or synchronous readback is allowed by shipping scope. | Reverse shear sign; read tilted depth as vertical ground; use last view's matrix; stamp valid after failed allocation; remove one publish barrier; publish map and matrix from different generations; leak GL_BLEND or draw-buffer state; introduce a current-frame listener fence wait (forbidden despite no audio consumer). |

Also check bias leakage, far-camera coverage, no swimming on camera motion/origin rebase, native alpha cutoff, default-off/no-work and all Director rows with observable production effects. Full-pipeline A/B at 4K enforces <=0.60 ms p95 above basic cover and <=16 MiB additional memory. State sentinels cover main/probe/Prism/VCam and early exits. Audio is excluded; adding a synchronous listener wait must fail.

Required gates: weather_shelter_gl_check.py real GL/mutations, all WP1–WP3 weather tests/review regressions, kit generators --check, xui_layout_check --changed 330d012e161 (0), kit_lint --all (0 errors), ui_draw_lint --changed 330d012e161 (0 new), git diff --check. Never weaken a gate. zs_check cannot run in this sandbox: list touched .cpp files for the lead. No viewer build here.

## Lead decisions (10/25)

User scope 10/25: WP1-WP3 + WP7 only; WP4/5/6/8/9 dropped

This supersedes the broader roadmap. No audio, snow-depth/relief/tracks, parcel/take, runoff or avatar UI/render consumers are scheduled. Imported unused CPU fields remain without shipping consumers. Native 3840×2160 on RTX 5090 targets 60 FPS where the scene allows. Simultaneous VCam/Prism cost is separate. The lead commits/reviews and performs the final native build; the user performs in-world acceptance. Snapshots are not a priority.

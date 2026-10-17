# CPU efficiency v2: busy-region preview and machinima capture

2026-10-17. Design only; no implementation, build, commit, or push. Read `CLAUDE.md` first. Source inspected at HEAD `7050a38cc4ed`, including the existing working-tree changes. Code references below use `NV/` for `indra/newview/`. v2 incorporates the Opus adversarial review (PLAN NOT GREEN), checked against raw events and code; driver mechanism and gains still require A/B validation. Estimates are hypotheses, not measured improvements.

## Decision

Prioritize **shadow-target flush/transition stalls**: approximately 8 ms/frame lies immediately after shadow draws, inside the previously unattributed generator self time. First zone flush/bind/clear, then A/B direct shadow-FBO transitions without returning to framebuffer 0. This offers the largest opportunity without a quality tradeoff, if the waits are removable rather than merely relocated.

Rerank existing far-alpha controls and preview-only far-cascade reuse after that A/B: both currently avoid draw work plus a coupled post-pass gap. Probe-shadow reuse is already active (0.019 ms/frame); extend the probe scheduler only if its remaining approximately 1.24 ms/frame live-probe cost warrants it. Keep texture churn and proven idle work behind these shadow measurements.

## Evidence and accounting

Inputs: `.tmp/cpu_traces/alchemy_20261017_{084442,084720,084918}.{csv,tracy}` and `Alchemy_20261017_live.log`. The supplied conditions are Release, on-demand Tracy 0.13.1 with GPU zones, RTX 5090 32 GB, 4096x2160, busy club, `RenderVolumeLODFactor=4`, `RenderAvatarMaxComplexity=350000`, `RenderFarClip=136`, `RenderShadowResolutionScale=1.2`.

| Capture | Supplied state | `doFrame` mean / completed calls | `display - 1` mean | Main shadow generation mean, raw events | `idle` mean | `Update Images` mean |
|---|---|---:|---:|---:|---:|---:|
| 084442 | Pin ON; 55–59 GB RAM; 29/32 GB VRAM | 49.969 ms / 600 | 43.239 ms | 22.165 ms / 601 | 4.945 ms | 2.432 ms |
| 084720 | Pin OFF; about 16 GB VRAM | 54.205 ms / 554 | 41.134 ms | 19.106 ms / 555 | 11.992 ms | 7.016 ms |
| 084918 | Pin OFF; VCam master OFF | 44.449 ms / 675 | 34.971 ms | 18.266 ms / 678 | 7.409 ms | 2.943 ms |

The means correspond to approximately 20.0, 18.45, and 22.50 fps. For detailed 084918 totals below, divide by **677 `display - 1` events**. Capture boundaries leave different numbers of complete outer/inner zones; 677 is not the `doFrame` count. Inclusive parents overlap their children. Worker waits and decode totals are not main-thread time. CSV `total_perc` is not a percentage of frame time.

The stock CSV exporter does not escape every comma in C++ template names. These files were parsed from the right-hand numeric columns; new exports used a tab separator. Raw `.tracy` events and self times were read with `tracy-csvexport.exe -u` and `-e`; no traces were changed.

### The extra shadow calls are already cheap

`generateSunShadow`: 984 calls, 12.598 ms/call, 18.311 ms/render frame. Raw events separate into:

| Capture | Main calls, all >1 ms | Extra calls, all <1 ms | Extra-call mean | Extra-call total |
|---|---:|---:|---:|---:|
| 084442 | 601 | 270 | 0.0467 ms | 12.603 ms |
| 084720 | 555 | 250 | 0.0414 ms | 10.354 ms |
| 084918 | 678 | 306 | 0.0413 ms | 12.638 ms |

Every 084918 main generator contains exactly nine `renderShadow` events; all 6,102 renders belong to those main calls. There is no `cube face shadow` zone. The 306 extra generator calls match the 306 probe faces and contribute **0.019 ms/frame**. This is strong code-backed evidence of the existing clip-only Reuse/Off branch, not 306 full shadow generations. The trace does not distinguish `RenderProbeSunShadows=1` from `2`.

Code: `NV/llviewerdisplay.cpp:1493` selects Full versus reuse/off; `:1508` calls `generateSunShadow(..., true)`. `NV/pipeline.cpp:29774` returns before rendering for clip-only. `NV/llreflectionmapmanager.cpp:2873` scopes `RenderProbeSunShadows`; `NV/app_settings/settings.xml:21631` defines 0 Full, 1 Reuse, 2 Off. Hero/mirror captures use their separate path.

### Main shadow and probe costs

Ordering the nine nested shadow draws against `NV/pipeline.cpp:29924` and `:30616` identifies four sun cascades followed by five projector slots in this capture. CPU **cull/sort/draw** inside `renderShadow` excludes target setup and the following gap:

| Main shadow pass | Mean `renderShadow` ms/main invocation | Following gap ms | Included `shadow alpha` ms |
|---|---:|---:|---:|
| Sun cascade 0 | 1.540 | 0.468 | 0.041 |
| Sun cascade 1 | 0.728 | 0.880 | 0.267 |
| Sun cascade 2 | 3.442 | 3.973 | 1.494 |
| Sun cascade 3 | 1.769 | 1.932 | 0.828 |
| Five projector slots combined | 0.787 | 0.804 (0.126–0.210 each) | 0.179 |

Gaps run from `renderShadow` return to the next cascade's `getVisiblePointCloud` entry, then to the next projector render (or generator exit for the final slot). They include small intervening setup, so they are not exact flush timings. Their **8.057 ms/main invocation** is attributable to pass completion/transition, with `mRT->shadow[j].flush()` (`NV/pipeline.cpp:30597`) and `mSpotShadow[i].flush()` (`:30884`) the dominant unzoned GL candidates. Cascade 2 repeats the relationship in 084720 (**4.435→4.909 ms**) and 084442 (**3.869→3.522 ms**). The approximately 1:1 relationship to draw cost supports deferred submission/driver waits; it does not prove a removable synchronization mechanism.

The generator's **9.425 ms/render frame self time** is therefore not an unattributable fitting bill. The interval from `getVisiblePointCloud` return to `renderShadow` entry is only **0.044/0.046/0.056 ms** for cascades 1/2/3. Cascade 0's interval is **1.197 ms** (median 0.131), still mixing fitting with bind/clear and requiring zones; do not label it pure fitting. Drop the unused-LiSPSM-fit optimization. Also, **1.004 ms of cascade 0's 1.540 ms** is `rebuildPriorityGroups`, called by first-pass `postSort` (`NV/pipeline.cpp:5702`): shared geometry rebuild work, not shadow drawing that disappears with fewer passes.

Far cascades 2–3 cost **5.211 ms render + 5.905 ms following gaps = 11.116 ms/main invocation**, including 2.322 ms alpha rendering. Before fixing transitions, period 2's nominal avoided half is **5.56 ms**, not 2.61; the alpha cap could approach **4.5 ms** if eligible draw reductions also shorten those waits. These are coupled budgets, not measured gains. If item 1 removes the gaps, their render-only budgets fall to **2.61 ms** and **at most 2.322 ms**, respectively; remeasure and rerank, never add both sets of estimates.

Other 084918 evidence:

| Zone / source | Calls | Mean/call | Inclusive ms/render frame |
|---|---:|---:|---:|
| `LLReflectionMapManager::update`, `NV/llreflectionmapmanager.cpp:295` | 677 | 1.957 ms | 1.957 |
| `updateProbeFace`, same file `:2860` | 306 | 3.612 ms | 1.633 |
| `cubeSnapshot`, `NV/llviewerwindow.cpp:6238` | 306 | 3.334 ms | 1.507 |
| `rmmu - cine budget`, `NV/llreflectionmapmanager.cpp:999` | 85 | 9.850 ms | 1.237 |
| `cube face light`, `NV/llviewerdisplay.cpp:1568` | 306 | 1.991 ms | 0.900 |
| `renderDeferredLighting`, `NV/pipeline.cpp:25977` | 983 | 3.690 ms | 5.358 |
| `stateSort`, `NV/pipeline.cpp:5168` | 7,088 | 0.383 ms | 4.010 |
| `postSort`, same file `:5672` | 7,088 | 0.274 ms | 2.873 |
| `LLVOAvatar::idleUpdate`, `NV/llvoavatar.cpp:2783` | 4,739 | 0.281 ms | 1.969 |
| `updateSkinInfoMatrixPalette`, same file `:11482` | 79,567 | 0.0014 ms | 0.163 |
| `renderGeomVelocity`, `NV/pipeline.cpp:8108` | 677 | 0.923 ms | 0.923 |

All 85 cine-budget events request **two faces**; 170 face events are nested in them, leaving 136 ordinary/other manager faces. Budget-event p50/p95/max are **11.05/17.31/22.40 ms**. This is intermittent rendering, not a 9.85 ms scheduler tax every frame. All probes together add 0.452 scene views/frame; approximately 677 main views + 306 probe views explain the deferred-lighting count, with boundary differences.

Likewise, 7,088 `stateSort` calls are approximately **9 shadow passes + 1 main view + 0.45 probe views/frame**, with boundary differences. Raw containment puts **3.452 of 4.010 ms/frame** inside `renderShadow` (6,102 calls). This is predominantly repeated per-pass work, not a separate 4 ms main-view sorting target; do not double count it.

`LLPrismLens::renderAuxiliaryView` averages only 0.0055–0.0058 ms in **all three** captures, with no `Prism visible auxiliary update` or `Prism preview-only auxiliary update` events. Its master/empty-demand early exits are at `NV/llprismlens.cpp:9734`. The 9.76 ms outer-frame difference after VCam OFF is not an isolated VCam render saving: idle fell 4.58 ms and image update fell 4.07 ms while main shadow time also changed. VCam driver/UI idle work remains unmeasured, rather than disproved.

The reported approximately 47% GPU use supports a main-thread/submission investigation, but does not prove every delay is CPU execution. `FrameLimiter wait` costs **1.658 ms/frame** (median **0.00119 ms**) and Swap costs **2.397 ms/frame** in 084918. The limiter path is active (`NV/llappviewer.cpp:1807`, `NV/alframelimiter.cpp:132`), but a 30 fps cap's 33.33 ms interval is not binding on average **44.45 ms** frames; some faster frames still wait. GPU shadow zones include elapsed gaps between timestamped submissions; their approximately 13.21 s total overlaps CPU work. Do not add CPU and GPU timings or treat Task Manager utilization as a throughput proof.

## Capture-quality contract

New quality reductions default OFF and are explicitly enabled in Director performance controls, with visible effective state. The Director singleton's existence, a nonempty Cast, or opening a floater is not consent. Existing controls retain their authored values; temporary effective overrides must not rewrite settings or scene files.

Default new behavior: preview may use lazy shadows/adaptive probes; **ACTION, recorder recording/playback, snapshots/360, and an explicit “capture quality” lock force fresh shadows and disable FPS-based adaptation**. Use `LLDirectorCast::isRunning()` (`NV/lldirectorcast.h:470`), recorder state/`isPlaybackActive()` (`NV/llflycamrecorder.h:75`, `:104`), and the actual snapshot entry points. Conservatively bypass lazy shadows for active VCam auxiliary rendering initially. Temporal Capture is a time-control panel, not a reliable “video is recording” signal; `LLTemporalFrameContext::active()` alone does not identify an export. Provide the explicit lock for OBS/external recording, which the viewer cannot infer.

During a take, keep a chosen probe policy fixed, never adapt it from wall-clock FPS. Strict quality requests full shadow freshness and full live-probe cadence; complete irradiance and radiance warm-up before ACTION's first delivered frame. “Every frame” still alternates irradiance/radiance passes, so warm-up needs both. Six probe faces/frame could add roughly **20–30 ms/frame** over this budgeted baseline: all-face mean cost is 3.61 ms, while live budget cost is about 4.93 ms/face. This extrapolation needs measurement because pass-end filtering/batching differs. Do not silently impose that cost on ordinary playback; make strict versus fixed-budget take quality an explicit choice.

Geometry/animation stays at existing quality, subject to the user's explicit hard mute/complexity choices. Fixed take policies prevent new timing-dependent visual changes; they cannot make a live network region bit-for-bit deterministic. `isCaptureQualityPinActive()` is a memory-pressure-aware texture policy (`NV/llviewertexture.cpp:515`), not the recording predicate for these features.

## Candidate designs

### Shadow completion — step 0 zones, item 1 direct transitions

**Code and mechanism candidates.** `LLRenderTarget::flush` (`indra/llrender/llrendertarget.cpp:620–652`) calls `gGL.flush()` and restores the previous target; with no parent it binds framebuffer 0, restores viewport, then calls `glReadBuffer(GL_BACK)` and `glDrawBuffer(GL_BACK)`. The next `bindTarget()` (`:534–566`) binds another FBO and selects depth-only `GL_NONE` buffers again. Shadow maps are depth-only allocations (`NV/pipeline.cpp:2341`, `:2379`). Candidates are deferred draw submission/implicit flushing, driver resolve or serialization on FB0 rebinding, and read/draw-buffer validation. None is proven by timing correlation. `gGL.flush()` drains LLRender's pending vertex batch (`indra/llrender/llrender.cpp:1778`); it is distinct from the explicit first-cascade `glFlush()` at `NV/pipeline.cpp:30605` (only 0.004 ms/call here).

**Step 0, measurement only.** Add CPU zones around the entire target flush, bind and clear, including their GPU-zone constructors/destructors; subdivide pending-batch drain, framebuffer bind, viewport and read/draw-buffer calls. Tag cascade/projector index and previous/next FBO; keep fit separate. This distinguishes the actual stall from an interval containing it. No implementation gain is credited to zones.

**Exact minimal A/B.** Add a scoped sibling-target transition used only by main-view depth-only shadow passes: finish the old target's pending `gGL` batch, bind the next actual shadow target directly (**FBO A→FBO B**), and restore the entry target/FB0 once on sequence exit. Carry the original parent across transitions rather than stacking shadow targets; update `sBoundTarget`, `mPreviousRT`, `sCurFBO`, resolution and viewport bookkeeping together. Initialize/retain each depth FBO's `GL_NONE` read/draw state, suppressing redundant buffer selections only while that state is known unchanged. Cover empty/cleared slices and the subject-column branch (`:30159`, `:30233`); restore on every exit, including no projectors. Preserve clear/draw order, map publication, pending-batch semantics and `RenderShadowFlushEarly`; do not globally change other `LLRenderTarget::flush()` callers. A temporary switch restores the stock flush/bind path exactly. This changes state transitions, not shadow quality or cadence.

**Instrumentation risk and acceptance.** `flush`, `bindTarget` and `clear` currently have GPU zones, not CPU zones. In the installed Tracy 0.13.1 `TracyOpenGL.hpp`, GPU-zone entry/exit calls `glQueryCounter(GL_TIMESTAMP)`; `Collect()` checks availability before reading results. Collection occurs after swap (`indra/llwindow/llwindowwin32.cpp:3933`), with only **0.040 ms/frame** in `GPU Collect` here, so no evidence of an explicit blocking collect at every shadow pass. Timestamp submission itself may nevertheless perturb/serialize the driver and place its cost in this gap. Later, run a **stock/direct-transition × GPU-zones-on/off** comparison with CPU zones retained (`USE_TRACY=ON`, `USE_TRACY_GPU=OFF` for the latter); this document requests no build. Inspect query entry/exit costs separately. GPU elapsed zones can move or change attribution after rebinding, so only reduced CPU `doFrame`/total submission time counts as a win, not a shorter flush followed by a longer next bind, swap or collect.

**Gain/risk.** Opportunity: **up to approximately 8 ms/frame**, with no intended visual tradeoff; actual gain may be zero if unavoidable work merely moves. Risks are RT-stack/FBO/viewport/read-draw state leaks, clearing the wrong map, missing pending draws, and changed GPU timing attribution. Check stock OFF behavior, main beauty immediately after shadows/probes, subject mode, projectors, mirror/VCam and nested targets. If gaps vanish only with GPU zones off, quantify profiler overhead before proposing a renderer change. Rebaseline all quality-reduction estimates after this experiment.

### Probe shadow reuse and deferred lighting — reuse exists; reject duplicate work

**Design/status.** Keep `RenderProbeSunShadows` and its existing Lightbox UI (`NV/skins/default/xui/en/floater_lightbox_settings.xml:1468`). First record its effective value. Reuse validates published main maps (`NV/pipeline.cpp:29359`), composes them for the probe eye (`:24117`), and scopes restoration of shadow state (`NV/llviewerdisplay.cpp`, `ScopedProbeShadowState` preceding `:1414`). Do not implement another cache or remove validation/fallbacks. Main maps do not cover every direction from a probe; Reuse intentionally treats uncovered areas as lit.

**Deferred decision.** Keep deferred lighting. `cube face light` is already only 0.900 ms/frame, and contains forward/post-deferred work too. Removing it would bake incorrect PBR irradiance/radiance and local-light response. Probe-centric light rebuilding at `NV/llviewerdisplay.cpp:1452` fixes missing first-face lights; preserve it. No reduced-lighting shader fork in this plan.

**Gain/risk/switch.** Additional shadow skipping here is at most about **0.019 ms/frame**, so reject as a performance slice. Full↔Reuse↔Off remains the existing kill switch. Risks are uncovered lit regions, stale matrices, spot-state leakage, and beauty-pass corruption after a face. Any future change must check feature-OFF and subsequent main-view rendering, per `CLAUDE.md`.

**Measure.** Standard capture procedure below; compare full-versus-clip-only events and `cube face shadow`, not generator call count. Confirm main/hero/mirror shadows and face lighting after every toggle. No credit for the hypothesized 5–6 ms/frame duplicate-shadow saving.

### Far-alpha cap and lazy far sun cascades — preview-only and gated

**Design.** After the transition A/B, first try `RenderShadowAlphaNearCascades=2`: the current implementation drops static alpha casters from distant sun cascades while retaining worn/rigged items, avatars, projectors, and the subject column (`NV/pipeline.cpp:30575`; setting semantics `NV/app_settings/settings.xml:18657`). This is a usable existing control. With the coupled gaps intact, the opportunity is **up to approximately 4.5 ms/frame**; after removing them, the render-only ceiling is **2.322 ms**, with **0.5–2 ms** a provisional target. Attachment preservation and subject mode reduce the eligible work, so neither ceiling is a promise. If already capped, selecting it again gains nothing. Missing distant foliage/fence shadows is the tradeoff; `0` restores stock. Measure render plus completion cost, not only `shadow alpha`.

For actual caching, propose one `DirectorFarShadowPeriod` control: **1 stock, 2, or 4**, default 1. Keep cascades 0 **and 1** fresh initially; update 2 and 3 on staggered main-frame phases. This protects the near band when cascade 0 is a Follow-subject column. Projector shadows remain unchanged. Cache only pixels and value metadata, never drawable/cull-result pointers across frames.

Retain each cached map's matching light view/projection, world-to-shadow matrix, coefficients, clip layout, validity and generation. `generateSunShadow` currently resets all `fitted[]` entries before rendering (`NV/pipeline.cpp:29789`); skipping after that reset is incorrect. Recompose retained world-to-shadow matrices with the **current** eye on every bind, including the legacy settings path; `uploadShadowUniforms` currently only does the general recomposition when engaged (`:24136`). Update the main publication token to mean all required maps are valid for this iteration, including permitted retained maps. Probes must see the same validated metadata.

Stable far projections need a light-aligned basis, quantized/padded extents and texel-snapped centers, with coverage retained until refreshed. Snapping a center while refitting its extent every frame will still crawl. Reuse only while the current required receiver volume fits inside the saved coverage, including blend overlap and low-sun caster reach; otherwise refresh immediately. Moving far casters may have up to N−1 frames of deliberate preview staleness. Do not invalidate on every animated crowd joint, which would eliminate reuse.

**Existing low-sun work.** Commit `de2e5b16ea7` made orthographic shadows the default (`RenderShadowPerspectiveWarp=false`) and enabled early submission. Preserve both. The elaborate phase-continuous snapping at `NV/pipeline.cpp:29943–30054` belongs to the **subject column**, not all camera cascades. Reuse its numerical lessons, not its subject-box semantics. For the first slice, perspective-warp ON bypasses lazy mode; do not reopen LiSPSM fitting or its low-sun caster-loss bug.

**Invalidation/risks/switch.** Refresh all on first use, reallocation/resolution/detail/split/bias-policy change, teleport/region shift, camera cut/seek/FOV discontinuity, light-direction change beyond a conservative texel threshold, or insufficient coverage. On capture lock/ACTION entry, refresh before the first captured frame; while locked update every frame. Active auxiliary VCam, hero/mirror capture and snapshot paths initially bypass scheduling and must not advance main-frame phases. Risks: stale moving shadows, seams, low-sun clipping, matrix/map mismatch and a cut hitch. Period 1 immediately restores fresh generation. Expose effective bypass reason; do not add a general render-graph scheduler.

**Expected gain/measure.** With current completion gaps intact, period 2 nominally avoids **11.116/2 = 5.56 ms/main frame**; period 4 avoids **8.34 ms**. If item 1 removes those gaps, the corresponding render-only figures are **2.61 / 3.91 ms**. Fit/clear savings are separate and small for far slices; invalidations and padding subtract, possibly leaving zero during a moving shot. No promised recording gain. Rerank from the post-transition baseline and remeasure again after alpha capping because the gains overlap. Add indexed fit, bind, clear, cull/sort/draw and completion zones plus reuse/invalidation counters. Verify cadence, total `doFrame`, p95, and stationary/slow-pan/low-sun/cut footage, then locked recording and VCam.

### Smart live probe — extend the shipped budget, not a new scheduler

**Design.** Existing modes already provide Every frame, Balanced, Economy, On change, and Manual. `NV/llreflectionmapmanager.cpp:609–740` dispatches them; `updateCinematicBudget()` at `:996` freezes the origin per pass and publishes at pass boundaries. `CineLightRigLiveProbeChangeFaces`, `...ManualFaces`, `...ManualFrames`, settle/watchdog settings already exist (`NV/app_settings/settings.xml:2328`). `RenderProbeOnDemand` also debounces animated scene changes; it does not mean “every probe is now idle.” Preserve dirty/barrier and irradiance/radiance ordering.

Try those controls first. If needed, add a single Adaptive preview option to that dispatcher: cap at one face per update under sustained low FPS, allow two after recovery with hysteresis; choose using a smoothed main-frame cost excluding intentional limiter sleep. Decisions occur between passes. Slowing the same transaction without lowering transaction frequency mainly reduces spikes, not total work. Gate new refresh starts by existing dirty/settle/watchdog state; a static camera alone is insufficient because club lights, actors and the probe anchor can move. Do not hash the whole scene again.

During takes use the explicit fixed-budget/strict policy above, with no FPS feedback. Initial VCam support retains the authored probe mode; a main-eye static test does not describe another camera. Preserve the old complete probe while building, and never publish an incomplete face set.

**Gain/risk/switch/measure.** Live capture is 1.237 ms/frame; removing every live update would save no more than that parent cost in this capture. Target **0.3–0.8 ms/frame**, plus lower p95 spikes. One-face bursts should reduce the measured 11–22 ms two-face bursts, but not necessarily halve them because face 5 also filters/publishes. Strict six-face takes can cost substantially more. Risks: stale reflections, cube seams, delayed light cues and mixed-time faces. Existing refresh mode, live-probe Enable and Adaptive OFF are kill switches. Compare live/ordinary faces separately, burst p95, publish latency and highlight/light-cue footage; check `RenderProbeSchedLog` and the capture barrier rather than only average FPS.

### Cine budget, idle and textures — attribute residuals; fix proven churn

**Cine budget.** It contains 170 face renders, 1.233 ms/frame versus its 1.237 ms/frame parent. Non-face overhead is about **0.004 ms/frame**. Reject an attempt to optimize the scheduler's bookkeeping or move its GL rendering to workers. Use the live-probe controls for fewer/better-distributed captures.

**Idle.** `LLAppViewer::idle` is 7.409 ms inclusive and **3.357 ms self**. It calls unzoned `gIdleCallbacks.callFunctions()` (`NV/llappviewer.cpp:5521`, `indra/llcommon/llcallbacklist.cpp:112`), object/avatar update (`:5609`), rig/VCam drivers (`:5756`), camera dispatch and recorder. Known children include 2.208 ms object update with the 1.969 ms avatar work nested inside, 0.445 ms `updateUI`, and 0.148 ms network. The self time cannot be called a particular UI or VCam hotspot yet.

Add only bounded zones around the callback group, rig tick, bone-POV tick, camera dispatch and recorder sampling; then instrument whichever child dominates. `ALVCamBonePov::tick` takes a registry snapshot every idle (`NV/alvcambonepov.cpp:324`), making it a candidate for a no-capture/no-demand fast path, not a confirmed culprit. Preserve hidden-tool model side effects and release gates; throttle only UI mirrors by revision/visibility using `aluirefresh.h`, as `CLAUDE.md` requires. `LLPanel::draw` is recursive (2.996 ms/frame inclusive here), so its sum is not a separate removable UI bill. Initial gain budget **0 ms for instrumentation**, then **0.2–1 ms** only if a specific redundant callback/refresh is demonstrated. Kill switch is the affected tool's existing enable plus an inert feature-off path. Measure child self times and interaction correctness, including closing/hiding panels.

**Texture churn.** This is real: the live log at lines **10542–10580** (12:47:28–12:47:44Z, corresponding to the 08:47 capture) repeatedly reports “Texture will be downscaled immediately after loading”; lines **10928–10968** show it continues during 08:49. `updateImagesCreateTextures` costs 5.845 ms/call in 084720 versus 1.809 in 084918. `scaleDown` is 900 calls / 2.744 ms per outer frame in 084720 versus 351 / 0.631 in 084918; `glGenerateMipmap` accounts for 2.404 and 0.567 ms/frame respectively. These overlap texture-update parents. The ON capture has a 189.9 ms texture-allocation outlier despite fewer creates. Pinning all visible textures is not a CPU-efficiency solution at 29/32 GB VRAM.

At `NV/llviewertexturelist.cpp:1121`, an existing redundant-load guard already skips uploads if the current GL texture suffices. Immediately after create, `:1133` may downscale it; queue processing at `:1154` deliberately exceeds its time budget to relieve memory pressure. Do not add the same guard again or simply reduce that emergency budget. First record bias/pressure/pin transitions, desired versus decoded/uploaded discard, queue lengths and duplicate downscale requests. Fix the demonstrated mismatch at the queue/creation boundary: coalesce redundant target changes, avoid repeated downscales to the same discard, and stabilize upgrades after pressure recovery. If an oversized decoded image is the only usable image, retain the safe create/downscale fallback; do not relabel its discard or leave it unloaded. Preserve raw-image callbacks, pin intent and memory emergency relief.

Expected **0.3–1 ms/frame** in the 084918 workload; **1–3 ms** is a provisional opportunity during the worse churn state, not a proven saving from turning Pin off. Existing pin/governor controls remain; any new coalescing gate must allow stock behavior. Measure warning-counter rate, creates/downscales per second, frame tails, texture sharpness and RAM/VRAM after settling. Worker JPEG2000 decoding is not the main-thread target: 084918 decode is only 0.393 ms/frame aggregated on workers.

### Other small cuts and explicit rejections

* **Shared sun culling: try the existing switch.** `RenderShadowCullMode=1` does a union octree cull and per-cascade rebucketing (`NV/pipeline.cpp:29823`, `:28882`), not one shared sort. The comment in `NV/app_settings/settings_alchemy.xml:1745` is stale; trust the code. All `updateCull` calls together cost 1.610 ms/frame, so a defensible trial target is **0.1–0.5 ms**, never several ms solely from culling. Mode 0 is the kill switch. A/B low-sun offscreen casters, camera pans, CPU cull time and GPU draw counts; reject if the conservative union increases work enough to offset savings.
* **Crowd policy deferred:** only seven avatar idle updates/frame and 1.969 ms/frame here; require a dense-crowd trace before proposing Cast/Crowd changes.
* **Batch/state overhead: reduce pass work first.** `pushBatch` has 3,823,946 zone calls (about 5,648/frame) and 1.961 ms/frame inclusive; `uniform1i` has 9,213,070 calls and 1.001 ms inclusive (`indra/llrender/llglslshader.cpp:1792`). Uniform values are already cached there. Zone counts are not hardware draw calls or GL state changes; instrumentation/lookup cost is included. Add per-pass actual submitted-draw/state-change counters, not another uniform cache. A local repeated-material setup fix could target **0.1–0.3 ms**, contingent on evidence and shader-relink invalidation; otherwise defer. Kill switch/validation belongs to that specific path, including beauty/alpha correctness.
* **Do not rewrite alpha sorting or the renderer.** `postSort` is nested within `stateSort`; main forward alpha (`LLDrawPoolAlpha::renderPostDeferred`) is only 0.890 ms/frame. Keep transparency order. No ECS/job-system migration, Vulkan rewrite, GPU skinning replacement, global attachment deletion, or shadow render graph in this plan. No speculative GPU tuning by lowering the 4K window or changing all quality settings at once. Keep the five projector slots unless a separate rig-light A/B proves they are disposable: their measured render portion is only 0.787 ms combined.

## Ranked delivery and acceptance

Small slices here isolate render-state/quality risks for in-world A/B, as requested. Each implementation includes its reachable control, effective-state indication, reset/restore wiring and necessary diagnostics; no engine-only feature delivery. Apply the repository's adversarial review loop before any later build. This document authorizes no implementation or build.

| Order | Small, reviewable result | Opportunity ms/frame, conditional | Reason / stop condition |
|---|---|---:|---|
| 0 | Repeated controls; indexed CPU zones for flush/bind/clear/fit and their GL/timer-query calls | 0 | Resolve pass-completion stalls and profiler perturbation before choosing a fix |
| 1 | A/B direct sibling shadow-target transitions, stock fallback; GPU zones on/off independently | Up to ~8 | Highest priority, no intended quality loss; reject relocated waits, state leaks or a profiler-only benefit |
| 2 | A/B existing far-alpha cap=2 on the new baseline | Up to ~4.5 if gaps persist; ≤2.32 render-only if removed | Low implementation risk; actual eligible saving is smaller; reject visible distant-shadow loss |
| 3 | Far-cascade period 2 with stable coverage, invalidation and capture bypass; period 4 later | Nominal 5.56 if gaps persist; 2.61 render-only if removed | Higher correctness risk; rerank after items 1–2, subtract invalidation/padding cost; stop on shimmer/seams |
| 4 | One demonstrated texture coalescing/pressure-transition fix, if churn persists after settling | 0.3–1 normally; 1–3 in churn | Addresses logged repeated work and tails; no new governor or larger cache |
| 5 | Existing live-probe mode/budget tuning; Adaptive preview only if warranted | 0.3–0.8 | Bounded opportunity with existing scheduler; keep fixed during takes |
| 6 | Proven idle fast path; separately A/B existing shared cull or a demonstrated material setup cut | 0.2–1 idle; 0.1–0.5 cull; 0.1–0.3 material | Require child-zone evidence and net frame improvement; preserve geometry rebuild work |

Do not add these ranges: transition stalls, far-alpha, lazy cascades and submission savings overlap. Item 1 comes first; items 2–3 use its measured residuals, never the old doubled estimates if those gaps are gone. Removing the entire ~8 ms opportunity alone would take 44.45 ms to roughly **36.4 ms (~27.4 fps)**; this is an upper-bound illustration, not a forecast. Probe burst redistribution may improve tails without changing means. Even removing the entire 18.31 ms shadow parent leaves about 26 ms. Capture lock may intentionally recover quality instead of FPS.

### One repeatable measurement procedure

1. Same build/configuration, region spot, camera/FOV, 4096x2160 foreground window, sky/time, Cast, light rigs, visible panels and avatar population as closely as possible. Keep the four supplied quality settings fixed. Log Pin, memory governor/pressure, `BDMergeMachinimaHighLOD`, shadow splits/subject mode, probe shadow/refresh modes, ordinary rate and VCam demand. Warm scene/queues to a stable state; separately capture the pin-transition/churn case.
2. Preserve one original-settings control. The 30 fps cap is not the average bottleneck at 44.45 ms/frame. For throughput tests disable `FramerateLimitEnabled` in **both** A and B, with matched VSync; also test the normal capped configuration for pacing. Intentional waits on faster frames can absorb a saving. Do not remove pacing code.
3. Use the same existing procedure, run by the operator for each state:

   ```powershell
   powershell -ExecutionPolicy Bypass -File "C:\Users\xianw\Desktop\Tracy\profile.ps1" -Seconds 30
   ```

   Match Tracy 0.13.1, Release and on-demand connection; retain CPU zones throughout. For item 1 run stock/direct transitions with GPU zones ON, then repeat both with GPU zones OFF, logging the configuration. Hold `RenderShadowFlushEarly` fixed; test changing it separately only if zones justify it. Keep the viewer focused. Run A/B/A, preferably twice, one setting or slice at a time; retain `.tracy`, `.csv`, settings and matching log interval. A changing club population or unsteady upload queue makes a small delta inconclusive.
4. From raw events report mean/median/p95/p99 `doFrame`, >50/>100 ms frames, intentional sleep/swap/collect, main generator versus clip-only calls, indexed fit/bind/clear/render/flush times and reuse counts, live versus ordinary faces, capture burst p95/publication age, avatar idle counts, and upload/downscale rates plus RAM/VRAM. Order thread-1 events by start time, contain the nine `LLPipeline::renderShadow` calls in each main generator, and measure each return-to-next-event interval with the boundaries defined above. Filter exact zone names/source lines to exclude overloads and avatar child functions. Separate first-pass `rebuildPriorityGroups`; compare **render + completion + next bind/clear** and total frame time so a shifted stall cannot masquerade as a gain. Read-only export examples:

   ```powershell
   & 'C:\Users\xianw\Desktop\Tracy\tracy-csvexport.exe' -u -s "`t" -f generateSunShadow '<capture.tracy>'
   & 'C:\Users\xianw\Desktop\Tracy\tracy-csvexport.exe' -u -s "`t" -f 'LLPipeline::renderShadow' '<capture.tracy>'
   & 'C:\Users\xianw\Desktop\Tracy\tracy-csvexport.exe' -u -s "`t" -f getVisiblePointCloud '<capture.tracy>'
   & 'C:\Users\xianw\Desktop\Tracy\tracy-csvexport.exe' -e -s "`t" '<capture.tracy>'
   ```

   If Python analysis is needed, use only `C:\Users\xianw\AppData\Local\Python\pythoncore-3.14-64\python.exe`. Statistics alone cannot identify every stall; inspect CPU nesting and GPU queue gaps. Do not interpret a GPU `rt flush` duration as CPU time or require identical GPU-zone attribution after a target transition.
5. Accept a slice only when repeated improvement exceeds A/A drift and its target zone/counter changes as designed, with no unexplained p95/p99 regression. Check saved stationary, slow-pan, low-sun foliage, animated Cast/crowd, mirror/VCam, ACTION/cut/seek, strict take and feature-OFF footage. Verify a beauty frame immediately after auxiliary capture. Report PASS / FAIL / INCONCLUSIVE; unchanged FPS under a cap is not failure if work/pacing improved. Exact-take bypass must produce fresh maps before the first delivered frame and restore preview policy on exit.

## Open questions for the operator (maximum five)

1. Is the recording workflow Director ACTION, camera-path playback, OBS/external capture, or a combination—and should strict quality force six live-probe faces/frame despite its possible approximately 20–30 ms/frame added cost?
2. Which operation actually owns the post-pass stall: pending submission, FB0 bind, read/draw-buffer validation, or Tracy timestamp submission? Does it persist with GPU zones off, and does direct binding remove it from total frame time?
3. Are slightly stale distant shadows acceptable in preview (period 2, about 45 ms age at current FPS; period 4, up to about 133 ms), and are missing distant foliage/fence shadows acceptable at all?
4. What were the effective `RenderProbeSunShadows`, `CineLightRigLiveProbeRefresh`/Manual rate, `RenderShadowAlphaNearCascades`, `RenderShadowCullMode`, `RenderShadowFlushEarly`, `BDMergeMachinimaHighLOD`, and frame-cap values? The traces establish behavior, but not every setting value.
5. Is the priority a responsive 25–30 fps club preview, or sustained full-quality recording at a specific frame rate?

## Review notes (10/17)

- **Status: PLAN GREEN.** Opus round 2, after round 1 was NOT GREEN with 4 findings, all folded in. Codex xhigh was the author.
- **Viewport after the direct-transition fix:** the stock code leaves `gGLViewport` at the last shadow map's size (`pipeline.cpp:30564`/`:30870`, which `flush()` then applies). The direct version must leave the same exit state; it must not restore "the entry viewport". This is harmless today only because `llviewerdisplay.cpp` sets its own viewport right after (~`:874`, ~`:892`).

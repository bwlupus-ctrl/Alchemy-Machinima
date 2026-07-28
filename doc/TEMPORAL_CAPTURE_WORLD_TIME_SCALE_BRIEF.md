# Temporal Capture — Landing 1: World Time Scale (v3, Codex-reviewed ×2)

Scope of THIS landing: **Pillar 1 (Local Time Scaling)** only, from
`SECOND_LIFE_TEMPORAL_CAPTURE_WORLD_TIME_DEEP_DIVE_WITH_CODE_ARCHITECTURE.md`.
A single client-side **presentation clock** that scales every locally-evaluated
cinematic dynamic together (0× freeze → 8×), while network/UI/watchdog time stays
real. No buffered world playback, no fixed-frame capture, no recorded takes yet —
but named and structured so those pillars slot in without a rename or rewrite.

**v3 incorporates TWO Codex adversarial design passes** (Codex session
`019f9e54-8a0d-7292-8113-d00bd72318b1`). §0 maps every finding from both passes to
its resolution; the rest is the design of record. Line numbers are Codex-verified
against the current tree.

User directives: visual UI feature for the Director Console **and** a standalone
floater from ONE **shared panel** ([[director-console-superset-rule]]); **name it
for the whole system from the start**; **ship WHOLE** ([[ship-features-whole]]);
**adversarial Codex review before code AND after** ([[codex-consult-mandatory]]).

---

## 0. Codex review resolution map

**Pass 1 (v1 → v2):**

| # | Finding | Resolution |
| --- | --- | --- |
| MUST-1 | Tick point wrong (globals recomputed mid-idle `llviewerobjectlist.cpp:925-940`) | Tick+freeze at **start of `idle()`** (`llappviewer.cpp:5011-5017`), monotonic timer. §3 |
| MUST-2 | "0× == Freeze World" false | Coexist; 0× = presentation clocks paused; never mutate `FreezeTime`. §5 |
| MUST-3 | Seam list omits Director/actor traversal → foot-slide | Actor gait+traversal+turns+gaze coupled under ONE gate. §4.2 |
| MUST-4 | Particle `visirate`/`mSkippedTime` double-account; clamp breaks at 8× | Single-source substitution + bounded substep/debt; 0× = zero-delta. §4.4 |
| MUST-5 | Texture anim can't use one global clock | Per-instance epoch rebasing; smooth-mode keeps phase. §4.3 |
| MUST-6 | "Avatars" gate hits every controller | Gate = `TemporalDriveAnimation` (all skeletal). §4.1 |
| SHOULD-1 | Target-omega call is `:2503`; scale only angular arg | §4.5 |
| SHOULD-2 | One owner for anim-scale restore | `LLPresentationTime` sole writer; restore on all exits. §2/§4.1 |
| SHOULD-3 | Freeze ownership fragile | Leave untouched. §5 |
| SHOULD-4 | Land small-but-real frame context | `LLTemporalFrameContext` now. §3 |
| SHOULD-5 | Scene-save must not save runtime state | Only requested mode/scale/gates. §7 |
| OPT | Tab "Temporal"; Flycam Recorder isolated | §1/§4.6 |

**Pass 2 (v2 → v3):**

| # | Finding | Resolution |
| --- | --- | --- |
| MUST-1 | Independent `Animation`/`Actors` gates still desync a moving actor | **Collapse:** actor gait + traversal + turns + gaze are ONE unit under `TemporalDriveAnimation`. Object-path drives move to `TemporalDriveObjects` (no skeleton → no foot-slide). §3/§4.1/§4.2 |
| MUST-2 | Particle `clamp(wall_dt,0,0.1)*scale` diverges from shared clock's 1.0s `wall_delta`; hitch time lost | Consume the **frozen `presentation_delta` directly** (same clamp as §3); substitute at the single source only; substep for fast scale. §4.4 |
| MUST-3 | Camera 0× divides by dt (`llcinematiccamera.cpp:1357-1368`) → NaN, or `0.0005f` floor (`:1202-1204`) creeps during pause | **Zero-delta early-out holds camera phase**; never divide by 0, never use the floor while driven. §4.6 |
| SHOULD-1 | Particle skip-accounting single-owner rule unstated | Scale applied **once at `:665`**; `visirate`(`:812-829`)+`mSkippedTime`(`:276-296,:838-840`) left structurally unchanged, now in presentation units. §4.4 |
| SHOULD-2 | Freeze World "own-owner shortcut" contradicts non-mutation (`UseFreezeWorld` is a shared bool `llviewercontrol.cpp:1021-1045`) | **Drop the shortcut.** 0× is presentation-pause only; existing Freeze World UI unchanged. §5/§6 |

## 1. Naming (Codex-confirmed no collisions)

| Concern | Name | File |
| --- | --- | --- |
| Engine time service (singleton) | `LLPresentationTime` | `indra/newview/llpresentationtime.{h,cpp}` |
| Immutable per-frame context | `LLTemporalFrameContext` + `LLTemporalMode` | `indra/newview/lltemporalframecontext.h` |
| Shared control panel | `ALPanelTemporalCapture` | `indra/newview/alpaneltemporalcapture.{h,cpp}` |
| Standalone floater | `ALFloaterTemporalCapture` | `indra/newview/alfloatertemporalcapture.{h,cpp}` |
| Panel XML (shared) | — | `skins/default/xui/en/panel_temporal_capture.xml` |
| Floater XML | — | `skins/default/xui/en/floater_temporal_capture.xml` |
| Floater registry key | `"temporal_capture"` | `llviewerfloaterreg.cpp` (idiom `:523`) |
| Panel injector key | `"panel_temporal_capture"` | `alpaneltemporalcapture.cpp` (idiom `alpanelactormover.cpp:24`) |
| Console tab label | **"Temporal"** | `floater_director.xml` |
| Settings prefix | `Temporal*` | `app_settings/settings.xml` |

`LLTemporalMode` carries the FULL set now (`LIVE, MANUAL_SCALE, ADAPTIVE_SCALE,
FIXED_FRAME_CAPTURE, BUFFERED_REPLAY, PAUSED, SEEK_REBUILD`); only `LIVE`,
`MANUAL_SCALE`, `PAUSED` are wired. Unsupported modes are NOT settings-selectable or
deserializable into partial behavior.

## 2. Non-negotiable invariants

1. Wall/network time is **never** multiplied by presentation scale.
2. Presentation time is **opt-in** per subsystem; every drive gate OFF = today's
   behavior **byte-identical**.
3. The presentation tick + frozen context happen **once, at the top of `idle()`**,
   before any consumer; all consumers in that idle→display pass read the SAME frozen
   context (no intra-frame skew).
4. `0×` is **paused**, not epsilon; guard divide-by-scale and divide-by-delta; 0×
   must not build thaw debt (call sims with zero presentation delta, don't skip).
5. `LLPresentationTime` is the sole **temporal** writer of `sGlobalTimeFactor`. It
   **captures the prior value** on takeover and **restores that exact value** on
   Live / gate-off / shutdown (not a blind 1.0), so an Advanced-menu slow-motion
   survives. It coexists with the Advanced animation-speed menu, taking precedence
   while active (re-asserts the scale each tick).
6. Never mutate `FreezeTime` or `UseFreezeWorld` from temporal code.
7. Never overwrite per-controller `mTimeFactor`; the global composes ×1 on top.

## 3. Engine: `LLPresentationTime` + `LLTemporalFrameContext`

- `LLSingleton`. `beginIteration(monotonic_now)` **and** `freezeFrame()` at the
  **start of `LLAppViewer::idle()`**, right after `updateFrameTime()`/
  `updateFrameCount()` (`llappviewer.cpp:5011-5017`) — before object-path/ghost
  updates (`:5308-5323`), `gObjectList.update()` (`:5325-5329`), particles
  (`:5424-5436`), camera dispatch, and `display()` (`:1528-1536`). `wall_delta`
  comes from an **independent monotonic timer**, NOT `gFrameIntervalSeconds`
  (recomputed later `llviewerobjectlist.cpp:925-940`; distrusted >200fps
  `llappviewer.cpp:5319`).
- `wall_delta = max(0, now-prev)` (lower-bounded only). The presentation **clock**
  advances by the full delta so `presentation_time` stays accurate; first tick is 0
  (init branch) → no startup step. **Consumers split by what they can integrate:**
  skeletal animation (the controller's own uncapped delta × `sGlobalTimeFactor`,
  `llmotioncontroller.cpp:858-871`) and texture animation consume the full
  `presentation_delta`. The position/emission consumers (traversal, gaze, ghost
  turns, particles, spin, object paths) apply a **uniform 0.25s subsystem hitch
  cap** at their call sites — they are not built to integrate one huge step
  (ping-pong reflection, rotation, emission), and removing the cap demonstrably
  breaks the ping-pong / `stepTurn` evaluators. Gait↔traversal coupling is therefore
  **exact while the per-frame `presentation_delta` stays under 0.25s** — normal
  operation at ANY scale. It degrades only on a genuine stall (a wall frame
  > 0.25/scale, so it can occur at any scale, not just fast playback), where the
  capped consumers behave exactly as the stock viewer already does — **no worse than
  stock**, since the scale shrinks gait's step too. Exact stall coupling needs
  per-consumer sub-stepping — **deferred** (§8).
  `MANUAL_SCALE` (not paused): `presentation_time += wall_delta * effective_scale`.
- **`LLTemporalFrameContext` (frozen once), Landing-1 fields:** `generation`,
  `mode`, `wall_delta`, `presentation_time`, `presentation_delta`
  (`= max(0, presentation_time - previous)`), `effective_scale`, `paused`,
  `drive_mask`. Consumers read this ONE struct; never re-query "now" mid-frame.
- Accessors: `frame()`, `active()` (`mode!=LIVE`), `drives(feature)`, `scale()`,
  `presentationDelta()`, `paused()`.
- **Drive gates** (bool settings, default = today):
  `TemporalDriveAnimation, TemporalDriveObjects, TemporalDriveTextureAnim,
  TemporalDriveParticles, TemporalDriveCamera`. A consumer reads presentation time
  **only** when `active() && drives(feature)`; else its wall path is byte-identical.

## 4. Consumer wiring (Codex-verified seams)

### 4.1 Animation & actor motion — `TemporalDriveAnimation`  (one coupled unit)
This gate scales a moving actor's **skeleton and its world motion together** — the
fix for the foot-slide desync. It drives:
- **Skeletal** (all controllers, incl. animesh/animated objects): set
  `LLMotionController::sGlobalTimeFactor = effective_scale`
  (`llmotioncontroller.cpp:867-871`; scope note `llmotioncontroller.h:185-190`).
  `LLPresentationTime` is the sole temporal writer; it **captures the prior global
  on takeover and restores it** on every exit (inv. 5) — coexisting with the
  Advanced anim-speed menu. Never write `sCurrentTimeFactor` (seeds new controllers
  only, `:131-133`). Composition:
  per-clone (`alghoststudio.cpp:225-226,615-616`) + actor-mover
  (`llactormover.cpp:1635-1639,1673-1678,2475-2492`) write `mTimeFactor`; the global
  multiplies once — never overwrite, never scale `delta_time` in the controller.
- **Actor world motion** (currently `gFrameIntervalSeconds`) → advance on
  `presentationDelta()`: actor-mover traversal (`llactormover.cpp:1874-1891`), gaze
  easing (`:2724-2734`), ghost turn interpolation (`alghoststudio.cpp:987-1004`).
  Since skeleton scales by `sGlobalTimeFactor=scale` and traversal by
  `presentation_delta = wall_delta*scale`, both scale by the SAME factor → gait and
  position stay locked (no double-apply) **while the per-frame delta is under the
  0.25s subsystem cap** — all slow-mo and fast at normal fps; see §3 for the
  large-delta fallback. Follower "hold" at exactly 0
  (`llactormover.cpp:2517-2520`) stays 0.

### 4.2 Object motion — `TemporalDriveObjects`  (no skeleton → own gate)
- **Target-omega spin:** integrator `applyAngularVelocity()`
  (`llviewerobject.cpp:7069-7092`), called per-frame at `:2503` (NOT `:2365`, which
  merely combines the server base rotation with `mAngularVelocityRot`). Scale **only
  the dt argument** to `applyAngularVelocity()`; do NOT touch the shared `dt` at
  `:2496-2513` (that drives simulator-derived linear extrapolation = Pillar 2).
  Scale `presentation_delta` capped at the uniform 0.25s subsystem hitch cap (§3),
  so spin stays synced with animation under the cap and never takes a huge single
  rotation step.
- **Object-path drives (props):** `dt_raw` from `llappviewer.cpp:5308-5322`,
  consumed at `alobjectpathmover.cpp:203-207` → advance on `presentationDelta()`.
- Flexi (`llviewerobjectlist.cpp:1009-1010`) and simulator linear interp/extrapolation
  are separate domains — **unsupported in Landing 1** (documented).

### 4.3 Texture animation — `TemporalDriveTextureAnim`
- `LLViewerTextureAnim` has an instance timer + accumulated state (`mLastTime`,
  `mLastFrame`; ctor `:37-47`, `reset()` `:63-67`, smooth vs total-elapsed `:126-135`,
  loop/pingpong/reverse/quantize `:137-185`). **Do NOT** switch to a shared global
  `presentationTime()*rate` (syncs all anims, jumps on enter/leave). Add a
  **per-instance presentation epoch/phase**: when driven, advance phase by
  `presentation_delta`; rebase the epoch on enter/leave temporal, `reset()`,
  parameter/rate change, pause, and gate flips so phase is continuous. **Smooth mode
  preserves accumulated phase** (`mLastTime`). Gate OFF = existing wall path,
  byte-identical. **Zero-rate policy:** when `mRate == 0`, route through the stock
  branch (`elapsed * 0 == 0` on both the driven and wall paths) — no phase to
  preserve, no jump. `reset()` clears the accumulated phase **only when driven**
  (stock reset stays byte-identical). Scripted texture SWAP = discrete event, Pillar 2.

### 4.4 Particles — `TemporalDriveParticles`
- Single `dt` at `LLViewerPartSim::updateSimulation()` (`llviewerpartsim.cpp:663-665`),
  fed to sources (`:788-791`) and groups. **Single-owner rule:** apply scale **once,
  here**, by replacing that `dt` with the frozen **`presentation_delta`**
  (`= wall_delta*scale`) capped at the uniform 0.25s subsystem hitch cap (§3) — the
  sim can't integrate one huge step. Everything downstream is left
  **structurally unchanged**, now in presentation units: `visirate` multiply
  (`:812-829`), `mSkippedTime += dt` (`:838-840`), re-add in `updateParticles()`
  (`:276-296`). No second scale anywhere → no new double-count.
- Reset the private `update_timer` each iteration even when driven (call
  `getElapsedTimeAndResetF32()` and ignore its value) so gate-flip back to wall time
  doesn't see a huge accumulated delta.
- **Fast scale:** the driven path uses `presentation_delta` **verbatim** (no
  per-consumer cap), so particles stay in lockstep with skeletal/texture/object
  motion at every scale. On a stall a large step is the honest fast-forward semantics
  and is shared coherently by all consumers (gait included); the particle system's
  own global count caps bound the burst. A multi-substep debt engine is DEFERRED (a
  later-pillar refinement, not needed for the slow-mo use case).
- **0×:** call `updateSimulation` with zero presentation delta (do NOT skip — skipping
  reproduces Freeze World's clamp jump). Freeze World's own particle skip
  (`llappviewer.cpp:5424-5436`) stays intact and separate.

### 4.5 (reserved — linear/flexi object motion) — Pillar 2, not wired.

### 4.6 Camera — `TemporalDriveCamera` (DEFERRED in Landing 1)
- Cinematic Camera phase is wall-driven (`llcinematiccamera.cpp:1187-1204`) and
  **divides by dt** at `:1357-1368` with a `0.0005f` floor at `:1202-1204`. Correct
  0× handling needs a mid-function early-out that holds camera phase and skips the
  operator-velocity divides, and framing a still-sim-moving target overlaps Pillar 2.
- **Landing-1 decision:** DEFER camera driving. The camera is OFF by default anyway
  (the recommended mode is a real-time camera over a slowed world — temporal
  isolation, §18.1), so the DEFAULT experience is unchanged. The `TemporalDriveCamera`
  setting exists (so the mask/enum is complete and a follow-up just wires it) but the
  panel checkbox is **disabled** with a "stays real-time in this version" tooltip —
  honest, not a silent no-op. No camera consumer is hooked this landing.
- **Flycam Recorder is its own time domain** (authored playhead/speed
  `llflycamrecorder.cpp:569-575`); NOT under this gate, now or later-by-default.

## 5. Freeze World reconciliation (coexist — pass-1 MUST-2, pass-2 SHOULD-2)
Freeze World (`BDMergeFreezeWorld`/`UseFreezeWorld`) is broader than zeroed clocks:
per-character pause handles (`llviewercontrol.cpp:1025-1033`), `FreezeTime`
(`:1034-1037`) halting non-avatar `idleUpdate`/flexi/texanim
(`llviewerobjectlist.cpp:986-1017`) and drawable movement (`pipeline.cpp:2662-2772`),
object-path stop (`alobjectpathmover.cpp:193-207`), particle skip
(`llappviewer.cpp:5424-5436`), pelvis-follow suppression (`llvoavatar.cpp:4716-4725`),
non-refcounted snapshot/360 ownership (`llfloatersnapshot.cpp:240-275,1005-1014`,
`llfloater360capture.cpp:805-849`), and `UseFreezeWorld` is a single **shared
boolean** (`llviewercontrol.cpp:1021-1045`).
- **Leave it entirely intact.** Temporal 0× means "cinematic presentation clocks
  paused"; the UI says so and it is NOT advertised as, nor implemented via, Freeze
  World. **No temporal-owned toggle of `UseFreezeWorld`** (a shared bool can't be
  independently owned — the pass-2 correction). A user who wants a true world freeze
  uses the existing Freeze World UI.

## 6. UI — shared panel `ALPanelTemporalCapture`
- **Mode** combo: Live / Manual scale (future modes greyed "coming soon").
- **World Speed** slider, LOG detents: 0 (Freeze) · .01 .02 .05 .1 .2 .25 .5 .75 ·
  1.0 (Realtime) · 1.25 1.5 2 4 8. Zero = pause.
- **Effective speed** + **Recommended post speed** (`1/scale`) readouts.
- **Drive** checkboxes: Animation & actor motion · Object motion · Texture anim ·
  Particles · Camera(off) — bound to the `TemporalDrive*` settings.
- **Freeze** (0×) and **Realtime** (1×) slider shortcuts. (No Freeze-World toggle —
  pass-2 SHOULD-2; a tooltip notes temporal 0× pauses cinematic clocks only, and
  points to the snapshot floater's Freeze World for a full world halt.)
- Status line: mode · effective scale · paused.
- Every control **settings-backed** (`control_name=`) so floater, Console tab and
  scene-save share ONE state. Injector: `static LLPanelInjector<ALPanelTemporalCapture>
  t_panel_temporal_capture("panel_temporal_capture");`. Bind children by `name`;
  never rename.

## 7. Ship-WHOLE checklist
- [ ] `llpresentationtime.{h,cpp}`, `lltemporalframecontext.h`,
      `alpaneltemporalcapture.{h,cpp}`, `alfloatertemporalcapture.{h,cpp}`
- [ ] `panel_temporal_capture.xml`, `floater_temporal_capture.xml`
- [ ] Tick `beginIteration()`+`freezeFrame()` at `llappviewer.cpp:5011-5017`
      (start of idle, monotonic timer)
- [ ] Consumer hooks §4.1–4.6, each behind its `TemporalDrive*` gate; OFF =
      byte-identical
- [ ] `settings.xml`: `TemporalMode, TemporalWorldScale, TemporalOutputFPS,
      TemporalDriveAnimation, TemporalDriveObjects, TemporalDriveTextureAnim,
      TemporalDriveParticles, TemporalDriveCamera`. Defaults = today.
- [ ] `llviewerfloaterreg.cpp`: add `"temporal_capture"` (idiom `:523`)
- [ ] Director Console: **"Temporal"** tab in `floater_director.xml` embedding
      `class="panel_temporal_capture"`; refresh hook in `llfloaterdirector.cpp`
- [ ] Menu entry (the machinima/Director menu that opens `director`/`actor_mover`)
- [ ] Scene-save: add ONLY `TemporalMode, TemporalWorldScale, TemporalDrive*` to
      `sceneSettingsList()` (`llfloaterdirector.cpp:543-574`). NOT effective scale,
      paused, timestamps, `UseFreezeWorld`/`FreezeTime`.
- [ ] `CMakeLists.txt`: 4 new .cpp + 2 new headers
- [ ] Teardown/exit restores `sGlobalTimeFactor=1`; no lingering presentation state

## 8. Deferred to later pillars (explicit non-goals)
**Per-consumer sub-stepping for exact large-delta coupling.** The position/emission
consumers cap at 0.25s per frame (they can't integrate one huge step). So whenever
the per-frame `presentation_delta` exceeds 0.25s — extreme fast scale, low frame
rate, OR a genuine stall at any scale (a wall frame > 0.25/scale) — position
consumers cap while gait/texture consume the full delta → a one-frame gait↔traversal
gap. This is **no worse than stock**, which caps actor traversal at 0.25 with
uncapped gait too (and the scale shrinks gait's step, so slow-mo desyncs ≤ stock).
Exact coupling here needs looping each consumer's advance in ≤0.25s chunks
(multi-boundary ping-pong, etc.); risky in the delicate path code and unnecessary for
the slow-mo use case.

Other deferrals: buffered server-driven motion (physical/scripted prims, remote avatars), linear
extrapolation slowing, flexi, texture-swap/property events, fixed-frame capture,
recorded takes, adaptive-FPS scaling, motion blur sub-sampling. `LLTemporalMode`
reserves the enum values; behavior is not wired and not selectable.

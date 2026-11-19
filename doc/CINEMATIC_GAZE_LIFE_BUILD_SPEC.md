# Cinematic Gaze — Life, Feel & Emotion: Authoritative Build Spec

Status: **BUILD SPEC** — the single source of truth for implementation.
Supersedes, where they conflict, both:
- `doc/CINEMATIC_GAZE_LIFE_DESIGN.md` (original design)
- `C:\Users\xianw\Documents\Codex\2026-08-20\i-alchemy-machinima-doc-cinematic-gaze\outputs\CINEMATIC_GAZE_LIFE_DEEP_RESEARCH.md` (deep-research companion + technical audit)

Where a detail below differs from the original design doc, **this spec and the Codex companion win.**
The design doc keeps its value as rationale/context; this doc is *what we build*.

North Star (reframed per companion §15):
> Actor Gaze produces **repeatable, directable performances** through coordinated gaze motor programs
> sampled from a **professional continuity core**. It borrows measured human timing where evidence is
> strong, uses explicit cinematic abstractions where directability matters more than physiology, and
> never confuses random motion with life. The result is **smooth, lifelike, robust, lively,
> professional** — and its continuity core is general enough to later lift the Poser.

Deepest principle — **separation of concerns**:
- **Intent** decides *why/where* the actor looks.
- **Motor programs** decide *which parts move and when*.
- **Trajectories** decide *how each channel crosses time*.
- **Rendering/capability** decides *which rig channels can safely express it*.

---

## 1. Architecture (five layers)

```
Director cues / target stream / dialogue role / Affect
        │
        ▼   [Layer 1] Gaze Intent Planner      → fixate, pursue, avert, scan, react, reorient
        ▼   [Layer 2] Motor Program Builder     → eye saccade, head follow, VOR recenter,
        │                                          recruitment, blink scheduling, durations
        ▼   [Layer 3] Trajectory Segment Core    → closed-form p/v/a sampling, C2 handoff   (= ALTrajectory)
        ▼   [Layer 4] Anatomy + capability compositor → eyes/head/neck/torso/hips/lids/brows
        ▼   Render pose
```

Layer 3 (`ALTrajectory`) is **general and gaze-agnostic** — the reusable core (Poser's future
consumer). Layers 1–2 (`ALGazeMotor`) own biological + performative policy. Layer 4 is the
capability-aware apply/restore in the actor mover.

---

## 2. Module contracts

### 2.1 `ALTrajectory` — general continuity core  *(CRITICAL → Fable build + Codex review)*

Location: `indra/newview/altrajectory.h` (header-only, matching the `algazemath.h` convention;
register in the source list near `CMakeLists.txt:1174` and add a TUT via `LL_ADD_INTEGRATION_TEST`
near `:2625`). **Zero** dependencies on gaze, anatomy, affect, RNG, or viewer joints. `F64` time.

Segment (per companion §10.1):
```cpp
template<typename T> struct ALTrajectorySegment {
    F64 mStartTime; F64 mDuration;
    T mP0, mV0, mA0;          // start pos/vel/accel
    T mP1, mV1, mA1;          // end   pos/vel/accel
    ALTrajectoryProfile mProfile;   // Quintic (default); pluggable for future eye profile
    U64 mSourceEventId; U32 mBehaviorVersion;
};
```

Required API:
- **Generalized quintic Hermite** sampling with position, velocity, AND acceleration, using the basis
  in companion §3.3:
  ```
  h00=1-10u³+15u⁴-6u⁵   h10=u-6u³+8u⁴-3u⁵   h20=0.5(u²-3u³+3u⁴-u⁵)
  h01=10u³-15u⁴+6u⁵     h11=-4u³+7u⁴-3u⁵    h21=0.5(u³-2u⁴+u⁵)
  p(u)=h00 p0 + h10(T v0) + h20(T² a0) + h01 p1 + h11(T v1) + h21(T² a1)
  ```
  Provide `v(u)`, `a(u)` as exact analytic derivatives (÷T, ÷T²).
- **C2 interruption handoff:** `sample(old_segment, now) → (p0,v0,a0)`; build new segment from that
  triple. Settling look uses `v1=0,a1=0`; predicted moving target may use terminal `v1`. (This is C2,
  not C3 — acceptable.)
- **Duration feasibility / overshoot pass** (companion §3.4): construct → check velocity roots / grid
  → if `monotonic` flag set and it reverses/overshoots, lengthen `T` and retry (capped) → fall back
  to a two-stage **brake-then-go** program.
- **Shortest-arc scalar adapter** for yaw/pitch: unwrap across ±π, stable convention, tests at
  +179°↔−179°.
- **Determinism:** pure closed-form of `(mStartTime,…,T)` sampled by absolute time; serialize +
  `mBehaviorVersion`. No internal state, no RNG, no time source of its own.
- **Do NOT** solve general quaternion trajectories in Phase 1, but structure the API so a quaternion
  (log/exp + angular velocity) channel can be added without a rewrite. No Euler-only assumptions baked
  into the name/structure.

Naming: `ALTrajectory` / `ALMotionRamp` — **never** `MinimumJerkGaze`. Quintic is a *motion-design
law*, not a claim about human saccades.

Acceptance tests (companion §13.1): endpoint p/v/a exact; finite for 0/negative/huge `T`;
sample-before/after clamp; retarget at 10/50/90 % progress; **C0/C1/C2 continuity at interrupted
handoff**; +179↔−179 shortest arc; monotonic mode no overshoot; non-monotonic permits requested
overshoot; frame-rate samples lie on one analytic curve.

### 2.2 `ALGazeMotor` — gaze motor-program policy  *(mixed; blink arbitration + recruitment + VOR = Fable+Codex; the rest Opus)*

Location: new `indra/newview/algazemotor.h`/`.cpp` (may stay header-heavy to match TUT pattern).
Owns:
- **Eye duration (main sequence, pluggable, population-prior):**
  `D_eye_ms = clamp(base + perDeg*amplitude_deg, base, 110)`, initial `base=25`, `perDeg=2.8`
  (companion §3.5). Marked tunable; profile pluggable so a skewed data-calibrated eye profile can
  replace the quintic for eyes later without touching the segment/event architecture.
- **Head latency policy, NOT a constant** (companion §3.6):
  `head_latency = base(≈40ms) + f(amplitude, initial_eye_eccentricity, predictability, modality,
  intent, head_mover_trait)`. Director can override.
- **VOR recenter by construction** (companion §10.5): do NOT key eye recenter to "head progress×0.7."
  Each sample, **solve eye-in-head from desired world gaze and sampled head orientation**, then
  soft-limit. VOR counter-rotation falls out while eyes stay on target. `DirectorGazeRecenter` sets
  how far the head ultimately aligns, never breaks the world-gaze equation.
- **Modes** (companion §3.7): `Fixation/discrete retarget`, `Pursuit` (2nd-order follow on
  recorded/deterministic target curve + predicted-error catch-up saccade), `Hybrid camera` (pursue
  smooth camera, promote cuts/large reframes to discrete, suppress micro-life during the ballistic
  shift). Pursuit is stateful → seekable only via recorded input or checkpoint (see §2.4).
- **Soft anatomical recruitment** *(CRITICAL)* (companion §4.2): compact **C2 `smoothExcess`**
  (integral of quintic smootherstep across a band `b`), exact `min`/residual fallback at `b=0`,
  angle-conserving. Replaces the hard knee at `algazemath.h:634-653`. Also account for **initial eye
  position** in later pass (companion §4.3). Torso/hips driven by coordinated programs; delete dead
  `mTorsoAim*` (`llactormover.h:951-952`).
- **Fixation / micro-life layers** (companion §6): distinct **fixation relocations**, **microsaccades**
  (optional, screen-gated), **drift** (deterministic band-limited value noise — hash adjacent time
  cells, C2 smootherstep interp, per-channel cell durations, small 2nd octave, zero-mean; **not** the
  fixed-sine Lissajous). **Tremor: not implemented, not a style knob.** Screen-space/shot-scale gating
  by projected eye size → Wide/Close/ExtremeClose tiers (companion §6.3). Keep the existing
  closed-form seed pattern (`castSeedFromUUID`, presentation time).
- **Blink scheduling** *(CRITICAL arbitration)* (companion §7): three sources —
  **spontaneous** (hashed cadence + refractory), **gaze-evoked** (probability rises with gaze-shift
  amplitude/head peak velocity — a smooth curve, NOT every saccade), **authored/cue** (never forced if
  it collides). Asymmetric close/open segments (`close 70–110ms`, `hold 0–35ms`, `open 120–220ms`;
  defaults 85/– /165), partial+full depth distribution, bilateral coordination (tiny asymmetry only —
  never independent winks). Blink rate = `base × task × visual_demand × style` (arousal is ONE input
  to style, not the law).

### 2.3 Intent + Affect  *(Opus build; presets are authored bundles)*

- **Affect (authoring space, keyframeable):** `{ valence[-1,1], arousal[0,1], dominance[-1,1] }`
  (Mehrabian–Russell PAD). Does **not** directly drive motor channels.
- **Intent State (new, interposed)** (companion §8.2):
  ```
  Intent = { engagement[0,1], vigilance[0,1], cognitiveLoad[0,1],
             dialogueRole{silent,listening,speaking,thinking},
             targetRelation{camera,ally,stranger,threat,object},
             deliberateness[0,1] }
  ```
- **Couplings go through the evidence-audited table** (companion §8.3) — arousal is a *modest style
  gain after task/dialogue factors*; valence↔approach/avert is replaced by relationship+role+dominance
  (hostile can stare); tremor coupling **removed**; vergence emotion is an **explicit cue only**.
- **Named postural styles** instead of signed dominance→pitch: `lofty, predatory_downlook, guarded,
  submissive, open` (companion §8.7).
- **Mutual gaze** = conversation rhythm (listening looks longer at speaker; speaking looks away,
  returns near turn end; hostile lengthens holds but keeps blinks/relocations) (companion §8.4).
- **Face scan** = weighted anchors (near eye / eye-midpoint / mouth / face-center) by role, screen
  size, head orientation, trait, with inhibition-of-return; collapse to face-center when anchors
  aren't screen-readable; director can pin an eyeline. **Not** a fixed eye-eye-mouth cycle
  (companion §8.6).
- **Thinking aversion** = cue/intent framed as cognitive-load management; direction chosen by
  blocking, **not** a memory-access code (companion §8.5).
- Presets are **documented authored bundles** mapping to Intent + motor style (companion §11.3), not
  raw VAD formulas.

### 2.4 Determinism / event model  *(CRITICAL → Fable+Codex)*

Four distinct properties (companion §5.1) — do not conflate: deterministic *sampling*, frame-rate
independence, deterministic *construction*, arbitrary-seek reconstruction. Closed-form segments give
the first two only.

- **Event-sourced** `GazeIntentEvent` stream (companion §5.2) is the source of truth; runtime segment
  ring is only a cache.
  ```cpp
  struct GazeIntentEvent { U64 event_id; F64 presentation_time; GazeIntentType type;
      TargetDescriptor target; GazeStyle style; AffectState affect; IntentState intent;
      U32 behavior_version; U64 deterministic_seed; };
  ```
- **Seek** = load last checkpoint ≤ seek time → replay later events in timestamp order → rebuild
  active motor programs → sample at seek time. Checkpoints at cue boundaries + periodically in long
  pursuit.
- **Three honest modes** (companion §5.3), surfaced in UI/tests: **Live** (responsive, no back-seek
  guarantee), **Recorded target** (target samples in the presentation stream; deterministic replay),
  **Authored timeline** (analytic; deterministic replay). "Render-only" ≠ "scrub-safe."
- **Behavior versioning** (companion §5.4): persist a behavior version per shot/project; a change to
  hysteresis/hash constants/duration calibration/recruitment bands changes all later segments — detect,
  don't silently reinterpret.
- **Dead-zone replacement** (companion §10.4): split into **retarget hysteresis** (enter/exit
  thresholds + dwell to prevent segment thrash — reuse the 3°/120ms Director values at
  `llactormover.cpp:3147-3151`) and a **pursuit noise filter** (low-pass only the measured target
  curve, checkpointed/recorded). No hard angular stick on the driven body aim.

### 2.5 Capability tiers + ownership  *(Opus, with careful arbitration)*

- **Tiers** (companion §9.2): **0** universal (`Blink_Left/Right` VP + default eye/head/body joints) —
  the guaranteed cross-avatar baseline; **1** Bento opt-in (upper/lower lid, brow/forehead bones *when
  present, owned, and not driven by another anim*) — must arbitrate priority; **2** known head profile
  (best, non-portable). **Do not auto-drive Tier 1 just because a joint name exists.**
- **Ownership discipline** (companion §9.3), extend the existing capture→apply→restore path
  (`llactormover.cpp:3427-3435,3497-3519,5093-5111`): capture native, apply only while owned, restore
  before normal update + on teardown/scrub, handle priority/AO conflicts, never leave a face distorted.
- **Lid composition order** (companion §7.4) — replace the current `max(...)` combine with an
  intent-preserving function: (1) gaze-position lid-follow → (2) emotional aperture posture →
  (3) blink override toward closure → (4) cue widen/narrow (can't block an authored blink unless asked)
  → (5) per-eye asymmetry. Base feature promises **gaze performance + aperture style**, not general
  facial emotion.

### 2.6 Actor Mover integration  *(Opus; the Phase-1 applied-stage swap is delicate)*

Replace the applied-aim stage (`llactormover.cpp:4657-4712`, the copy/rate-limit/snap) with a sample
of the coordinated motor program, **behind the master gate**; feed presentation-time events / recorded
curves; combine with cue weights + priorities (existing `priority_env` ramp is fine); apply
joints/visual params; capture/restore. Micro-life stays **post-smoothing additive** as today
(`llactormover.cpp:4744`).

---

## 3. Control surface (companion §11)

Director-facing = **performance concepts**: Gaze style, Engagement, Tempo, Head alignment, Contact
style, Life, Blink style, Affect (keyframeable). Advanced/global (debug) keys: `DirectorGazeMotionPrograms`
(master gate), `DirectorGazeEyeDurationBaseMs`=25, `…PerDegMs`=2.8, `DirectorGazeHeadLatencyMs`=40,
`DirectorGazeSoftRecruitDeg`=0 (test 3–8), pursuit freq/catchup (perceptual tune),
`DirectorGazeBlinkCloseMs`=85, `…OpenMs`=165, `…BlinkPartialChance`, `DirectorGazeMicroScreenGatePx`.
**No** single global `MainSeqGain` driving both eyes and head. Keep existing knobs (`DirectorGazeBlinks`,
`LidFollow`, `CameraRoll`, `Exaggerate`, priority).

**Compatibility gating** (companion §10.6): one master `DirectorGazeMotionPrograms` gates ALL new
segment/motor behavior; when off, new math is not even evaluated on applied channels; per-feature
defaults may be "on *inside* the master" but cannot touch the legacy path; persist behavior version so
old shots stay legacy while new projects opt in after burn-in. **Reorganize the original doc's mixed
defaults (Trajectory off / microsaccades+facescan on) under this one master.**

---

## 4. Phase plan (companion §12) + build orchestration

> **SINGLE COMBINED IMPLEMENTATION — tested as one complete system.** The "phases" below denote
> build **ownership and sequence only** (who writes what, and what is critical → Fable). They are
> **not** incremental in-world test checkpoints. All code is written, then **integrated once and built
> combined** (same model as the prior cine feature stack: build combined, don't relink every phase),
> and the whole system is validated as a unit — legacy master **off** must stay byte-identical
> throughout, and the acceptance criteria in §5 are the single gate applied to the finished system.
> Unit/TUT tests still run per-module as they're built (they're viewer-independent); the *system*
> test is one complete in-world pass at the end.

Orchestration rule (user directive): **Opus subagents** for general work; **anything critical → Fable
build + Codex review.** Critical = the trajectory math, C2 handoff, determinism/event model,
soft-recruitment, blink arbitration, VOR eye-in-head solve. General = instrumentation, settings
plumbing, UI, presets, glue.

| Phase | Content | Who | Viewer-independent? |
|---|---|---|---|
| **0 — Instrument** | Debug trace capture: presentation time, target/applied yaw-pitch, per-joint out, finite-diff v/a; record the standard shot battery (companion §12 Phase 0). Instrument BEFORE changing motion. | Opus | Needs viewer to record, but code is additive |
| **1 — ALTrajectory core** | §2.1 in full + unit tests. Isolated, no motion change. | **Fable + Codex** | **Yes** (pure math + TUT) |
| **2 — Soft recruit + torso** | §2.2 recruitment; C2 `smoothExcess`; band-0 exact; delete `mTorsoAim*`; coordinated torso/hips. | **Fable + Codex** (recruit math) / Opus (wiring) | Recruit math yes; wiring no |
| **3 — Gaze motor programs** | §2.2 modes: main-sequence eye (pluggable profile), head latency/alignment policy, VOR eye-in-head solve, pursuit + catch-up. | **Fable + Codex** | Partly |
| **4 — Blink/lid** | §2.2 blink schedulers + arbitration; asymmetric close/open; partial depth; lid-follow-first composition; ownership/restore tests on legacy + mesh heads. | **Fable + Codex** (scheduler) / Opus (apply) | Scheduler yes |
| **5 — Intent + Affect** | §2.3 Intent State, presets, evidence-audited couplings; Affect keyframeable *only after* the event model is stable; validate each micro-channel perceptually — don't add all at once. | Opus | Partly |
| **6 — Bento/head tier** | §2.5 capability/profile detection, eyelid/brow ownership arbitration, opt-in profiles, safe Tier-0 fallback. | Opus | No |

Integration order constraint: build **ALTrajectory core (Phase 1) in isolation first** (changes no
motion), and **instrument (Phase 0)** before wiring the core into the live applied stage. Keep the
"one viewer build/link at a time" discipline; standalone TUT targets don't need the viewer link.

---

## 5. Hard acceptance criteria (gate every phase) (companion §13.6)

- Master **off** → **no changed pose values** in covered golden traces (byte-identical legacy).
- Seek vs forward render → channel diff ≤ declared epsilon at every sampled frame.
- **No velocity discontinuity** at retarget beyond finite-diff tolerance; **no acceleration
  discontinuity** at a C2 retarget beyond tolerance.
- Soft recruit **band 0 → exact old output**.
- No segment/event growth proportional to render frames during a fixed hold.
- Disable/teardown restores all owned joints/visual params (no distorted face after scrub).
- **No true-tremor channel ships.**
- No preset can force a permanent no-blink stare unless explicitly named + warned as stylized.
- Blinded A/B vs legacy: new version must improve smoothness + life **without** reducing target
  clarity or increasing distraction ("more movement" doesn't win by default).

---

## 6. Product decisions — DECIDED (2026-08-20)

1. **Scrub contract**: **LIVE-ONLY for v1.** No event-sourced/checkpoint replay layer now. The
   motor/trajectory retarget layer runs live (responsive, no backward-seek guarantee). The closed-form
   layers (blink timing, drift, microsaccade — pure functions of presentation time + seed) stay
   scrub-safe as-is. Recorded-target/event-sourcing is a LATER addition, not built now.
2. **v1 expression ceiling**: **TIER 0 (aperture-only).** Lids via the existing `Blink_Left/Right`
   closure channel only (`llactormover.cpp:5093-5111`). No Bento brow/lid-bone detection or
   animation-priority arbitration in v1.
3. **Affect authoring**: **single per-actor Affect for v1.** valence/arousal/dominance set per actor;
   timeline-keyframeable Affect is a later addition.
4. **New default**: defer — settle `DirectorGazeMotionPrograms` default at the end after burn-in.

## 6A. ALGazeMotor — assembly contract (v1: live, tier-0, per-actor affect)

The serial assembly layer. A mostly-pure, unit-testable module `ALGazeMotor` (new
`indra/newview/algazemotor.h`/`.cpp`) that owns the per-actor motor STATE and a per-frame `step()`,
consuming the validated leaves + `ALTrajectory`. The `llactormover` integration (§2.6) calls `step()`
and applies its outputs; keep viewer-specific concerns (joint lookup, apply/restore, priority, cues)
in the integration, not in `ALGazeMotor`.

**State** (`GazeMotorState`, per actor, persists across frames — live mode, so carrying state is fine):
per-channel `ALTrajectory::ScalarProgram`/segment + last-sampled `(p,v,a)` for eye_yaw, eye_pitch,
head_yaw, head_pitch, head_roll, neck_yaw, neck_pitch, torso_yaw, torso_pitch; last committed target;
retarget hysteresis state (reuse 3°/120ms from `llactormover.cpp:3147-3151`); nothing for blink (it
stays closed-form of time+seed).

**Input** (`GazeMotorInput`, per frame): desired world gaze direction; current head world orientation
(for VOR); presentation time (F64) + dt; per-actor seed; `AffectState {valence,arousal,dominance}`;
settings (durations, latencies, comfort cone, micro amplitudes, master gate); active cue/priority
weights passed through from the integration.

**Output** (`GazeMotorPose`): chain aim yaw/pitch to distribute; per-joint recruited angles OR the
inputs for `distributeAnatomicalChain`; eye-in-head yaw/pitch (post-VOR, post-recenter); head roll;
lid closure L/R in [0,1]. The integration maps these onto joints + the `Blink_*` visual params.

**Per-frame `step()` algorithm:**
1. **Retarget detection** — if desired target moved beyond the enter-hysteresis (and dwell), emit new
   trajectory segments: eye channels via `ALTrajectory::retarget` with `ALGazePolicy::eyeSaccadeDurationMs`;
   head channels delayed by `ALGazePolicy::headLatencyMs` with a longer duration; neck/torso staggered
   further. Eyes lead, head follows. Sub-hysteresis → keep programs (no thrash).
2. **Sample** each channel's program at presentation time → base chain aim (`p`), carrying `v/a` for
   the next C2 handoff.
3. **Soft recruit** the chain aim across eye→head→neck→torso→hips via `ALGazeRecruit::recruitChain`
   (band from `DirectorGazeSoftRecruitDeg`, default 0 = legacy hard knee).
4. **VOR** — `ALGazePolicy::eyeInHeadFromWorldGaze(desired_world_dir, head_world_rot, comfort…)` so the
   eyes stay on the world target as the head rotates in; recenter falls out. Never key to a constant.
5. **Micro-life** — add `ALGazeNoise::driftOffset` + `microsaccadeOffset` to eye/head, POST-smoothing
   additive (same ordering as `llactormover.cpp:4744`), amplitude scaled by arousal.
6. **Blink/lid** — schedule blinks closed-form (spontaneous cadence from time+seed + gaze-evoked on
   large retargets, refractory); shape via `ALGazeBlink::blinkClosure` (asymmetric); fold into lid
   closure; affect sets aperture posture (arousal→widen, low-valence→droop) and blink rate.
7. **Affect tempo** — arousal shortens trajectory durations (within clamps); dominance biases head
   tilt/contact (contact rhythm is a later add).

**Unit-testable (no viewer):** feed synthetic frame sequences and assert — eyes-lead-head ordering on
a retarget; velocity-continuous chain output across a retarget (no jump); recruitment continuity;
retarget hysteresis prevents thrash on jitter; blink scheduling fires + is deterministic; master-gate
off ⇒ passthrough. `AffectState`/`GazeMotorState`/input/output structs are POD.

**Build orchestration:** `ALGazeMotor` = CRITICAL assembly → Fable build + Codex review. The
`llactormover` integration + settings + UI (§2.6, §3) = Opus (integration judgment) / Sonnet (plumbing).

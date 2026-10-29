# Cinematic Gaze — Life, Feel & Emotion Deep-Dive

> ⚠️ **SUPERSEDED FOR IMPLEMENTATION.** Build from `doc/CINEMATIC_GAZE_LIFE_BUILD_SPEC.md`, which
> merges this design with the Codex deep-research audit (`…/CINEMATIC_GAZE_LIFE_DEEP_RESEARCH.md`).
> Where this doc and the build spec differ, **the build spec wins** (notably: the core is
> `ALTrajectory` not "minimum-jerk"; the handoff carries acceleration for C2; scrub-safety needs an
> event/checkpoint model, not closed-form segments alone; tremor does not render; Affect routes
> through an Intent layer). This doc remains valid as rationale/context.

Status: **DESIGN** (nothing built). Branch: `feature/cine-light-rig`.
Two intertwined goals:

1. **Fluidity backbone** — kill the residual jerkiness by replacing the current mix of smoothing
   laws with one velocity-continuous **keyframe-ramp trajectory system** running behind the scenes.
2. **Life / feel / emotion** — a single **Affect Vector** driving physiologically-coupled channels
   (saccades, eyes-lead-head cascade, gaze rhythm, emotional lids/blink) that all ride on the
   fluidity backbone.

Everything is render-only, presentation-time, scrub-safe. Defaults reproduce today's look exactly.
All `file:line` anchors verified against the current tree.

---

## ★ North Star — the end goal

**Professional, smooth keyframing.** The bar for Actor Gaze is not "good for SL" — it is
**smooth, lifelike, robust, lively, and professional**: motion that a film animator would sign off
on, with no jerk, no pop, no dead-eye stare, and a clear sense of a mind behind the eyes. Every
decision in this doc is measured against that bar.

The mechanism that gets us there is a **real keyframing engine running behind the scenes** — closed-
form, velocity-continuous trajectory segments (§Part 1). That is deliberately more than a gaze fix:
it is a general, deterministic, scrub-safe keyframe/interpolation core. **Down the road the same core
can lift the Poser** — the Poser today sets joint rotations directly (hard poses); giving it these
professional min-jerk / eased-ramp transitions between poses would make posing-to-posing motion
buttery in exactly the same way. So we build the backbone clean and general now, with Actor Gaze as
its first consumer, and keep the door open for the Poser to become its second. **For now, though, the
focus is Actor Gaze: smooth, lifelike, robust, lively, professional.**

---

## PART 0 — Root cause of the jerkiness (why "keyframe ramp" is the right instinct)

The per-frame solve `gazePaint()` (`llactormover.cpp:4177`) runs **five temporal stages, each with a
different smoothing law, and they don't share a filter or any velocity state.** The channels that
*feel* smooth are upstream; the channel that actually drives the joints is mostly a **hard cut**.
Four concrete jerk sources:

1. **Law-switching on the applied aim** (`llactormover.cpp:4657-4712`). In the normal case
   `mAppliedYaw/Pitch` is **not filtered — it copies the target every frame** (`:4707-4710`).
   Smoothing only engages past a 90° discontinuity, and then it's a **linear deg/sec rate limiter**
   (480°/s, `:4684`), and the last ≤1e-5 rad **snaps** (`:4698-4704`). So a large retarget produces
   *two* velocity discontinuities (C1 breaks): exp-chase → constant-velocity ramp → snap.
2. **Stateless hard-knee anatomical allocation** (`algazemath.h:634-653`). `distributeAnatomicalChain`
   fills each joint to its clamped capacity before recruiting the next (eye → head → neck → torso).
   As the aim sweeps past a joint's cap, that joint's angular *velocity* has a corner. The chain
   carries **no history**, so the corner passes straight to `setRotation` (head `:4979`, neck `:4925`,
   etc.); nothing re-smooths it.
3. **Dead-zone stick/slip** on the body aim (`llactormover.cpp:4632`, `deadZoneChase`, 3° default).
   Sub-threshold the aim is frozen; at the edge it resumes with a velocity step → micro stick/slip on
   small target motion.
4. **Torso has no dedicated filter.** `ALGazeMath::torsoChase()` (`algazemath.h:48`) and the state
   `mTorsoAimYaw/Pitch` (`llactormover.h:951-952`) exist and are seeded once (`:4663-4664`) **but are
   never chased or read again** — dead code. The torso inherits (1)+(2).

Crucially: **there is no spring/velocity (`SmoothDamp`) helper anywhere, and no `mVelocity` state on
`Gaze`.** Every smoother is a first-order exponential lag (`chaseAlpha = 1-exp(-dt/tau)`,
`algazemath.h:26`) or a linear/`smoothstep` envelope. First-order lag *cannot* be velocity-continuous
across a target change — its velocity jumps to `Δ/tau` the instant the target moves. That is the
textbook signature of the jerk.

**The fix is exactly what you described:** stop chasing/copying, and instead lay down an eased
**keyframe ramp** (a closed-form trajectory segment) that is continuous in position *and velocity*,
sampled by presentation time. That is both the fluidity cure and, conveniently, the biological model
of how eyes and heads actually move.

---

## PART 1 — The fluidity backbone: keyframe-ramp trajectory system

### 1.1 Concept

> This is the **professional keyframing core** the North Star calls for. Design it as a standalone,
> channel-agnostic module (`ALGazeMath`/a small `algraph`-style unit) that knows nothing about eyes
> specifically — it interpolates *any* scalar/rotation DOF with velocity-continuous, deterministic,
> scrub-safe segments. Actor Gaze is consumer #1; the Poser is the intended consumer #2. Keep it
> free of gaze-only assumptions so that reuse costs nothing later.

Replace the hard-cut applied stage (jerk source #1) and give every driven channel — eye yaw/pitch,
head, neck, torso, hips, and the lid aperture — a **trajectory buffer**: a tiny ring of **closed-form
motion segments**. Each segment is defined by `(t0, p0, v0, p1, v1, duration, easing)`. Per frame we
*sample* the active segment at absolute presentation time; we never integrate a running filter.

Two segment shapes, picked by transition type:

- **Minimum-jerk ramp** — for discrete gaze shifts (a retarget, a saccade, a cue). The 5th-order
  minimum-jerk polynomial is the *measured* model of human eye/head/reach movement (the CNS minimizes
  jerk). With boundary conditions `p(0)=p0, p(T)=p1, v(0)=v0, v(T)=0, a(0)=a(T)=0` it is a unique
  quintic. It starts at the *current* velocity (so it hands off smoothly from whatever was playing)
  and lands with zero velocity and zero acceleration — no corner at either end.
- **Critically-damped follow** — for smooth *pursuit* of a continuously moving target (tracking the
  live camera, a walking actor). A second-order critically-damped response (or the "second-order
  dynamics" freq/damping/response form) gives lag without overshoot and stays C2. Used when the
  target is moving slowly relative to the segment; a large jump promotes it to a min-jerk ramp.

**Hand-off rule (this is what kills the law-switch jerk):** on any retarget, sample the active
segment's *current* position **and velocity** (both closed-form), then emit a *new* min-jerk segment
from that `(p, v)` to the new target. Because the new segment inherits `v0` from the old one, velocity
is continuous across the switch. No snap, no rate-limiter seam, no dead-zone step.

### 1.2 Minimum-jerk segment (the "keyframe ramp")

```
// normalized time u = clamp((t - t0)/T, 0, 1)
// quintic with v0 handoff, zero end velocity/accel:
p(u) = p0 + (p1 - p0)*(10u³ - 15u⁴ + 6u⁵)
           + v0*T*(u - 6u³ + 8u⁴ - 3u⁵)          // v0 handoff term
v(u) = [ (p1-p0)*(30u² - 60u³ + 30u⁴) + v0*T*(1 - 18u² + 32u³ - 15u⁴) ] / T
```

- Duration `T` comes from a **main-sequence** law for saccades/head turns: `T = Tmin + k·|p1-p0|`
  (bigger moves take proportionally longer, peak velocity scales with amplitude — the real
  oculomotor signature). Tunable per channel; eyes fast (`Tmin≈20ms`), head slower, torso slowest.
- `v(u)` is sampled too so the *next* retarget can hand off from it — store nothing but the segment.

### 1.3 Soft recruitment (fixes jerk source #2)

Replace the hard `allocate()` knee (`algazemath.h:634-653`) with a **smooth recruitment curve**: each
joint's share of the total aim ramps in over a small blend band around its capacity (a soft-min /
`smoothstep` blend between "eye-only", "eye+head", "eye+head+neck", …) instead of a hard fill-then-
spill. The chain output then has continuous per-joint velocity as the aim crosses caps. Keep the
`anatomy_scale` exaggerate param (`distributeAnatomicalChain(..., F32 anatomy_scale=1.f)`,
`algazemath.h:565`) — soft recruitment multiplies cleanly with it. **Byte-identical at the tuning
that reproduces today** only if we ship the soft band at width 0 by default and widen it behind a
setting; otherwise this is a deliberate (small) look change — call it out in review.

### 1.4 Wire the torso (fixes jerk source #4)

Either route the torso through the same trajectory buffer as head/neck, or finally use
`torsoChase()`. Preferred: one unified trajectory buffer for the whole chain so torso, neck, head,
eyes share the recruitment and hand-off — deletes the dead `mTorsoAim*` path.

### 1.5 Determinism / scrub-safety — the crux, and why segments *beat* today

A naïve spring carries an integrator (`mVelocity`) whose value at frame *N* depends on the whole
history — **not** reproducible by jumping to frame *N* (scrub). That's why we do **not** ship a raw
`SmoothDamp`. The **segment model is the scrub-safe form**:

- A segment is a **pure closed-form function of `(t0, p0, v0, p1, T)`** sampled by absolute
  presentation time (`LLPresentationTime::currentFrame().presentation_time`, already the time base at
  `llactormover.cpp:4291-4292`). Given the segment, frame *N* is reproducible directly.
- Segments are (re)generated only on **retarget events**, which on a recorded/director timeline are
  themselves deterministic (cue times, recorded camera transform). So the *segment layout* replays
  identically → the whole trajectory replays identically.
- On a **seek/scrub**, rebuild the active segment from the deterministic event stream up to the seek
  time (the Director already bypasses the reaction latch on seek — `llactormover.cpp:4058` — same
  hook). For pure live/interactive use (no timeline) we accept the critically-damped follow and its
  history; that path was never frame-reproducible anyway.

Net: this is **more** scrub-safe than today's `mSmoothDir` exp-lag (which is itself a hidden
integrator). Micro-life stays exactly as it is — already closed-form of presentation time + UUID seed
(`evalMicroLife`, `algazemath.h:401`), added **after** smoothing (`:4744` "so it cannot feed
smoothing"). We keep that ordering: backbone produces a smooth base pose, micro-life rides on top.

### 1.6 New state & helpers

- `algazemath.h`: `minJerkSample(p0, v0, p1, T, t_rel, out_p, out_v)`, `mainSequenceDuration(amp,
  Tmin, k, Tmax)`, `critDampedFollow(...)`, `softRecruit(...)`. No global state — all pure.
- `Gaze` (`llactormover.h:907`): replace the ad-hoc `mSmoothDir` / `mApplied*` / dead `mTorsoAim*`
  with a small `TrajChannel { F32 t0, p0, v0, p1, T; }` per DOF (eye_yaw, eye_pitch, head/neck/torso
  as chain-aim yaw/pitch, lid). This is the "keyframe buffer behind the scenes."

---

## PART 2 — The Affect Vector (the emotion core)

One continuous state drives everything, so a director sets an *emotion* and life/feel fall out
coherently instead of hand-tuning 20 knobs. Extends the existing persona layer
(`algazemath.h:265,294-296`).

```
Affect = { valence  ∈ [-1,+1],   // negative=sad/hostile … positive=warm/happy
           arousal  ∈ [ 0, 1],   // calm … agitated/alert
           dominance∈ [-1,+1] }  // submissive … dominant
```

Couplings (each is a small gain the Affect feeds into an existing channel):

| Channel | valence | arousal | dominance |
|---|---|---|---|
| Blink rate / dynamics | — | ↑ rate, ↑ flutter | — |
| Saccade frequency / amplitude | — | ↑ both | — |
| Lid aperture posture | droop(−) / bright(+) | widen(+) | — |
| Micro-tremor amplitude | — | ↑ | — |
| Eye-contact hold vs break | approach(+) / avert(−) | ↓ hold | ↑ hold, win the stare |
| Head tilt / chin | — | — | chin-up(+) / chin-tuck(−) |
| Trajectory duration `T` | — | ↓ (snappier) | — |

Affect is set per actor (Director UI + presets: *neutral, warm, tense, sad, hostile, shy, alert*),
blendable, and — like everything — a deterministic input, so it's scrub-safe.

---

## PART 3 — LIFE channels (physiological realism)

Build on `evalMicroLife` (`algazemath.h:401-465`), which already does saccade flicks, Lissajous head
drift, and blink — all closed-form. Upgrades:

1. **Real saccade engine.** Today saccades are a `smoothstep` flick (`algazemath.h:425-437`). Replace
   with a **min-jerk micro-shift on the main sequence** (§1.2): gaze holds a fixation, then jumps to a
   new micro-target with amplitude-scaled duration. Frequency/amplitude scale with **arousal**. This
   is the single biggest "alive" upgrade if the eyes currently glide.
2. **Fixational micro-movement.** Between saccades add **microsaccades + slow drift + tremor** (tiny,
   high-frequency, arousal-scaled) so a "held" gaze is never perfectly still. Extends the head-drift
   hash to the eye channel; amplitude ~0.1–0.3° so it reads as *life*, not shake.
3. **Blink engine upgrade.** Today the blink is a **hardcoded symmetric parabola** `4·b·(1−b)`,
   fixed 140 ms (`algazemath.h:457-463`). Real blinks are **asymmetric — fast close (~70-100ms),
   slower open (~150-200ms)** — and vary in depth (partial blinks). Replace the parabola with an
   asymmetric eased close/open, variable duration, occasional partial/flutter, and **gaze-evoke** it:
   bias a blink to fire *at* large saccades and at cue/"sentence" boundaries. Rate from **arousal**.
   Keep the deterministic per-cycle hash (`unitHash(seed,3,blink_cycle,…)`, `:453`).
4. **Breath coupling.** A sub-degree head-bob + eye-level drift on a slow breathing sine (closed-form
   of presentation time), faster/shallower with **arousal**. Anchors the whole head to a life pulse.
5. **Per-actor imperfection seed.** From `castSeedFromUUID` (`algazemath.h:135`) derive a tiny fixed
   asymmetry (one lid a hair lower, minute inter-ocular offset). Kills the uncanny perfect symmetry
   and differentiates actors. Zero runtime cost, fully deterministic.

---

## PART 4 — FEEL channels (intentional, temporal performance)

These are where the trajectory backbone pays off — they're all about *timing between channels*.

6. **Eyes-lead-head cascade with in-socket recentering.** The #1 *intention* tell. On an attention
   shift: eyes fire a fast min-jerk saccade **first**; head starts its (slower, longer-`T`) ramp after
   a short delay; then as the head rotates in, the **eyes counter-ramp back toward socket-neutral**
   (VOR-style recentering) so they don't stay pinned at the corner. The backbone makes this trivial —
   three staggered segments with different `t0`/`T`, plus a recenter segment on the eye channel keyed
   to head progress. The difference between "a camera panning" and "a person deciding to look."
7. **Mutual-gaze make/break rhythm.** Eye contact is a hold/break cadence, not a lock. Model a
   hold-duration distribution + periodic break (small aversion saccade away and back), with hold
   length and break frequency driven by **valence** (approach vs avert) and **dominance** (who holds).
   Formalizes the existing `evalContactRation` / `evalNaturalBreak` (`llactormover.cpp:4748-4751`).
8. **Face-scan triangle.** When aimed at another avatar, dwell-weight a small **eye–eye–mouth**
   scan (mouth during their "speech", eyes during listening) instead of a fixed point. Micro-targets
   fed to the saccade engine (§3.1).
9. **Thinking aversion.** On a "thinking/recall" beat (cue or dialogue role), break fixation and drift
   off-target in a patterned way, then return — reads as an inner life. A cue-driven aversion segment.

---

## PART 5 — EMOTION channels (what the lids and rhythm say)

Routed through the **single lid `closure` channel** you already own — the `max(blink, lid-follow,
persona-narrow) × (1 − cueLidWiden)` combine at `llactormover.cpp:5093-5111`, which already accepts
continuous [0,1] values through `setVisualParamWeight("Blink_Left/Right", closure)` (`:5099-5111`).

10. **Emotional lid aperture.** Add an Affect-driven aperture term into the `closure` combine:
    - **arousal** → widen (fear/alert): extend the existing `cueLidWiden` path.
    - **valence<0 / low energy** → droop (sad/tired): a positive closure floor.
    - **anger/scrutiny** → narrow: extends `persona.mLidNarrow` (`algazemath.h:265`).
    Honest limit: SL's lid channel is essentially a **single aperture morph per eye** — no separate
    brow/Duchenne muscles unless the *mesh head* rigs face bones/extra morphs (can't rely on it
    cross-avatar). So emotion = aperture posture + blink dynamics + gaze/head rhythm. That's where
    most screen-readable eye emotion lives anyway.
11. **Blink dynamics as emotion.** Nervous = frequent + shallow flutter; focused = fewer, longer,
    deeper; sad = slow heavy lids. All fall out of §3.3 driven by Affect.
12. **Head tilt / chin by dominance.** Chin-up (dominant) vs chin-tuck-and-look-up (submissive) — a
    small pitch/roll bias on the head segment; composes with the existing camera-roll follow
    (`cameraRollAboutForward`, `algazemath.h:92-120`).
13. **Emotional focus (vergence).** Sharp near-vergence = engaged; unfocused "looking through" (verge
    to infinity) = dissociation/grief. Uses the vergence machinery already present.

---

## PART 6 — Settings (per-actor via Director; globals via `DirectorGaze*`)

Fluidity backbone:

| Key | Type | Default | Meaning |
|---|---|---|---|
| `DirectorGazeTrajectory` | Bool | 0 → 1 after burn-in | Master: use keyframe-ramp backbone (0 = today's law) |
| `DirectorGazeEyeSaccadeMs` | F32 | 25 | Eye main-sequence `Tmin` |
| `DirectorGazeHeadTurnMs` | F32 | 180 | Head `Tmin` |
| `DirectorGazeMainSeqGain` | F32 | — | main-sequence `k` (ms per degree) |
| `DirectorGazeSoftRecruit` | F32 | 0.0 | Soft-knee band width (0 = today's hard knee) |
| `DirectorGazeCascadeDelayMs` | F32 | 60 | Head-follows-eyes delay |
| `DirectorGazeRecenter` | F32 | 0.7 | In-socket VOR recenter amount |

Affect + life:

| Key | Type | Default | Meaning |
|---|---|---|---|
| `DirectorGazeValence` / `Arousal` / `Dominance` | F32 | 0/0.2/0 | Affect vector (per actor) |
| `DirectorGazeBlinkAsym` | F32 | 0 → on | Asymmetric fast-close/slow-open blink |
| `DirectorGazeMicroSaccades` | Bool | 1 | Fixational microsaccade/drift/tremor |
| `DirectorGazeBreath` | F32 | 0.3 | Breath-coupling amount |
| `DirectorGazeContactRhythm` | F32 | 0.5 | Make/break cadence strength |
| `DirectorGazeFaceScan` | Bool | 1 | Eye–eye–mouth scanning on avatar targets |

Existing knobs kept: `DirectorGazeBlinks`, `DirectorGazeLidFollow` (0.6), `DirectorGazeCameraRoll`,
`DirectorGazeExaggerate`, priority (`DirectorGazePriority`).

---

## PART 7 — Integration map (exact sites)

- **Backbone replaces** the applied stage `llactormover.cpp:4657-4712` (delete the copy/rate-
  limit/snap; sample the trajectory buffer instead) and re-smooths the chain output feeding
  `setRotation` (`:4849/4871/4920/4974/4979` etc.).
- **Soft recruitment** in `distributeAnatomicalChain` `algazemath.h:634-653`.
- **Torso**: delete dead `mTorsoAim*` (`llactormover.h:951-952`, seed `:4663-4664`) / retire
  `torsoChase()` in favor of the unified buffer.
- **New pure helpers** in `algazemath.h` (§1.6). **New `TrajChannel` state** on `Gaze`
  (`llactormover.h:907`).
- **Saccade/microsaccade/blink** upgrades inside `evalMicroLife` `algazemath.h:401-465` (blink parabola
  at `:457-463`).
- **Emotional lid** term folded into the `closure` combine `llactormover.cpp:5093-5111`.
- **Affect** extends the persona mapping `algazemath.h:265,294-296`; wired from Director like the
  existing per-target fields (`mBlinksOverride`/`mBlinkRateScale` pattern, `llactormover.cpp:3951-3988`).
- Determinism time base + seed already exist (`:4291-4292`); scrub bypass hook (`:4058`).

---

## PART 8 — Phased implementation

**Phase 1 — Fluidity backbone (do first; it's the felt problem).** Add the pure trajectory helpers +
`TrajChannel` state; replace the applied stage (§1.1-1.2) with min-jerk + hand-off; wire the torso.
Gate behind `DirectorGazeTrajectory`; verify off = byte-identical, on = no velocity discontinuity on
retarget (record a fast look-shift and diff velocity). Ship soft-recruit at band 0.

**Phase 2 — Cascade + saccade engine.** Eyes-lead-head cascade with VOR recenter (§4.6); real
main-sequence saccades + fixational micro-movement (§3.1-3.2). This is where "aims correctly" becomes
"performs".

**Phase 3 — Affect vector + emotional lids/blink.** Wire Affect (Part 2), the asymmetric/gaze-evoked
blink (§3.3), emotional aperture (§5.10-11), head-tilt/focus (§5.12-13). Add Director UI + presets.

**Phase 4 — Rhythm & scanning.** Mutual-gaze make/break (§4.7), face-scan triangle (§4.8), thinking
aversion (§4.9), breath (§3.4), imperfection seed (§3.5).

Each phase is independently revertible via its setting; each keeps default-off byte-identical.

---

## PART 9 — Risks

- **Soft recruitment changes the neutral look** slightly if band>0 — ship at 0, opt in.
- **Scrub after live pursuit**: the critically-damped follow path (interactive, no timeline) is not
  frame-reproducible; only the segment/min-jerk path is. Force the segment path when a record/scrub is
  active (tie to the same flag §1.5 uses).
- **Segment thrash**: a target that jitters every frame would emit a new segment every frame. Guard
  with a small retarget threshold (reuse the 3° hysteresis / 120ms debounce already in the Director
  body-turn path, `llactormover.cpp:3147-3151`) so segments regenerate only on real shifts.
- **Blink asymmetry vs mesh heads**: some mesh heads remap `Blink_*`; test on a few and keep the
  symmetric fallback.
- **Determinism regressions**: any future edit that adds a raw integrator (`mVelocity` chase) to the
  trajectory path breaks scrub-safety silently — keep a twice-render diff in the test suite.

---

## PART 10 — Open questions for the author

1. Trajectory backbone default: ship `DirectorGazeTrajectory` **on** after burn-in, or leave off and
   opt in per project? (Recommend on once Phase 1 diffs clean.)
2. Affect authoring: single per-actor Affect vector for v1, or keyframeable Affect on the Director
   timeline (emotion arcs across a shot)?
3. Face-scan needs the *other* avatar's eye/mouth joints — reuse the gaze target's
   `getJoint("mHead")` path (`llactormover.cpp:4252`) for eye/mouth anchors, or approximate from head
   transform only?
4. Do we expose the main-sequence constants per-actor (some people are "darty", some "languid") or
   fold that into arousal?
5. Emotional brow: detect and use mesh face bones when present (richer emotion on Bento heads), or
   stay aperture-only for guaranteed cross-avatar behavior?

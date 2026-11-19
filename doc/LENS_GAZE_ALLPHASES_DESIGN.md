# Lens Gaze — Phases 1-4 — implementation blueprint (feature-complete delivery)

**Status:** design/blueprint pass, 2026-08-16. No source modified. Feeds Codex (all phases in one
delivery) -> Opus review -> build.
**Inputs:** `doc/LENS_GAZE_ALLPHASES_SEED.md` (the brief), `doc/LENS_GAZE_RESEARCH_FINDINGS.md`
(research, 2026-07-25), and the CURRENT tree (verified this pass, 2026-08-16). Where the research
or the seed disagrees with the code, **the code wins** and the disagreement is called out.

Evidence labels: **PROVES** (read the code doing it), **IMPLIES** (strong static inference),
**INFERS** (judgment / needs in-world proof). All file:line references verified against the
current tree on 2026-08-16.

---

## 0. Executive summary

The tree has moved a long way past the 3-week-old research. **Most of what the seed calls
"Phases 1 and 4" already shipped**:

| Seed item | Status in current code |
|---|---|
| P1 dead zone (~3-5 deg) | **SHIPPED** — `BDMergeGazeDeadZone` (3.0), chase-outside-deadband math, `llactormover.cpp:3688-3740` |
| P1 selectable break-off past ~120 deg | **SHIPPED** — `BDMergeGazeBehindPolicy`/`BDMergeGazeBehindAngle` (0/120.0) + smooth `mBehindEnv` release envelope, `llactormover.cpp:3690-3712` |
| P1 torso/head lag differential | **NOT SHIPPED — this is the real Phase 1 work** (one weight, one smoothing state for the whole chain; see §4) |
| P4 R1 bone-lock interlock | **SHIPPED, deliberately HARD (not eased)** — `gazeCameraSafe()` `llactormover.cpp:3024-3041` + hard-cut branch `:3100-3110`. Seed's "ease-to-zero" is overruled by the code, with reason (§7.1) |
| P4 eyeline angular offset | **NOT SHIPPED — this is the real Phase 4 work** (§7.2) |
| P2 clone ANIM_AGENT_EYE | **NOT SHIPPED** (§5) |
| P3 shared-panel promotion | **NOT SHIPPED** as specified. Two gaze UIs exist (Path tab + Ghost Studio Pose tab); neither is the shared superset, and the settings-backed knobs (dead zone, break-off, clamps) have **no UI at all** (§6) |

So the buildable delivery is exactly four work packages:

1. **Phase 1 (remaining): torso lag differential.** Add one extra first-order chase on the torso
   aim (`tau_extra = (ratio − 1) · tau_user`, ratio default **1.8** = the built-in 0.27 s / 0.15 s
   half-life ratio, `llheadrotmotion.cpp:50-51`). The head chain is **byte-identical to today**;
   `BDMergeGazeTorsoLagRatio = 1.0` is an exact-current-behavior escape hatch. §4.
2. **Phase 2: clone life signals.** `LLGhostAvatar::setEntityEyeMotionEnabled()` mirroring the
   existing `setEntityPhysicsEnabled()` pattern (`llghostavatar.cpp:433-462`), driven per-frame
   from `applyGaze()` off `Gaze::mEnabled`, torn down at drop/replace/hide, with blink-param
   neutralization so eyes never freeze half-closed. Never `ANIM_AGENT_HEAD_ROT`. §5.
3. **Phase 3: one shared gaze panel.** New `ALPanelLensGaze` / `panel_lens_gaze.xml` embedded in
   BOTH Move tabs (Director console + standalone Actor Mover), superset control set (enable,
   target mode, blend/torso/intensity/smoothing, dead zone, break-off, eyeline). The Path-tab
   gaze block is **removed**. Exact layout + all six wrapper-height/reflow deltas enumerated
   (the CineLightRig M1 lesson). §6.
4. **Phase 4 (remaining): eyeline offset** (per-actor, head-vertical-frame angular offset applied
   to the desired direction so head AND eyes go off-lens together), plus **regression armor**
   around the already-shipped interlocks. §7.

Pure math (dead-zone chase, lag chase, behind envelope, eyeline rotation) is factored into a new
header-only `algazemath.h` with a TUT test (`indra/newview/tests/algazemath_test.cpp`, same idiom
as `alcinelightrigmodel_test.cpp`). The R1 interlock and mesh-head behavior remain in-world-only
verification. Highest risk is **regressing the working R1 interlock while editing the same
functions** — see §10 and the OFF-LIMITS list §11.

---

## 1. Verification — research vs current code

### 1.1 Where the code moved past the research (trust the code)

- **PROVES** — `applyGaze()` is now at `llactormover.cpp:3071-3154` (research said ~2639). Camera/
  cast/point gaze is locomotion-independent (`:3084-3094, 3111-3112`): the Phase-0 Move-gate
  removal shipped, and only `GAZE_TANGENT` still requires an active non-suspended Move.
- **PROVES** — a whole **Director look-at subsystem** now exists that the research never saw:
  `restoreDirectorLookAtPose`/`applyDirectorLookAt` (`llactormover.cpp:3156-3529`,
  `llactormover.h:357-361`, `DirectorGaze` struct `llactormover.h:884-905`), settings
  `DirectorLookAtCamera*` (`settings.xml:4689-4766`, scene-saved via
  `llfloaterdirector.cpp:657-665`). It captures/restores real-avatar joint poses around
  `updateMotions()`, has a body-turn mode with AO-aware turn animations, and **reuses
  `gazePaint()`** with `constrain_eye_cone = true`. Arbitration in
  `LLVOAvatar::updateCharacter()`: Director gets first refusal, Actor Mover gaze is skipped when
  Director paints (`llvoavatar.cpp:5218-5221`); the pre-motion restore is at `:5113`; the
  LOD-skipped branch still services Director release gates (`:5137-5144`).
- **PROVES** — `gazeCameraSafe()` (`llactormover.cpp:3024-3041`) now takes a `director_look_at`
  flag. For **all** camera-gaze: bone-lock target check
  (`LLCinematicCamera::isActiveBoneLockTarget`, `llcinematiccamera.cpp:575-592`) and the
  `UseCinematicCamera`-on-self check. **Director-only additions:** mouselook-self, head-framing
  target (`:594-613`), orbit anchor (`:615-636`) — kept Director-only "so a disabled Director
  remains byte-identical" (comment `:3021-3023`).
- **PROVES** — the interlock is **hard**: `applyGaze()` `:3100-3110` zeroes `mEnv` and all
  smoothing state and returns **without painting even an ease-out frame**, with the comment
  "Safety interlock is intentionally hard: do not write even an ease-out frame while the camera
  depends on this head pose." The seed asked for ease-to-zero; the code overrules it (§7.1).
- **PROVES** — dead zone, behind policy, head/eye clamps and a discontinuity slew limiter shipped
  as settings-backed controls read inside `gazePaint()`:
  `BDMergeGazeHeadYawMax` 72, `BDMergeGazeHeadPitchMax` 45, `BDMergeGazeHeadSlewRate` 480,
  `BDMergeGazeDeadZone` 3, `BDMergeGazeBehindPolicy` 0, `BDMergeGazeBehindAngle` 120,
  `BDMergeGazeEyeYawMax` 35, `BDMergeGazeEyePitchMax` 25
  (`llactormover.cpp:3682-3693, 3847-3850`; `settings.xml:24017-24072`). **None of these has any
  UI.**
- **PROVES** — gaze dt now rides the presentation clock under Temporal Capture
  (`LLPresentationTime::drives(ANIMATION)`, `:3122-3132`), capped 0.25 s.
- **PROVES** — ghost outer-scale-aware aiming (`gazeRenderedJointPosition`, `:3002-3019`) for both
  the gazing head and a cast target's head (`:3543, 3594-3599`).
- **PROVES** — a second gaze UI exists: Ghost Studio Pose tab "Look at the lens" block
  (`panel_ghost_studio.xml:259-285`, wiring `alpanelghoststudio.cpp:291-299, 443-460, 2136-2169`,
  keyed by entity id) alongside the original Path-tab block
  (`panel_path_editor.xml:697-816`, wiring `alpanelpatheditor.cpp:104-111, 163-169, 1235+`).

### 1.2 What the research still gets right (spot-verified)

- **PROVES** — post-blend placement: called from `updateCharacter()` after `updateMotions()`
  (`llvoavatar.cpp:5210-5221`), for every avatar class; map-miss is the first thing checked
  (`llactormover.cpp:3073-3081`) so a no-gaze avatar is a two-branch no-op.
- **PROVES** — anatomical distribution intact: torso share `nlerp(mTorsoAmount, ID, …)`
  (`:3818-3826`), neck/head split `GAZE_NECK_LAG = 0.5` in the neck-parent frame (`:2988,
  3828-3839`) exactly like `LLHeadRotMotion` (`llheadrotmotion.cpp:268-274`); eyes carry the
  residual relative to the head's **actual** world rotation recomputed after the head write
  (`:3842-3892`).
- **PROVES** — built-in half-lives for the differential target: `HEAD_LOOKAT_LAG_HALF_LIFE
  0.15 s`, `TORSO_LOOKAT_LAG_HALF_LIFE 0.27 s`, `TORSO_LAG 0.35`, `NECK_LAG 0.5`
  (`llheadrotmotion.cpp:48-51`), applied as two separate interpolants (`:205-206, 261-265`).
- **PROVES** — clone anim facts: `mEnableDefaultMotions = false` (`llghostavatar.cpp:100`); the
  physics-motion pattern to copy (`:415-431, 433-462`) uses `LLCharacter::startMotion`/
  `stopMotion` (never the `LLVOAvatar` wrapper) gated by `mEntityCloneVisible`;
  `ANIM_AGENT_HEAD_ROT` with no `LookAtPoint` drives the head chain toward root-forward every
  frame (`llheadrotmotion.cpp:250-253`) — the fight `llghostavatar.cpp:450-451` warns about.
- **PROVES** — `LLEyeMotion` with null `LookAtPoint` contributes identity + saccade jitter on the
  eye joints (`llheadrotmotion.cpp:431-435, 440-453`) and blink **morphs** `Blink_Left`/
  `Blink_Right` (`:516-555`) — and gazePaint overwrites the eye joints downstream, so on a
  gazing clone only the morphs and jitter-as-overwritten survive. Registration is per-instance in
  `LLVOAvatar::initInstance` (`llvoavatar.cpp:1311`), inherited by ghosts.
- **IMPLIES** — the 25000 px² eye-motion LOD gate (`llheadrotmotion.h:37,171`) is enforced by
  `LLMotionController` fade-out (`llmotioncontroller.cpp:649-651`): clones blink only when big on
  screen. Acceptable — that is exactly the close-up case blinking exists for.
- **PROVES** — per-actor gaze state lifecycle: keyed by `path_key()` (resolved id,
  `llactormover.cpp:414-421`); migrated on runtime replace (`:2448-2453`); erased in
  `dropActor()` (`:2521`); accessors `:748-828`; `Gaze` struct `llactormover.h:854-878`.
- **PROVES** — scene files save settings keys + cast data only (`llfloaterdirector.cpp:645+`);
  per-actor mover state (paths, gaze) does not round-trip.

### 1.3 Stale line references in seed/research (for the reviewer's map)

| Research/seed said | Current |
|---|---|
| applyGaze/gazePaint 2639-2916 | applyGaze 3071-3154; gazePaint 3531-3893 |
| gaze accessors ~742-770 | 748-828 |
| gazeCameraSafe ~3024 | 3024-3041 (accurate) |
| Gaze config struct llactormover.h:798-813 | 854-878 |
| gaze decls llactormover.h:305-350 | 305-361 |
| updateCharacter gaze call llvoavatar.cpp:5156 | 5218-5221 (plus 5113, 5137-5144) |
| llghostavatar.cpp clone-anim pattern 224-248 | 415-431 + 433-462 |

---

## 2. As-built gazePaint pipeline (order of operations, current)

`LLActorMover::gazePaint(av, g, mv, dt, advance, constrain_eye_cone)` — `llactormover.cpp:3531`:

| # | Stage | Lines |
|---|---|---|
| 1 | Resolve joints; rendered head position (ghost outer scale) | 3534-3543 |
| 2 | Resolve desired direction `dir` (camera origin / cast head / point / tangent, with fallbacks) | 3549-3626 |
| 3 | Exponential direction smoothing `mSmoothDir`, `tau_user = 0.04 + 0.46·mSmoothing`; reseed on activation | 3628-3650 |
| 4 | `env_i = smoothstep(mEnv) · mIntensity` | 3655-3656 |
| 5 | Build root-relative target quat from `look = mSmoothDir` (degenerate-overhead guard); euler `raw_pitch/raw_yaw` | 3658-3696 |
| 6 | **Break-off**: if policy 1 and `|raw_yaw| > behind_angle`, ease `mBehindEnv` down over `GAZE_EASE_TIME` (0.35 s); smoothstep -> `behind_eased`; `wEye = env_i · behind_eased` | 3698-3713 |
| 7 | **Dead zone**: accepted aim `mBodyAimPitch/Yaw` chases raw only while combined error > dead zone, exponential with `tau_user`, destination pulled back to the zone edge; `wBody = env_i · mHeadEyeBlend · behind_eased` | 3714-3741 |
| 8 | Clamp accepted aim to head yaw/pitch max; `mAppliedYaw/Pitch` copies it, or slew-limits (480 deg/s) across a >90 deg seam (`APPLIED_SLEW_TRIGGER`); rebuild `head_rot_local` roll-free | 3746-3811 |
| 9 | Apply: torso `nlerp(wBody, anim, nlerp(mTorsoAmount, ID, head_rot_local))`; neck+head 50/50 in neck-parent frame `nlerp`ed by `wBody` | 3813-3839 |
| 10 | Eyes: residual toward `look` vs the head's recomputed world rotation, per-axis clamps, optional total-angle cone, `nlerp` by `wEye`; four eye joints | 3845-3892 |

**The rigid-block defect, precisely:** stages 7-9 hold ONE smoothed aim (`mBodyAim*` -> `mApplied*`)
and ONE weight (`wBody`) for torso+neck+head. During any transient the whole chain moves in fixed
ratios and **arrives simultaneously** — nothing trails. The built-in instead runs two interpolants
(head 0.15 s vs torso 0.27 s half-life, `llheadrotmotion.cpp:205-206`), which is why stock
tracking's chest visibly lags the head.

---

## 3. Delivery decisions at a glance (parameters: fixed vs settings vs per-actor)

| Parameter | Value / range | Kind | UI |
|---|---|---|---|
| Torso lag ratio | default **1.8**, clamp [1, 4] | NEW setting `BDMergeGazeTorsoLagRatio` (F32, persist) | debug settings only (deliberate; §4.4) |
| Head chase dynamics | unchanged (`tau_user` via Smoothing slider) | existing | Smoothing slider (promoted panel) |
| Dead zone | existing default 3.0 deg, UI range 0-15 | existing `BDMergeGazeDeadZone` | NEW spinner on shared panel |
| Break-off policy / angle | existing 0 / 120 deg, UI range 90-180 | existing `BDMergeGazeBehindPolicy`, `BDMergeGazeBehindAngle` | NEW checkbox + spinner on shared panel |
| Break-off ease width | temporal: `GAZE_EASE_TIME` 0.35 s envelope (`:2983, 3704-3706`) | fixed constant | none |
| Neck/head split | `GAZE_NECK_LAG` 0.5 (`:2988`) | fixed constant | none |
| Head/eye clamps, slew | existing settings (72/45/480/35/25) | existing | none (debug settings) |
| Eyeline offset yaw/pitch | default 0, yaw ±15 deg, pitch ±10 deg | NEW per-actor authored fields in `Gaze` (session-only) | NEW two sliders on shared panel |
| Clone eye motion | on iff `Gaze::mEnabled` on a ghost | derived (no knob) | none (implicit) |

---

## 4. Phase 1 — torso/head lag differential

### 4.1 Design principle

**Do not touch the head chain.** The head's dynamics today are `tau_user` smoothing at the
direction (stage 3) + `tau_user` chase at the accepted aim (stage 7) + seam slew (stage 8); that
tuning is live and looks acceptable. Adding per-joint half-lives by *replacing* stage 7 would
change the head feel for every existing user. Instead, add **one extra first-order stage on the
torso only**:

- head total lag  ≈ today's (unchanged, bitwise where ratio semantics allow)
- torso total lag ≈ today's × ratio (cascade of the shared chase + the new torso chase)

with `ratio = BDMergeGazeTorsoLagRatio`, default **1.8** — the built-ins' 0.27/0.15
(`llheadrotmotion.cpp:50-51`). `ratio = 1.0` short-circuits to exactly the current single-aim
behavior (regression escape hatch and A/B switch).

### 4.2 Exact math change (gazePaint)

New runtime state in `Gaze` (`llactormover.h`, after `mAppliedYaw`):

```cpp
F32  mTorsoAimPitch = 0.f;   // torso's own lagged aim, radians (root-relative)
F32  mTorsoAimYaw   = 0.f;
```

Seed them inside the existing `if (!g.mAppliedValid)` activation block (`:3755-3763`):
`mTorsoAimPitch = target_pitch; mTorsoAimYaw = target_yaw;` — same never-slew-in-from-stale rule.
Reset nothing extra on release: the `mAppliedValid = false` writes at `:3104-3108` and
`:3140-3147` already gate reseeding; add no new flags (reuse `mAppliedValid`).

Inside the stage-8 block, immediately after `target_yaw`/`target_pitch` are computed
(`:3751-3754`) and after the `mApplied*` update, add the torso chase (all pure math via
`algazemath.h`, §9):

```cpp
static LLCachedControl<F32> torso_lag_ratio(
    gSavedSettings, "BDMergeGazeTorsoLagRatio", 1.8f);
const F32 ratio = llclamp((F32)torso_lag_ratio, 1.f, 4.f);
if (advance)
{
    if (ratio <= 1.001f)
    {
        g.mTorsoAimPitch = target_pitch;          // exact legacy behavior
        g.mTorsoAimYaw   = target_yaw;
    }
    else
    {
        const F32 tau_user  = GAZE_TAU_MIN + (GAZE_TAU_MAX - GAZE_TAU_MIN) * g.mSmoothing;
        const F32 tau_extra = (ratio - 1.f) * tau_user;      // cascade => total ≈ ratio·tau_user
        const F32 a = 1.f - expf(-dt / llmax(tau_extra, 0.01f));
        g.mTorsoAimPitch += (target_pitch - g.mTorsoAimPitch) * a;
        g.mTorsoAimYaw    = llsimple_angle(
            g.mTorsoAimYaw + llsimple_angle(target_yaw - g.mTorsoAimYaw) * a);
    }
}
```

The chase target is the **clamped accepted aim** (`target_*`), not `mApplied*`: the torso must
never aim outside the head clamps, and chasing the clamp output keeps the behind-seam jump
(~144 deg) bounded by the exponential itself — peak torso-visible rate ≈
`mTorsoAmount · error/tau_extra` ≈ 0.25 · 144° / 0.37 s ≈ **~97 deg/s** at defaults, well under
the head's 480 deg/s slew cap, so **no separate torso slew stage** (INFERS — tuning judgment,
validated in-world; the ratio knob is the fallback).

Then change ONLY the torso application (`:3818-3826`): build the torso's own local target from
its lagged aim instead of the head's applied aim:

```cpp
if (g.mTorsoAmount > 0.f)
{
    if (LLJoint* torso = av->getJoint("mTorso"))
    {
        LLQuaternion torso_aim_local;
        torso_aim_local.setEulerAngles(0.f, g.mTorsoAimPitch, g.mTorsoAimYaw);
        const LLQuaternion torso_target =
            nlerp(g.mTorsoAmount, LLQuaternion::DEFAULT, torso_aim_local);
        torso->setRotation(nlerp(wBody, torso->getRotation(), torso_target));
    }
}
```

**Everything downstream is untouched and self-correcting:** the neck/head stage already converts
the head target into the neck-parent-local frame via the neck parent's *live* world rotation
(`torsoRotLocal`, `:3833-3834`), which reflects the torso rotation just written. A slower torso
therefore automatically leaves a larger residual for neck+head — the head still lands on the
(applied) aim while the chest is still travelling. This is exactly how `LLHeadRotMotion`
composes it (`llheadrotmotion.cpp:261-274`). The eye stage recomputes the head's world rotation
after the head write (`:3855`), so **eyes-carry-residual is preserved by construction**.

### 4.3 Composition order (dead zone / lag / break-off) — final

Unchanged relative order, one inserted stage:

1. direction resolve -> **eyeline offset (§7.2)** -> direction smoothing (`tau_user`)
2. break-off decision from the raw (pre-dead-zone) target yaw; `mBehindEnv` ease (weights only)
3. dead zone gating of the shared accepted aim (`mBodyAim*`) — one deadband for the whole body
   chain, so inside the zone neither head nor torso creeps; eyes keep micro-tracking (their
   stage never consults the deadband — current, kept)
4. clamp -> head applied/slew (unchanged) **and** the new torso chase (both consume the same
   clamped accepted aim; head fast path, torso trailing path)
5. apply torso (from torso aim), neck+head (from applied aim), eyes (residual)

Rationale for dead zone *before* the split: a single acceptance keeps head and torso converging
to the same held pose (no steady-state twist between chest and head from asymmetric deadbands),
and it is what the code already does — minimal diff.

### 4.4 Knob policy

`BDMergeGazeTorsoLagRatio` stays **debug-settings only** (not on the panel): it is an anatomical
constant, not a per-shot creative control; the panel already carries eight controls and every
extra row costs reflow in two hosts (§6.4). Settings entry (settings.xml, next to the other
BDMergeGaze keys at `:24017+`): F32, default 1.8, persist 1, comment
"Torso trailing ratio for gaze: torso smoothing time vs head (1 = legacy rigid chain)".

**Shared-consumer note (deliberate):** `applyDirectorLookAt` reuses `gazePaint`
(`llactormover.cpp:3527`), so real-cast Director gaze gains the same torso trail. Its torso
capture/restore is already gated by `mTorsoAmount > 0` on both sides (`:3183-3186`, apply
`:3818`), which the change preserves. The new `Gaze` fields are POD defaults inside
`DirectorGaze::mGaze` — no capture changes needed.

---

## 5. Phase 2 — clone life signals (ANIM_AGENT_EYE only)

### 5.1 What starting `ANIM_AGENT_EYE` on a ghost does (verified)

- Blink morphs `Blink_Left`/`Blink_Right` + `updateVisualParams()`
  (`llheadrotmotion.cpp:516-555`) — visual params, never touched by the gaze joint paint ->
  **survive** (the whole point).
- Eye-joint rotations: with no `LookAtPoint` (`getAnimationData` null on a ghost — no HUD
  effect), identity + saccade jitter (`:431-435, 440-453`), blended by the motion controller in
  `updateMotions()` and then **overwritten** by gazePaint's eye stage whenever `wEye > 0.001`
  (`llactormover.cpp:3845-3892`). Harmless; at `mIntensity = 0` the raw saccades show, which
  still reads as "alive". (INFERS: acceptable; verify in-world.)
- LOD: faded out below 25000 px² (`llheadrotmotion.h:37`, `llmotioncontroller.cpp:649-651`).
  Blinks appear only in close-up — accepted, not fought.
- **Never `ANIM_AGENT_HEAD_ROT`**: null-target branch drives the head chain to root-forward every
  frame (`llheadrotmotion.cpp:250-253`), exactly the clone-anim fight `llghostavatar.cpp:450-451`
  documents.

### 5.2 Implementation — the `setEntityPhysicsEnabled` pattern, exactly

**`llghostavatar.h`** (by `:117` / `:184-189`):

```cpp
void setEntityEyeMotionEnabled(bool enabled);       // public, beside setEntityPhysicsEnabled
...
bool mEntityEyeMotionEnabled = false;               // default OFF (physics defaults on; eye does not)
void neutralizeEntityEyeParams();                   // private helper
```

**`llghostavatar.cpp`** — mirror `:433-462` with these deltas:

```cpp
void LLGhostAvatar::setEntityEyeMotionEnabled(bool enabled)
{
    if (enabled == mEntityEyeMotionEnabled)
    {
        // re-ensure: default motions are disabled, and visibility may have
        // stopped the motion since (same fresh-clone re-arm as physics :437-443)
        if (enabled && mEntityCloneVisible && !isMotionActive(ANIM_AGENT_EYE))
        {
            LLCharacter::startMotion(ANIM_AGENT_EYE);
        }
        return;
    }
    mEntityEyeMotionEnabled = enabled;
    if (enabled && mEntityCloneVisible)
    {
        LLCharacter::startMotion(ANIM_AGENT_EYE);
    }
    else
    {
        LLCharacter::stopMotion(ANIM_AGENT_EYE, true);   // stop_immediate
        neutralizeEntityEyeParams();
    }
}
```

`neutralizeEntityEyeParams()`: `setVisualParamWeight("Blink_Left", 0)` + `"Blink_Right"` +
`updateVisualParams()` (idiom of `neutralizeEntityPhysicsParams`, called at `:429, 459`).
**Why:** `LLEyeMotion::onDeactivate` resets eye *joints* only (`llheadrotmotion.cpp:569-594`);
stopping mid-blink would otherwise freeze the lids half-closed — a worse tell than no blinking.

**`setEntityCloneVisible`** (`:415-431`): add the eye motion beside the physics motion — on show,
start if `mEntityEyeMotionEnabled`; on hide, `stopMotion(ANIM_AGENT_EYE, true)` +
`neutralizeEntityEyeParams()`.

`ANIM_AGENT_EYE` is declared in `llvoavatar.h` (defined `llvoavatar.cpp:163`) and registered
per-instance (`llvoavatar.cpp:1311`) — ghosts inherit both. Use `LLCharacter::` qualified calls
only (no sim path), same as physics (`:423, 428`).

### 5.3 Driver and teardown ("tied to gaze-enabled")

Single choke point, per-frame idempotent, in `LLActorMover::applyGaze()` right after
`Gaze& g = git->second;` (`llactormover.cpp:3082`) — i.e., **after** the map-miss fast path, so
non-gaze avatars are untouched:

```cpp
if (av->isGhostAvatar())
{
    static_cast<LLGhostAvatar*>(av)->setEntityEyeMotionEnabled(g.mEnabled);
}
```

(`isGhostAvatar()` is the established discriminator — see `applyDirectorLookAt` `:3434`; use the
repo's existing downcast idiom.) This runs before any early return below it, so enable/disable,
hard-cut suppression, and env decay all keep the blink lifecycle correct: blinking tracks
**`mEnabled`**, not per-frame paint activity, per the seed — a bone-lock-suppressed or
behind-released clone keeps blinking, which is right (its face is or may soon be on camera).

Teardown paths beyond the per-frame sync (each one line):

- **`dropActor()`** (`llactormover.cpp:2493-2529`): after `mGazes.erase(actor_id)` (`:2521`),
  `resolve_actor(actor_id)` -> if ghost, `setEntityEyeMotionEnabled(false)`.
- **`replaceRuntime()`** (`:2448-2453`): before migrating the gaze entry, disable on the OLD
  body if it still resolves as a ghost (the new body's first `applyGaze` re-enables).
- **Clone destruction**: the motion controller dies with the character — PROVES no dangling
  state possible.
- **Gaze disabled via any UI**: the map entry persists with `mEnabled = false`; next frame's
  sync stops the motion. No extra hook needed.

---

## 6. Phase 3 — one shared gaze panel

### 6.1 Host decision

**NEW shared panel `ALPanelLensGaze`** (`alpanellensgaze.h/.cpp`,
`skins/default/xui/en/panel_lens_gaze.xml`, injector
`static LLPanelInjector<ALPanelLensGaze> t_panel_lens_gaze("panel_lens_gaze");` — idiom
`alpanelactormover.cpp:24`), embedded in **both Move tabs**:

- Director console Move tab (`floater_director.xml`), directly under the shared
  `actor_mover_panel` (`:359-368`);
- standalone Actor Mover Move tab (`floater_actor_mover.xml:98-118`).

Why not extend `panel_actor_mover.xml`: that panel is the *transport* half by charter
(`alpanelactormover.h:23-29`) and both hosts embed it at fixed 360 high; a separate sibling panel
keeps the transport untouched (smaller blast radius) while preserving the never-drift rule the
console is built on (comments `floater_director.xml:353-358, 550-554`). Why the Move tab and not
Path: camera/cast/point gaze is locomotion-independent (`llactormover.cpp:3084-3094`) — it is an
actor property, not a path property.

**Selection feed:** same idiom as the transport — `void setSelectedActors(const uuid_vec_t&)`
(`alpanelactormover.h:56`), fed by each host at the same call sites that already feed
`ALPanelActorMover::setSelectedActors` (Director `refreshMoveTab()`; the standalone floater's
draw) (IMPLIES — Codex locates the two existing feed sites and adds the parallel call).
**Write semantics:** commits apply to every selected actor via the existing per-actor accessors
(`LLActorMover::setGaze*`, `llactormover.cpp:748-828`); **empty selection = self** (that is what
`path_key()` null->agent resolution already does, `:414-421`). This delivers the research's
"all selected look at lens". **Refresh semantics:** display state from the FIRST selected actor
(document in a tooltip); status line via `getGazeStatus()` (`:830+`).

### 6.2 Path-tab block: REMOVED (decided)

One authoring surface for per-actor gaze. Two live copies would not drift (both write the same
accessors) but cost duplicate reflow forever and confuse "where do I set this".

- `panel_path_editor.xml`: delete the gaze block (`:697-816`, `gaze_lbl` .. `gaze_status`);
  root panel height 716 -> **604**.
- `alpanelpatheditor.cpp/.h`: delete members (`:104-111`), callbacks (`:163-169`), the
  `refreshGaze()` call (`:243`) and `refreshGaze()`/`refreshGazeCastCombo()` bodies (`:1232+`),
  handlers `onGazeEnableToggle/onGazeTargetCommit/onGazeCastCommit/onGazeSetPoint/
  onGazeBlendCommit/onGazeIntensityCommit/onGazeSmoothingCommit`, and their header decls.

**Ghost Studio Pose-tab block: KEPT UNCHANGED** this delivery. Its selection model is entity-
instance based (`alpanelghoststudio.cpp:2136-2169`) and it already writes the same accessors;
global knobs (dead zone / break-off / ratio) apply to clones automatically because they are
settings read inside `gazePaint`. Per-actor eyeline for a clone is reachable through the console
once the clone is cast. Unifying that block onto `ALPanelLensGaze` is a named deferral (§12).

### 6.3 Control set + layout (`panel_lens_gaze.xml`, width 374, height **188**)

| top | Controls (names) |
|---|---|
| 4 | `gaze_lbl` "Look-at (gaze)" (bold, w120) ; `gaze_status` right column (left 182, w186, LtGray_50, from `getGazeStatus`) |
| 18 | `gaze_enable_check` "Enable" (w80) ; `gaze_target_combo` (left 92, w276): Where it's going / Camera / Cast member / Fixed point (values 0-3, `LLActorMover::GAZE_*`) |
| 40 | `gaze_cast_combo` (w362) and `btn_gaze_setpoint` "Set point from camera" (w200) — same-slot auto-swap by mode, exactly the Path-tab idiom (`panel_path_editor.xml:736-756`) |
| 62 | `gaze_blend_slider` "Head/eyes" (left 6, w172, 0-1, init 0.7) ; `gaze_torso_slider` "Torso" (left 182, w186, 0-1, init 0.25 — restores the torso knob the Path tab never had; accessor `setGazeTorsoAmount`) |
| 80 | `gaze_intensity_slider` (left 6, w172, init 1.0) ; `gaze_smoothing_slider` (left 182, w186, init 0.5) |
| 100 | `gaze_feel_lbl` "Tracking feel (all actors)" (small bold, h12) |
| 114 | `gaze_deadzone_spinner` "Dead zone (deg)" (left 6, w150, 0-15, inc 0.5, **control_name="BDMergeGazeDeadZone"**) ; `gaze_breakoff_check` "Break off past" (left 162, w110, writes `BDMergeGazeBehindPolicy` 0/1) ; `gaze_breakoff_spinner` (left 274, w94, 90-180 deg, inc 5, **control_name="BDMergeGazeBehindAngle"**) |
| 138 | `gaze_eyeline_yaw_slider` "Eyeline yaw" (left 6, w172, −15..15 deg, inc 0.5, init 0) ; `gaze_eyeline_pitch_slider` "Eyeline pitch" (left 182, w186, −10..10 deg, inc 0.5, init 0) — per-actor, §7.2 |
| 158-184 | `gaze_hint` (LtGray_50, h24): "Applies to the selected cast actors; empty selection = your avatar. Dead zone and break-off are global." |

Settings-backed rows use `control_name` two-way binding (the `panel_actor_mover.xml` idiom);
per-actor rows use commit callbacks -> accessors, with the has-focus/has-mouse-capture refresh
guards copied from `alpanelghoststudio.cpp:1393-1403` / `alpanelpatheditor.cpp:1246-1255`.
`gaze_breakoff_check` cannot use `control_name` (bool vs S32 policy) — commit handler writes
`gSavedSettings.setS32("BDMergeGazeBehindPolicy", checked ? 1 : 0)`, refresh reads it.

New per-actor accessors (beside `:819-828`):
`setGazeEyelineOffset(const LLUUID&, F32 yaw_deg, F32 pitch_deg)` (clamped ±15/±10),
`F32 getGazeEyelineYaw(const LLUUID&) const`, `F32 getGazeEyelinePitch(const LLUUID&) const`
(defaults 0 on map miss). CMake: add the three new source files to `indra/newview/CMakeLists.txt`
beside `alpanelactormover.*`.

### 6.4 Reflow deltas — the complete wrapper-height list (the M1 lesson)

The CineLightRig M1 must-fix (`doc/CINE_LIGHT_RIG_STATUS.md:172, 207-210`): a grown embedded
panel clips unless **every host wrapper height** is bumped. Every affected number:

**A. `floater_director.xml` Move tab** (insert `lens_gaze_panel`, class/filename
`panel_lens_gaze`, top 364, h 188 -> occupies 364-552; group block shifts **+192**):

| Widget | top now -> new |
|---|---|
| `group_run_lbl` | 369 -> 561 |
| `group_run_combo` / `btn_group_start` / `btn_group_stop` | 366 -> 558 |
| `groups_lbl` | 392 -> 584 |
| `groups_list` | 408 -> 600 |
| `group_delay_spinner` | 490 -> 682 |
| `btn_group_dissolve` | 488 -> 680 |
| `group_rename_editor` | 516 -> 708 |
| `btn_group_rename` | 515 -> 707 |
| **`move_scroll_content` height** (`:347`) | **554 -> 746** |

**B. `floater_actor_mover.xml` Move tab**: insert `lens_gaze_panel` top 372 (below
`actor_mover_panel` top 8 + h 360), h 188; **`move_scroll_content` height 376 -> 568** (`:101`).

**C. Path-tab shrink (both hosts; shrink cannot clip, but do it — no dead scroll):**
`floater_director.xml`: `path_editor` 716 -> 604 (`:539`), `path_scroll_content` 732 -> 620
(`:529`); `floater_actor_mover.xml`: `path_editor` 716 -> 604 (`:164`), `path_scroll_content`
750 -> 638 (`:142`). Update the stale comment at `floater_director.xml:503-508` ("its Look-at /
Gaze section sits at the very bottom" — no longer true).

**Review checklist:** at default floater sizes, scroll each Move tab to the bottom — the last
row (`btn_group_rename` in the console; `gaze_hint` in the standalone) must be fully visible.

---

## 7. Phase 4 — safety interlocks + eyeline

### 7.1 R1 bone-lock feedback guard — SHIPPED; keep it HARD (decision)

Current condition (PROVES, `llactormover.cpp:3024-3041` + `:3095-3110`): for Actor-Mover gaze
with `mTarget == GAZE_CAMERA`, `gazeCameraSafe(av, false)` returns false when

1. the avatar is the **active** cinematic-camera Bone-Lock target — `CinematicCamEnabled` on,
   effective mode (switcher-aware) == `MODE_BONE_LOCK`, and the resolved live target is this
   avatar (`llcinematiccamera.cpp:575-592`); or
2. `UseCinematicCamera` (BD head-follow camera) is on and the avatar is self (`:3037-3040`).

On unsafe, `applyGaze` **hard-cuts**: `mEnv = 0`, all smoothing seeds invalidated, return with
no paint (`:3100-3110`).

**The seed asked for "ease gaze weight to 0, non-destructively." The code overrules the ease,
and this blueprint keeps the code's behavior.** Reasoning:

- Easing out means writing head-pose frames **while the camera is consuming the head pose** —
  each written frame moves the camera, which is the feedback loop the guard exists to break.
  The code comment says exactly this (`:3101-3103`).
- The suppression IS non-destructive without any ease: gazePaint layers post-blend on a pose the
  motion controller rebuilds every frame (banner `:2966-2979`); not painting simply reveals the
  animation pose. Nothing needs restoring.
- The one-frame snap lands on the frame Bone Lock engages on that subject — a camera *cut*,
  where a discontinuity is expected and largely self-hidden (the shot is FROM the head).
- Re-engagement is already smooth: seeds invalidated -> on release the envelope eases back in
  from zero through the normal activation path (`:3140-3147`, `:3628-3635`).

**Directive to Codex: no code change to the interlock.** The Phase-1/Phase-4 edits live in the
same functions; the OFF-LIMITS list (§11) pins what must not move. Also **decided against**
extending the Director-only head-framing / orbit-anchor guards (`:3030-3034`) to the legacy
Actor-Mover path: the coupling there is positional (cm-scale head translation vs a camera meters
away), heavily damped by the one-frame lag, and the Director-only split exists precisely to keep
the legacy path byte-identical (`:3021-3023`). INFERS — flagged as an in-world watch item
(§9.2 #4); if watching shows drift, flipping `gazeCameraSafe(av, false)` to `true` for camera
mode is a one-word follow-up, not a redesign.

### 7.2 Eyeline angular offset (NEW)

**Intent:** let a subject look just off-lens (interview eyeline) or dead-centre (talk-to-camera),
as an *angular* offset — never positional (research: the camera origin is the entrance pupil).

**Frame (decision):** the offset rotates the **desired look direction** about the actor's
vertical (root-up) for yaw and about the aim-left axis for pitch — i.e., the same aim frame the
head/eye solves are built in. The seed says "head-local"; for a roll-free aim frame (roll is
zeroed at `:3809-3810`) yaw/pitch about (root_up, aim_left) IS the head-local angular offset,
while staying well-defined even while the head is still travelling. (IMPLIES — equivalent where
it matters; stated so the reviewer can check the frame choice.)

**Where in the pipeline (decision): pre-smoothing, applied to `dir` at resolve time** — inserted
in `gazePaint` immediately before the `dir.normVec()`/`haveDir` acceptance (`:3621-3625`), for
all target modes:

```cpp
// eyeline: small angular offset so the subject reads just off-lens.
// Applied to the RAW desired direction => the smoother, dead zone, break-off,
// clamps AND the eye residual all see the offset target consistently.
const F32 eyeline_yaw   = g.mEyelineYawDeg * DEG_TO_RAD;
const F32 eyeline_pitch = g.mEyelinePitchDeg * DEG_TO_RAD;
if (haveDirCandidate && (fabsf(eyeline_yaw) + fabsf(eyeline_pitch)) > 1e-4f)
{
    const LLQuaternion rootWorld = root->getWorldRotation();   // hoist of :3658
    LLVector3 up_axis = LLVector3(0.f, 0.f, 1.f) * rootWorld;
    LLVector3 left_axis = up_axis % dir;
    if (left_axis.normVec() > 1e-4f)                // skip only if aiming straight up/down
    {
        LLQuaternion q_yaw(eyeline_yaw, up_axis);
        LLQuaternion q_pitch(eyeline_pitch, left_axis);
        dir = dir * (q_pitch * q_yaw);
    }
}
```

(The `rootWorld` fetch at `:3658` becomes a reuse of the hoisted value — behavior-neutral.)

Why pre-smoothing and shared: (a) slider drags ease through the existing `mSmoothDir` chase and
dead zone instead of stepping; (b) **both** the head solve and the eye solve consume the same
offset direction (`look = mSmoothDir`, `:3661`, eye stage `:3856-3877`) — this is essential,
because eyes carry the residual: a head-only offset would be exactly cancelled by the eyes and
read as no offset at all.

**Struct/accessors:** authored fields `F32 mEyelineYawDeg = 0.f; F32 mEyelinePitchDeg = 0.f;` in
`Gaze` (`llactormover.h:856-864` block); accessors per §6.3; migrated/erased with the struct for
free (`:2448-2453, 2521`). Ranges yaw ±15 deg, pitch ±10 deg (beyond that it reads as "looking
away", not eyeline — INFERS, tune in-world). Default 0 skips the branch — **bitwise-identical
paint for every existing user** (provable by inspection).

**Sign convention:** positive yaw offsets the gaze toward the actor's left (target appears
screen-right of the eyes when facing camera). GUESS on LL quaternion handedness reading —
in-world check flips the sign at one site if wrong; the unit test (§9.1) pins rotation
*magnitude* regardless.

**Director path:** `DirectorGaze::mGaze` eyeline fields stay 0 -> Director real-cast gaze
unchanged. A Director-side eyeline setting is a named deferral (§12).

---

## 8. Settings & persistence — decided

- **New settings key:** `BDMergeGazeTorsoLagRatio` (F32 1.8, persist) only. Eyeline is per-actor
  session state, not a setting.
- **Existing BDMergeGaze\* keys**: unchanged; two of them (`DeadZone`, `BehindPolicy`/`Angle`)
  gain UI. All persist globally already (`settings.xml:24017-24072`) — they are look-tuning
  preferences, **not** added to `sceneSettingsList` (scene files stay creative-intent-only, and
  these keys predate scenes without being in them).
- **Director scene round-trip of per-actor gaze: NO (deferred, §12).** Scenes persist a settings
  whitelist + cast data (`llfloaterdirector.cpp:611, 645+`); there is no per-actor Actor-Mover
  serialization channel — authored paths do not round-trip either (IMPLIES from the save-tooltip
  inventory `:611` and `sceneSettingsList`). Inventing that channel is a schema change out of
  scope for this delivery; per-actor gaze (incl. eyeline) remains session-only, consistent with
  paths.

---

## 9. Verification plan

### 9.1 Unit-testable (new `algazemath.h` + `indra/newview/tests/algazemath_test.cpp`)

Factor the pure scalar math into a header-only namespace `ALGazeMath` (no viewer deps beyond
llmath), called from `gazePaint`; TUT-registered like `alcinelightrigmodel_test.cpp`:

| Helper (exact port of) | Test asserts |
|---|---|
| `chaseAlpha(dt, tau)` | monotone in dt; `alpha(0)=0`; half-life relation `alpha(tau·ln2) ≈ 0.5` |
| `deadZoneChase(aim_p, aim_y, raw_p, raw_y, dead_zone, alpha)` (port of `:3723-3740`) | raw within zone of seeded aim => aim unchanged for any dt; raw outside => monotone convergence with steady-state error == dead_zone (pull-back-to-edge, no limit-cycle across the threshold); yaw wraps via `llsimple_angle` |
| `torsoChase(t_p, t_y, tgt_p, tgt_y, ratio, dt, tau_user)` (§4.2) | ratio<=1 snaps exactly to target (legacy equivalence); ratio 1.8: after a step target, head-path error (deadZoneChase output) < torso error for all sampled t>0 (the trail exists); both converge to the same pose (no steady-state twist) |
| `behindEnvStep(env, behind, dt, ease_time)` (port of `:3704-3706`) | clamped [0,1]; monotone per direction; symmetric rates; ease_time 0 => immediate |
| `eyelineOffsetDir(dir, up_axis, yaw, pitch)` (§7.2) | zero offset returns input bit-exact; pure yaw rotates by exactly yaw about up (angle-between check); degenerate up×dir leaves dir untouched |

Also keep the CineLightRig M2 lesson: float assertions with tolerance, never exact equality.

### 9.2 In-world only (checklist for the post-build pass)

1. **R1 (highest):** camera-gaze subject + engage Bone Lock on the same subject -> gaze cuts hard
   the same frame, camera rock-steady, no oscillation; release Bone Lock -> gaze eases back in
   (~0.35 s), no snap-from-stale-direction. Repeat via the Director switcher mode path.
2. **Lag differential:** whip-pan the flycam around a camera-gazing clone at Smoothing 0.5:
   head leads, chest visibly trails and settles later; `BDMergeGazeTorsoLagRatio = 1` restores
   today's rigid block (A/B).
3. **Break-off + dead zone via the new panel:** enable break-off at 120 deg, orbit behind the
   actor -> gaze releases smoothly, animation reclaims the head; re-enter the frontal cone ->
   re-engages through the ease. Dead zone 0 vs 5 deg: micro-servo visible vs gone on a
   near-centred hold; eyes still micro-track inside the zone.
4. **Interlock watch item:** camera-gaze subject as head-framing target (non-bone-lock cinematic
   modes) — confirm no visible hunt (decision §7.1 says none expected).
5. **Phase 2:** close-up on a tracking clone -> blinks + saccade morphs; disable gaze mid-blink
   -> lids reopen (neutralization works); hide clone / drop actor / replace runtime -> no
   dangling motion (re-show a hidden gaze-enabled clone -> blinking resumes). Confirm a
   long-shot clone does not blink (LOD gate) and that non-ghost cast members are untouched.
6. **R2 mesh heads:** LeLutka/Catwa/Genus-class heads + eyeline offset — offset direction and
   magnitude read correctly (sign check §7.2); Bento alt eyes track (`:3890-3891`).
7. **UI reflow:** both hosts, default floater size — bottom row of both Move tabs reachable;
   Path tabs show no dead scroll; Ghost Studio Pose tab unchanged.
8. **Eyes-only mode** (blend 0) with eyeline: eyes sit off-lens by the offset (proves the shared
   direction is what offsets, not the head clamp path).

---

## 10. Risks, ranked

1. **R1 regression (highest).** The interlock exists and is correct; Phases 1/4 edit the SAME
   functions (`applyGaze`, `gazePaint`). A misplaced early-return, a reordered check, or an
   "improvement" easing the hard cut ships an oscillating camera. Mitigation: OFF-LIMITS list
   (§11), zero interlock code changes, in-world check #1 mandatory before merge.
2. **Every-avatar shared path.** `applyGaze` runs from `updateCharacter` for every avatar class
   (`llvoavatar.cpp:5218-5221`). The map-miss fast path (`:3073-3081`) must remain the first
   statements; the Phase-2 ghost sync sits after the map hit and behind `isGhostAvatar()`; all
   new `Gaze` fields are POD with safe defaults (no allocation, no behavior for non-configured
   avatars). Any deviation turns a machinima feature into an all-avatars perf/correctness bug.
3. **UI clipping (the M1 class).** Six wrapper heights + ten shifted widgets across two hosts
   (§6.4). Mitigation: the enumerated table is the diff; review checklist #7.
4. **Torso-trail feel drift.** Default ratio 1.8 changes existing users' gaze look (deliberately
   — it is the feature). Escape hatch `= 1.0`; Director real-cast gaze inherits the trail (§4.4,
   intended). Behind-seam torso rate ~97 deg/s is an INFERS tuning judgment — watch in check #2.
5. **Phase-2 lifecycle leaks.** Dangling `ANIM_AGENT_EYE` after drop/replace/hide, or frozen
   half-blink. Mitigation: single idempotent setter + three enumerated teardown hooks +
   neutralization (§5.2-5.3); in-world check #5.
6. **R2 mesh heads (unchanged exposure, medium).** Third-party rigs may parent eyes oddly;
   not determinable statically. Eyeline makes it more visible, not more broken. Check #6.
7. **R3 impostored real avatars (accepted, deferred).** `updateCharacter` early-outs before
   `applyGaze` for throttled non-self avatars (`llvoavatar.cpp:5137-5144`) -> stale head angle at
   distance. Fix sketch (pin `mUpdatePeriod = 1` for gaze-enabled real avatars) touches the
   every-avatar LOD path — separate follow-on (§12).

---

## 11. OFF-LIMITS (Codex must not touch)

- `gazeCameraSafe()` (`llactormover.cpp:3024-3041`): no edits — not the check order, not the
  `director_look_at` split, not the cached controls.
- The hard-cut branch (`:3095-3110`): keep position (before envelope advance), keep full state
  invalidation, keep the no-paint return. Additive-only edits elsewhere in `applyGaze` (the
  Phase-2 sync insert at `:3082` and Phase-1 state fields).
- The Director look-at subsystem (`:3156-3529`) and its arbitration in `llvoavatar.cpp:5113,
  5137-5144, 5218-5221`: read-only. New `Gaze` fields must default to no-op inside
  `DirectorGaze::mGaze`.
- The idle camera dispatch order (`llappviewer.cpp`) — the avatar-before-camera one-frame lag is
  a free feedback damper; never "fix" it.
- Never write: `gAgentCamera.mLookAt` / any `LLHUDEffectLookAt`,
  `setAnimationData("LookAtPoint", …)`, `startMotion`/`stopMotion` on **real** avatars,
  `mSignaledAnimations` (the research's leak paths — all still valid).
- `indra/llcharacter/` sources (`llheadrotmotion.*`, `llmotioncontroller.*`, `llpose.*`,
  `lljointstate.*`): consumed as-is; no priority-system edits.
- Head chain dynamics: `mBodyAim*` dead-zone chase, `mApplied*` slew stage, `GAZE_NECK_LAG`,
  eye stage math — unchanged except the enumerated torso-application swap (§4.2) and the
  `rootWorld` hoist (§7.2).
- `panel_actor_mover.xml` / `ALPanelActorMover` contents (new sibling panel instead);
  Ghost Studio panel + `ALGhostStudio` (except nothing — zero edits there);
  `setEntityPhysicsEnabled` and the physics-motion wiring.
- Existing settings keys' names/defaults; `sceneSettingsList` (no additions this delivery).
- `GAZE_TANGENT`'s Move requirement (`:3084-3094, 3111-3112`) — legacy behavior stays.

---

## 12. Deferrals (decided up front, with reasons)

| Item | Reason |
|---|---|
| Per-actor gaze in Director scene files | No per-actor mover serialization channel exists (paths don't round-trip either); schema work, separate slice. §8 |
| `/ghostlens`-style chat command | Research wishlist; not in the seed's decision list; orthogonal plumbing. |
| Ghost Studio Pose-tab superset rows (eyeline/break-off there too) | Global knobs already reach clones via settings; eyeline reachable via console for cast clones; avoids a third widget copy + `pose_section`/`pose_scroll_body` (700-high, `panel_ghost_studio.xml:204,207`) reflow. Future: embed `ALPanelLensGaze` there outright. |
| R3 impostor pinning (`mUpdatePeriod = 1` for gaze-enabled real avatars) | Touches the every-avatar LOD path; risk class of its own; one-line sketch recorded in §10.7. |
| Director-side eyeline setting | Director gaze intentionally minimal (`DirectorLookAtCamera*` set); add on demand. |
| Extending head-framing/orbit guards to legacy Actor-Mover camera gaze | Decided against on damping analysis (§7.1); one-word change if the watch item (§9.2 #4) disproves it. |

---

## 13. Codex work order (file-by-file)

1. **`indra/newview/algazemath.h`** (NEW): `ALGazeMath::{chaseAlpha, deadZoneChase, torsoChase,
   behindEnvStep, eyelineOffsetDir}` — pure, header-only. §4.2, §7.2, §9.1.
2. **`indra/newview/llactormover.h`**: `Gaze` += `mTorsoAimPitch/mTorsoAimYaw` (runtime),
   `mEyelineYawDeg/mEyelinePitchDeg` (authored); accessor decls `setGazeEyelineOffset` /
   `getGazeEyelineYaw` / `getGazeEyelinePitch`.
3. **`indra/newview/llactormover.cpp`**: eyeline accessors (by `:819-828`); Phase-2 ghost sync
   insert (`:3082`); dropActor + replaceRuntime eye-motion teardown (`:2451, 2521`); gazePaint:
   eyeline offset insert + `rootWorld` hoist (§7.2), torso chase + torso application swap
   (§4.2), all math via `ALGazeMath`.
4. **`indra/newview/llghostavatar.h/.cpp`**: `setEntityEyeMotionEnabled`,
   `neutralizeEntityEyeParams`, `mEntityEyeMotionEnabled`, `setEntityCloneVisible` hook. §5.2.
5. **`indra/newview/alpanellensgaze.h/.cpp`** (NEW) + **`panel_lens_gaze.xml`** (NEW): §6.3;
   injector + CMakeLists entries.
6. **`floater_director.xml`** / **`floater_actor_mover.xml`**: embeds + the full §6.4 table.
7. **`panel_path_editor.xml`** + **`alpanelpatheditor.h/.cpp`**: gaze block removal. §6.2.
8. **`app_settings/settings.xml`**: `BDMergeGazeTorsoLagRatio`. §4.4.
9. **`indra/newview/tests/algazemath_test.cpp`** (NEW) + CMake test registration. §9.1.
10. **`llfloaterdirector.cpp`**: feed `ALPanelLensGaze::setSelectedActors` beside the existing
    transport feed in `refreshMoveTab()` (and the standalone floater's equivalent). §6.1.

Anything not listed here or in §6.4 is off-limits per §11.

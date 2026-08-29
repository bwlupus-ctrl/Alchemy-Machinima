# Character Mover V2 — Deep Research and Next Coding Steps

**Repository:** `I:\alchemy-machinima`  
**Code baseline:** `feature/ultimate-diopter` at `3db1c1b4a943b943e8583925e6191085e763c11d`  
**Research date:** 2026-08-29  
**Status:** implementation-ready research and sequencing document; no production code is changed by this document  
**Scope:** native Character/Actor Mover locomotion, path fidelity, orientation, grounding, and air movement for locally directed avatars and control-avatar actors

This replaces the earlier open-question brief. It distinguishes verified behavior in the current tree from recommendations, resolves the first architectural decisions, and orders the work into reviewable patches with explicit gates.

## 1. Executive decision

Do **not** begin V2 with walk/run thresholds, new IK, rotation-minimizing frames, or parametric circles. The first dependency is a mover-owned animation sampling layer.

The current mover calls `LLCharacter::setAnimTimeFactor()` at every start, update, dwell, follower hold, stop, suspend, and replacement path (`indra/newview/llactormover.cpp:2276`, `:2315`, `:2354`, `:2375`, `:2601`, `:2668`, `:2690`, `:2921`, `:3042`, `:3076`, `:3169`, `:3357`, `:3483`, `:3493`, `:3521`, `:3621`). That function delegates to `LLMotionController::setTimeFactor()` (`indra/llcharacter/llcharacter.h:169`). The factor advances the controller's single `mAnimTime` (`indra/llcharacter/llmotioncontroller.cpp:858-904`), and every active motion samples that same clock (`:724`, `:748`, `:771`). It therefore retimes the entire avatar: locomotion, AO replacements, gestures, gaze clips, secondary animations, and any other active motion.

This is not merely a limitation for future gait blending. It is a present ownership defect:

- Actor Mover cadence can visibly slow or accelerate unrelated animations.
- `stop()` writes `1.0` rather than restoring an owned value, so it can overwrite slow-motion or another subsystem's time factor.
- A follower hold writes `0.0`, freezing every active motion on the avatar, not only the stride.
- Two gait clips cannot be independently phase-aligned or cadence-matched while both are driven by the same controller clock.

The V2 foundation must therefore provide, per motion instance:

1. an opt-in **absolute local sample time**;
2. an opt-in **presentation weight multiplier**;
3. default values that reproduce the stock controller exactly;
4. complete reset on deactivation; and
5. an exact local-start/stop ownership transaction that accounts for AO remapping.

Once that layer exists, Character Mover should derive locomotion sample time from **distance traveled**, not integrate another playback-rate clock:

```text
cycle_duration  = loop_out - loop_in
stride_distance = nominal_speed * cycle_duration
clip_time       = loop_in + phase_offset * cycle_duration
                  + traveled_distance / nominal_speed
```

The key production analogy is distance matching: animation pose is selected from distance, rather than allowing an independent animation clock to drift away from character displacement. Epic's official implementation likewise advances or selects an animation pose from a distance curve rather than time alone ([Unreal Engine Distance Matching](https://dev.epicgames.com/documentation/unreal-engine/distance-matching-in-unreal-engine)). Character Mover has a simpler case because it owns the path distance and uses in-place clips.

### Recommended ship order

| Order | Patch | Outcome |
|---:|---|---|
| 0 | Characterization and pure clock tests | Locks down stock behavior, ownership, loading, and scrub invariants before controller changes. |
| 1 | Per-motion sample clock and weight | Supplies a default-inert primitive; no Actor Mover behavior changes. |
| 2 | Local-motion ownership resolver | Starts/stops the exact local motion and balances AO state without network side effects. |
| 3 | One-clip migration | Removes **all Actor Mover** writes to avatar-global time and proves no visual regression for a single clip. |
| 4 | Locomotion-set data and phase metadata | Generalizes the existing foot-phase analyzer to arbitrary user keyframe assets; adds versioned scene data. |
| 5 | Walk/run/start/stop/dwell state machine | Adds phase-matched, distance-driven crossfades and deterministic transition records. |
| 6 | Path math extraction and adaptive arc table | Removes fixed 24-sample speed ripple behind unit-tested pure geometry. |
| 7 | Facing and orientation | Fixes reverse travel, look-ahead, scalar C2 yaw, then adds 3D frames only where required. |
| 8 | Mover-aware contact hints and ground profile | Composes with existing Pose Polish; does not build another IK system. |
| 9 | Fly/hover/takeoff/land | Reuses the locomotion runtime with air roles and no contact lock. |
| 10 | Parametric arc/circle/helix spans | Adds exact primitives after the shared evaluator, persistence, and tests are stable. |

Patches 0–3 are the first coding block. Patch 5 is the first major visible V2 feature. Patches 6–10 should not delay landing the animation ownership correction.

## 2. Verified current architecture

### 2.1 Root and path ownership

`LLActorMover::applyOverride()` advances a move once per viewer frame, caches the result, and writes the avatar root's world position and rotation (`indra/newview/llactormover.cpp:2512-2613`). The root path is local/render-only; no simulator position is authored.

The path core is a global-coordinate centripetal Catmull–Rom evaluator (`centripetalCR`, `llactormover.cpp:180-205`) with reflected open endpoints and loop wrapping (`Path::evalSegment`, `:329-364`). `Path::rebuild()` produces a cumulative arc table using a fixed 24 subdivisions per segment (`:366-401`). `Path::evalAtDistance()` binary-searches the table and re-evaluates the true curve (`:403-450`). Position remains on the curve; table error appears primarily as distance-to-parameter error and therefore speed ripple.

`advancePath()` supports stop, loop, ping-pong, dwell, speed overrides, ease, arrival facing, ground follow, and recorder-playhead sync (`llactormover.cpp:3196-3496`). `advanceFollower()` samples the leader's path at an offset (`:3506-3623`). Preview and traversal call the same `evalAtDistance()` evaluator, so XY preview parity is structurally strong.

The native self-turn feedback loop is already blocked while the mover owns the root. `LLVOAvatar::updateOrientation()` queries `LLActorMover::isDriving()` and suppresses `mTurning`/native control feedback (`indra/newview/llvoavatar.cpp:4774-4830`). `isDriving()` means a move exists and is not suspended (`llactormover.cpp:2615-2619`). Every V2 state must preserve that definition. A locomotion transition must not temporarily remove the move entry merely because one clip is ending.

### 2.2 Current locomotion choice and lifecycle

`locomotion_anim()` chooses, in order, a cast-member override, a shared custom UUID, or system walk (`llactormover.cpp:124-145`). `apply_custom_anim_priority()` applies a per-instance priority override to a loaded custom motion (`:153-168`). The priority override itself is correctly instance-local and is cleared on deactivation (`indra/llcharacter/llmotion.cpp:168-207`).

The `Move` runtime stores one locomotion UUID and one dwell UUID (`indra/newview/llactormover.h:963-1020`). Animation lifecycle logic is spread across start, path dwell, arrival, follower hold, suspend, resume, actor replacement, stop, and stop-all paths. V2 should centralize that logic before adding a second simultaneous clip.

`stop()` now erases a stale move even when the actor cannot be resolved, which is necessary for the existing drift fix (`llactormover.cpp:2331-2357`). That erase-before-visual-cleanup behavior must remain. V2's clip records must be safely destructible even when no avatar instance is available.

### 2.3 Motion-controller facts that constrain V2

`LLMotionController::startMotion(id, start_offset)` creates or activates one canonical motion instance per UUID and implements phase seeking by setting activation time to `mAnimTime - start_offset` (`indra/llcharacter/llmotioncontroller.cpp:414-450`). A restart can deprecate the old instance and replace the canonical map entry (`:419-429`). UUID-only control is therefore insufficient if a feature deliberately restarts the same asset; V2 should avoid same-UUID crossfades and treat two roles resolving to the same UUID as one clip.

The controller computes stock pose weights for LOD fade, ease-in, active, and ease-out states (`llmotioncontroller.cpp:646-778`) and then adds the motion to `LLPoseBlender` (`:790`). `LLMotion::mFadeWeight` is specifically LOD state (`llmotion.h:213`); V2 must not repurpose it as a cinematic blend envelope.

Looping keyframe motions delay their ordinary stop to a loop/tail boundary (`indra/llcharacter/llkeyframemotion.cpp:1256-1275`). That is valuable for normal animation playback but unsuitable for a precisely timed walk/run crossfade. The V2 transition should keep both loops active, drive explicit weights, then stop the fully faded outgoing motion immediately.

Asynchronous loading is another boundary. A loading motion is marked started, then later activated at the controller's current time (`llmotioncontroller.cpp:807-826`); the original start offset is not retained. A V2 external sample seed must be stored on the motion instance so the first loaded pose is the desired phase, not frame zero.

### 2.4 Useful locomotion work already exists

The earlier brief proposed offline foot-phase inference as new work, but this fork already implements it in `LLKeyframeWalkMotion`:

- It samples left/right ankle rotation curves and optional position curves over the loop.
- It finds low-activity windows as plant proxies.
- It rejects implausibly short/long windows and non-alternating feet.
- It caches phase information per asset.
- It publishes a canonical phase per character and seeds a newly activated walk/run clip from a fresh compatible handoff.

See `indra/llcharacter/llkeyframewalkmotion.h:70-146` and `llkeyframewalkmotion.cpp:250-498`.

The limitation is class reach: unknown/custom animation UUIDs are created as generic `LLKeyframeMotion` (`llmotioncontroller.cpp:96-120`). The plant analyzer and handoff are private to `LLKeyframeWalkMotion`, so an arbitrary user-selected locomotion asset cannot use them. V2 should extract/generalize this proven analyzer; it should not create a competing detector in `LLActorMover`.

### 2.5 AO and local-motion ownership

`LLVOAvatar::startMotion()` is not a purely local visual call for self. It consults `AOEngine::override()` and may send an animation request for the replacement before returning without locally starting the requested UUID (`indra/newview/llvoavatar.cpp:7223-7260`). `LLVOAvatar::stopMotion()` mirrors that behavior (`:7265-7301`). Current Actor Mover calls this wrapper, although its root-driving contract is render-only.

There is already a correct local-ownership pattern in the same class. Director body-turn animation:

1. records source and resolved UUID separately;
2. balances every AO `override(source, true)` with `override(source, false)`;
3. calls qualified `av->LLCharacter::startMotion()` / `stopMotion()` to stay local;
4. avoids taking ownership of a motion that was already active; and
5. checks simulator ownership before stopping a remote motion.

See `LLActorMover::stopDirectorTurnAnimation()` and `updateDirectorTurnAnimation()` (`llactormover.cpp:4134-4221`). V2 locomotion should reuse a generalized form of this transaction rather than duplicate slightly different AO rules.

Recommended policy:

- Explicit custom role assets play exactly as authored and do not get replaced by AO.
- Built-in fallback roles may honor the self AO, because that preserves the user's selected walk/run style.
- Every start returns a record containing source UUID, actual local UUID, whether the AO transaction is active, and whether Actor Mover owns the local instance.
- Every exit path balances AO state even if loading failed, the actor disappeared, or no replacement UUID was returned.

### 2.6 Pose Polish and grounding

The fork already has the required light-touch grounding pieces:

- `ALContactStab` infers plant/release with speed hysteresis and dwell, then holds a world-space lock (`indra/newview/alcontactstab.h:48-195`).
- `ALPosePolish::runContact()` copies both leg chains, solves them with the existing `LLJointSolverRP3`, preserves animated ankle orientation, and blends only local hip/knee/ankle rotations (`indra/newview/llvoavatar.cpp:5219-5435`).
- Contact corrections ramp in over 0.15 s and out over 0.08 s, reject corrections above 0.35 m, and reject near-unreachable goals (`:5223-5226`, `:5362-5392`).
- The stage already resets on invalid/big `dt`, sitting, in-air, underwater, skeleton changes, and explicit discontinuities (`alposepolish.h:76-85`; `llvoavatar.cpp:5241-5255`, `:5541-5557`).

The integration gap is input, not IK. `runContact()` currently uses `av->getGround()` under the animated ankle and has no frame-stamped knowledge of Actor Mover path state, expected plant phase, a baked ground profile, or a mover-owned airborne state. V2 should add optional hints to this layer and preserve its current behavior when hints are absent.

### 2.7 Persistence is uneven

Paths and their undo records are session-only. `mPaths` is explicitly documented as session-only (`llactormover.h:1448`), and `PathState` is an authored-field undo snapshot, not durable scene serialization (`:1043-1068`). The prior brief's wording made this too easy to misread.

Cast data does have a scene path. `LLDirectorCast::sceneData()` writes `mLocoAnim` (`indra/newview/lldirectorcast.cpp:1926-1941`) and the load path reads it (`:2124-2144`). Director scenes currently carry `SCENE_VERSION = 4` (`indra/newview/llfloaterdirector.cpp:108`, `:991-993`). A locomotion set belongs in this versioned cast schema, with `mLocoAnim` retained as the legacy walk-role fallback.

Path primitives and baked ground cannot claim persistence until path data is added to the scene schema. That is a separate migration and should be reviewed as such.

### 2.8 `ALTrajectory` is useful, but narrower than the old brief claimed

`ALTrajectory::Segment<T>`, `sample()`, `buildSegment()`, and `retarget()` are templated (`indra/newview/altrajectory.h:51-365`). The finite-value sanitization, polynomial root isolation, monotonicity proof, never-cross proof, duration solver, brake fallback, and shortest-arc adapter are scalar-only (`:570-1089`). A vector type may compile through the generic basis, but it does not automatically gain scalar feasibility guarantees or non-scalar finite-value guards.

The safe first reuse is therefore:

- a scalar arc-distance timing law `s(t)`; and
- a scalar shortest-arc yaw trajectory.

Do not begin by advertising a proven `Segment<LLVector3>` or quaternion solver. A 3D/vector feasibility policy and a quaternion log/exp adapter require their own tests and behavior version.

## 3. Target architecture

### 3.1 Per-motion presentation controls

Add a small default-inert control block to each `LLMotion` instance. Names below are proposals, not a required API spelling:

```cpp
struct LLPresentationControl
{
    enum class ClockMode : U8 { NATIVE, EXTERNAL_SAMPLE };

    ClockMode mClockMode = ClockMode::NATIVE;
    F32       mExternalSampleTime = 0.f;
    F32       mWeight = 1.f;
    U64       mOwnerToken = 0;
};
```

Required semantics:

- `NATIVE` returns exactly `controller_time - activation_timestamp`.
- `EXTERNAL_SAMPLE` returns the last finite nonnegative local clip time supplied by the owner when a keyframe motion evaluates its curves.
- Weight defaults to `1.0` and multiplies the controller-computed pose weight **after** stock lifecycle/ease calculation and **without** replacing LOD fade.
- Non-finite sample times hold the last valid value; non-finite weights resolve safely to `1.0` or reject the write.
- Deactivation resets clock mode, sample time, weight, and owner token before any later replay.
- Mutating calls require the correct owner token; an unrelated subsystem cannot silently take over a motion already presentation-controlled by Actor Mover.
- `LLKeyframeMotion::onUpdate()` must resolve the effective sample before loop wrapping, seam repair, constraint evaluation, and keyframe application. The controller may continue passing its native active time; the keyframe layer replaces that input only for an externally controlled instance.
- `LLKeyframeWalkMotion::onUpdate()` must bypass its internal `"Walk Speed"` adjustment while an external sample is active and publish phase handoff from the effective external sample. Otherwise the special built-in walk/run subclass would time-warp the distance-matched clock a second time.
- The initial `onUpdate(0)` performed by `activateMotionInstance()`, including activation after asynchronous loading, must resolve through the same keyframe-layer helper. This makes a stored external seed take effect on the first evaluated pose without changing stock controller timestamps.
- Non-keyframe motions reject external sample ownership in V2. Locomotion role assets are keyframe motions; widening the contract to procedural motions is separate work.
- Every controller branch that writes a nonzero pose weight—active, ease-in, ease-out, and final-stop—must use one weight helper. Do not patch only the main active branch.

Why absolute sample time first:

- A dwell freezes one clip without freezing the avatar.
- Sync-to-take can seek forward or backward deterministically.
- Path loop seams do not create a one-frame cadence spike.
- A clip can be seeded at a plant-compatible phase after asynchronous load.
- A test can evaluate the same path distance in any call order and obtain the same clip sample.

A per-motion anchored rate mode may be useful elsewhere, but it is unnecessary for Character Mover's first patch and enlarges the continuity surface. Add it only with a separate use case.

#### Stock-ease interaction

The explicit weight is a multiplier, so stock ease can still reduce an incoming motion. For a looping locomotion clip, seed `start_offset` at a phase-equivalent time that is beyond the asset's ease-in interval:

```text
start_offset = loop_in + phase * cycle_duration + N * cycle_duration
choose smallest N such that start_offset > ease_in_duration + epsilon
```

The motion is then in the controller's fully active branch while its local pose is at the desired loop phase. Keep the outgoing loop stock-active during the explicit fade and stop it immediately only after external weight reaches zero. This avoids `LLKeyframeMotion::setStopTime()` delaying the handoff to a loop-tail boundary.

### 3.2 Exact local-motion transaction

Extract the body-turn ownership pattern into a reusable helper owned by the viewer layer, not `llcharacter`:

```cpp
struct ALLocalMotionHandle
{
    LLUUID mSource;
    LLUUID mActual;
    U64    mOwnerToken = 0;
    bool   mAOActive = false;
    bool   mOwnsLocalMotion = false;
    bool   mPendingLoad = false;
};
```

The helper should expose `startLocal()`, `stopLocal()`, `setSampleTime()`, `setWeight()`, and `isReady()`. It must use qualified `LLCharacter` calls after performing explicit remap/AO policy. It must never send a simulator animation request.

Do not make raw `LLMotion*` a long-lived public handle: motion deprecation and purge can invalidate it. Store UUID + owner token and resolve through the controller for each mutation. Avoid same-UUID crossfades; if two roles resolve to the same actual UUID, update metadata/rate in place.

For asynchronous loading, `startLocal()` may return a pending handle. Keep the outgoing clip at weight 1.0 until the incoming motion reports loaded and has valid loop metadata. If loading fails, cancel the transition and continue the outgoing role. Never fade the only valid clip while its replacement is unresolved.

### 3.3 Locomotion phase metadata

Move the existing ankle-curve analyzer out of `LLKeyframeWalkMotion` into a reusable keyframe-motion utility. Preserve its existing acceptance tests and cache behavior.

Recommended metadata:

```cpp
struct LLLocomotionPhaseInfo
{
    bool mValid = false;
    F32  mLoopIn = 0.f;
    F32  mCycleDuration = 0.f;
    F32  mLeftPlantPhase = 0.f;
    F32  mRightPlantOffset = 0.5f;
    F32  mConfidence = 0.f;
};
```

The analyzer should accept `LLKeyframeMotion::JointMotionList` and remain asset-only: no avatar stepping, raycasts, or frame clock. `LLKeyframeWalkMotion` should consume the same shared result so there remains one algorithm and one cache.

Fallback order when phase metadata is invalid:

1. user-authored phase marker in the locomotion-set entry;
2. asset loop-in as phase zero;
3. cold start without phase matching.

Do not reject a locomotion clip solely because plant inference failed. Phase matching is quality metadata, not basic playability.

### 3.4 Locomotion-set schema

Add the set to `LLDirectorCast::CastMember`, because that object already owns the per-actor locomotion override and durable scene representation.

Minimum useful V2 roles:

```text
idle
walk_forward
run_forward
turn_left
turn_right
walk_backward
strafe_left
strafe_right
fly
hover
takeoff
land
```

Do not require all roles. Fallbacks should be deterministic:

```text
run_forward  -> walk_forward
walk_backward/strafe -> walk_forward + authored facing/travel offset
turn_left/right -> root-yaw turn without a turn clip
fly -> walk_forward only when explicitly allowed; otherwise no body clip
hover -> idle
takeoff/land -> fly/hover crossfade
```

Each role entry should include:

- asset UUID;
- nominal speed in metres/second;
- optional manual left-plant phase;
- whether AO replacement is allowed;
- optional minimum/maximum natural retime band for warnings;
- validation state (unknown/loading/valid/non-loop/missing ankle data/failed).

Persist it under a new `locomotion_set` map in the cast member's LLSD. On load:

- if `locomotion_set` exists, use it;
- otherwise map legacy `loco_anim` to `walk_forward`;
- if both are empty, use built-in defaults.

Increment the Director scene version only when the load path is backward compatible and round-trip tests exist. Do not erase `loco_anim` in the first version; keep writing it as the legacy walk mirror for at least one schema generation.

### 3.5 Per-move locomotion runtime

Replace `Move::mAnim`/`mDwellAnim` bookkeeping with a contained runtime while preserving compatibility fields during the first migration:

```cpp
struct LocomotionRuntime
{
    ERole mRole = ERole::NONE;
    ALLocalMotionHandle mPrimary;
    ALLocalMotionHandle mSecondary;

    F64 mTravelOdometer = 0.0;
    F64 mLastPresentationTime = 0.0;
    F32 mPrimaryPhaseOrigin = 0.f;

    TransitionRecord mTransition;
    bool mHavePathSample = false;
    F32  mLastPathDistance = 0.f;
};
```

All starts, holds, dwell swaps, arrival, suspension, resume, actor replacement, and stop operations go through one lifecycle API. A `Move` is still the root-ownership record; locomotion runtime is subordinate and can be empty.

On runtime replacement, never copy a motion handle to the new avatar. Stop/release the old local transaction, keep semantic role/phase/odometer, and start fresh handles on the replacement body. This mirrors the current `migrateActor()` responsibility without carrying invalid controller identity across avatars.

### 3.6 Distance-driven clip time

For a valid cyclic role with nominal speed `v_nom > 0`:

```text
t_clip = phase_origin_seconds + odometer / v_nom
```

The key is the odometer, which records absolute ground distance actually traveled by this move:

- Normal path: add `abs(new_arc - old_arc)`, corrected at loop wrap.
- Ping-pong: add absolute movement through the reversal; do not reset phase at the endpoint.
- Legacy straight move: add the absolute change in evaluated segment distance.
- Follower: add seam-corrected absolute change in follower arc.
- Dwell/hold: add zero, so the stride freezes while other avatar motions continue.
- Recorder sync: derive clip time directly from playhead-driven arc for deterministic seeks. Backward scrubbing intentionally samples the locomotion clip backward.

Use F64 for accumulated distance and presentation time. Convert only the final local sample to F32 because the keyframe API is F32. Keep clip time bounded by reducing whole loop cycles before conversion; otherwise a long capture session eventually loses phase precision.

This is the strongest anti-footskate foundation available without root-motion curves. It is more deterministic than integrating a rate and directly follows the path-distance authority.

### 3.7 Gait selection and transitions

Use a small state machine, not a general animation graph.

Inputs already available or cheaply derivable:

- absolute and signed path speed;
- acceleration from the path speed law;
- horizontal travel direction;
- desired facing direction;
- signed yaw rate/curvature;
- path slope;
- distance to stop/dwell;
- ground/air state; and
- recorder seek/discontinuity flag.

First visible state set:

```text
IDLE -> STARTING -> WALK -> RUN -> STOPPING -> IDLE
                      \-> TURN_IN_PLACE
                      \-> DWELL
```

Add backward/strafe and air states only after this path is stable.

Walk/run selection should use hysteresis, but threshold values must be derived from the configured clips, not copied blindly from another engine:

```text
upshift   = midpoint(walk_nominal, run_nominal) + hysteresis
downshift = midpoint(walk_nominal, run_nominal) - hysteresis
```

Froude number is useful as an authoring/default sanity check, not as the sole runtime selector. Human walk/run transition is commonly near `Fr = v²/(gL) ≈ 0.5`, with leg length `L` ([Kram, Domingo, and Ferris 1997](https://spot.colorado.edu/~kram/walk-run.pdf); [Alexander 1983](https://zslpublications.onlinelibrary.wiley.com/doi/pdf/10.1111/j.1469-7998.1983.tb04266.x)). SL animation assets and avatar proportions vary too widely to hardcode the biological threshold as a visual truth.

Transition record:

```cpp
struct TransitionRecord
{
    ERole mFrom;
    ERole mTo;
    F64   mStartPresentationTime;
    F32   mDuration;
    F32   mFromWeight;
    F32   mToWeight;
    U64   mSerial;
};
```

Weights are pure functions of presentation time (or recorder playhead), for example quintic smootherstep over `[start, start + duration]`. Do not use `weight += dt * rate`; that produces call-order and frame-rate dependence.

For normal forward playback, hysteresis state is retained in the move. For a discontinuous recorder seek, rebuild the desired role from speed at the target sample and either:

- snap to the single role when accurate scrubbing is more important than a transition pose; or
- evaluate a persisted transition record if the scene/take explicitly authored one.

Do not invent historical state by replaying thousands of frames during a seek. OpenUSD's model—author/query values at explicit time samples—is the right mental model for capture determinism ([OpenUSD: Time and Animated Values](https://openusd.org/release/user_guides/time_and_animated_values.html)).

For a two-clip system, prefer hysteretic selection plus a short phase-matched crossfade over continuous blend-tree mixing. Continuous blend spaces are useful with coordinated clip families and multiple dimensions ([Unity 2D Blend Trees](https://docs.unity3d.com/cn/2018.3/Manual/BlendTree-2DBlending.html)), but arbitrary SL user clips are not guaranteed to be registration-compatible. Kovar and Gleicher's registration-curve work shows that successful blends depend on aligned timing, coordinate frames, and constraints, not weight interpolation alone ([Flexible Automatic Motion Blending with Registration Curves](https://graphics.cs.wisc.edu/Papers/2003/KG03/regCurves.pdf)).

### 3.8 Orientation strategy

Do orientation in three increments.

#### Increment A: grounded facing correctness

- Multiply travel tangent by the movement direction before deriving yaw. Current ping-pong reverse motion keeps the forward tangent, which can read as backpedaling when the intent was turn-and-walk.
- Keep an explicit facing mode: travel, fixed, target, or travel-plus-offset.
- Add distance/time look-ahead for the target yaw so the body begins a turn before the root reaches high curvature.
- Drive yaw through `ALTrajectory::solveShortestArc()` for ordinary playback. On a recorder seek, sample/snap directly rather than pretending the intervening turn occurred.

This uses the proven scalar path and removes the current frame-integrated turn-rate clamp without requiring a quaternion solver.

#### Increment B: grounded lean

Estimate signed horizontal curvature from neighboring arc samples. A physically motivated seed is `atan(v² * curvature / g)`, then multiply by an artistic strength and clamp conservatively. For bipeds, apply most of the lean as a post-blend pelvis/torso adjustment, not permanent root roll, so foot contact and world-up placement remain stable. Epic's current sample similarly applies a lateral-acceleration lean over locomotion and warns against over-leaning once curved source clips exist ([Game Animation Sample](https://dev.epicgames.com/documentation/unreal-engine/game-animation-sample-project-in-unreal-engine)).

#### Increment C: 3D/flying frame

Only fly/helix paths need a full moving frame. Use the double-reflection rotation-minimizing frame method, which has fourth-order global approximation error at comparable per-step cost to older second-order projection/rotation methods ([Wang et al. 2008](https://doi.org/10.1145/1330511.1330513)). Cache frames at the adaptive arc samples, hemisphere-correct quaternions, and distribute loop-closure holonomy over the loop.

Do not use a Frenet frame as the default: curvature approaching zero makes its normal ill-conditioned and inflection points can flip the frame.

A C2 quaternion retargeter is later research. Shoemake's spherical spline work is the baseline for smooth quaternion interpolation ([Animating Rotation with Quaternion Curves](https://doi.org/10.1145/325334.325242)), but it does not automatically provide the angular-velocity/acceleration cap and interruption guarantees of this fork's scalar solver. V2 should not block grounded facing on that larger problem.

### 3.9 Ground profile and Pose Polish hints

Separate curve geometry from environmental ground data.

`Path::mArc` should remain a geometry cache. Add an optional `GroundProfile` cache keyed by arc distance with:

- resolved ground height;
- optional surface normal;
- hit/miss confidence;
- bake generation/scene epoch; and
- explicit invalidation state.

The director chooses **Bake Ground**. Preview and traversal then sample the same profile. Live ground follow may apply a small, bounded correction over the bake, but it should not silently replace the promised curve with unrelated per-frame ray results. This also makes capture playback deterministic when scene geometry does not change.

Do not begin with 2–3 new rays per actor per frame. First profile the existing root ray and `ALPosePolish`'s two `getGround()` calls. Then choose among edit-time baking, spatial cache, and temporal decimation using measured viewer timings. There is no defensible universal ray budget for SL's mixture of terrain, prims, mesh, rigged objects, and octree state.

Add a frame-stamped optional hint channel to Pose Polish:

```cpp
struct ContactHints
{
    U32  mFrame = 0xFFFFFFFF;
    bool mMoverOwnsRoot = false;
    bool mGrounded = false;
    bool mDiscontinuity = false;
    F32  mExpectedPlant[2] = { -1.f, -1.f }; // -1 unknown, 0 swing, 1 plant
    bool mHaveGround[2] = { false, false };
    F32  mGroundZ[2] = { 0.f, 0.f };
    LLVector3 mGroundNormal[2];
};
```

Hints are advisory:

- stale frame -> ignore;
- missing expected phase -> keep current velocity/height inference;
- expected swing -> accelerate release, never force a planted foot to teleport;
- expected plant -> shorten candidate dwell only when height/reach checks pass;
- discontinuity -> reset contact state before solving;
- airborne -> release both contacts.

This keeps the existing tested contact inference as the fallback and prevents the mover from directly manipulating leg joints.

Kovar, Schreiner, and Gleicher's footskate cleanup work supports the same constraint-first principle: explicitly identify footplant intervals and enforce them with smooth, local corrections rather than globally filtering away high-frequency contact detail ([Footskate Cleanup for Motion Capture Editing](https://pages.cs.wisc.edu/~kovar/footskateCleanup.pdf)).

### 3.10 Path fidelity

Before adding primitives, extract the current path evaluator into a pure, header-level math/data module that can be unit tested without the `LLActorMover` singleton or viewer globals. `LLActorMover`, object path mover, Ghost Studio formation placement, and the preview renderer must all call the same module.

Replace fixed `SUB = 24` with adaptive subdivision that produces inversion samples directly. For an interval `[t0, t1]`, evaluate quarter, midpoint, and three-quarter points; compare the fine polyline length with the endpoint chord and measure sample deviation from the chord. Subdivide until both error tests pass or a hard depth/sample cap is reached.

Initial tuning seeds—not frozen product constants:

```text
position deviation tolerance: 0.005 m
length excess tolerance:      0.002 m
maximum recursion depth:      12
maximum samples/segment:      4096
```

The table needs both a position-flatness check and a length check because an S-shaped interval can have a misleading midpoint. The exact formal upper/lower bounds available for Bézier control polygons do not directly apply to the current evaluator without converting each segment to an equivalent polynomial representation. Gravesen's adaptive work explains the chord-versus-control-polygon bound for Bézier curves and motivates error-controlled subdivision ([Adaptive Subdivision and the Length and Energy of Bézier Curves](https://doi.org/10.1016/0925-7721(95)00054-2)). The practical evaluator-based test above is the lower-risk first patch; a formal Bézier conversion can follow if required.

Acceptance is measured, not visual only:

- monotonically increasing `mDist`;
- exact first/last node positions;
- closed loop seam position and tangent tolerance;
- maximum speed ripple under fixed-distance stepping;
- identical table for identical authored inputs;
- hard bounds on sample count for coincident and pathological nodes.

The current centripetal parameterization is a sound base; Yuksel, Schaefer, and Keyser document why centripetal Catmull–Rom avoids the cusp/self-intersection pathologies of other parameterizations ([On the Parameterization of Catmull–Rom Curves](https://www.cemyuksel.com/research/catmullrom_param/catmullrom_slides.pdf)). Preserve it.

### 3.11 Parametric spans

After pure path math and persistence exist, represent a path as a list of spans behind one interface:

```cpp
eval(t) -> position
derivative(t) -> tangent
appendArcSamples(tolerance, output)
serialize()/deserialize()
```

First span types:

1. existing waypoint Catmull–Rom;
2. circular arc (center, plane, radius, start angle, sweep);
3. helix (center/axis/radius/pitch/sweep).

Circle and helix arc length are analytic; use exact distance-to-parameter mapping. Spiral fitting, clothoid joins, and automatic shape recognition are later authoring features, not prerequisites for exact user-created primitives. If circle suggestion is added, use a geometric least-squares fit and show residual error before conversion; never silently replace waypoints. Gander, Golub, and Strebel provide the relevant geometric-fit foundation ([Least-Squares Fitting of Circles and Ellipses](https://www.research-collection.ethz.ch/items/c59d9093-9710-46d2-aa51-0f2029de55e4)).

Blender's current Follow Path contract is a useful UX reference: explicit path position/evaluation time, forward axis, up axis, and follow-curve toggle are separate controls ([Blender Follow Path Constraint](https://docs.blender.org/manual/en/dev/animation/constraints/relationship/follow_path.html)). Character Mover should likewise keep path position, facing, and look target independent.

## 4. Detailed coding plan

### Patch 0 — Characterization, test seams, and ownership inventory

**Purpose:** make controller and mover behavior observable before changing it.

**Files:**

- `indra/llcharacter/llmotion.h/.cpp`
- `indra/llcharacter/llmotioncontroller.h/.cpp`
- `indra/llcharacter/tests/llmotionpresentation_test.cpp` (new)
- `indra/llcharacter/CMakeLists.txt`
- `indra/newview/llactormover.cpp/.h`

**Work:**

1. Add a pure helper for native/external sample mapping and presentation weight sanitation; test it independently.
2. Add debug-only/controller test access sufficient to assert the active time passed to a test motion.
3. Add a scoped `ActorMoverLocomotionDebug` trace showing actor, role, source/actual UUID, owner token, local time, effective weight, and transition serial.
4. Enumerate all Actor Mover global time-factor writes in one comment/checklist. Do not change behavior yet.
5. Record the current single-clip output at normal speed, pause, dwell, path loop seam, ping-pong, follower wait, recorder scrub, suspend/resume, runtime replacement, and stop.

**Tests/gates:**

- Native sample mapping is exact for ordinary, negative-offset, and large controller times.
- External sample holds and permits backward seeks.
- Default weight is exactly 1.
- NaN/Inf inputs never reach motion `onUpdate()`.
- No shipping behavior change with the V2 gate off.

### Patch 1 — Per-motion sample clock and explicit weight

**Purpose:** add the core primitive without touching Actor Mover.

**Files:**

- `indra/llcharacter/llmotion.h/.cpp`
- `indra/llcharacter/llmotioncontroller.h/.cpp`
- `indra/llcharacter/llkeyframemotion.h/.cpp`
- `indra/llcharacter/llkeyframewalkmotion.h/.cpp`
- `indra/llcharacter/llcharacter.h`
- `indra/llcharacter/tests/llmotionpresentation_test.cpp`

**Work:**

1. Store presentation control per `LLMotion` instance.
2. Add controller-owned mutation functions keyed by UUID + owner token.
3. Resolve the external sample inside `LLKeyframeMotion::onUpdate()` before curve evaluation; keep the native argument unchanged when no owner exists.
4. Make `LLKeyframeWalkMotion` consume the external sample directly rather than applying its `"Walk Speed"` clock a second time; preserve the exact stock branch when no owner exists.
5. Multiply each nonzero stock pose weight by the explicit weight in one controller helper.
6. Verify the existing `activateMotionInstance()->onUpdate(0)` and loading activation paths both reach the keyframe resolver.
7. Reset controls during `LLMotion::deactivate()` alongside the priority override.
8. Reject ownership conflicts and log once per conflict.

**Adversarial cases:**

- motion is loading when controls are installed;
- motion deprecates/restarts;
- motion is LOD-faded;
- controller is paused;
- controller global time factor changes;
- external sample moves backward;
- caller disappears without explicitly releasing controls;
- asset is non-looping and self-terminates.

**Gate:** with no presentation owner, test-motion active times and pose weights must match the pre-patch baseline exactly.

### Patch 2 — Local motion ownership resolver

**Purpose:** make all mover animation starts local, explicit, and balanced.

**Files:**

- `indra/newview/allocalmotion.h/.cpp` (new; name may vary)
- `indra/newview/llactormover.cpp/.h`
- `indra/newview/CMakeLists.txt`
- pure/helper tests where possible

**Work:**

1. Extract/generalize the Director turn-animation AO transaction.
2. Start exact local motions via qualified `LLCharacter::startMotion()`.
3. Track source/actual UUID, AO transaction, local ownership, load state, and owner token.
4. Balance AO and release presentation controls on every stop path.
5. Refactor body-turn code to consume the same helper, proving the abstraction handles an existing feature.

**Gate:** starting/stopping a self-avatar built-in walk, an AO replacement, a custom asset, a remote avatar animation, and a ghost/control-avatar animation leaves no orphan loop and sends no Actor-Mover-originated animation request.

### Patch 3 — Single-clip Character Mover migration

**Purpose:** remove the current global-clock defect before adding gait features.

**Files:**

- `indra/newview/llactormover.cpp/.h`
- optional `indra/newview/alactormoverlocomotion.h/.cpp`

**Work:**

1. Add `LocomotionRuntime` but initially support one role only.
2. Route legacy, path, follower, dwell, suspend/resume, replacement, stop, and stop-all through it.
3. Compute clip sample from traveled distance and nominal speed.
4. Hold sample time during dwell/follower wait instead of setting avatar time factor to zero.
5. Remove every `setAnimTimeFactor()` call owned by `LLActorMover`; do not alter Ghost Studio's intentional whole-entity time controls.
6. Do not capture/restore the old global factor. Actor Mover should simply cease owning it; restoring a stale value could still clobber a concurrent owner.

**Gate:** while an actor walks, an unrelated simultaneous gesture/AO/gaze animation retains its original timing. Stopping the mover does not change the avatar's pre-existing global time factor.

### Patch 4 — Shared phase metadata and locomotion-set persistence

**Purpose:** make arbitrary user clips phase-aware and authorable.

**Files:**

- `indra/llcharacter/lllocomotionphase.h/.cpp` (or equivalent)
- `indra/llcharacter/llkeyframemotion.h/.cpp`
- `indra/llcharacter/llkeyframewalkmotion.h/.cpp`
- `indra/newview/lldirectorcast.h/.cpp`
- `indra/newview/llfloaterdirector.cpp`
- scene round-trip tests

**Work:**

1. Extract the existing ankle analyzer and cache.
2. Add manual phase-marker override and confidence/result diagnostics.
3. Add `LocomotionSet` to cast members.
4. Version and round-trip the LLSD schema with legacy `loco_anim` migration.
5. Add a compact validation UI before the full editor: role, UUID, nominal speed, phase status, load status.

**Gate:** old v4 scene loads identically; new scene round-trips all roles; an unknown/missing asset fails soft; a custom looping clip receives stable phase metadata without being registered as `LLKeyframeWalkMotion`.

### Patch 5 — Smart gait transitions

**Purpose:** deliver the main visible V2 improvement.

**Work:**

1. Add idle/walk/run role selection with clip-derived hysteresis.
2. Pre-start the incoming loop at weight zero.
3. Wait for load/metadata readiness.
4. Seed compatible canonical plant phase.
5. Evaluate deterministic explicit crossfade weights.
6. Stop outgoing immediately at zero weight.
7. Fold path arrival and dwell into the same transition system.
8. Add turn-in-place only after start/stop/walk/run pass.

**Suggested initial tuning seeds:**

- walk/run fade: 0.20–0.35 s;
- idle/walk fade: 0.15–0.25 s;
- maximum automatic clip stretch warning: outside 0.70–1.40 of nominal;
- turn-in-place candidate: low linear speed plus sustained heading error, values exposed for A/B rather than frozen in code.

These are test seeds, not claims of universal perceptual thresholds. User clips vary too much for one hidden constant.

**Gate:** no foot-phase discontinuity at a valid walk/run handoff, no outgoing clip leak, no state oscillation near threshold, and deterministic pose/weights at repeated recorder time samples.

### Patch 6 — Pure path math and adaptive arc table

**Purpose:** make path fidelity testable and remove fixed-density speed ripple.

**Files:**

- `indra/newview/alactorpathmath.h/.cpp` (new)
- `indra/newview/llactormover.cpp/.h`
- `indra/newview/alobjectpathmover.cpp`
- `indra/newview/alghoststudio.cpp`
- `indra/newview/tests/alactorpathmath_test.cpp` (new)
- `indra/newview/CMakeLists.txt`

**Work:** extract evaluator/data, implement bounded adaptive samples, add direct test target, retain old fixed table behind a comparison/debug gate for A/B.

**Gate:** tests cover open/loop paths, coincident nodes, sharp corners, long global coordinates, tension endpoints, node-distance exactness, inversion error, tangent continuity, deterministic rebuild, and sample caps.

### Patch 7 — Facing, look-ahead, and scalar C2 yaw

**Purpose:** make the body turn with movement before building full 3D orientation.

**Work:** explicit travel-vs-facing direction, reverse correction, distance look-ahead, signed curvature, `ALTrajectory::solveShortestArc()` yaw program, recorder-seek snap rule, optional post-blend lean hint.

**Gate:** no heading-line drift regression; no 180° seam spin; ping-pong behavior is explicit; a seek cannot leave a stale turn trajectory integrating toward an old target.

### Patch 8 — Ground profile and contact hints

**Purpose:** make stairs/stops read better with the existing solver.

**Work:** optional ground bake, shared preview/traversal profile, frame-stamped Pose Polish hints, expected plant windows, discontinuity reset, measured ray profiling, bounded pelvis slack only after foot goals prove unreachable.

**Gate:** prim-stair A/B matrix, flat-ground no-regression, two-height stance, step edge, moving platform limitation documented, fly/air contact disabled, no extra rays when feature disabled.

### Patch 9 — Air movement

**Purpose:** native fly/hover/takeoff/land behavior.

Air state must be authored or mover-resolved, not inferred solely from `av->mInAir`, because a locally overridden ghost/root may disagree with simulator avatar flags. While airborne:

- contact hints release both feet;
- sample time derives from air distance or role-native time according to clip metadata;
- hover is selected below a speed threshold;
- pitch follows velocity within clamps;
- full RMF is enabled only for paths that need roll/up transport;
- takeoff/land are timeline transition records so reverse scrubbing has defined behavior.

### Patch 10 — Exact primitives and path persistence

**Purpose:** add circle/arc/helix authoring without approximating intent with waypoints.

Land path scene serialization before shipping primitive authoring. Include a behavior/schema version and retain current waypoint paths as the default span. Add circle fitting only as a non-destructive suggestion with residual display.

## 5. Validation matrix

### 5.1 Unit tests

Motion presentation:

- default-native byte behavior;
- external forward/hold/backward sample;
- explicit weight in each controller lifecycle branch;
- LOD fade multiplication;
- deactivate reset;
- ownership conflict;
- loading-to-active phase seed;
- non-finite values;
- large-time precision.

Phase metadata:

- known alternating test clip;
- missing ankle;
- one stationary ankle;
- simultaneous plants;
- asymmetric but valid right offset;
- manual override;
- cache invalidation with keyframe cache flush.

State machine:

- upshift/downshift hysteresis;
- pending incoming load;
- failed load;
- same UUID in two roles;
- stop during crossfade;
- dwell during crossfade;
- suspend/replacement during crossfade;
- repeated time sample and backward seek.

Path math:

- all cases listed in Patch 6;
- compare integrated length against a much denser oracle in tests only;
- assert table error and cap, never an exact sample count.

### 5.2 In-viewer test scenes

Test each with self, remote avatar, ghost avatar, and animated-object control avatar where applicable:

1. no AO;
2. AO enabled with walk/run replacements;
3. explicit custom locomotion;
4. gesture and gaze active during movement;
5. global slow-motion already active before movement;
6. start/stop and stop/start rapidly;
7. loop and ping-pong path seams;
8. follower leader loss/recovery;
9. dwell with custom node animation;
10. recorder play, pause, forward seek, backward seek;
11. actor derez/teleport/suspend/resume;
12. Ghost Studio runtime replacement;
13. flat terrain, slope, prim stairs, mesh stairs, step edge;
14. 10 and 20 actors with UI visible and hidden.

Capture video plus debug traces for A/B. “Looks good once” is not an acceptance gate.

### 5.3 Performance instrumentation

Add scoped profiling for:

- Actor Mover path evaluation;
- locomotion role/transition update;
- motion-controller extra mapping/weight work;
- ground queries;
- Pose Polish contact;
- path rebuild.

Report median, 95th, and worst frame cost for 1/10/20 actors. Separate edit-time rebuild from per-frame cost. Confirm that default-off presentation controls add only a predictable branch/read in the controller and no allocation.

## 6. Adversarial review checklist

The implementation should not be approved until a reviewer can answer all of these from code and tests:

- Does **any** Actor Mover path still call `setAnimTimeFactor()`?
- Can stopping Character Mover change a global factor it did not create?
- Can a follower hold freeze a gesture or gaze clip?
- Does every AO start transaction balance on success, load failure, stop, suspend, actor loss, and destruction?
- Can Actor Mover stop a motion that was already active for simulator/AO/user reasons?
- Can the same actual UUID appear as both sides of a transition?
- Can an async-loaded clip display frame zero before its desired phase seed?
- Does every keyframe evaluation path resolve the external sample exactly once, including the initial/loading pose and the specialized walk/run subclass?
- Is LOD fade still independent of cinematic weight?
- Are presentation controls cleared when a motion deactivates or is deprecated?
- Does `isDriving()` remain true throughout a clip transition and false while suspended?
- Does actor runtime replacement recreate handles rather than copy them?
- Is clip time bounded before F32 conversion?
- Do path loop and ping-pong seams update odometer without a false full-path jump?
- Does a recorder seek reset stateful contact/orientation work?
- Are missing/non-loop/invalid clips soft failures with a usable fallback?
- Does the V2-off path reproduce current behavior?
- Are path and locomotion schema migrations round-trip tested?
- Are new rays measured and gated rather than assumed cheap?
- Is any new joint correction routed through Pose Polish rather than painted from Actor Mover?

## 7. Explicit non-goals for V2.0

- no motion-matching database;
- no learned locomotion controller;
- no full-body IK graph;
- no replacement of `LLPoseBlender`;
- no simulator/network locomotion changes;
- no root-motion authority from clips;
- no silent conversion of waypoint paths to fitted shapes;
- no promise of C2 quaternion feasibility until a separate tested adapter exists.

Inertialization remains useful polish, and this fork already has it. Bollo's method is specifically valuable because it turns a transition into a post-process rather than requiring both source and target evaluation for the full fade ([GDC 2018: Inertialization](https://media.gdcvault.com/gdc2018/presentations/bollo_david_inertialization_high_performance.pdf)). It is not a substitute for phase alignment or correct local motion ownership. Use it to suppress residual pose pops after the primary timing and contact constraints are correct.

## 8. First build recommendation

The first implementation review should cover Patches 0–3 as one architectural series, with each patch independently buildable:

1. pure tests and characterization;
2. default-inert per-motion sample/weight support;
3. reusable local-motion ownership transaction;
4. complete single-clip Actor Mover migration off `setAnimTimeFactor()`.

Do not include UI, walk/run selection, RMF, new raycasts, or path primitives in that first series. Its acceptance demonstration is simple and decisive:

> Start a Character Mover walk while AO, gaze, and a gesture are active and the avatar/controller is already under a non-1 global time factor. The feet remain distance-locked to the mover, unrelated animations keep their own timing, a dwell freezes only the locomotion clip, recorder scrubbing seeks the locomotion pose deterministically, and Stop changes no clock it does not own.

After that passes adversarial review, Patch 5 can deliver the “smart” walk/run/start/stop behavior on a trustworthy foundation.

## 9. Source index

Primary papers and official engine/DCC documentation used for the recommendations:

- Cem Yuksel, Scott Schaefer, John Keyser, [On the Parameterization of Catmull–Rom Curves](https://www.cemyuksel.com/research/catmullrom_param/catmullrom_slides.pdf).
- Wenping Wang et al., [Computation of Rotation Minimizing Frames](https://doi.org/10.1145/1330511.1330513), ACM TOG 2008.
- Lucas Kovar and Michael Gleicher, [Flexible Automatic Motion Blending with Registration Curves](https://graphics.cs.wisc.edu/Papers/2003/KG03/regCurves.pdf), SCA 2003.
- Lucas Kovar, John Schreiner, Michael Gleicher, [Footskate Cleanup for Motion Capture Editing](https://pages.cs.wisc.edu/~kovar/footskateCleanup.pdf), SCA 2002.
- David Bollo, [Inertialization: High-Performance Animation Transitions](https://media.gdcvault.com/gdc2018/presentations/bollo_david_inertialization_high_performance.pdf), GDC 2018.
- Ken Shoemake, [Animating Rotation with Quaternion Curves](https://doi.org/10.1145/325334.325242), SIGGRAPH 1985.
- Jens Gravesen, [Adaptive Subdivision and the Length and Energy of Bézier Curves](https://doi.org/10.1016/0925-7721(95)00054-2), Computational Geometry 1997.
- Walter Gander, Gene Golub, Rolf Strebel, [Least-Squares Fitting of Circles and Ellipses](https://www.research-collection.ethz.ch/items/c59d9093-9710-46d2-aa51-0f2029de55e4), ETH report / BIT.
- R. McNeill Alexander, [A Dynamic Similarity Hypothesis for the Gaits of Quadrupedal Mammals](https://zslpublications.onlinelibrary.wiley.com/doi/pdf/10.1111/j.1469-7998.1983.tb04266.x), Journal of Zoology 1983.
- Rodger Kram, Anthony Domingo, D. P. Ferris, [Effect of Reduced Gravity on the Preferred Walk–Run Transition Speed](https://spot.colorado.edu/~kram/walk-run.pdf), Journal of Experimental Biology 1997.
- Epic Games, [Distance Matching in Unreal Engine](https://dev.epicgames.com/documentation/unreal-engine/distance-matching-in-unreal-engine).
- Epic Games, [Game Animation Sample Project](https://dev.epicgames.com/documentation/unreal-engine/game-animation-sample-project-in-unreal-engine).
- Unity Technologies, [2D Blend Trees](https://docs.unity3d.com/cn/2018.3/Manual/BlendTree-2DBlending.html).
- Pixar, [OpenUSD Time and Animated Values](https://openusd.org/release/user_guides/time_and_animated_values.html).
- Blender Foundation, [Follow Path Constraint](https://docs.blender.org/manual/en/dev/animation/constraints/relationship/follow_path.html).

---

**Bottom line:** Character Mover V2 should become smart by owning the correct things—its root, its local locomotion clip samples, its transition records, and its authored path—while leaving avatar-global time, simulator animation state, priority arbitration, and leg solving to the systems that already own them.

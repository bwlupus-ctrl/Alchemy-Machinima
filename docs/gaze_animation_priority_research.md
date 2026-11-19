# Actor Gaze: selectable Second Life animation priority

Research target: `feature/cine-light-rig` at `3341fc06b1c131a9020634b91a7b75ef285dfddc` (2026-08-22)

Source checkout inspected: `I:\alchemy-machinima`

## Executive conclusion

Actor Gaze is not currently an animation and does not have a Second Life (SL) animation priority. It is a procedural, post-motion paint. `LLVOAvatar::updateCharacter()` runs the motion controller and only then calls Director/Actor Gaze (`indra/newview/llvoavatar.cpp:5223-5231`). The gaze code directly calls `LLJoint::setRotation()` and interpolates from each joint's already-animated rotation (`indra/newview/llactormover.cpp:5482-5609`, `5742-5747`, `6064-6196`, `6291-6296`). Consequently, motion priority cannot stop a gaze write. The current three-level setting named `DirectorGazePriority` is instead an ownership/scope policy—Blend, Head+Eyes, or Upper Body (`indra/newview/llactormover.h:327-335`; `indra/newview/app_settings/settings.xml:5341-5350`).

The recommended implementation is the **Hybrid**, specifically a conservative **strict-yield post-motion gate**:

1. Keep the existing gaze solver and final joint-write order.
2. During `LLPoseBlender`'s real motion blend, retain the highest non-additive rotation priority that actually contributed to each joint.
3. Give gaze a separate selectable SL priority, 0 through 6.
4. Immediately before every gaze joint write, skip that write when the recorded joint priority is greater than gaze's selected priority. Equal and lower priorities allow the existing write unchanged.
5. Add a `Legacy final` mode and make it the default, so existing scenes remain byte-for-byte on the current path until a director opts into priority-aware yielding.

This is not a perfect reimplementation of `LLMotionController` blending: a partially weighted higher-priority motion causes a strict yield rather than allowing gaze to fill its unused weight, and gaze wins equal-priority ties because it remains the final post pass. It does, however, provide the requested practical behavior—“yield to an AO above P, otherwise retain the tuned cinematic gaze”—with substantially less regression risk than moving the entire final-pose solver into a new `LLMotion`.

## 1. What SL animation priority means in this viewer

### 1.1 Values: named 0-4, valid normal values through 6, additive at 7

The type in this checkout is named `LLJoint::JointPriority` (not `EJointPriority`). Its declared values are (`indra/llcharacter/lljoint.h:93-103`):

| Numeric value | Viewer symbol | Meaning for this work |
|---:|---|---|
| -1 | `USE_MOTION_PRIORITY` | A joint-state sentinel: use its motion's effective base priority. Not an SL priority exposed to the director. |
| 0 | `LOW_PRIORITY` | Normal priority 0. |
| 1 | `MEDIUM_PRIORITY` | Normal priority 1. |
| 2 | `HIGH_PRIORITY` | Normal priority 2. |
| 3 | `HIGHER_PRIORITY` | Normal priority 3. |
| 4 | `HIGHEST_PRIORITY` | Normal priority 4. |
| 5 | no named enumerator | Valid normal `.anim` priority 5. |
| 6 | no named enumerator | Valid normal `.anim` priority 6. |
| 7 | `ADDITIVE_PRIORITY` / `LL_CHARACTER_MAX_PRIORITY` | Reserved by this viewer for additive blending, not a selectable normal gaze priority (`indra/llcharacter/lljoint.h:54`, `102`). |

The apparent enum gap is important. Deserialization accepts a base priority below 7 and clamps 7 or above to 6 (`indra/llcharacter/llkeyframemotion.cpp:1302-1319`). The viewer's BVH preview control displays 0-6 (`indra/newview/skins/default/xui/en/floater_animation_bvh_preview.xml:195-205`), but `LLKeyframeMotion::setPriority()` clamps imported joint priorities to the named 0-4 range (`indra/llcharacter/llkeyframemotion.cpp:2291-2306`). This matches the official limits page: BVH upload supports 0-4, while custom `.anim` data supports per-joint values through 6. See [Second Life Wiki: Limits](https://wiki.secondlife.com/wiki/KB2/Limits) and [Anim File Format](https://wiki.secondlife.com/wiki/Anim_File_Format).

The new gaze control should therefore expose **normal priorities 0-6 only**. It must not present 7 as “higher than 6”; in this code 7 marks an additive contribution with different blend behavior.

### 1.2 Where keyframe priority lives

An `LLJointState` stores a joint pointer, transform usage bits, transform values, weight, and a `JointPriority`; its priority defaults to `USE_MOTION_PRIORITY` and has direct get/set accessors (`indra/llcharacter/lljointstate.h:39-77`, `80-113`).

An `LLKeyframeMotion` has both:

- a shared motion `mBasePriority`; and
- a `JointMotion::mPriority` for each animated joint (`indra/llcharacter/llkeyframemotion.h:384-415`).

When cached keyframe data is attached to an avatar, each joint motion's priority is copied into the corresponding `LLJointState` (`indra/llcharacter/llkeyframemotion.cpp:555-575`). `LLMotion::getJointPriority()` resolves the effective value in this order: per-instance override, explicit joint priority, then motion priority when the joint says `USE_MOTION_PRIORITY` (`indra/llcharacter/llmotion.h:91-115`). `LLKeyframeMotion::getPriority()` likewise returns the per-instance override when present, otherwise the asset's shared base priority (`indra/llcharacter/llkeyframemotion.h:112-118`).

The branch's `LLMotion::setPriorityOverride()` is the correct precedent for changing a **real motion** locally. It rebuilds that motion instance's joint signatures without mutating the shared keyframe asset (`indra/llcharacter/llmotion.cpp:126-162`). Actor Mover uses it for a just-started custom locomotion motion (`indra/newview/llactormover.cpp:100-121`). It does not directly help today's gaze because there is no gaze `LLMotion` instance on which to call it.

### 1.3 Arbitration is priority-ordered, weight-limited blending

Priority does not multiply an animation's strength. It changes ordering and masking.

Each motion encodes a joint/transform priority signature as a bit mask (`indra/llcharacter/llmotion.cpp:103-122`). `LLMotionController::updateMotionsByType()` visits active motions in chronological order, merges those signatures, and can skip a fully weighted motion whose joint contributions add no priority bits beyond motions already visited (`indra/llcharacter/llmotioncontroller.cpp:574-644`). Newly activated motions are pushed to the front of the active list (`indra/llcharacter/llmotioncontroller.cpp:1001-1008`), so recent equal-priority motions reach the blender first.

The definitive pose result is built in `LLJointStateBlender`:

- up to six states for a joint are retained (`indra/llcharacter/llpose.h:81-94`);
- states are inserted in descending priority, while an existing equal-priority state stays ahead (`indra/llcharacter/llpose.cpp:196-229`);
- position, rotation, and scale weights accumulate independently and are capped at 1 (`indra/llcharacter/llpose.cpp:271-373`); and
- the final transforms are applied to the joint at `indra/llcharacter/llpose.cpp:386-390`.

For rotation, a lower-priority state only fills weight not already consumed by higher-priority states. A higher-priority state at weight 1 therefore completely excludes lower-priority rotation. If the higher state is easing at weight 0.4, lower states can still occupy the remaining 0.6. Equal-priority recency becomes a complete “newest wins” rule only when the newer contribution fills the available weight. This code-level nuance is more precise than the useful public shorthand that the highest priority wins and the most recently started wins ties. See [Second Life Wiki: Animation Priority](https://wiki.secondlife.com/wiki/Animation_Priority).

`LLMotionController::updateMotions()` updates additive motions, resets its signatures, updates normal motions, and then calls `blendAndApply()` (`indra/llcharacter/llmotioncontroller.cpp:849-932`). Additive states are specially composed rather than consuming the normal blend's usage/weight (`indra/llcharacter/llpose.cpp:284-309`, `386-390`). A gaze priority test must therefore ignore additive states; treating priority 7 as a blocking normal animation would be incorrect.

### 1.4 Typical AO priorities

There is no single guaranteed AO priority. The SL guidance recommends priority 2 for stands and allows priority 3 to prevent built-in body noise; many walks, dances, furniture poses, and pose-ball animations are priority 4. Priorities 5 and 6 exist in custom `.anim` content and should be handled even though they should be used sparingly. See [Animation Upload Priority](https://wiki.secondlife.com/wiki/Animation_Upload_Priority) and [Animation Priority](https://wiki.secondlife.com/wiki/Animation_Priority).

Practical expectations for testing are therefore:

- AO stands commonly exercise P2 or P3.
- AO locomotion and strong poses commonly exercise P3 or P4.
- A robust viewer feature must also behave deterministically against P5/P6 content.
- Priority matters only on a joint the animation actually supplies. An AO that omits head and neck leaves those joints available to other motions; SL guidance explicitly recommends omitting head/neck when camera/pointer head tracking should remain possible.

The implementation should not try to identify “the AO.” The motion blender sees ordinary active motions—AO, built-ins, gestures, furniture, attachments—and SL priority semantics apply to all of them per joint.

## 2. Current Actor Gaze behavior

### 2.1 Frame order

The relevant normal-update order is:

```text
restore last Director render-only pose
    -> updateMotions(NORMAL/FORCE)
        -> LLMotionController blends and applies motions
    -> applyDirectorLookAt(), or applyGaze() when Director declines
        -> raw joint setRotation() writes
```

Evidence:

- Director restores the prior captured pose before root/motion work (`indra/newview/llvoavatar.cpp:5121-5124`; restore writes are at `indra/newview/llactormover.cpp:3909-3947`).
- Normal/forced motion update occurs at `indra/newview/llvoavatar.cpp:5211-5224`.
- Director receives first refusal after motions; Actor Gaze runs only when Director does not paint (`indra/newview/llvoavatar.cpp:5226-5232`).
- `applyGaze()` is the per-actor `mGazes` path (`indra/newview/llactormover.cpp:3675-3795`).
- `applyDirectorLookAt()` is the `mDirectorGazes` path and ultimately calls the same `gazePaint()` (`indra/newview/llactormover.cpp:4236-4269`, `4623-4638`, `4673-4684`).
- On a non-self LOD skip, only a minimal motion update and `applyDirectorLookAt()` run; `applyGaze()` is not called (`indra/newview/llvoavatar.cpp:5148-5155`). The minimal update does not blend a new pose (`indra/llcharacter/llcharacter.cpp:181-198`; `indra/llcharacter/llmotioncontroller.cpp:940-956`).

The capture/restore bracket exists for Director's render-only pose. Capture records the potentially owned pelvis, torso, neck, head, eyes, and optional blink parameters (`indra/newview/llactormover.cpp:3841-3906`); restore calls raw `setRotation()` before the next update (`indra/newview/llactormover.cpp:3909-3947`).

### 2.2 The final writes are outside priority arbitration

Both motion-program and legacy gaze branches directly read a joint's current rotation and write an `nlerp` result:

- motion-program pelvis/torso/neck/head: `indra/newview/llactormover.cpp:5482-5609`;
- motion-program eyes: `indra/newview/llactormover.cpp:5738-5747`;
- legacy pelvis/torso/neck/head: `indra/newview/llactormover.cpp:6060-6196`; and
- legacy eyes: `indra/newview/llactormover.cpp:6287-6296`.

No `LLJointState` is created, no state is added to an `LLPose`, and no priority comparison occurs in those blocks. By the time they execute, `LLMotionController` has already applied its result. Gaze therefore has final say whenever its write weight is nonzero, regardless of whether the underlying motion was priority 0 or 6.

In the legacy Blend scope, the result has the form:

```text
final = nlerp(gaze_weight, animated_rotation, gaze_target)
```

so the animated/AO pose remains in proportion to approximately `1 - gaze_weight`. The exact weights are assembled from the eased enable envelope, intensity, head/eye allocation, behind-target envelope, and cue channel weights (`indra/newview/llactormover.cpp:5125-5129`, `5166-5199`, `5238`).

There is an important branch-specific correction to the simple “gaze always partially blends” description: the existing setting already implements **pragmatic full ownership**. `DirectorGazePriority` resolves to Blend, Head+Eyes, or Upper Body (`indra/newview/llactormover.cpp:3565-3573`, `5188-5199`). In an ownership mode, the authored target is first scaled from identity by intensity/cue weight, then the animated pose is interpolated toward that owned target by `priority_env`. At full acquire, `priority_env == 1`, so selected joints contain no AO bleed even when authored intensity is less than one (`indra/newview/llactormover.cpp:5457-5467`, `5478-5603`; legacy equivalent at `6036-6190`). The remaining intensity changes the authored gaze offset, not AO ownership.

Thus today's behavior is:

- **Blend:** final post-motion interpolation; AO contributes `1 - weight`.
- **Head+Eyes:** neck/head/eyes become fully gaze-owned at full acquire; pelvis/torso retain legacy weighted behavior when used.
- **Upper Body:** adds full ownership of pelvis/torso at full acquire.
- **All three:** cannot yield based on an AO's SL priority, because even the smallest allowed post write occurs after arbitration.

### 2.3 The current “priority” is joint ownership, not SL priority

The enum itself says it controls how completely the target replaces the animation pose (`indra/newview/llactormover.h:327-335`). The setting declares values 0-2 with those three labels (`indra/newview/app_settings/settings.xml:5341-5350`). The UI presents “Look-at priority” with Blend, Head+Eyes, and Upper body (`indra/newview/skins/default/xui/en/panel_lens_gaze.xml:90-104`). Its handler clamps 0-2 and writes `GazeTarget::mGazePriorityOverride` (`indra/newview/alpanellensgaze.cpp:859-870`, `1119-1127`); that per-actor field otherwise inherits the global (`indra/newview/llactormover.h:378-383`). `floater_actor_gaze.xml` embeds this same panel rather than defining a second control (`indra/newview/skins/default/xui/en/floater_actor_gaze.xml:24-42`).

Calling this control “priority” beside a new 0-6 control would be ambiguous. It should become **Ownership scope** (or **Gaze joint ownership**) and the new control should be explicitly **SL anim priority**.

## 3. Design paths

### Path A — make gaze a first-class motion

#### What it would require

Create a procedural `LLMotion` for each participating avatar, modeled structurally on `LLHeadRotMotion`: allocate rotation `LLJointState`s, bind them to avatar joints, mark `ROT` usage, and add them to the motion pose (`indra/llcharacter/llheadrotmotion.cpp:100-102`, `117-178`). Register and start that motion through the existing motion registry/lifecycle (`indra/newview/llvoavatar.cpp:1262-1319`; default motion startup is at `2157-2167`). Its effective priority could be returned directly or set per instance through the existing `setPriorityOverride()` hook.

The new motion would need joint states for pelvis, torso, neck, head, both classic eyes, and both alternate Bento eyes. Its `onUpdate()` would have to produce the current gaze target rotations before `LLPoseBlender::blendAndApply()`. Dynamic scope and per-joint cue weights are not free: `LLPose` normally receives one motion weight and propagates it to all states (`indra/llcharacter/llpose.cpp:145-155`), while gaze currently has different eye/head/body weights. Per-state weights/usages and joint signatures would need careful synchronization so a disabled or zero-weight gaze state does not mask a lower motion merely because it remains in the signature.

#### Why this disrupts the current solver

The existing gaze algorithm is deliberately final-pose-dependent:

- Direction smoothing and dead-zone/body-aim chase settle before the write (`indra/newview/llactormover.cpp:5096-5118`, `5166-5237`).
- Large-turn slew/cone handling carries applied state across frames (`indra/newview/llactormover.cpp:5822-5889`).
- Natural breaks and the anatomical head/neck/torso/hips distribution are layered after smoothing (`indra/newview/llactormover.cpp:5919-6013`).
- Micro-life is added at final head/eye construction (`indra/newview/llactormover.cpp:6036-6040`, `6152-6155`, `6275-6280`).
- In ownership mode, neck/head local targets cancel the **actual already-animated parent world rotation**, preventing an AO chest from dragging a gaze-owned head (`indra/newview/llactormover.cpp:6120-6136`, `6175-6190`).
- VOR/eyeline weld reads head world rotation only after the head has received its final drift/write, then solves and clamps eye-in-head rotation (`indra/newview/llactormover.cpp:6201-6227`, `6252-6296`; motion-program equivalent at `5614-5622`, `5690-5737`).

During an ordinary motion `onUpdate()`, the current frame's competing motion states have not yet been blended into the skeleton. A gaze motion therefore cannot read the same final parent/head pose that those calculations use. Moving computation earlier changes the frame phase; splitting motion blending into pre-gaze and final passes changes `LLMotionController` and pose-blender invariants; leaving a post correction restores the current assumptions but is no longer pure Path A.

Director root/body turning and blink visual parameters also sit outside ordinary joint-state rotation blending. They would remain a separate post layer or require additional architecture (`indra/newview/llactormover.cpp:4191-4194`, `5765-5807`, `6305-6337`).

#### Can top priority be byte-identical?

Not as a general guarantee. At full motion weight and a normal priority of 6, a gaze `LLJointState` can exclude all lower normal states, so a precomputed local target can numerically match an existing full-ownership target in simple cases. During ease/cue weights, however, inserting gaze into the priority-ordered blend changes which lower states fill the remaining weight. More importantly, parent cancellation, final-head VOR, capture/restore timing, motion-program state, and LOD behavior would run at a different phase. Whole-frame byte identity would require retaining much of the post-motion layer, which defeats the stated architecture.

#### Assessment

Path A has the highest theoretical fidelity to priority ordering, including weighted residuals and—if lifecycle timing is modeled—equal-priority recency. It also has the highest implementation and regression risk. It is appropriate only if Actor Gaze is intentionally being redesigned as an animation subsystem rather than incrementally made priority-aware.

### Path B — pragmatic post-motion ownership

Keep all raw post-motion writes. Interpret low selected values as legacy blending and high values as full ownership of the joints selected by Ownership scope. A possible policy is “0-2 blend, 3-6 full-own eligible joints,” but any threshold is a product convention, not SL semantics.

The core of Path B already exists: Head+Eyes and Upper Body remove animation bleed at full acquire while preserving all final-pose gaze tuning (`indra/newview/llactormover.cpp:5188-5199`, `5457-5603`, `6036-6190`). Extending the UI to show 0-6 would be low risk, and selected priority 6 could look convincingly dominant.

Its fundamental limitation is decisive: a priority-6 AO cannot beat a post-motion gaze labeled priority 0. This path lets gaze “win harder” but never truly yield. Calling its control “SL animation priority” would be misleading, and mapping higher values to broader joint scope would be especially unlike SL, where priority affects only joints actually keyed by the animation.

### Hybrid — post-motion gaze with per-joint priority-aware yield

Keep gaze's final-pose architecture, but condition each joint write on the priority that reached that joint during this frame's normal motion blend.

#### Feasibility of obtaining effective joint priority

There are two implementation choices:

1. **Scan active motions at gaze time.** `LLCharacter` exposes its controller and the controller exposes `getActiveMotions()` (`indra/llcharacter/llcharacter.h:164-172`). Code could walk every motion pose and use `LLMotion::getJointPriority()`. This is a useful prototype, but it can count a motion that was LOD-faded, zero-weight, skipped by joint signatures, or dropped by the six-state blender limit. It is not the exact set that formed the rendered pose.
2. **Publish a blend snapshot.** `LLPoseBlender::addMotion()` already sees the effective priority used for every joint state (`indra/llcharacter/llpose.cpp:469-503`), and `LLJointStateBlender::blendJointStates()` sees actual usage, weight, priority ordering, and additive status (`indra/llcharacter/llpose.cpp:271-310`). Record the highest qualifying normal rotation priority before joint states are cleared at `394-399`; the active-blender list is also cleared after application at `508-520`. This is the recommended source because it describes the states that actually reached the blend.

`LLMotionController::mJointSignature[1]` should not be exposed as the answer. Although it represents rotation masks, motions below full pose weight bypass the signature merge (`indra/llcharacter/llmotioncontroller.cpp:595-638`), and the signatures are optimization masks rather than a durable record of weighted contributors.

#### Recommended Hybrid rule

For each candidate joint rotation:

```text
Legacy final selected                         -> use existing gaze alpha
no valid regular-motion rotation sample      -> use existing gaze alpha
highest contributing priority > gaze priority -> alpha = 0 (yield/skip)
highest contributing priority <= gaze priority -> use existing gaze alpha
```

This deliberately uses a strict greater-than comparison. It satisfies the requested “yield to a higher-priority AO” behavior. Equal priority remains gaze-wins because gaze is conceptually the newest contribution and physically remains the final pass. That is predictable but not a full emulation of SL start-order ties.

Strict yield is safer than a naive attenuation. If a P4 motion contributes 0.4 and lower motions fill 0.6, the already-applied joint rotation is a composite. A single post `nlerp()` cannot replace only the lower-priority 0.6 without also altering the P4 component. Multiplying gaze alpha by `1 - 0.4` looks smoother but does not preserve the higher-priority pose mathematically. A later experimental mode may use recorded priority-weight histograms for perceptual attenuation, but it should be labeled approximate.

The final-pose architecture remains valuable when different joints make different decisions. If a high-priority AO blocks head but not eyes, the eye solve reads the unchanged AO head world rotation at `indra/newview/llactormover.cpp:5690-5707` or `6224-6266`, so VOR still computes the correct eye-local fixation. If torso yields but head is allowed, the ownership path cancels the actual animated parent when deriving the local head/neck rotation (`indra/newview/llactormover.cpp:6120-6136`, `6175-6190`). Path A cannot obtain those same final values in a conventional single motion update.

## 4. Comparison

| Criterion | Path A: first-class motion | Path B: post full-ownership | Hybrid: post priority-aware yield |
|---|---|---|---|
| Fidelity to SL priority semantics | Highest in principle; can use the real ordered weighted blender | Low; values are only a dominance convention | Medium-high for strict higher/lower precedence; partial weights and equal-priority start order remain approximate |
| Can a higher-priority AO beat gaze on the same joint? | Yes | No | Yes |
| Can lower-priority AO motion remain on joints gaze does not own/write? | Yes | Yes | Yes |
| Preserves final-head VOR, parent cancellation, cone clamp, smoothing, torso chase, and micro-life architecture | Requires substantial redesign | Yes | Yes |
| Can preserve current output by default? | Feature-off can; opted-in top-priority output is not generally byte-identical | Yes | Yes, with `Legacy final` default; allowed writes are unchanged |
| Core complexity | High: new motion, lifecycle, dynamic states/signatures, phase redesign | Low; mostly UI/policy because core already exists | Medium: small pose-blend observer plus gates at every write |
| Regression risk | High | Low | Low-medium |
| User-facing honesty as “SL anim priority” | Honest if fully implemented | Misleading | Honest if documented as priority-aware yielding rather than a first-class animation |

## 5. Recommendation

Implement the **Hybrid strict-yield gate**, and retain **Legacy final** as the default.

This recommendation follows from four code facts:

1. The current solver's most valuable details depend on running after the final animated hierarchy, especially parent cancellation and VOR.
2. Path B's full ownership is already present, so it does not solve the missing “deliberately yield” half of the product requirement.
3. The pose blender has all information needed to publish a small exact per-joint precedence observation without changing how it blends.
4. A default legacy sentinel makes the new feature a no-op until explicitly selected, matching this branch's repeated byte-identical-default design principle.

Use product language that sets the right expectation:

- UI label: **SL anim priority**
- Legacy option: **Legacy final (always apply)**
- Priority options: **0 (Low)** through **6 (Maximum normal)**
- Help text: “Gaze yields on each joint to animations above this priority. Equal priority favors gaze. Legacy final ignores animation priority.”

Retitle the current control to **Ownership scope** with options **Blend**, **Head + eyes**, and **Upper body**. Ownership scope answers “which gaze joints remove animation bleed when gaze is allowed?” SL anim priority answers “when is gaze allowed to write each joint?” They are orthogonal and should coexist.

## 6. Concrete implementation outline

### 6.1 Motion-blend observation

Touch `indra/llcharacter/llpose.h` and `indra/llcharacter/llpose.cpp`:

- Add a retained `mLastRegularRotationPriority` plus validity flag to each `LLJointStateBlender`, initialized invalid.
- At the start of each actual `blendJointStates()` application, clear the retained value.
- While visiting the retained states, consider only states that:
  - have `LLJointState::ROT` usage;
  - have weight above a small, documented epsilon; and
  - are not marked additive.
- Record the maximum effective priority already passed into `addJointState()`.
- Preserve that observation before `mJointStates` is cleared (`indra/llcharacter/llpose.cpp:394-399`).
- Add a const query on `LLPoseBlender`, keyed by `LLJoint*`, that returns `(valid, priority)` from its persistent `mJointStateBlenderPool` (`indra/llcharacter/llpose.h:105-123`).

Touch `indra/llcharacter/llmotioncontroller.h/.cpp` only to expose a narrow forwarding method such as:

```cpp
bool getLastAppliedRegularRotationPriority(const LLJoint* joint,
                                           S32& priority) const;
```

Do not expose the signature arrays and do not report additive priority 7 as a blocker. Optionally attach a blend-generation counter for diagnostics. On the LOD-minimal path, no new blend occurs; retaining the last observation matches the skeleton pose that also remained un-reblended. If a generation is considered stale or absent, the safe compatibility fallback is to allow the existing gaze write, not unexpectedly suppress it.

### 6.2 Gaze model and naming

Touch `indra/newview/llactormover.h/.cpp`:

- Rename `EGazePriority` conceptually to `EGazeOwnershipScope` and the `GAZE_PRIORITY_*` values to ownership names. To avoid silently breaking saved settings/presets, either retain aliases during migration or introduce a new `DirectorGazeOwnershipScope` setting initialized from the old key once.
- Rename `GazeTarget::mGazePriorityOverride` and `Gaze::mGazePriorityOverride` to ownership-scope fields, updating the existing copy/equality/resolution sites (`indra/newview/llactormover.cpp:483-499`, `882-925`, `1279-1296`, `4384-4422`).
- Add a distinct per-actor SL-priority override. A practical integer encoding is:
  - `-2`: inherit global (per-actor storage only);
  - `-1`: Legacy final;
  - `0..6`: selected normal priority.
- Resolve and clamp that value once near the existing ownership resolution at `indra/newview/llactormover.cpp:5188-5199`.

Add a small helper that returns an allowed alpha for `(avatar, joint, base_alpha, selected_priority)`. Legacy/missing-sample returns `base_alpha`; a higher recorded normal priority returns zero. Apply it independently to pelvis, torso, neck, head, and every classic/alternate eye in **both** write blocks (`5482-5747` and `6064-6296`). When it returns zero, skip `setRotation()` but continue advancing smoothing, gaze-motor, micro-life, cue, and envelope state so gaze resumes without a stale-state jump.

Do not gate blink/lid visual parameters with joint animation priority: they are visual parameters rather than `LLJointState` rotations. Document that boundary. Director root/body replanting is likewise a separate root-ownership behavior and should remain unchanged in the first implementation.

### 6.3 Settings and UI

Touch `indra/newview/app_settings/settings.xml`:

- Add `DirectorGazeAnimationPriority` (`S32`, persistent, default `-1` for Legacy final).
- Rename or migrate the current `DirectorGazePriority` to `DirectorGazeOwnershipScope`, preserving its current default value `1` (Head+Eyes) and behavior.

Touch `indra/newview/skins/default/xui/en/panel_lens_gaze.xml` and `indra/newview/alpanellensgaze.h/.cpp`:

- Relabel the current `gaze_priority_combo` row to **Ownership scope**.
- Add `gaze_anim_priority_combo` with Legacy final and priorities 0-6.
- Add/reset per-actor override behavior analogous to the existing control. Reset sets the per-actor value to inherit (`-2`), and refresh shows the resolved global value.
- Keep the controls separate; do not let a higher SL priority automatically expand ownership from Head+Eyes to Upper Body.

`indra/newview/skins/default/xui/en/floater_actor_gaze.xml` embeds `panel_lens_gaze.xml` (`:24-42`), so it needs no behavioral code; adjust its panel/content height only if the added row exceeds the current scroll content.

### 6.4 Diagnostics

For development builds, add an optional one-line joint diagnostic showing selected gaze priority, observed regular rotation priority, and allow/yield result. This is more useful than Animation Info's motion-level display because `.anim` can carry per-joint priority (`indra/llcharacter/llkeyframemotion.h:384-415`). Keep it disabled by default.

## 7. Risks and mitigations

| Risk | Consequence | Mitigation |
|---|---|---|
| Strict yield overreacts to a partially weighted higher-priority motion | Gaze may disappear during that motion's ease rather than fill residual weight | Treat this as the documented conservative v1 behavior; optionally add an experimental histogram-based attenuation later |
| Equal-priority tie is not true start-order arbitration | Gaze always wins a tie even if an AO starts later | State this in the tooltip/docs; use “higher than” wording; Path A is required for exact tie lifecycle semantics |
| Additive state is mistaken for P7 normal priority | Gaze would always yield when poser/additive motion is active | Explicitly exclude `mAdditiveBlends` from the recorded blocking priority |
| Stale/no blend observation on hidden/paused/loading avatars | Incorrect transient allow/yield result | Add validity/generation diagnostics; default missing data to legacy allow; test the LOD-minimal path |
| A zero-weight state is reported as active | Gaze yields to a motion that did not affect the pose | Ignore weights at/below epsilon at the actual blend site |
| Only some parents yield | Child target could be dragged or double-applied | Gate per joint and retain current actual-parent cancellation; test every mixed parent/child combination |
| A new write site misses the gate | One eye or one solver branch ignores priority | Centralize the helper and enumerate both current write blocks in code review/tests |
| P5/P6 encourages indiscriminate dominance | User content becomes hard to mix | Label 5/6 clearly, retain Legacy as compatibility rather than “maximum,” and include priority guidance in help text |
| Existing saved value changes meaning after rename | Scenes change appearance | Migrate/alias `DirectorGazePriority` solely as Ownership scope; never reinterpret its 0-2 values as SL priority |

## 8. Test plan

### 8.1 Unit-level tests

Add focused pose-blender tests under `indra/llcharacter/tests/`:

1. A full-weight P4 rotation over P3 records P4 and applies P4.
2. A partial-weight P4 plus P3 still records P4 for strict-yield purposes.
3. Equal-priority states retain the existing newest-first order.
4. Zero-weight and non-`ROT` states do not produce a rotation-priority observation.
5. An additive P7 state changes the additive pose but is excluded from the blocking normal priority.
6. More than six candidate states report only what the existing six-slot blender actually retained.
7. Clearing after apply preserves only the last observation and resets validity correctly on the next real blend.

Add small policy tests alongside the existing gaze math/motor tests in `indra/newview/tests/`:

- Legacy final always returns the original alpha.
- P3 gaze yields to observed P4 and applies against P0-P3.
- P6 gaze applies against every valid normal priority 0-6.
- Missing observation follows the compatibility policy.
- Each ownership scope still selects the same joints as before.

Run all existing gaze regression suites (`indra/newview/tests/algazemath_test.cpp`, `algazemotor_test.cpp`, `algazepolicy_test.cpp`, `algazerecruit_test.cpp`, `algazenoise_test.cpp`, and `algazeblink_test.cpp`) unchanged as a guard on solver behavior.

### 8.2 In-world priority matrix

Prepare test animations that visibly key head, neck, torso, pelvis, and both eyes at priorities 2, 3, 4, 5, and 6. Also prepare one AO animation that deliberately omits head/neck and a custom `.anim` with mixed per-joint priorities.

For each gaze priority P0-P6:

1. Play an AO whose keyed joint priority is lower than gaze. Gaze must apply on that joint.
2. Play an AO at equal priority. Gaze must win under the documented Hybrid tie policy.
3. Play an AO one level higher. Gaze must not change that joint's animated rotation.
4. Use mixed per-joint data (for example torso P4, head P2) and verify independent decisions: P3 gaze yields torso but controls head.
5. Use an animation that omits head/neck and verify it does not block gaze there regardless of its motion-level/base priority.
6. Repeat under Blend, Head+Eyes, and Upper Body ownership scopes. Scope must affect ownership strength/joints, not the priority comparison.
7. At P6, verify gaze defeats all normal P0-P6 motions; run an additive poser and verify it is not misreported as a higher normal priority.

### 8.3 Transition and regression scenarios

- Start/stop and cross-fade the competing AO while gaze is locked; verify the strict-yield transition is deterministic and does not leave a stale joint pose.
- Change gaze priority live above and below the active joint priority.
- Switch gaze targets, enable/disable, and switch motion-program gate while a joint is yielding; when permission returns, smoothing/motor state must be current rather than snapping from the last visible gaze pose.
- Exercise a moving camera and moving actor to verify direction smoothing and dead-zone torso chase (`llactormover.cpp:5096-5118`, `5166-5237`).
- Exercise targets behind the actor to verify large-turn slew/cone behavior (`llactormover.cpp:5822-5889`).
- Verify head drift, microsaccades, blinks, and natural breaks at fixed presentation times (`llactormover.cpp:5919-5939`, `6036-6040`, `6233-6285`).
- Block head but allow eyes, then block torso but allow head, to verify final-pose VOR and parent cancellation.
- Test classic eyes and Bento alternate eyes in both solver branches.
- Test per-actor `applyGaze()` and Director `applyDirectorLookAt()`, including Director capture/release, camera safety interlock, seated actors, root/body turn, and deselection.
- Force non-self LOD/minimal updates and return to detailed updates; ensure no stale priority decision or unrecovered captured pose.
- With `Legacy final` selected, compare recorded joint quaternions/frame captures against the pre-change build. They should be unchanged because the new helper returns the original alpha without consulting priority.

## 9. Decision boundary

The Hybrid is the right incremental feature if the product requirement is: **“Let a director choose a familiar SL priority threshold so gaze can deliberately yield to stronger animation on each joint, while retaining the cinematic solver.”**

Choose Path A only if the stronger requirement is: **“Actor Gaze must be indistinguishable from a genuine started SL animation in weighted residual blending and equal-priority start ordering.”** That requirement entails a motion-system redesign and should not be presented as a small extension of the present post-motion layer.

# Gaze IK, animation overlay, limits, influence, and lean design

Status: design/integration plan only. No source implementation is included.

Repository baseline: `feature/cine-light-rig` at `f4eba62c3ee` (2026-08-22). Every `file:line` anchor below was re-verified against the working tree at this HEAD.

## Executive decision

Implement the five requested features as independent axes, not as one new gaze mode:

1. Add `GAZE_PRIORITY_PLANTED_SPINE` as a fourth ownership scope. Do not change the numeric values or behavior of `Blend`, `Head+Eyes`, or `Upper body`. The planted scope never allocates a gaze hips contribution and never writes `mPelvis`. It owns `mTorso`, optional `mChest`, `mNeck`, `mHead`, and eyes. This is the safest answer to Goal 1 because the existing `Upper body` behavior remains available and byte-identical.
2. Add a separate pose-composition enum: `Replace`, `Additive overlay`, and `Blend replace/overlay`. Composition selects how an allowed joint endpoint is built; ownership selects joints; SL animation priority may veto the write; torso amount shapes the solve. None substitutes for another.
3. Add an opt-in resolved limit profile used by both the legacy allocator and motor allocator. Keep the old constant paths intact when the profile is disabled. A shared custom-profile helper and parity tests prevent the two capacity tables from drifting.
4. Add a separate, stateless `GazeInfluenceKeyList` on the same presentation-time domain as `GazeCueList`. Do not key `mIntensity` and do not drive `mEnv`. The evaluated influence is a final write-weight multiplier, so a key value of zero reveals the animation rather than replacing it with a neutral gaze pose.
5. Add an opt-in `Angle ease` spine-recruitment curve. Its threshold/softness/max parameters decide the spine contribution from root-relative aim angle. `DirectorGazeSoftRecruitDeg` continues to smooth handoff at individual capacity knees; it does not become the lean curve.

The unchanged default configuration must enter none of the new math: existing ownership default `Head+Eyes`, composition `Replace`, adjustable limits disabled, lean curve `Legacy capacity fill`, no influence keys (implicit 1), and no planted-spine scope. This is stronger than merely choosing numerically equivalent defaults: the current branches and operation order remain the default branches.

## Current architecture and the source of the two complaints

### Paint order and ownership

Gaze is a post-animation render-pose layer. `LLVOAvatar` runs `updateMotions()` and then gives Director first refusal, falling back to Actor Mover gaze (`indra/newview/llvoavatar.cpp:5211`, `indra/newview/llvoavatar.cpp:5226`). At entry to gaze, `joint->getRotation()` is therefore the animation pose; the code documents that timing and the next-frame motion rebuild at `indra/newview/llactormover.cpp:3533`.

The existing ownership enum is:

- `GAZE_PRIORITY_BLEND = 0`
- `GAZE_PRIORITY_HEAD_EYES = 1`
- `GAZE_PRIORITY_UPPER_BODY = 2`

It is declared at `indra/newview/llactormover.h:327`. The global default is `Head+Eyes` and the SL-priority setting is a separate control (`indra/newview/app_settings/settings.xml:5341`, `indra/newview/app_settings/settings.xml:5352`). The UI already describes these as distinct “Ownership scope” and “SL anim priority” rows (`indra/newview/skins/default/xui/en/panel_lens_gaze.xml:90`, `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:106`).

`gazeAllowedAlpha()` is the per-joint strict-yield gate. `-1` allows the legacy final write; priorities `0..6` yield only to a strictly higher regular animation priority (`indra/newview/llactormover.cpp:3579`). The observation comes from the normal pose blend: regular contributing rotations are recorded at `indra/llcharacter/llpose.cpp:297`, while SL additive rotations are composed separately as `added_rot * blended_rot` at `indra/llcharacter/llpose.cpp:308` and `indra/llcharacter/llpose.cpp:410`. This distinction must remain intact.

### Why Upper body swings the legs

`AnatomicalChainPose` contains eye, head, neck, torso, and hips contributions (`indra/newview/algazemath.h:205`). `distributeAnatomicalChain()` fills them in that order (`indra/newview/algazemath.h:562`), with hardcoded yaw/pitch capacities of 25/14, 35/42, 35/26, 45/20, and 35/15 degrees (`indra/newview/algazemath.h:583`). The hips share is filled after torso at `indra/newview/algazemath.h:646` (yaw) and `indra/newview/algazemath.h:653` (pitch).

In Upper-body ownership, the motor branch writes that hips target to `mPelvis` (branch condition `indra/newview/llactormover.cpp:5565`, joint fetch `:5569`, owned write `:5581`), and the legacy branch does the same (condition `indra/newview/llactormover.cpp:6178`, joint fetch `:6182`, owned write `:6193`). `mPelvis` is the top skeleton bone (`indra/newview/character/avatar_skeleton.xml:2`); both leg hips are its children (`indra/newview/character/avatar_skeleton.xml:179`, `indra/newview/character/avatar_skeleton.xml:191`). Consequently, a gaze-authored pelvis rotation carries the legs.

The base skeleton does contain `mChest` between the extended spine and `mNeck` (`indra/newview/character/avatar_skeleton.xml:12`, `indra/newview/character/avatar_skeleton.xml:14`, `indra/newview/character/avatar_skeleton.xml:19`), but gaze currently writes only `mTorso` (`indra/newview/llactormover.cpp:5594`, `indra/newview/llactormover.cpp:6206`). All proposed chest access remains `getJoint("mChest")` guarded so unusual or partial rigs degrade to torso-only.

### Why owned gaze replaces a performance animation

At full acquire, `priority_env` is intentionally independent of intensity so an owned joint contains no animation bleed (`indra/newview/llactormover.cpp:5292`). An owned torso target is built relative to identity and nlerped over the current pose in the motor branch (`indra/newview/llactormover.cpp:5600`) and legacy branch (`indra/newview/llactormover.cpp:6212`). Neck/head owned paths additionally construct a neutral-root-frame desired world pose and cancel the actual animated parent (`indra/newview/llactormover.cpp:5633`, `indra/newview/llactormover.cpp:5685`, `indra/newview/llactormover.cpp:6244`, `indra/newview/llactormover.cpp:6300`). That cancellation is correct for `Replace`, but it is exactly why AO/talking spine motion disappears.

Eyes are already solved after the painted head. The motor path explicitly re-solves eye-in-head against the now-painted head for VOR (`indra/newview/llactormover.cpp:5734`, `indra/newview/llactormover.cpp:5810`); the legacy path reads the head world rotation after body/head writes (`indra/newview/llactormover.cpp:6351`, `indra/newview/llactormover.cpp:6374`). Any overlay design must retain this ordering.

### Two execution paths

`DirectorGazeMotionPrograms` defaults off (`indra/newview/app_settings/settings.xml:5495`). When on, the integration fills `GazeMotorSettings`, calls `ALGazeMotor::step()`, and returns after applying the motor pose (`indra/newview/llactormover.cpp:5336`, `indra/newview/llactormover.cpp:5348`, `indra/newview/llactormover.cpp:5413`, `indra/newview/llactormover.cpp:5933`). When off, the legacy applied-aim limiter and hard allocator run at `indra/newview/llactormover.cpp:5948` and `indra/newview/llactormover.cpp:6017`.

The motor restates the same capacity table in `effectiveCapacities()` (`indra/newview/algazemotor.h:449`) and applies soft recruitment at `indra/newview/algazemotor.h:1129`. Existing tests explicitly require band-zero motor allocation to be bit-identical to the legacy function (`indra/newview/tests/algazemotor_test.cpp:755`).

### Existing time and configuration model

Static actor strength is `Gaze::mIntensity`; acquisition/release state is `Gaze::mEnv` (`indra/newview/llactormover.h:976`, `indra/newview/llactormover.h:981`). The shared paint computes `effective_intensity`, smoothsteps `mEnv`, and forms the write weights at `indra/newview/llactormover.cpp:4975` and `indra/newview/llactormover.cpp:5211`.

`GazeCueList` is already an ordered per-cast presentation-time track (`indra/newview/lldirectorcast.h:60`, `indra/newview/lldirectorcast.h:119`). Its evaluator is a pure `upper_bound` lookup at the requested presentation time (`indra/newview/lldirectorcast.cpp:676`) and returns target, eye/head/body weights, and macro envelope state (`indra/newview/lldirectorcast.cpp:762`). Director gaze evaluates it from `LLPresentationTime::currentFrame().presentation_time` (`indra/newview/llactormover.cpp:4392`, `indra/newview/llactormover.cpp:4403`).

Per-actor UI edits correctly use the recent two-box authority pattern: read the cast `GazeTarget`, mutate it, then call `cast.setGazeTarget()` (`indra/newview/alpanellensgaze.cpp:206`). The cast stores the authored value and immediately forwards it to `LLActorMover::setGazeTargetConfig()` (`indra/newview/lldirectorcast.cpp:535`); the mover copies overrides into its runtime box at `indra/newview/llactormover.cpp:883`. New static options must use the same route.

## Control model: six orthogonal axes

| Axis | Question answered | Proposed values / source | Does not do |
|---|---|---|---|
| Ownership scope | Which joints are strict-owned versus legacy-weighted? | Existing `Blend`, `Head+Eyes`, `Upper body`; new `Planted spine` | Does not inspect animation priority or select overlay math |
| Pose composition | What endpoint is written when gaze is allowed? | `Replace`, `Additive overlay`, `Blend replace/overlay` | Does not choose joints |
| SL animation priority | Is this joint write allowed this frame? | Existing `Legacy final`, `0..6` | Does not blend or change cones |
| Anatomical distribution | How much aim belongs to each gaze joint? | Head/eyes, torso amount, cone profile, planted/chest split, lean curve | Does not decide animation ownership |
| Static strength | What is the actor/cue’s authored gaze strength? | Existing `mIntensity` / `mIntensityOverride` | Is not a timeline envelope |
| Temporal weight | Is gaze active at this presentation time? | Existing `mEnv`, cue weights, new influence keys | Does not mutate `mIntensity` or motor state |

The UI should call the current scope item “Legacy blend” to disambiguate it from pose composition, while retaining enum value 0 and behavior. No scene value is renumbered.

## Goal 1: planted-spine IK

### Recommended scope

Append `GAZE_PRIORITY_PLANTED_SPINE = 3` to `EGazePriority` at `indra/newview/llactormover.h:330`. Do not replace or reinterpret `GAZE_PRIORITY_UPPER_BODY`.

Resolve scope with explicit predicates rather than extending the current numeric `>=` assumptions at `indra/newview/llactormover.cpp:5274`:

```text
strict_head_eyes = scope is Head+Eyes, Upper body, or Planted spine
strict_spine     = scope is Upper body or Planted spine
strict_pelvis    = scope is Upper body only
planted_spine    = scope is Planted spine
```

This prevents enum ordering from accidentally making scope 3 own the pelvis. Use `strict_pelvis` for both the motor pelvis branch at `indra/newview/llactormover.cpp:5565` and the legacy pelvis branch at `indra/newview/llactormover.cpp:6178`. In the planted solve, set the hips yaw/pitch capacities to exactly `+0.0f`; do not merely skip the write after allowing hips to consume residual. Otherwise the head/spine would still under-recruit by an invisible hips share.

“Planted” means gaze does not rotate the pelvis. It does not erase pelvis motion authored by the underlying animation. In Additive mode, that animation remains visible; in Replace mode, gaze still leaves the animated pelvis untouched in this scope.

### Two-joint spine curve without extra reach

Extend `AnatomicalChainPose` with `mChestYaw` and `mChestPitch` next to the existing torso fields at `indra/newview/algazemath.h:205`; extend `GazeMotorPose` at its corresponding pose declaration (the recruited fields are copied into the shared chain at `indra/newview/llactormover.cpp:5484`).

Treat the current torso allocation as one conserved spine bucket:

```text
chest_share = clamp(configured share, 0, 1)
torso contribution = spine bucket * (1 - chest_share)
chest contribution = spine bucket * chest_share
torso + chest = old torso bucket, component by component
```

Do not give `mChest` a second capacity. This preserves total reach and keeps adjustable “Torso/Spine” cone values meaningful. A suggested opt-in chest share is 0.55, but it is read only in `Planted spine`; all existing scopes keep the current torso-only write.

Write parent to child: torso, chest, neck, head, eyes. Because `mSpine3`/`mSpine4` sit between torso and chest in the standard skeleton (`indra/newview/character/avatar_skeleton.xml:12`), the Replace endpoint for chest must be derived in world space and converted through the actual chest parent, just as neck/head currently cancel their actual parent at `indra/newview/llactormover.cpp:5654` and `indra/newview/llactormover.cpp:5710`. If `mChest` is absent, fold its share back into `mTorso` for that frame; never discard the angle.

Director capture/restore must add `DirectorJointPose mChest` beside `mTorso` (`indra/newview/llactormover.h:1089`), capture it when the planted scope can write it, and restore it with the other joints (`indra/newview/llactormover.cpp:3877`, `indra/newview/llactormover.cpp:3945`). Without this, an animation that does not key chest every frame can inherit the prior render paint.

### Reach limit and Turn-body handoff

For the current allocator, the directional yaw reach is the sum of the weighted eye, head, neck, torso, and (today) hips capacities. The weights are not simply the raw constants: eye capacity is `(1 - 0.75 * head_eye_blend)`, head/neck use `head_eye_blend`, and torso/hips use `head_eye_blend * torso_amount` (`indra/newview/algazemath.h:627`). Planted reach is the same sum with hips capacity zero. Pitch follows the same rule.

When `Turn body` is not selected, the correct behavior beyond planted reach is a stable undershoot. Keep the desired eye world direction, clamp every authored gaze contribution, and report residual/unreached yaw for diagnostics; do not reintroduce pelvis rotation to “fix” it.

When Director `mMode == 1`, preserve the existing root-yaw handoff. `applyDirectorBodyTurn()` follows presentation time, rate limits the yaw, and writes an upright world root rotation (`indra/newview/llactormover.cpp:4081`, `indra/newview/llactormover.cpp:4110`, `indra/newview/llactormover.cpp:4227`). It is intentionally different from a gaze pelvis bend. The current integration subtracts the root delta and re-recruits the remaining upper-chain yaw in both motor and legacy paths (`indra/newview/llactormover.cpp:5502`, `indra/newview/llactormover.cpp:6024`); the planted implementation must re-run the planted/chest-aware allocator there.

For planted scope, compute the trigger threshold as:

```text
effective turn threshold = min(DirectorGazeBodyTurnThresholdDeg,
                               resolved planted yaw reach)
```

then keep the existing function’s start/stop/debounce hysteresis. This hands off no later than the point at which the planted chain cannot absorb more yaw, while honoring a director’s lower requested threshold. The setting’s current 90-degree value is defined at `indra/newview/app_settings/settings.xml:5649`. Trigger calculation uses the unweighted, break-free body aim, as the current trigger and break-free body solve do (`indra/newview/llactormover.cpp:6017`, `indra/newview/llactormover.cpp:6116`). Natural glances and cue recoil must never turn the root.

Keep current safety gates: no root turn while seated, no cue-weighted partial body turn, and no Turn-body behavior in standalone `applyGaze()` where `director_runtime` is null. The existing guards are at `indra/newview/llactormover.cpp:5512`, `indra/newview/llactormover.cpp:6026`, and `indra/newview/llactormover.cpp:4692`.

### Both execution paths

- Legacy path: add a planted/custom overload or companion to `distributeAnatomicalChain()` at `indra/newview/algazemath.h:565`; use it for trigger, expressive solve, break-free body solve, and post-root-turn re-solve at `indra/newview/llactormover.cpp:6019`, `indra/newview/llactormover.cpp:6121`, and `indra/newview/llactormover.cpp:6127`.
- Motor path: add `mPlantPelvis` and `mChestShare` to `GazeMotorSettings` near the current chain-shaping inputs (`indra/newview/algazemotor.h:194`). Set hips capacities to zero before recruitment, return chest fields, and use the same planted profile in the root-delta re-recruit block at `indra/newview/llactormover.cpp:5536`.
- Default path: when scope is not Planted spine, call the existing functions and writes with their existing arguments and operation order.

## Goal 2: additive gaze over animation

### Public semantics

Add:

```cpp
enum EGazeComposition : S32
{
    GAZE_COMPOSE_REPLACE  = 0,
    GAZE_COMPOSE_ADDITIVE = 1,
    GAZE_COMPOSE_BLEND    = 2
};
```

`Replace` is the exact current behavior. `Additive overlay` computes the smallest gaze-authored correction from the post-animation pose toward the target, clamps that correction by the resolved gaze limits, and composes it over the animation. `Blend` nlerps between the Replace and Additive local endpoints using `composition_mix`: 0 is Replace, 1 is Additive.

This “Blend” is a hybrid endpoint, not the existing ownership scope named `GAZE_PRIORITY_BLEND`. The latter continues to select the legacy partial write alpha; the new mode selects the endpoint used by that write.

### Exact additive solve

At the start of `gazePaint()` (`indra/newview/llactormover.cpp:4759`), before any gaze write, snapshot the local rotations and required world rotations of pelvis, torso, chest, neck, head, and all eye joints. Director already captures the animation pose for later restoration (`indra/newview/llactormover.cpp:4709`), but standalone Actor Mover also needs a frame-local snapshot.

For head/spine:

1. Build the desired world look direction through the existing target, smoothing, eyeline, and cue machinery. Do not create another target resolver.
2. Measure the post-animation head forward vector from the snapshot. Compute the shortest swing that maps that forward vector to the desired world direction. Express its yaw/pitch in the same root-relative frame as `g.mBodyAimYaw/Pitch` so the existing anatomical distribution, body dead zone, limits, planted scope, and lean curve can be reused.
3. Distribute only this residual correction. If the animation already aims at the target, the residual is identity. A naive `animated_rotation * absolute_gaze_target` is rejected because it double-turns an animation that already looks toward the target.
4. Convert each allocated correction into the current parent frame and compose it in the viewer’s established additive order: `Q_overlay_local = Q_delta_local * Q_anim_local`. This matches the pose blender’s additive rotation order at `indra/llcharacter/llpose.cpp:326` and `indra/llcharacter/llpose.cpp:414`.
5. Apply parent to child, recomputing/caching the painted parent world rotation as necessary. Preserve animation roll unless the existing camera-roll/head-roll gaze channel intentionally contributes roll.

The limit profile constrains the gaze-authored delta, not arbitrary rotation already present in the animation. Additive gaze must not destructively “repair” an AO pose outside the comfort cone. At full additive strength, fixation is exact only when the remaining target error fits inside the cumulative correction capacity; otherwise it undershoots predictably.

For eyes, wait until head/neck/spine writes finish, then compare each animated eye forward (under the painted head) with the desired eye-world direction. Compose the smallest clamped eye delta over that eye’s animation rotation. Preserve the current split-eye, relaxed, near-lens, vergence, micro-saccade, and radial-cone stages at `indra/newview/llactormover.cpp:5752` and `indra/newview/llactormover.cpp:5835` for motor, and `indra/newview/llactormover.cpp:6071` and `indra/newview/llactormover.cpp:6378` for legacy. Solving after the head is the VOR/parent-cancellation invariant.

An animated talking torso will therefore continue to read in roll, breathing, and motion that does not conflict with fixation. Animated yaw/pitch that moves the face off target is necessarily countered to the extent required by the overlay; “keep all animation” and “keep exact fixation” cannot both hold on the same rotational degree of freedom.

### Write composition and weights

For each existing joint branch, construct both local endpoints:

```text
Q_replace  = current neutral/root-frame replacement endpoint
Q_additive = clamped correction delta * Q_animation_snapshot

Replace:  Q_endpoint = Q_replace
Additive: Q_endpoint = Q_additive
Blend:    Q_endpoint = nlerp(composition_mix, Q_replace, Q_additive)
```

Then call the existing yield gate and write once:

```text
write_alpha = gazeAllowedAlpha(avatar, joint, existing_base_alpha,
                               resolved_sl_animation_priority)
Q_final = nlerp(write_alpha, current_joint_rotation, Q_endpoint)
```

The gate must remain immediately before `setRotation()`, as it is now in the motor torso/head/eye paths (`indra/newview/llactormover.cpp:5607`, `indra/newview/llactormover.cpp:5716`, `indra/newview/llactormover.cpp:5862`) and legacy paths (`indra/newview/llactormover.cpp:6218`, `indra/newview/llactormover.cpp:6333`, `indra/newview/llactormover.cpp:6443`). A higher-priority regular animation means no write in all three composition modes. Equal priority still favors gaze. Overlay does not imply “ignore priority,” and priority yield does not imply Replace.

Preserve current intensity semantics by scope:

- Strict-owned scopes currently put intensity/cue weight into the owned target while `priority_env` performs acquire/release. Additive mode analogously scales the correction delta by the same pose weight.
- Legacy `Blend` scope currently includes intensity in `wEye`/`wBody`. Additive mode uses that existing alpha and does not multiply intensity a second time.
- The new keyframed influence multiplies the final write alpha once, as specified under Goal 3b.

Root Turn-body is outside pose composition. When explicitly selected it keeps its existing upright world-root write and the gaze chain composes over the root-adjusted residual. `Additive overlay` must not turn the root by itself.

### Integration points

- Data enum and per-actor sentinels: `indra/newview/llactormover.h:327` and `indra/newview/llactormover.h:337`.
- Runtime mirrors: `indra/newview/llactormover.h:939`.
- Copy/equality/readback: `indra/newview/llactormover.cpp:464`, `indra/newview/llactormover.cpp:883`, `indra/newview/llactormover.cpp:1257`.
- Director cue/live target copy: `indra/newview/llactormover.cpp:4392` and `indra/newview/llactormover.cpp:4463`.
- Resolve axes/weights once: `indra/newview/llactormover.cpp:5211` through `indra/newview/llactormover.cpp:5295`.
- Apply to both duplicated joint-write blocks: motor `indra/newview/llactormover.cpp:5553` through `indra/newview/llactormover.cpp:5879`; legacy `indra/newview/llactormover.cpp:6168` through `indra/newview/llactormover.cpp:6460`.

Do not refactor the Replace branch through a new generalized quaternion helper in the first implementation. Leave its statements intact under `if (composition == REPLACE)` and add the new endpoint path beside it. That makes the default byte-equivalence review tractable.

## Goal 3a: adjustable per-joint cone limits

### Limit profile

Add a plain `ALGazeMath::AnatomicalLimitProfile` containing degrees, not already-scaled radians:

| Field | Current default | Current source |
|---|---:|---|
| Eye allocation yaw / pitch | 25 / 14 | `indra/newview/algazemath.h:583` |
| Head yaw / pitch | 35 / 42 | `indra/newview/algazemath.h:589` |
| Neck yaw / pitch | 35 / 26 | `indra/newview/algazemath.h:591` |
| Spine bucket yaw / pitch | 45 / 20 | `indra/newview/algazemath.h:593` |
| Hips yaw / pitch | 35 / 15 | `indra/newview/algazemath.h:595` |
| Applied eye yaw / pitch | 24 / 14 legacy; 25 / 14 motor | `indra/newview/app_settings/settings.xml:25048`, `indra/newview/app_settings/settings.xml:25055`, `indra/newview/app_settings/settings.xml:5550`, `indra/newview/app_settings/settings.xml:5561` |
| Applied eye radial cone | 19.8 | `GAZE_DIRECTOR_EYE_ROT_MAX` at `indra/newview/llactormover.cpp:3558` |

The allocation eye cap and final socket cap are deliberately separate because they are already separate today. The UI should explain that the allocator reserves an eye share, while the final axis/radial values enforce socket safety. A user raising allocation yaw without raising the applied socket cap will not get a larger visible eye turn.

Sanitize every value as finite and nonnegative, with a conservative UI maximum (180 degrees for axes, 90 degrees for eye radial). Preserve `DirectorGazeExaggerate`: at 1 it uses exact profile values; above 1 it scales eye/head/neck and applied eye caps only, while spine/hips remain unscaled, matching `indra/newview/algazemath.h:598` and `indra/newview/algazemotor.h:472`.

### Lockstep strategy

There are two required behaviors:

1. Legacy/default profile disabled: `distributeAnatomicalChain()` and `effectiveCapacities()` execute their current constant code unchanged. This protects the documented bit-exact band-zero contract at `indra/newview/algazemotor.h:449`.
2. Custom profile enabled: both functions call one shared `fillEffectiveCapacities(profile, head_eye_blend, torso_amount, anatomy_scale, recruit_hips, yaw[], pitch[])`. The legacy distributor then runs its existing direct-min allocation order; the motor passes the exact same arrays to `recruitSlot()`.

This keeps the two effective tables in lockstep without forcing the default path through reorganized floating-point arithmetic. Extend the existing exhaustive bit-parity grid at `indra/newview/tests/algazemotor_test.cpp:755` with custom profiles and planted hips-off cases.

### Global and per-actor resolution

Use a tri-state per-actor mode rather than a single bool:

```text
mLimitProfileModeOverride = -1  inherit global
                             0  force legacy constants
                             1  use actor custom profile
```

The global `DirectorGazeCustomLimitsEnabled` defaults false. Global profile values carry the current constants. This permits a global tuned default, a per-actor custom profile, and a per-actor escape back to legacy.

Store the mode/profile in both `GazeTarget` and runtime `Gaze`, copy them through `setGazeTargetConfig()`, `getGazeTargetConfig()`, Director’s configured-target copy, and `sameGazeTargetConfigFields()`. These are the same authority points used by the fixed continuous controls (`indra/newview/llactormover.cpp:469`, `indra/newview/llactormover.cpp:883`, `indra/newview/llactormover.cpp:1271`, `indra/newview/llactormover.cpp:4463`). UI commits must go through `editGazeTargetsFor()` (`indra/newview/alpanellensgaze.cpp:211`).

## Goal 3b: keyframable gaze influence

### Use a separate presentation-time channel

Do not key `mIntensity`. It is an actor/cue style parameter, participates in preset transitions, and is smoothed toward Director target strength (`indra/newview/llactormover.cpp:4568`, `indra/newview/llactormover.cpp:4660`). Do not drive `mEnv`; that envelope owns enable/acquire/release state and has stateful stepping in both Actor Mover and Director paths (`indra/newview/llactormover.cpp:3765`, `indra/newview/llactormover.cpp:4645`).

Add:

```cpp
enum EGazeKeyInterpolation : S32 { STEP = 0, LINEAR = 1, SMOOTHSTEP = 2 };

struct GazeInfluenceKey
{
    F64 mTimeSec = 0.0;
    F32 mValue = 1.f;
    EGazeKeyInterpolation mInterpolation = SMOOTHSTEP; // outgoing segment
};

using GazeInfluenceKeyList = std::vector<GazeInfluenceKey>;
```

Store one list beside `CastMember::mGazeCues` and one beside `mSelfGazeCues` (`indra/newview/lldirectorcast.h:100`, `indra/newview/lldirectorcast.h:302`). It is a parallel channel on the same presentation timeline, not fields embedded in target cues. Embedding it in `GazeCue` would force directors to create duplicate target/acquisition events merely to shape strength and would couple two unrelated edit operations.

Evaluation is pure and scrub-stable:

- Empty list returns exactly 1 and the caller keeps the current formula branch.
- Sort stably by time when authored, like cue sorting at `indra/newview/lldirectorcast.cpp:627`.
- Before the first key, hold the first value; after the last, hold the last value.
- Between keys, evaluate the previous key’s interpolation mode using only the two keys and requested `presentation_time`.
- Duplicate-time keys resolve to the last stable key at that time.
- Clamp values to `[0,1]`; reject/sanitize non-finite times and values.

Evaluate at the same `presentation_time` already used for gaze cues (`indra/newview/llactormover.cpp:4392`). Pass the result to shared `gazePaint()` so both Director and standalone Actor Mover paths can use it. For a nonempty track:

```text
legacy weighted path:
    env_i = env_eased * effective_intensity * timeline_influence

strict-owned path:
    priority_env = env_eased * behind_eased * timeline_influence
    owned/additive pose strength retains existing mIntensity and cue weighting
```

This placement is important. Multiplying only the owned target by zero would produce identity and let `priority_env` replace a performance animation with a neutral joint. Multiplying the final write envelope makes zero influence perform no gaze write. Cue eye/head/body weights continue to multiply their respective channels after this common influence. The SL-priority gate runs last; either a zero influence or a priority veto yields no write.

Influence does not reset the gaze motor, direction smoother, reaction latch, or `mEnv`. Ramping to zero hides the layer while its aim remains warm; ramping back up reveals the correct current target without a fake reacquisition. That is the closest equivalent to Blender constraint influence.

### Persistence and cue UI

Add `writeGazeInfluenceKeys()` / `readGazeInfluenceKeys()` beside cue serialization at `indra/newview/lldirectorcast.cpp:333`, persisted as `gaze_influence_keys` / `self_gaze_influence_keys` beside the existing cue arrays at `indra/newview/lldirectorcast.cpp:1185` and `indra/newview/lldirectorcast.cpp:1296`. Absent keys in old scenes produce an empty list and exact influence 1.

The existing `Gaze Cues...` button opens a dedicated presentation-track floater (`indra/newview/skins/default/xui/en/panel_lens_gaze.xml:532`, `indra/newview/alpanellensgaze.cpp:1452`). Add an Influence lane/list and Add/Update/Delete key controls to `floater_gaze_cues.xml`, whose current cue list and time editor are at `indra/newview/skins/default/xui/en/floater_gaze_cues.xml:24` and `indra/newview/skins/default/xui/en/floater_gaze_cues.xml:48`. New keys default to the current playhead, as new cues already do at `indra/newview/alfloatergazecues.cpp:162`.

## Goal 3c: angle-driven lean falloff

### Separate it from soft recruitment

Current hard capacity fill starts torso only after upstream capacity is exhausted (`indra/newview/algazemath.h:641`). `DirectorGazeSoftRecruitDeg` smooths each of those knees in the motor by replacing hard excess with a C2 band (`indra/newview/algazerecruit.h:22`, `indra/newview/algazerecruit.h:98`, `indra/newview/algazemotor.h:1146`). It cannot choose a director-authored body-engagement threshold, and it is motor-only (`indra/newview/app_settings/settings.xml:5539`).

Add `EGazeLeanCurve { GAZE_LEAN_LEGACY = 0, GAZE_LEAN_ANGLE_EASE = 1 }`. Legacy is the default and calls the current allocator unchanged. Angle ease is initially supported for the new Planted-spine scope; extending it to legacy Upper body later is possible but should not mix an angle-driven spine with a hips-filling behavior in v1.

For aim vector `v = (yaw, pitch)` and magnitude `a = length(v)`, with threshold `T`, softness `S`, and maximum lean `M`, use:

```text
u = clamp((a - T) / max(S, epsilon), 0, 1)
ease = u*u*u*(u*(u*6 - 15) + 10)       // smootherstep, C2
requested lean magnitude = min(M, a * torso_amount * head_eye_blend * ease)
directional spine capacity = intersection of v/a with the resolved
                             spine yaw/pitch ellipse
lean magnitude = min(requested lean magnitude, directional spine capacity)
spine vector = (v/a) * lean magnitude
face residual = v - spine vector
```

If `a` is zero, both vectors are exact zero. If softness is zero in the opt-in mode, use a step at the threshold; do not divide by epsilon and pretend it is smooth. The maximum is a combined angular magnitude in degrees, while the per-axis spine cone remains the anatomical safety cap. The conserved spine vector is then split across torso/chest. The face residual is allocated across eyes/head/neck; excess beyond face plus spine reach remains an undershoot.

Recommended initial opt-in values are threshold 25 degrees, softness 20 degrees, maximum 20 degrees. They are inert while the mode is Legacy. `Torso` remains the performance amount: zero disables spine lean, one permits the full curve. `Stillness` and `Restraint` continue to scale the recruited result afterward at `indra/newview/algazemotor.h:1175` and `indra/newview/llactormover.cpp:6142`.

### Motor integration

The motor maintains different delayed samples for head, neck, and torso (`indra/newview/algazemotor.h:875`, `indra/newview/algazemotor.h:896`, `indra/newview/algazemotor.h:917`). Do not collapse those programs into one legacy sample.

In the opt-in planted/angle path:

- Evaluate `spineVector()` from the torso channel’s sampled yaw/pitch and write that result directly to torso/chest after clamping.
- For the head and neck channels, evaluate the same pure curve from that channel group’s sampled aim, subtract its curve spine vector, and run the face-only capacity allocator for that group’s slot. At convergence, the contributions sum to the same angle-conserving planted solution; during a retarget, the existing eye/head/neck/torso temporal stagger remains visible.
- Set hips exactly zero.
- Keep the motor eye desired-world-gaze/VOR solve aimed at the full target, not the face residual.
- In the root-turn residual block at `indra/newview/llactormover.cpp:5502`, run the same opt-in planted/angle solve after subtracting root yaw.

For the legacy path, run `spineVector()` once on the break-free applied body aim, allocate the face residual, then add expressive head/eye offsets without feeding them back into spine. This belongs next to the current break-free double solve at `indra/newview/llactormover.cpp:6116`.

`DirectorGazeSoftRecruitDeg` is applied only inside the remaining face-capacity allocator. Curve softness shapes body engagement across aim angle; soft recruit shapes transfer at eye/head/neck capacity boundaries. The UI must keep the labels and tooltips separate, and tests must exercise both nonzero together.

## New enums, settings, and stored fields

### Enums and per-actor fields

Add to `LLActorMover` data near `EGazePriority` / `GazeTarget` (`indra/newview/llactormover.h:327`, `indra/newview/llactormover.h:337`):

- `GAZE_PRIORITY_PLANTED_SPINE = 3`.
- `EGazeComposition` values `REPLACE=0`, `ADDITIVE=1`, `BLEND=2`.
- `EGazeLeanCurve` values `LEGACY=0`, `ANGLE_EASE=1`.
- `S32 mCompositionOverride = -1` and `F32 mCompositionMixOverride = -1.f`.
- `F32 mChestShareOverride = -1.f`.
- `S32 mLimitProfileModeOverride = -1` plus `AnatomicalLimitProfile mLimitProfile`.
- `S32 mLeanCurveOverride = -1`, and `mLeanThresholdDegOverride`, `mLeanSoftnessDegOverride`, `mLeanMaxDegOverride`, each `-1.f` for inherit.

Mirror these in runtime `Gaze` near existing overrides (`indra/newview/llactormover.h:950`) and include them in exact-config equality, config set/get, Director copy, and scene/cue serialization. Composition, scope, limit mode, and lean mode are discrete and should not be added to `ParamTransition::P_COUNT`, whose comment explicitly excludes discrete fields (`indra/newview/llactormover.h:1000`). Continuous mix, chest share, custom limits, and lean parameters may join preset transitions only in a later feature; v1 applies them immediately so the preset transition contract does not expand silently.

### Global settings and defaults

Add beside current gaze settings in `indra/newview/app_settings/settings.xml`:

| Setting | Type | Default | Resolution/use |
|---|---|---:|---|
| `DirectorGazeComposition` | S32 | 0 | Replace; actor `-1` inherits |
| `DirectorGazeCompositionMix` | F32 | 0.5 | Used only by composition Blend |
| `DirectorGazeChestShare` | F32 | 0.55 | Used only in Planted spine; no extra capacity |
| `DirectorGazeCustomLimitsEnabled` | Boolean | false | False takes untouched constant paths |
| `DirectorGazeLimitEyeYawDeg` / `PitchDeg` | F32 | 25 / 14 | Allocation capacity |
| `DirectorGazeLimitHeadYawDeg` / `PitchDeg` | F32 | 35 / 42 | Allocation capacity |
| `DirectorGazeLimitNeckYawDeg` / `PitchDeg` | F32 | 35 / 26 | Allocation capacity |
| `DirectorGazeLimitSpineYawDeg` / `PitchDeg` | F32 | 45 / 20 | Conserved torso+chest bucket |
| `DirectorGazeLimitHipsYawDeg` / `PitchDeg` | F32 | 35 / 15 | Legacy Upper-body only |
| `DirectorGazeLimitEyeApplyYawDeg` / `PitchDeg` | F32 | 24 / 14 | Custom-profile final eye axes |
| `DirectorGazeLimitEyeRadialDeg` | F32 | 19.8 | Custom-profile radial eye cone |
| `DirectorGazeLeanCurve` | S32 | 0 | Legacy allocator |
| `DirectorGazeLeanThresholdDeg` | F32 | 25 | Inert while curve 0 |
| `DirectorGazeLeanSoftnessDeg` | F32 | 20 | Inert while curve 0 |
| `DirectorGazeLeanMaxDeg` | F32 | 20 | Inert while curve 0 |

Do not remove or reinterpret `DirectorGazeComfortYawDeg/PitchDeg`, `BDMergeGazeEyeYawMax/PitchMax`, or `GAZE_DIRECTOR_EYE_ROT_MAX` on the legacy/default branch. The custom profile becomes authoritative only when explicitly enabled.

### Serialization

`writeGazeExpression()` and `readGazeExpression()` are the shared persistence points for cast targets and cue targets (`indra/newview/lldirectorcast.cpp:84`, `indra/newview/lldirectorcast.cpp:152`, `indra/newview/lldirectorcast.cpp:232`). Add optional keys for every new static override. Omit inherited/default fields; absent values construct the documented sentinels and preserve old scenes.

The current helper serializes continuous expression fields but not the recently added scope, SL animation priority, or camera-mode override. That is visible in the fields written at `indra/newview/lldirectorcast.cpp:84` compared with the full `GazeTarget` at `indra/newview/llactormover.h:378`. Do not assume those discrete fields already round-trip. As a small compatibility cleanup in the same schema work, persist them as optional `perf_scope`, `perf_anim_priority`, and `perf_camera_mode` keys; absence retains their current inherit sentinels. This prevents the new composition field from being the only discrete axis that survives a scene reload.

## UI design

### `panel_lens_gaze.xml`

The panel already has the per-actor selector at the top (`indra/newview/skins/default/xui/en/panel_lens_gaze.xml:10`), the two priority axes (`indra/newview/skins/default/xui/en/panel_lens_gaze.xml:90`), static intensity/torso controls (`indra/newview/skins/default/xui/en/panel_lens_gaze.xml:242`), and Turn-body mode (`indra/newview/skins/default/xui/en/panel_lens_gaze.xml:448`). Extend it as follows:

1. Ownership row: retain values 0–2 and append `Planted spine (IK)` value 3. Relabel value 0 to `Legacy blend` for clarity only.
2. Immediately below SL anim priority, add `Pose composition` with `Replace (current)`, `Additive overlay`, and `Blend replace/overlay`. Add an `Animation retention` slider shown/enabled only for Blend; 0 means Replace and 1 means Additive.
3. Relabel existing `Intensity` to `Base intensity`. Its tooltip should say that it is static style strength and is multiplied by timeline influence.
4. Add `Influence @ playhead` with a 0..1 slider/readout and diamond `Set/Update key` plus delete button. Disable key editing in `All` mode; per-actor tracks are not safely represented by the first display actor. Keep the existing `Gaze Cues...` button as the full track editor.
5. Add an `IK & limits` section:
   - `Chest share` 0..1, enabled only for Planted spine.
   - `Limits source`: `Inherit`, `Legacy constants`, `Custom`.
   - A compact yaw/pitch grid for Eye allocation, Head, Neck, Spine, Hips, Eye apply, plus Eye radial.
   - `Lean`: `Legacy fill` / `Angle ease`, and Threshold, Softness, Max spinners enabled only for Angle ease and Planted spine.
6. Keep `DirectorGazeSoftRecruitDeg` in a separately labelled motor-advanced row; tooltip: “smooths joint capacity handoffs; not the angle/lean curve.”

Bind all per-actor rows through new callbacks alongside the current priority/torso/intensity callbacks (`indra/newview/alpanellensgaze.cpp:295`, `indra/newview/alpanellensgaze.cpp:1195`, `indra/newview/alpanellensgaze.cpp:1263`). Refresh uses override-or-global resolution like the current priority code (`indra/newview/alpanellensgaze.cpp:922`) and respects `isEditing()` so draw refresh cannot fight an active control (`indra/newview/alpanellensgaze.cpp:975`). Reset buttons restore sentinels, not copied global values.

Performance presets should leave composition, scope, limits, chest share, and lean settings unchanged. `applyPreset()` currently mutates a deliberate list of performance fields (`indra/newview/alpanellensgaze.cpp:132`); do not silently expand it. The reset-performance handler likewise should not reset the new structural IK/overlay choices (`indra/newview/alpanellensgaze.cpp:1351`).

### `floater_actor_gaze.xml` and cue editor

`floater_actor_gaze.xml` is a scroll wrapper around `panel_lens_gaze.xml` (`indra/newview/skins/default/xui/en/floater_actor_gaze.xml:14`, `indra/newview/skins/default/xui/en/floater_actor_gaze.xml:24`). Keep the visible floater 560×720, increase the scroll-content and embedded-panel heights by the added rows, and raise `min_width` only if the yaw/pitch grid cannot fit the existing 516-pixel panel. Do not make the full floater taller than typical laptop work areas; the scroll container already exists for this reason.

Add the full influence lane to `floater_gaze_cues.xml` and its handlers to `ALFloaterGazeCues`, whose add/edit/delete path already rewrites a sorted per-actor cue list (`indra/newview/alfloatergazecues.cpp:375`, `indra/newview/alfloatergazecues.cpp:386`, `indra/newview/alfloatergazecues.cpp:402`). Use a separate influence revision or a shared track revision so cue-list refreshes do not depend on floater lifetime.

## Byte-identical default strategy

“Byte-identical” is an execution-path requirement, not just a default-value table.

- Keep `DirectorGazePriority` default 1 and all existing enum values unchanged (`indra/newview/app_settings/settings.xml:5341`).
- Composition default Replace: enter the current statements verbatim; do not build a mathematically equivalent generalized endpoint first.
- Scope not Planted: do not look up/write chest and do not change pelvis conditions.
- Custom limits disabled: call the current constant functions, including their exact `blend <= 0.001f` early return and direct-min allocation (`indra/newview/algazemath.h:620`, `indra/newview/algazemotor.h:495`).
- Lean mode Legacy: call the existing allocation without precomputing aim magnitude or curve values.
- Empty influence list: use the current `env_i` and `priority_env` assignments, not a reorganized expression multiplied by `1.f`.
- Existing scenes: absent LLSD fields produce constructor sentinels and empty influence lists.
- `DirectorGazeMotionPrograms` off: the legacy branch remains the default, and new motor state cannot leak into it. The current code already resets stale motor state on the legacy branch (`indra/newview/llactormover.cpp:5936`).
- Default Replace must preserve the existing gate call order, parent cancellation, camera roll, micro-life, vergence, lids, cue weights, Stillness/Restraint, split-eye target, and near-lens behavior.

Use recorded per-frame quaternion bit patterns as the acceptance test, not visual similarity.

## Recommended design

Ship the new `Planted spine (IK)` scope rather than changing Upper body. In that scope, hips capacity is zero, `mPelvis` is never gaze-written, and the conserved spine bucket is split across `mTorso`/`mChest`. Keep `Turn body` as an explicit upright root replant for yaw beyond the smaller of the authored threshold and planted reach; otherwise hold a comfortable undershoot.

Ship `Additive overlay` as error-corrective post-animation composition, not as multiplication by the existing absolute gaze target. Snapshot the animation pose, solve the shortest residual to the target, distribute/clamp that delta, compose `delta * animation`, and solve eyes after the final head. Provide a hybrid Blend endpoint. Continue to run the existing SL priority veto immediately before each write.

Use an opt-in limit profile shared by both custom allocator paths, with the untouched constant paths retained for default compatibility. Store keyframed influence as an independent stateless presentation-time lane. Implement angle-driven lean only for Planted spine in v1, with the existing soft-recruit band still responsible solely for per-joint knee smoothing.

## Integration plan and phasing

### Phase 0 — compatibility scaffold and golden baseline (cheap)

1. Add golden capture tests for default joint quaternions in both `DirectorGazeMotionPrograms` states, all three current ownership values, animation-priority settings, and empty cue tracks.
2. Add enums, sentinels, settings, resolver structs, equality/copy/readback, and optional LLSD fields, but keep every new mode at its inert default.
3. Extend panel controls and resets through `editGazeTargetsFor()`; verify the cast box and mover box match after every commit.

### Phase 1 — planted pelvis and Turn-body residual (cheap to medium, highest value)

1. Add scope 3, explicit scope predicates, hips-zero capacities, and suppressed pelvis writes in both paths.
2. Reuse the planted allocator for Turn-body trigger and root-delta re-solve.
3. Add residual/reach diagnostics and seated/cue guards.

This phase fixes the reported whole-body swing without requiring additive quaternion work.

### Phase 2 — chest split and adjustable limits (medium)

1. Add chest pose fields and capture/restore.
2. Add the conserved torso/chest split and parent-world conversion.
3. Add custom limit resolver/helper, final eye socket fields, settings, persistence, UI grid, and legacy/motor parity tests.

### Phase 3 — keyframable influence (medium, isolated)

1. Add the influence key list/evaluator and LLSD round trip.
2. Thread evaluated influence into both callers of shared paint.
3. Add the panel playhead control and full cue-floater lane.
4. Verify that empty lists execute the old formulas and that zero influence performs no writes.

### Phase 4 — angle-driven lean (medium to large)

1. Add the pure C2 curve and directional elliptical capacity clamp.
2. Integrate the break-free legacy solve and sampled motor group solves.
3. Integrate root-turn residual re-solve and combined nonzero soft-recruit testing.

### Phase 5 — Additive and hybrid composition (largest)

1. Add frame-local animation snapshots for standalone gaze and use Director captures where available.
2. Implement residual world-direction solve, parent-frame deltas, and endpoint composition for torso/chest/neck/head.
3. Implement post-head additive eye correction with split-eye/vergence/micro-life.
4. Apply it to every motor and legacy write branch without changing Replace statements.
5. Optimize only after quaternion and gate parity tests pass.

## Risks and mitigations

| Risk | Consequence | Mitigation |
|---|---|---|
| Quaternion multiplication/frame error | Overlay doubles aim, rolls unexpectedly, or varies by parent animation | Match the established `added_rot * blended_rot` order; test known rotations and convert world endpoints through actual parent world rotations |
| Chest is separated from torso by extended spine joints | Replace chest can inherit/cancel unexpected `mSpine3/4` animation | Construct chest endpoint in world space; guard parent; preserve extended-spine motion in Additive; fold share to torso if missing |
| Pelvis is skipped only at write time | Invisible hips allocation steals reach | Zero hips capacities before allocation and assert hips fields are exact +0 in planted mode |
| Root Turn-body and chain both consume yaw | Double-turn or snap at handoff | Preserve root-delta subtraction and re-run the same planted/lean allocator in motor and legacy branches |
| Intensity/influence zero neutralizes owned joints | Performance animation disappears at a zero key | Influence gates final write alpha; it never scales only the owned endpoint |
| Additive animation is already outside limits | Impossible promise of both exact fixation and anatomical pose | Clamp only gaze-authored delta; document that underlying animation is not repaired |
| Motor and legacy capacity tables diverge | Gate toggle changes pose or Turn-body trigger | One shared custom-profile capacity helper plus bit-parity grids; untouched default functions |
| Angle curve is conflated with soft recruit | Double easing, unexpected early joints, or motor-only behavior | Separate mode/settings/math; curve shapes spine vector, soft recruit shapes remaining face-chain knees |
| Priority gate is bypassed by new helper | Overlay fights higher-priority animation | Compute endpoint first, call `gazeAllowedAlpha()` immediately before the sole write in every branch |
| Director capture omits chest | Render paint accumulates when a motion does not key chest | Add symmetric chest capture/restore and release tests |
| Cue/preset data overwrites structural choices | A performance preset unexpectedly changes IK/overlay | Leave structural fields out of `GazePerformancePreset`; influence is a separate track |
| Scene schema silently loses discrete modes | Reload changes scope/composition/priority | Optional LLSD keys, absent=inherit, and round-trip tests; include current discrete overrides during schema work |
| UI “Blend” ambiguity | Operators confuse ownership with composition | Rename display label to `Legacy blend`; use `Blend replace/overlay` for composition; keep numeric values |
| Added per-frame world solves cost CPU | Large casts lose frame time | Run residual snapshots/solves only when composition is non-Replace; no-key/no-custom/no-lean defaults remain fast branches |
| Recent behavior regresses | Priority ramp/release, closed-floater auto-cycle, or slider authority breaks | Keep yield gate/write order, keep the startup idle registration at `indra/newview/llstartup.cpp:2244`, and route UI through `editGazeTargetsFor()` |

## Test plan

### Pure math and byte compatibility

1. Extend the anatomical-chain test at `indra/newview/tests/algazemath_test.cpp:281`:
   - planted hips are bitwise `+0.0f` for positive/negative/zero aims;
   - planted reach equals the weighted sum excluding hips;
   - angle beyond reach returns stable residual and no pelvis compensation;
   - torso plus chest equals the old torso component at shares 0, 0.55, and 1;
   - missing-chest fallback preserves the full torso bucket.
2. Preserve and expand the explicit 1x exact-equality test at `indra/newview/tests/algazemath_test.cpp:638`.
3. Extend the motor/legacy bit grid at `indra/newview/tests/algazemotor_test.cpp:755` across custom profiles, planted hips off, blend edge `0.001`, anatomy scales, torso amounts, positive/negative zero, and yaw seam values.
4. With every new option at default, compare old/new serialized per-frame local joint quaternion bytes for pelvis, torso, neck, head, four eyes, and lid weights over acquisition, hold, release, cue macros, and a 180-degree target seam. Run with motor off and on.

### Planted spine and Turn body

1. Aim through 0..180 degrees yaw and ±100 degrees pitch. Assert `mPelvis` before/after gaze is identical in Planted scope and changes exactly as before in Upper body.
2. Verify legs/feet remain on the animation axis in Planted scope while torso/chest curve.
3. Verify full-scale torso+chest never exceeds the resolved spine cone and the split does not increase total reach.
4. With Turn body off, verify stable undershoot beyond reach. With Turn body on, verify trigger occurs at `min(setting, planted reach)`, root stays upright, root delta plus chain residual tracks without double counting, and hysteresis avoids direction chatter.
5. Repeat seated, cue body weight below 1, target behind, safety release, LOD-skipped Director frames, and missing chest/neck joints. Current release/LOD behavior is rooted at `indra/newview/llvoavatar.cpp:5148` and `indra/newview/llactormover.cpp:4277`.

### Composition and animation priority

1. Identity animation: Additive reaches the same target as Replace within limits.
2. Animation already aimed at target: additive delta is identity and the local animation quaternion is unchanged.
3. Talking/gesturing animation with torso/head yaw, pitch, and roll: target remains fixed within capacity; non-conflicting roll/breath motion remains visible.
4. Hybrid endpoints: mix 0 is exactly Replace, mix 1 is exactly Additive, intermediate values follow shortest quaternion interpolation without sign flips.
5. For each joint and composition, test SL priority lower/equal/higher than gaze. Higher performs no write; equal writes; Legacy final always applies. Include a regular priority-6 motion and an SL additive motion, matching the observation semantics at `indra/llcharacter/llpose.cpp:297`.
6. Test Head+Eyes, legacy Blend, Upper body, and Planted spine independently. Verify composition never expands the selected joint set.
7. Repeat motor off/on, split-eye targets, Relaxed, Near-lens, vergence, camera roll, natural breaks, Stillness, Restraint, and blink/lid follow.

### Adjustable limits and lean curve

1. For every profile field, set smaller/larger values and verify both paths clamp the same converged contributions. Verify the applied eye axis/radial caps are independently effective.
2. Verify per-actor Custom beats global, Force legacy defeats a global custom profile, and Inherit follows later global edits.
3. Verify Exaggerate scales only eye/head/neck and eye-apply fields, not spine/hips.
4. Lean mode Legacy must be bit-identical. In Angle ease, sample immediately below/at/above threshold and both softness edges; check value, first derivative, and second derivative continuity for nonzero softness.
5. Verify maximum and directional ellipse clamps for yaw-only, pitch-only, and diagonal aim. Verify torso amount zero gives exact zero spine.
6. Run nonzero angle softness together with `DirectorGazeSoftRecruitDeg` 3 and 8 degrees; assert no discontinuity and demonstrate that changing one control does not change the other’s threshold.
7. Retarget during the motor’s head/neck/torso stagger and assert bounded per-frame position/velocity, extending the motor continuity style at `indra/newview/tests/algazemotor_test.cpp:740`.

### Influence track, persistence, and scrubbing

1. Empty key list returns the exact old branch. One key holds. Multiple Step/Linear/Smoothstep keys evaluate correctly before, on, between, and after keys, including duplicate times and non-finite input sanitization.
2. At influence zero, assert no joint write even under strict ownership and intensity 1. At influence one, assert exact unkeyed output. Verify no double multiplication in legacy Blend scope.
3. Seek directly to arbitrary presentation times, scrub backward/forward, pause at 0×, change time scale, close both gaze floaters, and compare repeated samples at the same time. Influence values and final pose weights must be deterministic.
4. Ramp influence while the motor retargets. Confirm motor/direction state stays warm and the layer reappears on the current target without reacquisition.
5. Round-trip new scenes with global/per-actor structural options, custom profiles, influence keys, and cue targets. Load old scenes with every new field absent and compare defaults. Verify current scope, SL priority, and camera-mode overrides also round-trip after the optional schema cleanup.

### Regression checklist for the three recent gaze commits

1. SL animation priority/hotkeys: exercise the look-at toggle actions registered around `indra/newview/llviewerinput.cpp:506`; confirm the per-joint gate and eased release remain unchanged.
2. Floater-independent auto-cycle: leave Actor Gaze closed and verify the idle callback registered at `indra/newview/alpanellensgaze.cpp:287` and `indra/newview/llstartup.cpp:2244` still changes presets.
3. Slider authority: for You and A–D, move Head/eyes, Torso, Base intensity, and every new per-actor row; verify cast storage, mover mirror, Director render path, preset override, reset-to-inherit, and `All` broadcast agree. The authoritative commit pattern is `indra/newview/alpanellensgaze.cpp:211`, and the fixed continuous callbacks are at `indra/newview/alpanellensgaze.cpp:1263`.

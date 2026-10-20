# Reflect — mirror-image clones (Part A) and mirrored self playback (Part B) — design brief

**Date:** 2026-10-20 (rev 2, after adversarial review round 1)
**Status:** DESIGN ONLY. Nothing implemented, nothing built.
**Process (per `CLAUDE.md`):** Codex implements from this brief (`--write`, `--prompt-file`), an
independent Opus sub-agent reviews the code ADVERSARIALLY, loop to 0 must-fix, THEN Claude builds once,
then the user tests in-world. Codex never builds.

> **BASE BRANCH — READ FIRST.** Every `file:line` below is from `origin/fix/animesh-clone-pose-polish`
> at `057e558`, the newest pushed branch that contains Ghost Studio's **True Mirror** drive. True Mirror
> is **not on `develop`**. If the user has unpushed local work, diff it against this brief and
> re-anchor the line references before implementing.

**Review history of this brief:**
- **Round 1** (independent Opus reviewer): 5 must-fix, 8 should-fix and 4 nits. All were applied in
  rev 2. Claude independently re-verified the load-bearing ones (seated root parenting, bones-only
  skeleton, LL quaternion order, hip asymmetry, proxy tick order). The disposition log is in the
  Appendix.
- Part B is new in rev 2 and has not yet been reviewed.

---

## 1. The ask

> "What about mirrored animation for clones, say by flipping the name of the bones, so an avatar and a
> clone could face each other." … "Reflect drive — that sounds perfect."
> "Would it be possible to apply a global [Reflect] to animations I can play from inventory?" … "Yes."

**Part A — Reflect drive.** A new Ghost Studio drive mode for **entity clones**. The clone shows the
live **mirror image** of its source's motion:

- The source raises its **right** arm toward the clone, and the clone raises its **left** arm toward
  the source.
- The source turns 30° to its left, and the clone turns 30° to its right.
- With **Face pair**, the two stand facing each other across a mirror plane.

**Part B — Reflect for self.** A global toggle. While it's on, animations you play **from inventory**
play on your own avatar as their mirror image. This is **local to your viewer**: the region and other
viewers still get the original animation.

Both parts are **motion** mirrors. Geometry is not reflected (§7). They share one maths header and one
test suite.

---

## 2. Scope (stated up front, per "ship whole")

**In this delivery, as one batch:**

*Shared*
1. A pure header, `alreflectmath.h` (maths and pairing rule), plus TUT tests.
2. `[GhostReflect]` log verdicts for both parts.
3. A user-guide section in `doc/MACHINIMA_USER_GUIDE.md`.

*Part A*

4. `DRIVE_REFLECT` drive mode, eligible exactly where `DRIVE_TRUE_MIRROR` is (entity clones only).
5. A reflected **bone** stamp, a reflected face stamp, and a reflected root rotation read in the source's
   **world** frame.
6. A per-instance mirror plane (Fixed or Live), optional Mirror position, and the **Face pair** helper.
7. UI in the shared Ghost Studio panel (both hosts), the `/ghostanim reflect` chat command, and
   tooltips.

*Part B*

8. A global setting plus a **Reflect** checkbox in the animation preview floater.
9. A mirrored-copy builder for inventory animations (in-memory, client-only ID, never uploaded).
10. Substitution for **Play locally** and for the region's echo of **Play in world** on your own
    avatar, with consistent stops.

**Explicitly NOT in this delivery** (each needs its own brief):

| Deferred | Why |
|---|---|
| Looking-glass (geometry) mirror, where asymmetric hair, straps and text are reversed | Needs a negative-determinant render transform and a front-face winding flip. That is shared GL state (`CLAUDE.md` rule 1), a different risk class. |
| Swapping held props to the other hand | Needs re-attaching the clone's cloned attachments to the paired attach point. It touches the clone-attachment pipeline. Open decision D3. |
| The clone's **own** gaze on top of the stamp ("both look at camera") | Gaze runs inside `updateCharacter`, before the post-object-list stamp. Running it after needs a reordering with its own review. |
| Part B for gestures, AO and the Animation Switchboard | Different playback paths (gesture manager, AO, Director). Open decision D6. |
| Uploading mirrored copies so others see them | That's redistribution, and it's not OK for animations the user didn't make. Part B never uploads. |
| Fixing True Mirror's pre-existing seated-source root defect (§4.6) | True Mirror stays byte-identical in this delivery. Open decision D5. |
| AutoAnimate under Reflect | It replays learned assets unmirrored. AutoAnimate is **dormant** under Reflect (§5.2). |
| Overlay ghosts | True Mirror ignores them (`alghoststudio.h:107`). Reflect matches. |

There are **no** pipeline, shader, render-target or GL-state changes anywhere in this delivery. If an
implementation finds it needs one, stop and report back.

---

## 3. What the code PROVES today (base-branch file:line)

### 3.1 Part A

| Fact | Where |
|---|---|
| Drive modes: `DRIVE_MIRROR=0, DRIVE_DIRECTED, DRIVE_FROZEN, DRIVE_TRUE_MIRROR`. New values must be **appended last** (enum comment). | `alghoststudio.h:97-111` |
| True Mirror stamps the source's FINAL pose from a post-pass right after `gObjectList.update()`. | `alghoststudio.cpp:2024-2053`, `alghoststudio.h:605`, call site `llappviewer.cpp:5417` |
| The stamp self-gates on the **ghost's own** drive mode, because the Director anim switcher can re-drive a ghost without touching the Studio model. | `alghoststudio.cpp:2038-2041`, `llghostavatar.cpp:1570` |
| The Director switcher reverts a ghost straight to MIRROR and leaves `inst.mDriveMode` unchanged. | `aldirectoranimswitcher.cpp:313, 351, 373, 405, 559` |
| Stamp: once per frame; holds the last pose if the source is missing; resolves the source via the ghost's `mAnimationSourceId` (not `inst.mSource`, which is a cast id where null means "me"). | `llghostavatar.cpp:1568-1624` |
| Bone stamp: root rotation copied with `getRootJoint()->getRotation()` (parent-local), root position never. Then, per `getSkeleton()` joint, rotation and position copied. Scale is not copied. | `llghostavatar.cpp:1626-1674` |
| **`getSkeleton()` holds only the 133 bones.** Collision volumes live separately in `mCollisionVolumes`. | `llavatarappearance.cpp:698-707`, `:1530` |
| Collision-volume morphs (including physics params 1206/1207) move volumes **incrementally**. Stamping volumes would make the error build up frame after frame. | `llpolymorph.cpp:648-650`, `:741-743` |
| **A seated avatar's root is parented to the seat.** Its `getRotation()`/`getPosition()` are seat-relative. | `llvoavatar.cpp:9246` (`setParent(&sit_object->mDrawable->mXform)`), `:5087-5088` |
| Face stamp copies `Express_*`, `Blink_*`, `Lipsync_*` and params 1200..1207, by id. | `llghostavatar.cpp:1676-1759` |
| **`setEntityDriveMode` clamps `mode` to `[DRIVE_MIRROR, DRIVE_TRUE_MIRROR]`.** A new value 4 would silently become True Mirror. It is the only drive-mode clamp. | `llghostavatar.cpp:1468-1471` |
| Entering True Mirror pauses the clone's controller and stops its eye motion. Leaving it releases the pause. | `llghostavatar.cpp:1489-1494`, `:1537-1559` |
| Actor Mover parks a walk and skips gaze for `isTrueMirrorDriven()` clones. | `llactormover.cpp:4399-4410`, `:5788-5797` |
| Studio facing writers skip clones where `trueMirrorOwnsFacing(inst)` is true. | `alghoststudio.cpp:1679, 1924, 2002, 2076`; defined `:2016-2022` |
| **`applyEntityTransform` writes position AND rotation.** `setGhostRotation` calls `mRoot->setWorldRotation(inst.mRotation)`. | `alghoststudio.cpp:1425-1438`, `llghostavatar.cpp:1158-1174` |
| `Instance::setFootGlobal` bumps `mTransformRevision` and clears `mChaosHasBase`. | `alghoststudio.h:476-482` |
| **The manip proxy ticks AFTER the post-pass.** On any revision change it calls `applyEntityTransform`, which would overwrite a stamped facing in the same frame. | `llappviewer.cpp:5417` vs `:5540`; `alghostmanipproxy.cpp:488`, `:748-756` |
| Formation motion writes `mFootGlobal` every frame. Copies duplicate the whole Instance. Chaos recomputes from `mChaosBaseFoot`. | `alghoststudio.cpp:2203-2289`; `:916`, `:965`; `:2310-2319` |
| Other placement writers. | `alghoststudio.cpp:1956, 1972, 2149, 2287, 2331, 2780, 2798, 2893, 3602, 3641, 3693, 4556, 4637` |
| AutoAnimate sites that test `MIRROR \|\| TRUE_MIRROR` or show dormant text. | `alghoststudio.cpp:523-528`; `llghostavatar.cpp:633`, `:892-895`, `:942-943`, `:3448-3451` (`autoAnimLinksetStep`); `alpanelghoststudio.cpp:2208-2209`, `:2238-2239` |
| Animesh on the clone keeps the ObjectAnimation ledger mirror under True Mirror. The repeat-held-animesh path is gated the same way. | `llghostavatar.cpp:3446-3451`, `:3191-3193` |
| Ghost Studio instances are **session-only**. Nothing persists `mDriveMode`. | `alghoststudio.h:351-366`, `alghoststudio.cpp:15`, `:554` |
| `setInstanceDriveMode` does **not** fan out to groups. The panel's `onDriveModeCommit` does. | `alghoststudio.cpp:2353-2374`; `alpanelghoststudio.cpp` |
| Drive combo values 0..3; the panel is hosted in two floaters. | `panel_ghost_studio.xml:218-224`; `floater_director.xml:567`, `floater_ghost_studio.xml:18` |
| Chat maps `mirror`, `truemirror` and `freeze`. The usage string appears twice. | `alchatcommand.cpp:820-840`; `:785`, `:839` |
| True Mirror settings: `GhostTrueMirrorCopyFacing`, `GhostTrueMirrorCopyFace` (default true). | `settings.xml:7230-7253` |
| LL `operator*`: `a*b` is the Hamilton product `b⊗a`, meaning **apply a, then b**. | `llquaternion.cpp:541-551` |

### 3.2 Part B

| Fact | Where |
|---|---|
| Inventory right-click "Play Inworld" and "Play Locally" both open the preview and call `LLPreviewAnim::play`. | `llinventorybridge.cpp:7270-7285` |
| `play()`: Inworld does `gAgent.sendAnimationRequest(assetID, START)`; Locally does `gAgentAvatarp->startMotion(assetID)`. | `llpreviewanim.cpp:78-130` |
| Precedent for a per-playback client-side option: a priority override that never mutates shared asset data. `draw()` re-applies it once the motion goes active, resolved via `mItemID`. | `llpreviewanim.cpp:116-141`, `:161-179`; `floater_preview_animation.xml:88` |
| Stop paths: the toggle-off branch and `onClose` both call `stopMotion(assetID)` and send STOP. | `llpreviewanim.cpp:144-147`, `:211-219` |
| The region's echo for any avatar, self included, goes through `LLVOAvatar::processSingleAnimationStateChange(anim_id, start)`, which ends in `startMotion`/`stopMotion(anim_id)`. | `llvoavatar.cpp:7158+` |
| `LLKeyframeMotion::onInitialize` checks the in-memory keyframe cache **before** any asset fetch. | `llkeyframemotion.cpp:585` |
| `deserialize(dp, id, …)` builds a JointMotionList and caches it under `id`. Constraint volumes resolve name → index (`getCollisionVolumeID`), and the chain indices are rebuilt. | `llkeyframemotion.cpp:1888`, `:2532`, `:2571`, `:2694` |
| `serialize(dp)` writes from `mJointMotionList`; constraint volumes are written **by name** via `findCollisionVolume(index)`. | `llkeyframemotion.cpp:2705+` |
| After a fetch, asset bytes are read from `LLFileSystem(id, AT_ANIMATION)`. The fetch is `gAssetStorage->getAssetData`. | `llkeyframemotion.cpp:3097+` (read ~`:3134-3144`), `:560` |
| Local-animation precedent: `createMotion(id)` + `deserialize(bytes, id, false)` + `startMotion(id)`; validation through a probe id plus `removeKeyframeData`. | `lllocalanim.cpp:477-507`, `:118-135` |
| **Load each id exactly once.** `addKeyframeData` overwrites the cache slot without freeing it, so a second load leaks a list. | `lllocalanim.cpp:363-368`, `llkeyframemotion.cpp:3211-3214` |
| Hand poses: `RELAXED_L/_R`, `POINT_L/_R`, `FIST_L/_R` pair up. `SALUTE_R`, `PEACE_R`, `PALM_R` have no left twin. | `llhandmotion.h:47-60` |

---

## 4. The maths

SL's avatar frame is **+X forward, +Y left, +Z up**. The sagittal reflection is `S = diag(1, −1, 1)`.

### 4.1 Why renaming bones alone is wrong

The right arm rests along −Y. "Reach forward" for it is +90° about Z, which carries −Y to +X. Swap the
name only, and the left arm (resting along +Y) receives +90° about Z, which carries +Y to **−X**: it
swings **behind the back**. With the reflected rotation (−90° about Z) it reaches forward. This is TUT
discriminator T2.

### 4.2 The per-joint rule (local frame)

For each source bone `J` with partner `P(J)` (itself for centre bones):

- **Rotation:** `(x, y, z, w)` becomes `(−x, y, −z, w)`, written to `P(J)`. This is conjugation by `S`
  (an axis is a pseudovector, so it maps to `det(S)·S·axis`).
- **Position:** `(x, y, z)` becomes `(x, −y, z)`, written to `P(J)`.
- **Scale:** not written.

**Why the local rule gives a world mirror image** (the reviewer's inference, by induction):

- Because the stamp fully overwrites every bone's local rotation and position, the result is
  `W'(P(J)) = S·W(J)·S` down the chain. This needs a symmetric hierarchy and symmetric joint scales.
- It does **not** depend on joint-frame alignment, so eyes, `mFaceEyeAlt*`, hind limbs, wings and tail
  are all fine.
- It does not depend on LL's row-vector convention either, because `S` is diagonal and its own
  inverse.

### 4.3 Preconditions (what's verified, and what isn't)

**Hierarchy:** a scripted parse of `avatar_skeleton.xml` found every partner's parent is the parent's
partner (0 mismatches). Centre joints have y = 0. All bones have **zero** rest rotation. The collision
volumes' rest rotations and positions are symmetric.

**Bone rest positions are only near-symmetric.**

| Bone | Left y | Right y |
|---|---|---|
| `mHip` | 0.127 | −0.129 |
| `mKnee` | −0.046 | 0.049 |
| `mAnkle` | 0.001 | 0.000 |

The stamp writes reflected source positions, so the clone's legs end up within **3 mm** of their bind.
That's invisible in practice, but it isn't "exact" (§7, L2).

**Collision volumes are NOT stamped** (§3.1). They ride their mirrored parent bones.

### 4.4 Pairing rule — by NAME, never by index

Rule, applied in order, first match wins:

1. If the name contains `Left`, replace it with `Right` (and vice versa).
2. Otherwise, if it starts with `L_`, swap to `R_` (and vice versa).
3. Otherwise, if it starts with `LEFT_`, swap to `RIGHT_` (and vice versa).
4. Otherwise the joint is a centre joint and is its own partner.

| Population | Pairs | Centre | Misses |
|---|---|---|---|
| **Bones (what the stamp touches)** — 133 | **50** | **33** | 0 |
| Full skeleton (bones + 26 collision volumes) — 159 | 59 | 41 | 0 |

The rule also reproduces all 116 paired entries of the FS poser table. The poser table lacks
`LOWER_BACK`, `mFaceEyeAltLeft/Right` and `mFaceJawShaper`, so use the rule. Part B needs the volume
pairs, for constraints.

Build a `src bone index → dst bone index` table **once per clone build**, by name via `getJoint()`.
Don't reuse True Mirror's index-parallel shortcut. Skip and log an unresolved partner.

### 4.5 Face and physics params

| Param | id | Range | Under Reflect |
|---|---|---|---|
| `Blink_Left` ↔ `Blink_Right` | 58 ↔ 59 | 0..1 | **swap** |
| `Butt_Physics_LeftRight_Driven` | 1206 | −1..1, default 0 | **negate** |
| `Breast_Physics_LeftRight_Driven` | 1207 | −2..2, default 0 | **negate** |
| every other param True Mirror copies | — | — | copy unchanged |

1206 and 1207 are `volume_morph` shifts along +Y that scale linearly with the weight, so negating them
is the reflection (`avatar_lad.xml:7586-7604`, `:8140-8153`). The only other lateral params in the copied
set are the one-sided `Express_*` morphs (§7, L3).

### 4.6 Mirror plane and root rotation — WORLD frame

A per-instance vertical plane: a point `P0` and a horizontal unit normal `n`. `M = I − 2nnᵀ`.

**Read the source root in WORLD space:** `R_src = getRootJoint()->getWorldRotation()`, and positions via
`getWorldPosition()` / global. When the source sits (a poseball dance, for instance), the root is
parented to the seat (§3.1), so `getRotation()` is seat-relative and would be wrong by the seat's
rotation. Use world values everywhere: the root formula, Face pair, the bisector, Mirror position and
the verdicts.

**Pre-existing:** True Mirror copies the parent-local `getRotation()`, and its comment says the root has
"no parent" (`llghostavatar.cpp:1627-1632`). That's wrong for a seated source. It's out of scope here
(True Mirror stays byte-identical); see open decision D5.

**Clone root:** written with `mRoot->setWorldRotation(...)`, which is correct whether or not the root
has a parent.

**Formula:** `R_clone = M · R_src · S`, column-vector matrix notation. With `M = −R_n(π)` and
`S = −R_y(π)`, the minus signs cancel. In **LL `operator*` order** (apply left first):

    q_clone = q_y(π) * q_src * q_n(π)

where `q_y(π)` is π about the local Y axis and `q_n(π)` is π about the world axis `n`.

**Tests must be behavioural and full-3D** (T5). For an upright source, `M·R·S` reduces to a yaw-only
"2φ − yaw" rule, so a yaw-only implementation would pass any upright-only test. Use pitched and rolled
source rotations.

When `GhostTrueMirrorCopyFacing` is off, the clone keeps its own facing and only the bones reflect.

**Plane modes** (per instance; source = the ghost's resolved `mAnimationSourceId`, the same one the
stamp uses):

- **Fixed** (default): the perpendicular bisector of the source and clone foot positions (horizontal
  components).
  - **Initial-capture fallbacks** when they are closer than 0.1 m horizontally:
    1. `n` = the source's horizontal world forward.
    2. If the source is lying down or steeply pitched (horizontal forward shorter than 0.2), the last
       valid plane.
    3. With no last valid plane, world +X. The log says so.
- **Live:** the bisector, recomputed every frame.
  - **Hysteresis:** it becomes degenerate below 0.1 m and recovers above 0.3 m. In between it holds
    the last plane. It never falls back to "source forward" once a plane exists, which avoids a
    snap of up to 180° when leaving the degenerate zone.
  - **Known behaviour (state it in the UI tooltip):** the clone's yaw turns at twice the bearing rate,
    so it whirls when the source passes close to it.
  - Live is unavailable while Mirror position is on.
- Flipping the sign of `n` is harmless, because `M` is unchanged.

**Plane lifecycle:**

- **Capture lazily** in the post-pass whenever the clone's own mode is REFLECT and the plane is invalid.
  This covers entry paths that skip `setInstanceDriveMode`: runtime replacement (`alghoststudio.cpp:422`)
  and switcher returns.
- **Invalidate:**
  - when leaving REFLECT for anything except FROZEN (freeze keeps it);
  - on a user gizmo move of the clone;
  - on every copy path.

### 4.7 Mirror position (optional, per instance)

The clone's desired foot = the source's world foot reflected across the plane in XY, at the source's
foot height.

**Who owns placement while it runs:**

`reflectOwnsPosition(inst)` is a **runtime** predicate, re-evaluated every frame. All of these must
hold:

- the **ghost's own** mode is REFLECT;
- `mReflectMirrorPosition` is on;
- the plane is Fixed and valid;
- the source is resolved;
- the Studio motion mode is OFF;
- the instance is not in a locked group;
- chaos is off.

If any of them goes false (motion started later, joined a group, chaos turned on, switcher re-drive,
source missing), the clone simply **holds where it is**. The status line says why. Greying the UI is
not enough; the predicate is the guard.

**Post-pass order, per instance:** (1) plane capture/update → (2) foot push → (3) stamp.

**The foot push:**

- Write `inst.mFootGlobal` **directly**, with **no** `mTransformRevision` bump, and clear
  `mChaosHasBase`. Then call `ghost->setGhostFootPosition(...)` **only**.
- Never `applyEntityTransform`, which would also write `inst.mRotation` over the facing, and never
  `setGhostPosition`, which disables the foot lock (`llghostavatar.cpp:1052`).
- Because the push happens before the stamp, the stamp's facing is the last write. Pose and position
  land in the same frame (by the foot lock's precedent; confirm in-world that attachments don't lag).

**The manip proxy** runs after the post-pass. It must not call `applyEntityTransform` for an instance
where `reflectOwnsPosition` holds (`alghostmanipproxy.cpp:488`, `:754`). Turning Mirror position off
leaves the clone where it is, because the model already holds the last reflected foot.

**Copies:** every copy path (`alghoststudio.cpp:916`, `:965`, plus any snapshot or duplicate path) must
reset `mReflectMirrorPosition = false` and `mReflectPlaneValid = false` on the copy. Otherwise N copies
all get slammed onto one reflected foot.

### 4.8 Why the pose stamp for clones, but asset copies for self

Reflection is an automorphism of the rotation group, and slerp commutes with it. So for keyframe content,
mirroring each asset and then blending equals blending and then mirroring the final pose.

- **Clones** have a live source with procedural layers (gaze, IK, Pose Polish), so the pose stamp is
  right.
- **Your own avatar** has no "source" to stamp from, so Part B mirrors the assets. The procedural layers
  stay unmirrored there (§7, L9).

---

## 5. Part A design

### 5.1 Enum and clamp

- `alghoststudio.h:110`: append `DRIVE_REFLECT` (value 4) and extend the comment.
- `llghostavatar.cpp:1470`: widen the clamp to `DRIVE_REFLECT`. **This is the trap:** if it's missed,
  everything compiles and Reflect silently runs as True Mirror (§9, FAIL-C).

### 5.2 Predicates and every gate site

- **`LLGhostAvatar::isPoseStampDriven()`**: `TRUE_MIRROR || REFLECT`. Keep `isTrueMirrorDriven()` as it
  is.
- **`ALGhostStudio::stampOwnsFacing(inst)`**: entity clone `&& (TRUE_MIRROR || REFLECT) &&
  GhostTrueMirrorCopyFacing`. Every `trueMirrorOwnsFacing` call site moves to this.
- **`ALGhostStudio::reflectOwnsPosition(inst)`**: §4.7.

A missed site is a second writer on the clone and fails silently. **The reviewer must re-grep**
`DRIVE_TRUE_MIRROR`, `isTrueMirrorDriven`, `trueMirrorOwnsFacing`, `mEntityDriveMode`, `mDriveMode`,
`setEntityDriveMode` and `applyEntityTransform`.

| Site | Under REFLECT |
|---|---|
| `llghostavatar.cpp:1470` clamp | widen |
| `llghostavatar.cpp:1489-1494` pause release on leaving | release when leaving TRUE_MIRROR **or** REFLECT |
| `llghostavatar.cpp:1537-1559` entry suppression | same as TRUE_MIRROR, plus reset the partner table and verify state |
| `llghostavatar.cpp:633` AutoAnimate learning presence | **exclude** REFLECT |
| `llghostavatar.cpp:892-895` `autoAnimOnDriveModeChanged` | treat REFLECT as dormant (like DIRECTED/FROZEN) |
| `llghostavatar.cpp:942-943` ghost-side dormant text | add Reflect |
| `llghostavatar.cpp:3448-3451` animesh ledger + `autoAnimLinksetStep` | keep the plain ledger mirror; **skip `autoAnimLinksetStep`** under REFLECT |
| `llghostavatar.cpp:3191-3193` repeat-held animesh | same as TRUE_MIRROR |
| `llactormover.cpp:4406`, `:5793` | `isPoseStampDriven()` |
| `alghoststudio.cpp:523-528` AutoAnimate Go | exclude REFLECT |
| `alghoststudio.cpp:1679, 1924, 2002, 2076` facing writers | `stampOwnsFacing(inst)` |
| `alghoststudio.cpp:2044-2052` post-pass | include REFLECT. For each instance: **gate on the ghost's own mode** (switcher re-drives, S2), then plane → foot push → stamp. Hold everything when the source is missing. |
| `alghoststudio.cpp:414-423`, `:1345-1352` freeze/resume | round-trip REFLECT; freeze keeps the plane |
| `alghoststudio.cpp:916`, `:965` copies | reset Mirror position and plane validity |
| `alghoststudio.cpp:2203-2289` formation motion, `:2310-2319` chaos | covered by the runtime predicate (§4.7). Confirm no write while it holds. |
| `alghoststudio.cpp:1956, 1972, 2149, 2287, 2331, 2780, 2798, 2893, 3602, 3641, 3693, 4556, 4637` `applyEntityTransform` callers | harmless **before** the post-pass (the stamp's root write comes later). Confirm none runs **after** the post-pass in the same frame. |
| `alghostmanipproxy.cpp:488`, `:754` | skip `applyEntityTransform` while `reflectOwnsPosition` (it runs after the post-pass) |
| `alpanelghoststudio.cpp:2208-2209`, `:2238-2239` | exclude REFLECT from AutoAnim Go; add `" (dormant: Reflect)"` |
| `alpanelghoststudio.cpp:1427, 1525` combo sync | value 4 round-trips |
| `alchatcommand.cpp:820-840` | add `reflect`; update **both** usage strings (`:785`, `:839`) |

### 5.3 The stamp

Generalise `applyTrueMirrorStamp` to dispatch on the ghost's own mode.

- **TRUE_MIRROR must stay byte-identical.** Its branch is the existing code path, moved, not rewritten.
  The reviewer proves this by diff.
- **REFLECT** follows the same steps, with three differences:
  1. Root rotation `q_y(π) * q_src_world * q_n(π)`, written with `setWorldRotation` (§4.6).
  2. **Bones only**, via the partner table, with the §4.2 rule.
  3. Face via §4.5.
- Then the same trailing `mRoot->updateWorldMatrixChildren()`.
- Non-finite guards on every quaternion, vector and plane value. A bad input skips that write; it never
  propagates.

### 5.4 Plane state (session-only, like the rest of `Instance`)

Add to `ALGhostStudio::Instance`: `mReflectPlaneMode` (Fixed / Live), `mReflectPlaneValid`,
`mReflectPlanePoint` (`LLVector3d`), `mReflectPlaneNormal` (horizontal unit `LLVector3`), and
`mReflectMirrorPosition` (default off).

The ghost reads the plane from the Studio when stamping. Pass it in; don't cache a second copy.

### 5.5 Face pair helper

A button, applied to the **selected instance only**. It's disabled for locked groups, with a status
explanation. Note that `setInstanceDriveMode` doesn't fan out to groups.

1. Clone foot = `source_world_foot + source_horizontal_world_forward × d`, where
   `d = GhostReflectFacePairDistance`. Use the Studio placement path (a revision bump is fine here,
   because this is a one-shot user edit).
2. Set `inst.mRotation` to face the source. That way the result is right even with
   `GhostTrueMirrorCopyFacing` off.
3. Capture a Fixed plane at the midpoint, with `n` = the source's horizontal world forward.
4. Set the drive to REFLECT.

### 5.6 UI (`panel_ghost_studio.xml`, both hosts)

- Combo: add `<combo_box.item label="Reflect" name="drive_reflect" value="4" />` and update the tooltip.
- A Reflect row, enabled when the selected clone is in Reflect:
  - `Plane:` combo (Fixed / Live, with the Live whirl note in its tooltip)
  - `Mirror position` check
  - `Face pair` button, with a distance spinner bound to `GhostReflectFacePairDistance` and a ↺ reset
    button
  - `Recapture plane` button
  - a status line, e.g. `Reflect: 50 bone pairs, plane fixed, mirroring` /
    `holding: source missing` / `position held: path motion owns placement` /
    `paused: Director switcher re-drive`
- **Reflow:** the row goes inside `pose_scroll_body` (height 790). Shift everything below and raise the
  body height. Prove there's no overlap or clipping **by rect arithmetic in both hosts** (the M1 lesson
  from the light rig).
- Multi-select drive changes follow `onDriveModeCommit`'s group fan-out.

### 5.7 Settings

- Reuse `GhostTrueMirrorCopyFacing` and `GhostTrueMirrorCopyFace`, and update their `Comment` text to
  say they also govern Reflect.
- New: `GhostReflectFacePairDistance` (F32, 2.0, range 0.5–10, step 0.1).
- New: `GhostReflectVerify` (Boolean, default true). It gates the periodic verdicts; the entry and build
  verdicts always log.

### 5.8 Chat

`/ghostanim reflect [all|selected]`. Add it to both usage strings.

---

## 6. Part B design — Reflect for self (inventory playback)

### 6.1 Behaviour

- **New setting:** `AlchemyAnimReflectInventory` (Boolean, default false, persisted;
  `settings_alchemy.xml`).
- **UI:** a **Reflect** checkbox next to `priority_combo` in `floater_preview_animation.xml`, bound to
  the setting, so it's global across previews. Tooltip: *"Plays the animation as its mirror image
  (left/right swapped). Your viewer only: other people see the original."*
- **Scope:** both "Play in world" and "Play locally", from the preview floater and from the inventory
  context menu (they share `LLPreviewAnim::play`). Gestures, AO and the Switchboard are not included
  (D6).

### 6.2 Module: `alreflectanim.{h,cpp}` (`LLSingleton`, mirroring the `LLLocalAnimMgr` precedent)

**Derived id:** deterministic per original, e.g. `LLUUID::combine` with a fixed namespace id. It is
**never** sent to the region; guard the preview's send path with `!isDerived(id)`.

**Registry:** original → { derived, state `PENDING`/`READY`/`FAILED`, mirrored bytes }, plus
`isDerived(id)` and `originalFor(derived)`.

**Reflect set:** the originals currently started reflected on self. **Membership, not the live toggle,
decides how start and stop are mapped.** So unticking the checkbox mid-play doesn't pop, and the stop
still maps correctly. Remove an original from the set once its stop has been processed.

**Teardown:** `removeKeyframeData` for every derived id in the destructor (precedent:
`lllocalanim.cpp:72`).

### 6.3 Build pipeline (once per original per session)

1. **Original bytes.**
   - If cached, read `LLFileSystem(original, AT_ANIMATION)` exactly as `onLoadComplete` does.
   - Otherwise call `gAssetStorage->getAssetData(original, AT_ANIMATION, …)` (as at
     `llkeyframemotion.cpp:560`) and read in the callback.
   - **Never start the original on self to obtain it.**
2. **Scratch decode:** on `gAgentAvatarp`, which provides collision-volume names,
   `createMotion(probe)` + `deserialize(bytes, probe, false)`. Dispose of the probe exactly as
   `lllocalanim.cpp:118-135` does.
3. **Mirror the scratch `JointMotionList` in place:**
   - every `JointMotion`: `mJointName` → partner (§4.4); every `mRotationCurve.mKeys` value → §4.2
     rotation rule; every `mPositionCurve.mKeys` value → y-flip; `mScaleCurve` untouched. Also transform
     each curve's `mLoopInKey`/`mLoopOutKey`; it's harmless if `serialize` doesn't write them.
   - after renaming, **no two JointMotions may share a name**. A duplicate means FAIL; abort.
   - every constraint: source volume index → name (`findCollisionVolume(i)->getName()`) → partner →
     `getCollisionVolumeID`. The same for the target, unless its type is GROUND. y-flip
     `mSourceConstraintOffset`, `mTargetConstraintOffset` and `mTargetConstraintDir`. The chain indices
     are rebuilt by step 5's `deserialize`.
   - `mHandPose`: swap `RELAXED_L/_R`, `POINT_L/_R`, `FIST_L/_R`. Any other pose is kept, with a log
     line for the one-sided `_R` poses.
   - `mPelvisBBox`: y-reflect (`min.y, max.y` → `−max.y, −min.y`).
   - emote, priority, ease, loop points and duration: unchanged.
4. **Re-encode:** `serialize` the scratch list to get the mirrored bytes, then
   `removeKeyframeData(probe)`.
5. **Register:** `createMotion(derived)` + `deserialize(mirrored, derived, false)`, **exactly once** per
   derived id (the leak rule). The state becomes READY.
6. **On FAIL:** log the reasons, play the **original** normally, and show a one-time notice: "Couldn't
   reflect <name>; playing it normally."

### 6.4 Substitution — the only hooks

- **Play locally** (`llpreviewanim.cpp:129`), toggle ON: add the original to the set, then start the
  derived motion when READY. Until then nothing plays. **Never flash the original.** Toggle OFF: the
  stock path, byte-identical.
- **Play in world** (`:125`): the request still sends the **original**. Add the original to the set.
- **Echo hook:** one guarded branch in `LLVOAvatar::processSingleAnimationStateChange`, at its
  `startMotion`/`stopMotion` calls:
  - `if (isSelf() && reflectSetNonEmpty && set.contains(anim_id))`: start or stop the **derived** motion
    instead. If the derived copy isn't READY, mark it pending and start it on READY if still signalled.
  - Everything above those calls (`computeBodySize`, typing sound, `sitDown`, snapshot) is unchanged.
    It keys on built-in ids, which are never in the set.
  - **Off-path:** not self, or the set empty → byte-identical. **The reviewer must prove this.** It's the
    only shared hot-path edit; it runs for every avatar.
- **Stops:** the preview toggle-off (`:144-147`) and `onClose` (`:211-219`) map `stopMotion(original)`
  to the derived motion. The region's STOP echo maps too, idempotently.
- **Priority override:** `draw()` re-applies it via `mItemID` (`:161-179`). Store the id that was
  actually started (the derived one when reflected) so `findMotion`/`isMotionActive` resolve.
- Only ids added by `LLPreviewAnim` are ever substituted. Never built-in `ANIM_AGENT_*` ids, never
  non-keyframe motions.

### 6.5 Effects to handle or state

- **Local-only:** other viewers see the original.
- **The Director anim inspector** shows which animations are playing. Map derived → original there with
  `originalFor`, so the original's row shows as playing. Other UIs that query by the original id are
  documented as a limitation (§7, L10).
- **Clones of you:**
  - **Mirror** drive copies signalled ids, so it shows the original.
  - **True Mirror** copies your final pose, so it shows your mirrored version.
  - **Reflect** mirrors it again, back to the original.

---

## 7. Known limitations (stated, not hidden)

- **L1 — The geometry isn't mirrored.** A hair parting, a single strap, a tattoo or text stays on its
  side.
- **L2 — Legs are within 3 mm, not exact.** The rest skeleton is 2–3 mm asymmetric at the hips, knees and
  ankles (§4.3). Asymmetric **mesh** joint-position overrides also get swapped (the left value lands on
  the right joint), which can distort an asymmetric mesh slightly.
- **L3 — One-sided expressions don't flip.** `Express_*` morphs have no mirrored twin. The same goes for
  Part B's one-sided hand poses (`SALUTE_R`, `PEACE_R`, `PALM_R`).
- **L4 — Held props stay on the same hand** (D3).
- **L5 — Animesh worn by the clone plays unmirrored**, via the ledger, exactly as under True Mirror.
- **L6 — Mutual gaze only works while the clone sits at the mirror position** (Live plane, or Mirror
  position on). With a Fixed plane and a moving source, it fails. Gaze at the camera never survives:
  the clone looks at the camera's mirror image.
- **L7 — Uneven ground:** mirrored foot plants can float or sink, because the clone's foot IK is paused.
- **L8 — Part A cost** is the same as True Mirror plus a table lookup. It isn't meant for crowds.
- **L9 — Part B mirrors animation files only.** Gaze, IK, Pose Polish and head turns on your own avatar
  stay unmirrored.
- **L10 — Part B: UIs that query "is X playing on me" by the original id** see the derived id instead,
  except the Director inspector (§6.5).

---

## 8. Instrumentation — the log states a verdict

Tag `[GhostReflect]`. Three outcomes are always distinct: **PASS**, **FAIL**, and **INCONCLUSIVE** (the
measurement itself can't be trusted).

**Part A**

1. **Entry verdict** (always logged):
   `entered Reflect: bone pairs 50/50, centre 33, unresolved 0 -> PASS`. Any unresolved partner gives
   FAIL, with the names listed.
2. **Foreign-writer check** (every 5 s): at the start of a stamp, compare the clone's **bone** rotations
   with the previous stamp's writes.
   - **Exclude `mRoot`.** The clone's own `updateCharacter` rewrites it every frame
     (`llvoavatar.cpp:4888`, `:4948`, `:5088`), so including it would guarantee a false FAIL.
   - Any difference > 1e-4 gives `FAIL: <bone> changed between stamps (second writer)`.
   - A skipped previous stamp gives INCONCLUSIVE.
3. **Bone symmetry check** (every 5 s, using the engine's world matrices rather than a re-read): for
   {`mWristLeft/Right`, `mAnkleLeft/Right`, `mHead`, `mPelvis`}, compute each joint's world position in
   its own root frame. Expect `clone(P(J)) == S·source(J)` within 1 cm × uniform scale.
   - The implementer must verify the joint world frame on a **scaled** clone. The light rig found joint
     world positions exclude the outer render scale (`doc/CINE_LIGHT_RIG_STATUS.md` on
     `feature/cine-light-rig`; re-locate the code on this base).
   - A shape mismatch or an unbuilt source gives INCONCLUSIVE.
4. **World facing check** (every 5 s; this is the only check that sees the root, so it catches a wrong
   root formula and the seated-source error): expect clone world forward `== M·(source world
   forward)` and clone world up `== M·(source world up)`, both within 1°. Skip it with INCONCLUSIVE
   when copy-facing is off.

**Part B**

5. **Build verdict:** `anim <orig> -> <derived>: joints N (P paired, C centre, U unresolved),
   constraints K, hand pose A->B, built in Z ms -> PASS`, or FAIL with reasons.
6. **Round-trip check** (when `GhostReflectVerify` is on): mirroring the mirrored scratch must reproduce
   the original decoded keys. Tolerance: max quaternion component ≤ 1e-4 and position ≤ 1e-4 m (U16
   quantisation). PASS or FAIL. It catches a non-involutive transform or a lost key. It does **not**
   prove the reflection is the right one; that's T2/T3.
7. **Substitution trace:** `self start <orig> as <derived> (local|echo|pending)` and the matching stop.
   A stop for a set member whose derived motion isn't active gives a WARN.

---

## 9. TUT tests (`indra/newview/tests/alreflectmath_test.cpp`)

Register it like `algazemath_test`. Tests run only with `-DBUILD_TESTING=ON`. Use literals, not the
header's own symbols, for expected values. Each test must fail if the thing it tests is broken.

- **T1 Pairing:** literal lists of all 133 bones (50 pairs + 33 self) and of the full 159 (59 + 41).
  Check involution, partner existence, and spot literals (`mWristLeft`↔`mWristRight`,
  `L_CLAVICLE`↔`R_CLAVICLE`, `LEFT_PEC`↔`RIGHT_PEC`, `mFaceEyeAltLeft`↔`mFaceEyeAltRight`,
  `mSpine1`↔`mSpine1`).
- **T2 The §4.1 discriminator:** the reflected rotation carries (0,1,0) to (1,0,0); the unreflected one
  gives (−1,0,0).
- **T3 Behavioural local reflection (the real discriminator):** ≥ 20 deterministic random full-3D
  quaternions and vectors satisfy `S·(q·v) == q'·(S·v)` within 1e-5.
- **T4 Involution and homomorphism:** it passes for any diagonal sign-flip rule, so it's a regression
  guard only. T3 is what pins the rule.
- **T5 Root and plane, full-3D:** random pitched and rolled source rotations × plane azimuths (0°, 37°,
  90°, 179°, −135°). Behavioural check: `world_clone(S·u) == M·world_source(u)` for test vectors `u`.
  It must distinguish `M·R·S` from `S·R·M`, from `M·R`, and from a yaw-only rule. Check the LL-order
  formula `q_y(π) * q_src * q_n(π)` against the behaviour. (No det test: det is +1 for any quaternion.)
- **T6 Facing check:** a source facing along `n` gives a clone facing −`n`.
- **T7 Plane:** the bisector; each fallback tier (coincident, lying-down source, no last plane); the
  Live hysteresis band (0.1 → 0.3 m) holds the plane; NaN input never produces NaN.
- **T8 Face params:** 58↔59 swap; 1206/1207 negate (literals −0.7 and 1.3); other ids pass through.
- **T9 Hand poses:** the three L/R pairs swap and are involutive; one-sided `_R` poses and the symmetric
  ones pass through.
- **T10 Pelvis bbox and constraint vectors:** y-reflect with min/max kept ordered.

---

## 10. In-world tests — outcomes stated in advance

**Part A.** Make an entity clone of yourself, click **Face pair** (2 m), and play an animation that
raises your **right** arm forward.

- **PASS:** the clone raises its **left** arm toward you. The log shows the entry, symmetry and facing
  PASS lines.
- **FAIL-A** (names swapped, rotation not reflected): the clone's left arm swings **backward**.
- **FAIL-B** (rotation reflected, names not swapped): the clone's right arm moves, and wrongly.
- **FAIL-C** (clamp missed): the clone raises its **right** arm while the combo reads Reflect.
- **Seated check:** sit on a rotated poseball. The clone must still face you mirror-correctly, and the
  facing verdict must be PASS. If the clone faces off by the seat's angle, the source root is being read
  in local space.
- Then: turn 45° left and the clone turns 45° right. With Mirror position on, step toward the clone and
  it steps toward you. In Ghost Edit mode, the clone must not lose its facing each frame. Switch to True
  Mirror: it behaves exactly as before.

**Part B.** Tick **Reflect** in the animation preview.

- **Play locally** a right-arm animation: your **left** arm moves. No flash of the right arm first.
- **Play in world:** you see the left arm. A second viewer sees the right arm (the original).
- Untick mid-play: it keeps reflecting until stopped. The next play is normal.
- Stop, or close the preview: the animation stops. **No stuck animation.** The log shows the build
  PASS and round-trip PASS.

---

## 11. Off-limits for the implementer

- `pipeline.cpp`, `pipeline.h`, any shader, any render target or GL state.
- `fsposeranimator.*` and `fsjointpose.*`: read-only reference.
- `indra/newview/character/*.xml` and `indra/llcharacter/*`: inputs, not edit targets. Part B uses
  `LLKeyframeMotion`'s existing public API only.
- `llvoavatar.cpp`: **only** the one guarded branch in `processSingleAnimationStateChange` (§6.4).
- The TRUE_MIRROR code path's behaviour (§5.3).
- Never build. Claude builds after review convergence.

---

## 12. Open decisions (defaults chosen; change before handing to Codex)

- **D1 — Mirror position default:** OFF.
- **D2 — Plane mode default:** Fixed.
- **D3 — Held props:** deferred (L4).
- **D4 — Settings reuse:** reuse the two `GhostTrueMirror*` keys.
- **D5 — True Mirror's seated-source root** (pre-existing, §4.6): leave it in this delivery (True Mirror
  byte-identical). Recommend a separate one-line fix: read `getWorldRotation()` and write it with
  `setWorldRotation`.
- **D6 — Part B scope:** preview and inventory only. Alternative: also the Animation Switchboard and
  gestures.

---

## Appendix — review round 1 disposition (all applied in rev 2)

| # | Finding | Disposition |
|---|---|---|
| M1 | Seated source: the root is seat-relative | World frame everywhere (§4.6); True Mirror's same defect → D5 |
| M2 | `getSkeleton()` holds bones only, so 50/33, not 59/41; stamping volumes would accumulate | Bones only; counts fixed (§4.4, §5.3, §8) |
| M3 | Post-pass `applyEntityTransform` overwrites facing; revision bump re-triggers the proxy after the stamp; missing sites | Order plane → foot → stamp; foot-only push with no bump; proxy gated; sites added (§4.7, §5.2) |
| M4 | Placement ownership holes (late motion, copies, chaos, groups) | Runtime predicate; copy resets (§4.7, §5.2) |
| M5 | T5 couldn't pin `M·R·S`; LL order unstated; det test tautological | Full-3D T5; LL-order formula stated; det test dropped (§4.6, §9) |
| S1 | AutoAnimate animesh autonomy live under REFLECT; 2 unclassified sites | Classified dormant; skip `autoAnimLinksetStep` (§5.2) |
| S2 | Switcher re-drive bypasses the Studio | Post-pass gates on the ghost's own mode (§5.2) |
| S3 | Two different "sources" | Plane and position use the ghost's `mAnimationSourceId` (§4.6) |
| S4 | Live-plane whirl, 180° snap, degenerate fallback | Hysteresis, tiered fallback, whirl stated (§4.6) |
| S5 | Rest positions aren't symmetric | §4.3 and L2 corrected |
| S6 | Plane invalidation undefined | Lazy capture plus invalidation rules (§4.6) |
| S7 | Symmetry check blind to the root | World facing verdict added (§8, item 4) |
| S8 | Foreign-writer check must exclude `mRoot` | Excluded (§8, item 2) |
| Nits | Citations, L6 nuance, Face pair with facing off, T4 weakness | Applied |

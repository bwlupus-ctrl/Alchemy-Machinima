# Reflect — mirror-image clones (Part A) and mirrored self playback (Part B) — design brief

**Date:** 2026-10-20 (rev 3, after adversarial review rounds 1 and 2)
**Status:** DESIGN ONLY. Nothing implemented, nothing built.
**Process (per `CLAUDE.md`):** Codex implements from this brief (`--write`, `--prompt-file`), an
independent Opus sub-agent reviews the code ADVERSARIALLY, loop to 0 must-fix, THEN Claude builds once,
then the user tests in-world. Codex never builds.

> **BASE BRANCH — READ FIRST.** Every `file:line` below is from `origin/fix/animesh-clone-pose-polish`
> at `057e558`, the newest pushed branch that contains Ghost Studio's **True Mirror** drive. True Mirror
> is **not on `develop`**. If the user has unpushed local work, diff it against this brief and
> re-anchor the line references before implementing.

**Review history of this brief** (independent Opus reviewers; Claude re-verified each load-bearing
claim against the code before applying it):

| Round | Must-fix | Should-fix | Nits | Status |
|---|---|---|---|---|
| 1 (Part A) | 5 | 8 | 4 | all applied in rev 2 |
| 2 (rev-2 fixes + new Part B) | 3 (all Part B) | 5 | 10 | all applied in rev 3 |

Round 2 verified Part A's maths as correct, including the LL-order root formula. Rev 3 has not been
re-reviewed. Before handing it to Codex, one more round should confirm that rev 3's Part B state
machine (§6.4) closes round 2's findings. The disposition log is in the Appendix.

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
viewers only ever see the original animation.

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
10. A start/stop state machine across three hooks (preview, region echo, natural end) that never puts
    the derived ID on the wire and never leaves an animation stuck.

**Explicitly NOT in this delivery** (each needs its own brief):

| Deferred | Why |
|---|---|
| Looking-glass (geometry) mirror, where asymmetric hair, straps and text are reversed | Needs a negative-determinant render transform and a front-face winding flip. That is shared GL state (`CLAUDE.md` rule 1), a different risk class. |
| Swapping held props to the other hand | Needs re-attaching the clone's cloned attachments to the paired attach point. It touches the clone-attachment pipeline. Open decision D3. |
| The clone's **own** gaze on top of the stamp ("both look at camera") | Gaze runs inside `updateCharacter`, before the post-object-list stamp. Running it after needs a reordering with its own review. |
| Part B for gestures, AO and the Animation Switchboard | Different playback paths. Open decision D6. |
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
| The Director switcher returns ghosts **only to MIRROR** (never to TRUE_MIRROR or REFLECT) and leaves `inst.mDriveMode` unchanged. | `aldirectoranimswitcher.cpp:313, 351, 373, 405, 559` |
| Stamp: once per frame; holds the last pose if the source is missing; resolves the source via the ghost's `mAnimationSourceId` (not `inst.mSource`, which is a cast id where null means "me"). | `llghostavatar.cpp:1568-1624` |
| Bone stamp: root rotation copied with `getRootJoint()->getRotation()` (parent-local), root position never. Then, per `getSkeleton()` joint, rotation and position copied. Scale is not copied. | `llghostavatar.cpp:1626-1674` |
| **`getSkeleton()` holds only the 133 bones.** Collision volumes live separately in `mCollisionVolumes`. | `llavatarappearance.cpp:698-707`, `:1530` |
| Collision-volume morphs (including physics params 1206/1207) move volumes **incrementally**. Stamping volumes would make the error build up frame after frame. | `llpolymorph.cpp:648-650`, `:741-743` |
| **A seated avatar's root is parented to the seat** at the `LLXformMatrix` level. `getRotation()`/`getPosition()` are seat-relative; `getWorldRotation()` includes the seat through the xform parent. | `llvoavatar.cpp:9246`, `:5087-5088`; `xform.cpp:64-82` |
| `LLJoint::setWorldRotation` checks only `LLJoint::mParent`, not the xform parent. That's harmless for clones, which never sit. | `lljoint.cpp:833` |
| Face stamp copies `Express_*`, `Blink_*`, `Lipsync_*` and params 1200..1207, by id. | `llghostavatar.cpp:1676-1759` |
| **`setEntityDriveMode` clamps `mode` to `[DRIVE_MIRROR, DRIVE_TRUE_MIRROR]`.** A new value 4 would silently become True Mirror. It is the only drive-mode clamp. | `llghostavatar.cpp:1468-1471` |
| Entering True Mirror pauses the clone's controller and stops its eye motion. Leaving it releases the pause. | `llghostavatar.cpp:1489-1494`, `:1537-1559` |
| No ghost code restores bone positions after a stamp mode ends. | grep; restore candidate `LLVOAvatar::rebuildAttachmentOverrides` `llvoavatar.cpp:7648` |
| Actor Mover parks a walk and skips gaze for `isTrueMirrorDriven()` clones. | `llactormover.cpp:4399-4410`, `:5788-5797` |
| Studio facing writers skip clones where `trueMirrorOwnsFacing(inst)` is true. | `alghoststudio.cpp:1679, 1924, 2002, 2076`; defined `:2016-2022` |
| **`applyEntityTransform` writes position AND rotation.** `setGhostRotation` calls `mRoot->setWorldRotation(inst.mRotation)`. | `alghoststudio.cpp:1425-1438`, `llghostavatar.cpp:1158-1174` |
| `Instance::setFootGlobal` bumps `mTransformRevision` and clears `mChaosHasBase`. | `alghoststudio.h:476-482` |
| **The manip proxy ticks AFTER the post-pass.** It calls `applyEntityTransform` on every drag frame (`:488`), on any revision change (`:748-756`), and at `endDrag` via `transformUnit`/`transformGroup` (`:577`, `:583` → `alghoststudio.cpp:2887-2893`). It only re-pushes the proxy box on a revision change. | `llappviewer.cpp:5417` vs `:5540`; `alghostmanipproxy.cpp:481-488, 577, 583, 748-756` |
| Formation motion writes `mFootGlobal` every frame. Chaos recomputes from `mChaosBaseFoot`; "chaos on" is `mChaosEnabled`. | `alghoststudio.cpp:2203-2289`; `:2310-2319` |
| Copies: `duplicateInstance` rejects entity clones. The entity copy funnel is `*copy = proto` at `:965`, reached from `:1028, :3571, :3681, :4576`. | `alghoststudio.cpp:912-916`, `:965` |
| Other placement writers. | `alghoststudio.cpp:1956, 1972, 2149, 2287, 2331, 2780, 2798, 2893, 3602, 3641, 3693, 4556, 4637` |
| AutoAnimate sites that test `MIRROR \|\| TRUE_MIRROR` or show dormant text. | `alghoststudio.cpp:523-528`; `llghostavatar.cpp:633`, `:892-895`, `:942-943`, `:3448-3451` (`autoAnimLinksetStep`); `alpanelghoststudio.cpp:2208-2209`, `:2238-2239` |
| Animesh on the clone keeps the ObjectAnimation ledger mirror under True Mirror. The repeat-held-animesh path is gated the same way. | `llghostavatar.cpp:3446-3451`, `:3191-3193` |
| Ghost Studio instances are **session-only**. Nothing persists `mDriveMode`. | `alghoststudio.h:351-366`, `alghoststudio.cpp:15`, `:554` |
| `setInstanceDriveMode` does **not** fan out to groups. The panel's `onDriveModeCommit` does. | `alghoststudio.cpp:2353-2374` |
| Drive combo values 0..3. The panel is hosted in two floaters, and `pose_section` (790) is nested in `pose_scroll_body` (790). | `panel_ghost_studio.xml:218-224`; `floater_director.xml:567`, `floater_ghost_studio.xml:18` |
| Chat maps `mirror`, `truemirror` and `freeze`. The usage string appears twice. | `alchatcommand.cpp:820-840`; `:785`, `:839` |
| True Mirror settings: `GhostTrueMirrorCopyFacing`, `GhostTrueMirrorCopyFace` (default true). | `settings.xml:7230-7253` |
| LL `operator*`: `a*b` is the Hamilton product `b⊗a` (apply a, then b); world = local·parent. | `llquaternion.cpp:541-551`, `xform.cpp:75` |

### 3.2 Part B

| Fact | Where |
|---|---|
| Inventory right-click "Play Inworld" and "Play Locally" both open the preview and call `LLPreviewAnim::play`. | `llinventorybridge.cpp:7270-7285` |
| `play()`: Inworld only sends `START(assetID)` (no local start); Locally does `startMotion(assetID)`. `mItemID` is set **only if** `findMotion` succeeds. | `llpreviewanim.cpp:78-141` |
| Precedent for a per-playback client-side option: the priority override, which `draw()` re-applies via `mItemID`. | `llpreviewanim.cpp:116-141`, `:161-179`; `floater_preview_animation.xml:88` |
| Stop paths: the toggle-off branch and `onClose` call `stopMotion(assetID)` and send STOP. | `llpreviewanim.cpp:144-147`, `:211-219` |
| **When a non-looping motion ends on its own, the motion controller calls `requestStopMotion`.** On self that reaches `LLAgent::requestStopMotion`, which calls `onAnimStop` and sends STOP for **`motion->getID()`**. | `llmotioncontroller.cpp:544, 663, 749, 800`; `llvoavatarself.cpp:839-844`; `llagent.cpp:2760-2769` |
| Region echo: `processAnimationStateChanges` stops ids that are in `mPlayingAnimations` but not signalled, and starts signalled ids. **`mPlayingAnimations[id]` is set only if `processSingleAnimationStateChange(id, true)` returns true.** | `llvoavatar.cpp:7085-7126`, `:7158+` |
| `LLKeyframeMotion::onInitialize` checks the in-memory keyframe cache first. A **second** `onInitialize` on an id with no cached data fetches that id from the asset service. | `llkeyframemotion.cpp:585`, `:551-567`, `:626-633` |
| `createMotion` followed by a synchronous `deserialize` triggers no fetch. A created, never-started motion has `mStopped = true` and isn't auto-activated. | reviewer-verified: `llkeyframemotion.cpp` first `onInitialize` marks NEEDS_FETCH only; `llmotion.cpp:46` |
| `deserialize(dp, id, allow_invalid_joints)` caches under `id`. The code's own comment says to use `true` for existing content, and stock playback passes `true`. | `llkeyframemotion.cpp:1885-1888`, `:2694`; `.h:170`, `:3148` |
| `serialize()` writes exactly what `deserialize()` reads, in order. It needs `mCharacter` and `mJointMotionList`. It dereferences `findCollisionVolume(i)` **without a null check**. | `llkeyframemotion.cpp:2705+`, `:2799`, `:2809` |
| **Not serialized:** `mPelvisBBox` (recomputed from the `mPelvis` position keys) and curve loop keys (recomputed in `setupPose`). | `llkeyframemotion.cpp:2436-2439`, `:704-705` |
| Positions are quantised to U16 over ±5 m (one quantum = 10/65535 ≈ 1.53e-4 m, `floor`). | `llquantize.h:60-69` |
| Constraint volume names ↔ indices share the `mCollisionVolumes` space. Source and BODY-target offsets are **volume-local**. A GROUND-target `mTargetConstraintOffset` is **agent-frame**, added unrotated. `mTargetConstraintDir` is never read at runtime. `JointConstraintSharedData` is a protected nested type. | `llavatarappearance.cpp:1294-1316`; `llavatarjoint.cpp:241-244`; `llkeyframemotion.cpp:1541`; `.h:257` |
| Local-anim precedent: `createMotion(id)` + `deserialize(bytes, id, …)` + `startMotion(id)`. Its validation **removes the probe motion immediately** after deserialize (`removeMotion` deletes the instance). | `lllocalanim.cpp:477-507`, `:118-135`; `llmotioncontroller.cpp:315-337` |
| **Load each id exactly once.** `addKeyframeData` overwrites the cache slot without freeing it. | `lllocalanim.cpp:363-368`, `llkeyframemotion.cpp:3211-3214` |
| Animation Explorer keys "playing" on `mAnimationSources`, which holds original ids. | `animationexplorer.cpp:358-369` |
| Hand poses: `RELAXED_L/_R`, `POINT_L/_R`, `FIST_L/_R` pair up. `SALUTE_R`, `PEACE_R`, `PALM_R` have no left twin. | `llhandmotion.h:47-60` |
| `LLUUID::combine` is a deterministic MD5 of two ids. | `lluuid.h:108-109` |

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

- **Rotation:** `(x, y, z, w)` becomes `(−x, y, −z, w)`, written to `P(J)`. This is conjugation by `S`.
- **Position:** `(x, y, z)` becomes `(x, −y, z)`, written to `P(J)`.
- **Scale:** not written.

**Why the local rule gives a world mirror image** (by induction):

- Because the stamp fully overwrites every bone's local rotation and position, the result is
  `W'(P(J)) = S·W(J)·S` down the chain. This needs a symmetric hierarchy and symmetric joint scales.
- It doesn't depend on joint-frame alignment, so eyes, `mFaceEyeAlt*`, hind limbs, wings and tail are
  all fine.
- It doesn't depend on LL's row-vector convention either, because `S` is diagonal and its own inverse.

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

The clone's legs end up within **3 mm** of their bind (§7, L2).

**Collision volumes are NOT stamped.** They ride their mirrored parent bones.

### 4.4 Pairing rule — by NAME, never by index

Rule, applied in order, first match wins:

1. If the name contains `Left`, replace it with `Right` (and vice versa).
2. Otherwise, if it starts with `L_`, swap to `R_` (and vice versa).
3. Otherwise, if it starts with `LEFT_`, swap to `RIGHT_` (and vice versa).
4. Otherwise the joint is a centre joint and is its own partner.

| Population | Pairs | Centre | Misses |
|---|---|---|---|
| **Bones (what Part A stamps)** — 133 | **50** | **33** | 0 |
| Full skeleton (bones + 26 collision volumes) — 159 | 59 | 41 | 0 |

The rule also reproduces all 116 paired entries of the FS poser table. The poser table lacks
`LOWER_BACK`, `mFaceEyeAltLeft/Right` and `mFaceJawShaper`, so use the rule. Part B needs the volume
pairs, for constraints.

Build Part A's `src bone index → dst bone index` table **once per clone build**, by name via
`getJoint()`. Don't reuse True Mirror's index-parallel shortcut. Skip and log an unresolved partner.

### 4.5 Face and physics params

| Param | id | Range | Under Reflect |
|---|---|---|---|
| `Blink_Left` ↔ `Blink_Right` | 58 ↔ 59 | 0..1 | **swap** |
| `Butt_Physics_LeftRight_Driven` | 1206 | −1..1, default 0 | **negate** |
| `Breast_Physics_LeftRight_Driven` | 1207 | −2..2, default 0 | **negate** |
| every other param True Mirror copies | — | — | copy unchanged |

1206 and 1207 are `volume_morph` shifts along +Y, linear in the weight (`avatar_lad.xml:7586-7604`,
`:8140-8153`). They are the only lateral physics params. The other lateral params in the copied set are
the one-sided `Express_*` morphs (§7, L3).

### 4.6 Mirror plane and root rotation — WORLD frame

A per-instance vertical plane: a point `P0` and a horizontal unit normal `n`. `M = I − 2nnᵀ`.

**Read the source root in WORLD space:** `R_src = getRootJoint()->getWorldRotation()`, and positions via
world / global accessors. For a seated source, this includes the seat through the xform parent
(§3.1). Use world values everywhere: the root formula, Face pair, the bisector, Mirror position and the
verdicts.

- **Inference:** on a moving seat, the value is one frame stale. That's the same value the source's
  own skeleton used, so the two stay consistent.
- **Pre-existing:** True Mirror copies the parent-local `getRotation()`, and its comment says the root has
  "no parent" (`llghostavatar.cpp:1627-1632`). That's wrong for a seated source. It's out of scope (True
  Mirror stays byte-identical); see D5.

**Clone root:** written with `mRoot->setWorldRotation(...)`. `setWorldRotation` only accounts for an
`LLJoint` parent (`lljoint.cpp:833`), so this is correct **because clones never sit**. If a future
change ever parents a clone's root xform, this must be revisited.

**Formula:** `R_clone = M · R_src · S` (column-vector notation). With `M = −R_n(π)` and `S = −R_y(π)` the
minus signs cancel. In **LL `operator*` order** (apply left first):

    q_clone = q_y(π) * q_src * q_n(π)

where `q_y(π)` is π about the local Y axis and `q_n(π)` is π about the world axis `n`. Round 2 verified
this against `operator*` and `xform.cpp:75`.

**Tests must be behavioural and full-3D** (T5). For an upright source, `M·R·S` reduces to a yaw-only
"2φ − yaw" rule, so a yaw-only implementation would pass any upright-only test.

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
    the last plane. Once a plane exists it never falls back to "source forward", which avoids a snap of
    up to 180°.
  - **Known behaviour (state it in the tooltip):** the clone's yaw turns at twice the bearing rate, so it
    whirls when the source passes close to it.
  - Live is unavailable while Mirror position is on.
- Flipping the sign of `n` is harmless, because `M` is unchanged.

**Plane lifecycle:**

- **Capture lazily** in the post-pass whenever the clone's own mode is REFLECT and the plane is invalid.
  This covers runtime replacement (`alghoststudio.cpp:422`) and any entry that skips
  `setInstanceDriveMode`. (The switcher never returns a ghost to REFLECT.)
- **Invalidate:**
  - when leaving REFLECT for anything except FROZEN (freeze keeps it);
  - on a user gizmo move of the clone;
  - on the entity copy funnel (`:965`).

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
- `!mChaosEnabled`.

If any of them goes false, the clone **holds where it is** and the status line says why. None of the
inputs flips from frame to frame (round 2 verified this). Greying the UI is not enough; the predicate is
the guard.

**Post-pass order, per instance:** (1) plane capture/update → (2) foot push → (3) stamp.

**The foot push:**

- Write `inst.mFootGlobal` **directly**, with **no** `mTransformRevision` bump, and clear
  `mChaosHasBase`. Then call `ghost->setGhostFootPosition(...)` **only**.
- Never `applyEntityTransform` (it also writes `inst.mRotation`), never `setGhostPosition` (it disables
  the foot lock, `llghostavatar.cpp:1052`).

**The manip proxy** runs after the post-pass (§3.1). Gate it on **`stampOwnsFacing(inst)`**, not on
`reflectOwnsPosition`, because the facing is stamped even with Mirror position off. For such an
instance:

- **The drag branch (`:488`) and `endDrag`** (`:577`, `:583` → `transformUnit`/`transformGroup`,
  `alghoststudio.cpp:2887-2893`) push **position only**: no `setGhostRotation`. Add a position-only
  variant of `applyEntityTransform` for this.
- **The revision re-push (`:748-756`)** is position-only too.
- **While `reflectOwnsPosition` holds:** re-push the proxy box from `inst.mFootGlobal` **every tick**.
  The no-bump foot write would otherwise leave the box stale, and a drag would pull the clone back to it
  (`:481`).

**Copies:** the entity copy funnel (`alghoststudio.cpp:965`, reached from `:1028, :3571, :3681, :4576`)
must reset `mReflectMirrorPosition = false` and `mReflectPlaneValid = false` on the copy.

### 4.8 Why the pose stamp for clones, but asset copies for self

Reflection is an automorphism of the rotation group, and slerp commutes with it. So for keyframe content,
mirroring each asset and then blending equals blending and then mirroring.

- **Clones** have a live source with procedural layers (gaze, IK, Pose Polish), so the pose stamp is
  right.
- **Your own avatar** has no source to stamp from, so Part B mirrors the assets. The procedural layers
  stay unmirrored there (§7, L9).

---

## 5. Part A design

### 5.1 Enum and clamp

- `alghoststudio.h:110`: append `DRIVE_REFLECT` (value 4) and extend the comment.
- `llghostavatar.cpp:1470`: widen the clamp to `DRIVE_REFLECT`. **This is the trap:** if it's missed,
  everything compiles and Reflect silently runs as True Mirror (§10, FAIL-C).

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
| leaving REFLECT (same function) | **restore bone positions**: call `rebuildAttachmentOverrides()` (`llvoavatar.cpp:7648`) on the clone. The implementer must verify it is safe on an entity clone and actually restores rest + override positions. If not, state it as a limitation (L11) instead. |
| `llghostavatar.cpp:1537-1559` entry suppression | same as TRUE_MIRROR, plus reset the partner table and verify state |
| `llghostavatar.cpp:633` AutoAnimate learning presence | **exclude** REFLECT |
| `llghostavatar.cpp:892-895` `autoAnimOnDriveModeChanged` | treat REFLECT as dormant (like DIRECTED/FROZEN) |
| `llghostavatar.cpp:942-943` ghost-side dormant text | add Reflect |
| `llghostavatar.cpp:3448-3451` animesh ledger + `autoAnimLinksetStep` | keep the plain ledger mirror; **skip `autoAnimLinksetStep`** under REFLECT |
| `llghostavatar.cpp:3191-3193` repeat-held animesh | same as TRUE_MIRROR |
| `llactormover.cpp:4406`, `:5793` | `isPoseStampDriven()` |
| `alghoststudio.cpp:523-528` AutoAnimate Go | exclude REFLECT |
| `alghoststudio.cpp:1679, 1924, 2002, 2076` facing writers | `stampOwnsFacing(inst)` |
| `alghoststudio.cpp:2044-2052` post-pass | include REFLECT. For each instance: **gate on the ghost's own mode**, then plane → foot push → stamp. Hold everything when the source is missing. |
| `alghoststudio.cpp:414-423`, `:1345-1352` freeze/resume | round-trip REFLECT; freeze keeps the plane |
| `alghoststudio.cpp:965` entity copy funnel | reset Mirror position and plane validity |
| `alghoststudio.cpp:2203-2289` formation motion, `:2310-2319` chaos | covered by the runtime predicate. Confirm no write while it holds. |
| `alghoststudio.cpp:1956, 1972, 2149, 2287, 2331, 2780, 2798, 2893, 3602, 3641, 3693, 4556, 4637` `applyEntityTransform` callers | harmless **before** the post-pass. Confirm none runs **after** it in the same frame (the proxy is handled below). |
| `alghostmanipproxy.cpp:488, 577, 583, 748-756` | position-only for `stampOwnsFacing` instances; per-tick box re-push while `reflectOwnsPosition` (§4.7) |
| `alpanelghoststudio.cpp:2208-2209`, `:2238-2239` | exclude REFLECT from AutoAnim Go; add `" (dormant: Reflect)"` |
| `alpanelghoststudio.cpp:1427, 1525` combo sync | value 4 round-trips |
| `alchatcommand.cpp:820-840` | add `reflect`; update **both** usage strings (`:785`, `:839`) |

### 5.3 The stamp

Generalise `applyTrueMirrorStamp` to dispatch on the ghost's own mode.

- **TRUE_MIRROR must stay byte-identical.** Its branch is the existing code path, moved, not rewritten.
  The reviewer proves this by diff.
- **REFLECT** follows the same steps, with three differences:
  1. Root rotation `q_y(π) * q_src_world * q_n(π)`, written with `setWorldRotation`.
  2. **Bones only**, via the partner table, with the §4.2 rule.
  3. Face via §4.5.
- Then the same trailing `mRoot->updateWorldMatrixChildren()`.
- Non-finite guards on every quaternion, vector and plane value. A bad input skips that write.

### 5.4 Plane state (session-only, like the rest of `Instance`)

Add to `ALGhostStudio::Instance`: `mReflectPlaneMode` (Fixed / Live), `mReflectPlaneValid`,
`mReflectPlanePoint` (`LLVector3d`), `mReflectPlaneNormal` (horizontal unit `LLVector3`), and
`mReflectMirrorPosition` (default off). The ghost reads the plane from the Studio when stamping; pass it
in, don't cache a second copy.

### 5.5 Face pair helper

A button, applied to the **selected instance only**. It's disabled for locked groups, with a status
explanation (`setInstanceDriveMode` doesn't fan out to groups).

1. Clone foot = `source_world_foot + source_horizontal_world_forward × d`, where
   `d = GhostReflectFacePairDistance`. Use the Studio placement path; a revision bump is fine for this
   one-shot edit.
2. Set `inst.mRotation` to face the source, so the result is right even with copy-facing off.
3. Capture a Fixed plane at the midpoint, with `n` = the source's horizontal world forward.
4. Set the drive to REFLECT.

### 5.6 UI (`panel_ghost_studio.xml`, both hosts)

- Combo: add `<combo_box.item label="Reflect" name="drive_reflect" value="4" />` and update the tooltip.
- A Reflect row, enabled when the selected clone is in Reflect:
  - `Plane:` combo (Fixed / Live, with the whirl note in the Live tooltip)
  - `Mirror position` check
  - `Face pair` button, with a distance spinner bound to `GhostReflectFacePairDistance` and a ↺ reset
    button
  - `Recapture plane` button
  - a status line, e.g. `Reflect: 50 bone pairs, plane fixed, mirroring` /
    `holding: source missing` / `position held: path motion owns placement` /
    `paused: Director switcher re-drive`
- **Reflow:** the row goes inside `pose_section`, which is nested in `pose_scroll_body`. Shift everything
  below and raise **both** heights. Prove there's no overlap or clipping **by rect arithmetic in both
  hosts**.
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
- **Scope:** "Play in world" and "Play locally", from the preview floater and from the inventory context
  menu (both go through `LLPreviewAnim::play`).

### 6.2 Module: `alreflectanim.{h,cpp}` (`LLSingleton`, mirroring the `LLLocalAnimMgr` precedent)

**Derived id:** `original.combine(kReflectNamespace)`, a fixed namespace id. It is **never** sent to the
region. The §6.4 H3 hook guarantees this on the one path where a motion id is sent automatically. Also
guard the preview's send path with `!isDerived(id)`.

**Registry:** original → { derived, build state `PENDING`/`READY`/`FAILED`, mirrored bytes }, plus
`isDerived(id)` and `originalFor(derived)`.

**Reflect set:** per original X currently reflected on self: { route `LOCAL`/`INWORLD`, phase
`STARTING`/`ACTIVE`/`STOPPING`, stopping-since time }. Rules are in §6.4.

**Teardown:** `removeKeyframeData` for every derived id in the destructor (precedent:
`lllocalanim.cpp:72`). Clear the set on logout or disconnect.

### 6.3 Build pipeline (once per original per session; strict order)

1. **Original bytes.**
   - If cached, read `LLFileSystem(original, AT_ANIMATION)` exactly as `onLoadComplete` does.
   - Otherwise call `gAssetStorage->getAssetData(original, AT_ANIMATION, …)` (as at
     `llkeyframemotion.cpp:560`) and read in the callback.
   - **Never start the original on self to obtain it.**
2. `probe = gAgentAvatarp->createMotion(probe_id)`, with a freshly generated id. `gAgentAvatarp`
   provides the collision-volume names.
3. `probe->deserialize(bytes, probe_id, /*allow_invalid_joints=*/true)`. Use `true`, matching stock
   playback (§3.2); with `false`, any animation with an unknown joint name would FAIL.
4. Get the list with `LLKeyframeDataCache::getKeyframeData(probe_id)` and **mirror it in place**:
   - every `JointMotion`: `mJointName` → partner (§4.4); every `mRotationCurve.mKeys` value → §4.2
     rotation rule; every `mPositionCurve.mKeys` value → y-flip; `mScaleCurve` untouched.
   - after renaming, **no two JointMotions may share a name**. A duplicate means FAIL.
   - every constraint (iterate `mConstraints` with `auto`; the type is protected, and no llcharacter
     edit is needed):
     - source volume index → name → partner → `getCollisionVolumeID`. The same for a BODY target. **An
       unresolved index (−1) means FAIL**, before `serialize` dereferences it.
     - y-flip `mSourceConstraintOffset` and, for BODY targets only, `mTargetConstraintOffset`. Both are
       volume-local.
     - **A GROUND target's `mTargetConstraintOffset` is agent-frame.** Leave it unchanged, and log a
       note when its XY is non-zero.
     - `mTargetConstraintDir` is never read at runtime. Y-flip it for consistency or leave it.
   - `mHandPose`: swap `RELAXED_L/_R`, `POINT_L/_R`, `FIST_L/_R`. Others are kept, with a log line for
     one-sided `_R` poses.
   - **Not touched:** `mPelvisBBox` and curve loop keys. They aren't serialized; they're recomputed
     from the mirrored keys (§3.2). The same goes for emote, priority, ease, loop points and duration.
5. `probe->serialize(dp)` into a buffer sized by `probe->getFileSize()`. These are the mirrored bytes.
6. `gAgentAvatarp->removeMotion(probe_id)`, **then** `LLKeyframeDataCache::removeKeyframeData(probe_id)`.
   This order avoids both a use-after-free and a registered motion with a freed list.
7. **Register:** `createMotion(derived)` + `deserialize(mirrored, derived, true)`, **exactly once** per
   derived id. The state becomes READY.
   - **Never `createMotion`/`startMotion(derived)` before READY.** A second `onInitialize` with no
     cached data would fetch the derived id from the asset service (§3.2).
8. **On FAIL:** log the reasons, **remove X from the Reflect set** (§6.4), play the original through the
   stock path, and show a one-time notice: "Couldn't reflect <name>; playing it normally."

### 6.4 The state machine — three hooks

**Hooks:**

- **H1 — `LLPreviewAnim`:** `play` / stop / `onClose` / `draw`.
- **H2 — one guarded branch in `LLVOAvatar::processSingleAnimationStateChange`,** at its
  `startMotion`/`stopMotion` calls: `if (isSelf() && reflectSetNonEmpty && set.contains(anim_id))`.
  - Everything above those calls (`computeBodySize`, typing sound, `sitDown`, snapshot) is unchanged. It
    keys on built-in ids, which are never members.
  - **Off-path:** not self, or the set empty → byte-identical. The reviewer must prove this; it's the
    only shared hot-path edit, and it runs for every avatar.
- **H3 — natural end:** in `LLAgent::requestStopMotion` (`llagent.cpp:2760-2769`), map the id first:
  `id = isDerived(motion->getID()) ? originalFor(motion->getID()) : motion->getID()`. Then call
  `onAnimStop(id)` and send STOP(id).
  - This restores stock behaviour exactly (stock sends STOP(original) here), and the derived id never
    reaches the region.
  - It then applies the member STOP rule below to X.

**Join** (H1 only, toggle ON at the moment of play):

- Only if X is **not already active on self**: not playing locally, and not in `mSignaledAnimations` or
  `mPlayingAnimations` (for example, from a poseball or another preview).
- Otherwise play stock, unmirrored, and log `already playing; not reflected`. This avoids a mixed state.
- The toggle changes **only joins**. Unticking never affects existing members, so there's no pop, and
  stops still map.

**Start:**

- **LOCAL:** if READY, `startMotion(derived)`. If not, the phase is STARTING and nothing plays (**never
  flash the original**); start the derived motion on READY if X is still a member.
- **INWORLD:** send START(X), the original, as stock. When the echo reaches H2 with start:
  - if READY, `startMotion(derived)` and **return true**;
  - if not, mark the phase STARTING and **return true anyway**, so `mPlayingAnimations[X]` is set and
    the region's later STOP is processed (§3.2). On READY, start the derived motion if X is still in
    `mSignaledAnimations`.
- Set `mItemID` **unconditionally** to the id actually started (the derived one when reflected), so the
  `draw()` priority re-apply and the button reset work for pending plays too.

**Member STOP rule** (every route: H1 toggle-off/`onClose`, the H2 region STOP echo, the H3 natural end,
and `stopMotionFromSource`, whose region echo reaches H2):

- `stopMotion(X)` **and** `stopMotion(derived)` (both idempotent), and cancel any pending start.
- **Leaving the set:**
  - **LOCAL** members leave at once.
  - **INWORLD** members go to STOPPING and leave only when X is **absent from `mSignaledAnimations`**
    (the region's STOP echo). That way a late START echo still plays mirrored, never unmirrored.
  - **Safety:** an INWORLD member that has been STOPPING for more than 10 s while X is absent from
    `mSignaledAnimations` is removed.
- **FAIL** (§6.3, step 8) removes X at once, before the stock play. So a FAILed original's stops are
  stock, and a looping original can always stop.

**Shared playback:** while X is a member, a different source starting the same X (an object script,
say) shares the one signalled id, so it plays mirrored too (§7, L12).

### 6.5 Effects to handle or state

- **Local-only:** other viewers see the original.
- **UI "is playing" queries:** Animation Explorer keys on `mAnimationSources` (original ids), so it needs
  no mapping. Any UI that queries `isMotionActive(original)` for self sees "not playing" while the
  derived copy plays (§7, L10).
- **Clones of you:**
  - **Mirror** drive copies signalled ids, so it shows the original.
  - **True Mirror** copies your final pose, so it shows your mirrored version.
  - **Reflect** mirrors it again, back to the original.

---

## 7. Known limitations (stated, not hidden)

- **L1 — The geometry isn't mirrored.** A hair parting, a single strap, a tattoo or text stays on its
  side.
- **L2 — Legs are within 3 mm, not exact.** The rest skeleton is 2–3 mm asymmetric at the hips, knees and
  ankles. Asymmetric **mesh** joint-position overrides also get swapped, which can distort an asymmetric
  mesh slightly.
- **L3 — One-sided expressions and hand poses don't flip.** `Express_*` morphs have no mirrored twin, nor
  do the `SALUTE_R`, `PEACE_R` and `PALM_R` hand poses.
- **L4 — Held props stay on the same hand** (D3).
- **L5 — Animesh worn by the clone plays unmirrored**, via the ledger, exactly as under True Mirror.
- **L6 — Mutual gaze only works while the clone sits at the mirror position** (Live plane, or Mirror
  position on). Gaze at the camera never survives: the clone looks at the camera's mirror image.
- **L7 — Uneven ground:** mirrored foot plants can float or sink, because the clone's foot IK is paused.
- **L8 — Part A cost** is the same as True Mirror plus a table lookup. It isn't meant for crowds.
- **L9 — Part B mirrors animation files only.** Gaze, IK, Pose Polish and head turns on your own avatar
  stay unmirrored.
- **L10 — Part B:** a UI that asks `isMotionActive(original)` for self sees "not playing" while the
  mirrored copy plays.
- **L11 — Leaving Reflect** may leave the clone's bones at their mirrored positions (2–3 mm, plus swapped
  mesh overrides) if `rebuildAttachmentOverrides` proves unsuitable (§5.2).
- **L12 — Part B, shared playback:** while a reflected animation is playing, another source starting
  the same animation plays mirrored too, because it is one signalled id.

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
     (`llvoavatar.cpp:4888`, `:4948`, `:5088`).
   - Any difference > 1e-4 gives `FAIL: <bone> changed between stamps (second writer)`.
   - A skipped previous stamp gives INCONCLUSIVE.
3. **Bone symmetry check** (every 5 s, using the engine's world matrices): for {`mWristLeft/Right`,
   `mAnkleLeft/Right`, `mHead`, `mPelvis`}, compute each joint's world position in its own root frame.
   Expect `clone(P(J)) == S·source(J)` within 1 cm × uniform scale.
   - The implementer must verify the joint world frame on a **scaled** clone. The light rig found joint
     world positions exclude the outer render scale (`doc/CINE_LIGHT_RIG_STATUS.md` on
     `feature/cine-light-rig`; re-locate the code on this base).
   - A shape mismatch or an unbuilt source gives INCONCLUSIVE.
4. **World facing check** (every 5 s; this is the only check that sees the root): expect clone world
   forward `== M·(source world forward)` and up `== M·(source world up)`, both within 1°. Skip it with
   INCONCLUSIVE when copy-facing is off.

**Part B**

5. **Build verdict:** `anim <orig> -> <derived>: joints N (P paired, C centre, U unresolved),
   constraints K (G ground-offset notes), hand pose A->B, built in Z ms -> PASS`, or FAIL with reasons.
6. **Round-trip check** (when `GhostReflectVerify` is on): decode the **mirrored bytes** into a second
   scratch motion, and compare against **mirror(decode(original))** computed independently in memory.
   Key counts must match per joint. Values must match within **2 quanta**: 2 × 1.53e-4 m for
   positions, and the matching U16 rotation quantum. PASS or FAIL.
   - This catches a lost or duplicated key, a serialize/deserialize asymmetry, and a wrong constraint
     remap.
   - It does **not** prove the reflection is the right one; that's T2/T3.
   - Dispose of the second scratch motion in the §6.3 step-6 order.
7. **Substitution trace:** `self start <orig> as <derived> (local|echo|pending)` and the matching stop,
   with the phase. **An H3 mapping event logs `natural end: STOP(original) sent`.** If a derived id
   ever reaches `sendAnimationRequest`, log `FAIL: derived id on the wire` (a guard-and-log at that
   call).

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
- **T4 Involution and homomorphism:** a regression guard only (any diagonal sign flip passes it).
- **T5 Root and plane, full-3D:** random pitched and rolled source rotations × plane azimuths (0°, 37°,
  90°, 179°, −135°). Behavioural check: `world_clone(S·u) == M·world_source(u)` for test vectors `u`.
  It must distinguish `M·R·S` from `S·R·M`, from `M·R`, and from a yaw-only rule. Check the LL-order
  formula against the behaviour.
- **T6 Facing check:** a source facing along `n` gives a clone facing −`n`.
- **T7 Plane:** the bisector; each fallback tier; the Live hysteresis band (0.1 → 0.3 m) holds the plane;
  NaN input never produces NaN.
- **T8 Face params:** 58↔59 swap; 1206/1207 negate (literals −0.7 and 1.3); other ids pass through.
- **T9 Hand poses:** the three L/R pairs swap and are involutive; one-sided `_R` and symmetric poses
  pass through.
- **T10 Constraint vectors:** source and BODY-target offsets y-flip; a GROUND-target offset is
  unchanged.
- **T11 Part B set rules** (pure state-machine helper):
  - join refused when X is already active;
  - toggle-off doesn't affect members;
  - LOCAL leaves on stop;
  - INWORLD leaves only when X is absent from the signalled set, or after the 10 s safety window;
  - FAIL removes at once;
  - a pending INWORLD start reports "handled" (true).

---

## 10. In-world tests — outcomes stated in advance

**Part A.** Make an entity clone of yourself, click **Face pair** (2 m), and play an animation that
raises your **right** arm forward.

- **PASS:** the clone raises its **left** arm toward you. The log shows the entry, symmetry and facing
  PASS lines.
- **FAIL-A** (names swapped, rotation not reflected): the clone's left arm swings **backward**.
- **FAIL-B** (rotation reflected, names not swapped): the clone's right arm moves, and wrongly.
- **FAIL-C** (clamp missed): the clone raises its **right** arm while the combo reads Reflect.
- **Seated check:** sit on a rotated poseball. The clone must still face you mirror-correctly, with a
  facing PASS. If it's off by the seat's angle, the source root is being read in local space.
- Then:
  - Turn 45° left: the clone turns 45° right.
  - With Mirror position on, step toward the clone: it steps toward you.
  - **Ghost Edit:** drag the clone with the gizmo. It must keep its mirrored facing throughout and after
    release.
  - Switch to True Mirror: it behaves exactly as before.
  - Leave Reflect: no visible leg or mesh distortion remains (or L11 applies).

**Part B.** Tick **Reflect** in the animation preview.

- **Play locally** a right-arm animation: your **left** arm moves. No flash of the right arm first.
- **Play in world:** you see the left arm. A second viewer sees the right arm (the original).
- **Natural end:** play a **non-looping** animation in world and let it finish. It stops for you **and**
  on the second viewer. The log shows `natural end: STOP(original) sent` and never
  `derived id on the wire`.
- Untick mid-play: it keeps reflecting until stopped. The next play is normal.
- Start the same animation from a poseball first, then press Play in world with Reflect ticked: it plays
  unmirrored, and the log says `already playing; not reflected`.
- Stop, or close the preview: the animation stops. **No stuck animation**, looping or not. The log shows
  the build PASS and round-trip PASS.

---

## 11. Off-limits for the implementer

- `pipeline.cpp`, `pipeline.h`, any shader, any render target or GL state.
- `fsposeranimator.*` and `fsjointpose.*`: read-only reference.
- `indra/newview/character/*.xml` and `indra/llcharacter/*`: inputs, not edit targets. Part B uses
  `LLKeyframeMotion`'s existing API only.
- `llvoavatar.cpp`: **only** the H2 guarded branch in `processSingleAnimationStateChange`.
- `llagent.cpp`: **only** the H3 id mapping in `requestStopMotion` (plus the wire guard-and-log, §8
  item 7).
- The TRUE_MIRROR code path's behaviour (§5.3).
- Never build. Claude builds after review convergence.

---

## 12. Open decisions (defaults chosen; change before handing to Codex)

- **D1 — Mirror position default:** OFF.
- **D2 — Plane mode default:** Fixed.
- **D3 — Held props:** deferred (L4).
- **D4 — Settings reuse:** reuse the two `GhostTrueMirror*` keys.
- **D5 — True Mirror's seated-source root** (pre-existing): leave it in this delivery. Recommend a
  separate one-line fix: read `getWorldRotation()` and write it with `setWorldRotation`.
- **D6 — Part B scope:** preview and inventory only. Alternative: also the Animation Switchboard and
  gestures.

---

## Appendix — review disposition

### Round 1 (applied in rev 2)

| # | Finding | Disposition |
|---|---|---|
| M1 | Seated source: the root is seat-relative | World frame everywhere; True Mirror's same defect → D5 |
| M2 | `getSkeleton()` holds bones only (50/33); stamping volumes would accumulate | Bones only; counts fixed |
| M3 | Post-pass `applyEntityTransform` overwrites facing; revision bump re-triggers the proxy; missing sites | Order plane → foot → stamp; foot-only push with no bump; sites added (proxy fix completed in rev 3) |
| M4 | Placement ownership holes | Runtime predicate; copy resets |
| M5 | T5 couldn't pin `M·R·S`; LL order unstated; det test tautological | Full-3D T5; LL formula; det test dropped |
| S1–S8, nits | AutoAnimate sites, switcher, source identity, Live plane, rest asymmetry, plane lifecycle, facing verdict, `mRoot` exclusion | Applied |

### Round 2 (applied in rev 3)

| # | Finding | Disposition |
|---|---|---|
| M1 | A natural end sends STOP(derived) to the region; the original stays signalled | H3 hook maps derived → original in `LLAgent::requestStopMotion` (§6.4) |
| M2 | Probe lifetime contradiction (use-after-free or stale motion) | Strict pipeline order (§6.3, steps 2–7) |
| M3 | Stop mapping leaves stuck animations (FAIL path, original already active, pending start, early removal) | State machine: stop both ids, pending returns true, FAIL clears, INWORLD leaves on signal absence, join refused when active (§6.4) |
| S1 | `allow_invalid_joints` should be true | Applied |
| S2 | Proxy gated on the wrong predicate; drag and `endDrag` write rotation; stale proxy box | Gate on `stampOwnsFacing`; position-only pushes; per-tick re-push (§4.7) |
| S3 | Leaving REFLECT leaves mirrored bone positions | Restore via `rebuildAttachmentOverrides`, else L11 |
| S4 | Round-trip verdict undefined or tolerance below one quantum | Bytes-vs-independent-mirror comparison, key counts, 2 quanta (§8 item 6) |
| S5 | A GROUND-target offset is agent-frame | Left unchanged and logged |
| Nits | bbox and loop keys aren't serialized; `setWorldRotation` caveat; copy funnel `:965`; no Director inspector (Animation Explorer needs no mapping); switcher never returns to REFLECT; raise both nested heights; −1 volume check; protected type; no derived `createMotion` before READY; `mItemID` unconditional | Applied |

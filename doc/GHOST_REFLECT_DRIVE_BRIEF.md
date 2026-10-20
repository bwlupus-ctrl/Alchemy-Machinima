# Ghost Studio — "Reflect" drive mode (mirror-image clone) — design brief

**Date:** 2026-10-20
**Status:** DESIGN ONLY. Nothing implemented, nothing built.
**Process (per `CLAUDE.md`):** Codex implements from this brief (`--write`, `--prompt-file`), an
independent Opus sub-agent reviews it ADVERSARIALLY, loop to 0 must-fix, THEN Claude builds once,
then the user tests in-world. Codex never builds.

> **BASE BRANCH — READ FIRST.** Every `file:line` below is from `origin/fix/animesh-clone-pose-polish`
> at `057e558`, the newest pushed branch that contains Ghost Studio's **True Mirror** drive. True Mirror
> is **not on `develop`**. If the user has unpushed local work, diff it against this brief and
> re-anchor the line references before implementing.

---

## 1. The ask

> "What about mirrored animation for clones, say by flipping the name of the bones, so an avatar and a
> clone could face each other." … "Reflect drive — that sounds perfect."

**Reflect** is a new Ghost Studio drive mode for **entity clones**. The clone shows the live **mirror
image** of its source's motion:

- The source raises its **right** arm toward the clone, and the clone raises its **left** arm toward
  the source.
- The source turns 30° to its left, and the clone turns 30° to its right.
- With **Face pair**, the two stand facing each other across a mirror plane, like dance partners or a
  person and their reflection.

This is a **motion** mirror. The clone's geometry is not reflected (§6).

---

## 2. Scope (stated up front, per "ship whole")

**In this delivery, as one batch:**

1. `DRIVE_REFLECT` drive mode, eligible exactly where `DRIVE_TRUE_MIRROR` is (entity clones only).
2. A reflected skeleton stamp: left/right partner joints, reflected rotations and positions.
3. A reflected face stamp: blink swap and left/right physics negation. Everything else is copied, as
   True Mirror does.
4. A per-instance **mirror plane**: Fixed (captured) or Live (bisector), plus reflected body facing.
5. An optional per-instance **Mirror position** toggle, so the clone's placement mirrors the source's
   movement across the plane.
6. A **Face pair** helper: places the clone in front of the source, facing back, sets the plane, and
   switches the clone to Reflect.
7. UI in the shared Ghost Studio panel (both hosts), the `/ghostanim reflect` chat command, and
   tooltips.
8. A pure header with the maths and pairing rule, plus TUT tests.
9. `[GhostReflect]` log verdicts: pairing, foreign-writer detection, and a world-space symmetry check.
10. A user-guide section in `doc/MACHINIMA_USER_GUIDE.md` (Ghost Studio chapter).

**Explicitly NOT in this delivery** (each needs its own brief):

| Deferred | Why |
|---|---|
| Looking-glass (geometry) mirror, where asymmetric hair, straps and text are reversed | Needs a negative-determinant render transform and a front-face winding flip. That is shared GL state (`CLAUDE.md` rule 1), a different risk class from this CPU-only feature. |
| Swapping held props to the other hand | Needs re-attaching the clone's cloned attachments to the paired attach point with a reflected offset. It touches the clone-attachment pipeline. See open decision D3. |
| The clone's **own** gaze running on top of the stamp ("both look at camera") | Gaze runs inside `updateCharacter`, before the post-object-list stamp. Running it after needs a reordering with its own review. |
| Mirrored `.anim` assets for Directed mode or the Animation Switchboard | A separate asset-transform path. §4.8 explains why the pose stamp comes first. |
| AutoAnimate under Reflect | AutoAnimate replays learned assets unmirrored, so the mirror would silently break. AutoAnimate is **dormant** under Reflect (§5.2). |
| Overlay ghosts | True Mirror already ignores them (`alghoststudio.h:107`). Reflect matches. |

There are **no** pipeline, shader, render-target or GL-state changes anywhere in this delivery. If an
implementation finds it needs one, stop and report back. Do not add it.

---

## 3. What the code PROVES today (base-branch file:line)

| Fact | Where |
|---|---|
| Drive modes are `DRIVE_MIRROR=0, DRIVE_DIRECTED, DRIVE_FROZEN, DRIVE_TRUE_MIRROR`. The enum comment requires new values to be **appended last** so the UI combo and chat values stay stable. | `alghoststudio.h:97-111` |
| True Mirror stamps the source's FINAL pose (after motions, Pose Polish, gaze and IK) onto the clone from a post-pass that runs right after `gObjectList.update()`. | `alghoststudio.cpp:2024-2053` (`updatePostObjectList`), `alghoststudio.h:599` |
| The stamp self-gates on the **ghost's own** drive mode, not the Studio instance's, because the Director anim switcher can re-drive a ghost without touching the Studio model. | `alghoststudio.cpp:2038-2041`, `llghostavatar.cpp:1570` |
| The stamp runs once per frame, holds the last pose if the source is missing, and resolves the source every frame through `gObjectList`. | `llghostavatar.cpp:1568-1624` |
| Skeleton stamp: root **rotation** only (when `GhostTrueMirrorCopyFacing` is on), root position never. Then, for every `getSkeleton()` joint, `dst->setRotation(src->getRotation())` and `dst->setPosition(src->getPosition())`. Index-parallel, with a by-name fallback. Scale is not copied. | `llghostavatar.cpp:1626-1674` |
| Face stamp copies `Express_*`, `Blink_*`, `Lipsync_*` and the physics-driven params 1200..1207. | `llghostavatar.cpp:1676-1730` |
| **`setEntityDriveMode` clamps `mode` to `[DRIVE_MIRROR, DRIVE_TRUE_MIRROR]`.** A new value 4 would be silently clamped to True Mirror. | `llghostavatar.cpp:1468-1471` |
| Entering True Mirror pauses the clone's controller and stops its eye motion. Leaving it releases the pause. | `llghostavatar.cpp:1489-1494`, `:1537-1559` |
| Actor Mover parks a walk on a True Mirror clone and skips painting its gaze, both via `isTrueMirrorDriven()`. | `llactormover.cpp:4399-4410`, `:5788-5797` |
| Studio facing writers (keep-facing, crowd facing, turn-to) skip the clone when `trueMirrorOwnsFacing(inst)` is true. | `alghoststudio.cpp:1679`, `:1924`, `:2002`, `:2076`; defined `:2016-2022` |
| AutoAnimate eligibility and learning presence test `MIRROR \|\| TRUE_MIRROR`. | `alghoststudio.cpp:523-528`, `llghostavatar.cpp:633`, `alpanelghoststudio.cpp:2208-2209` |
| Animesh attachments on the clone keep the ObjectAnimation ledger mirror under True Mirror, because the joint stamp doesn't cover their separate skeletons. The repeat-held-animesh path is gated the same way. | `llghostavatar.cpp:3446-3449`, `:3191-3193` |
| Clone placement: the Studio model holds `inst.mFootGlobal` and `inst.mRotation`. `applyEntityTransform` pushes them through `setGhostFootPosition`, which sets the desired foot, applies it immediately and updates the outer transform. The per-frame foot lock in `idleUpdate` re-applies it unless Actor Mover owns the root. | `alghoststudio.cpp:1425-1438`, `llghostavatar.cpp:1062-1117`, `:3656-3684` |
| Ghost Studio instances are **session-only**. Nothing persists `mDriveMode`; the only LLSD persistence in `alghoststudio.cpp` is the AutoAnimate pool. | `alghoststudio.h:351-366`, `alghoststudio.cpp:15`, `:554` |
| Freeze/resume stores and restores the previous drive mode, which may be the new one. | `alghoststudio.cpp:1345-1352`, `:414-423` |
| The drive combo lists items with values 0..3. The panel is hosted in **two** floaters. | `panel_ghost_studio.xml:218-224`; hosts `floater_director.xml:567`, `floater_ghost_studio.xml:18` |
| The chat command maps `mirror`, `truemirror` and `freeze`. The usage string appears **twice**. | `alchatcommand.cpp:820-840`; usage at `:785` and `:839` |
| True Mirror settings: `GhostTrueMirrorCopyFacing`, `GhostTrueMirrorCopyFace` (both default true). | `settings.xml:7230-7253` |
| The FS poser already pairs left/right joints and reflects with `mQ[VX] *= -1; mQ[VZ] *= -1`. | `fsposeranimator.h:219-408` (table), `fsjointpose.h:303-315` |

---

## 4. The maths

SL's avatar frame is **+X forward, +Y left, +Z up**. Reflect means reflecting across the body's
sagittal (XZ) plane, written `S = diag(1, −1, 1)`.

### 4.1 Why renaming bones alone is wrong

The right arm rests along −Y. "Reach forward" for that arm is +90° about Z, which carries −Y to +X.
Swap the name only, and the left arm (resting along +Y) receives the same +90° about Z, which carries
+Y to **−X**: the arm swings **behind the back**. With the reflected rotation (−90° about Z) it carries
+Y to +X and reaches forward correctly. This is the TUT discriminator in §5.11 (T2).

### 4.2 The per-joint rule

For each source joint `J` with partner `P(J)` (itself, for centre joints):

- **Rotation:** `q = (x, y, z, w)` becomes `q' = (−x, y, −z, w)`, written to `P(J)` on the clone.
  This is conjugation by `S`: a rotation axis is a pseudovector, so it maps to `det(S)·S·axis`. The
  rule doesn't depend on LL's row-vector convention, because `S` is diagonal and its own inverse. The
  poser uses the same rule (`fsjointpose.h:303-315`).
- **Position:** `(x, y, z)` becomes `(x, −y, z)`, written to `P(J)` (parent-local, like True Mirror's
  copy).
- **Scale:** not written (True Mirror parity).

### 4.3 Precondition — verified, not assumed

The rule produces a true mirror image only if the rest skeleton is itself mirror-symmetric.
`avatar_skeleton.xml` was checked:

- All 133 `m*` bones have **zero** rest rotation.
- Of the 26 collision volumes, the 13 with non-zero rest rotation are either symmetric pairs, or centre
  joints that rotate only about Y (which `S` leaves alone).
  - Symmetric pairs: `L_UPPER_ARM` −5/0/0 ↔ `R_UPPER_ARM` +5/0/0; `L_LOWER_ARM` −3/0/0 ↔ `R_LOWER_ARM`
    +3/0/0; `L_HAND` −3/0/−10 ↔ `R_HAND` +3/0/+10; `L_FOOT`/`R_FOOT` both 0/10/0;
    `LEFT_PEC`/`RIGHT_PEC` both 0/4.29/0.
  - Pure-pitch centre joints: `PELVIS` 0/8/0, `BELLY` 0/8/0, `CHEST` 0/−10/0.

Shapes and mesh joint-position overrides can still make a *particular* avatar asymmetric (§6, L2).

### 4.4 Pairing rule — by NAME, never by index

Rule, applied in order, first match wins:

1. If the name contains `Left`, replace it with `Right` (and vice versa).
2. Otherwise, if it starts with `L_`, swap to `R_` (and vice versa).
3. Otherwise, if it starts with `LEFT_`, swap to `RIGHT_` (and vice versa).
4. Otherwise the joint is a centre joint and is its own partner.

Checked against the full `avatar_skeleton.xml` (159 joints = 133 bones + 26 collision volumes): **59
pairs + 41 centre joints, every partner exists, 0 misses.** It also reproduces all 116 paired entries
of the FS poser table with 0 disagreements. The poser table lacks 4 skeleton joints (`LOWER_BACK`,
`mFaceEyeAltLeft`, `mFaceEyeAltRight`, `mFaceJawShaper`), so **use the rule, not the poser table.** The
poser table is also an instance member of `FSPoserAnimator`, which pulls in `llvoavatar.h`.

Build a `src index → dst index` partner table **once per clone build**, by name, through `getJoint()`.
Do not reuse True Mirror's index-parallel shortcut: it is only valid for the identity copy. A joint
whose partner can't be resolved is skipped and logged (§5.10).

### 4.5 Face and physics params (`avatar_lad.xml`)

| Param | id | Range | Under Reflect |
|---|---|---|---|
| `Blink_Left` ↔ `Blink_Right` | 58 ↔ 59 | 0..1 | **swap** (left lid takes the source's right value) |
| `Butt_Physics_LeftRight_Driven` | 1206 | −1..1, default 0 | **negate** |
| `Breast_Physics_LeftRight_Driven` | 1207 | −2..2, default 0 | **negate** |
| every other param True Mirror copies | — | — | copy unchanged |

Both LeftRight params are centred on 0, so negating them is the reflection. `Express_*` are single
morphs with no mirrored twin, so a one-sided expression (for example `Express_Wink_Emote`) stays on the
same side (§6, L3).

### 4.6 Mirror plane and body facing

A per-instance vertical plane: a point `P0` (global) and a horizontal unit normal `n`. The world
reflection is `M = I − 2nnᵀ`.

- **Root rotation** (when `GhostTrueMirrorCopyFacing` is on): `R_clone = M · R_source · S`, where
  `R_source` is the source root's local-to-world rotation (`getRootJoint()->getRotation()`, as True
  Mirror reads it). `det = (−1)(+1)(−1) = +1`, so the result is a proper rotation.
  - **Write the tests in terms of behaviour**, not the formula. For a local test vector `u`:
    `world_clone(S·u) == M · world_source(u)` (directions). That holds whichever multiplication order
    LL's math types use.
  - Upright case, as a sanity check: yaw′ = 2·φ_line − yaw, where φ_line is the azimuth of the plane's
    in-plane horizontal direction (normal azimuth + 90°). This is the same reflect-across-a-line idiom
    as the light rig's Mirror fix (`wrap180(2*facing − yaw)`).
  - Facing check: the source faces along `n`, toward the clone, so the clone faces along −`n`, back at
    the source.
- When `GhostTrueMirrorCopyFacing` is off, the clone keeps its own Studio facing and only the joints
  reflect. The pose is still a mirror image, in the clone's own frame.

**Plane modes** (per instance):

- **Fixed** (default): captured when the clone enters Reflect, by **Face pair**, or by **Recapture
  plane**. The plane is the perpendicular bisector of source and clone foot positions (horizontal
  components). Degenerate case: if they are closer than 0.1 m horizontally, use the plane through the
  source with `n` = the source's horizontal forward.
- **Live:** recomputed every frame as the bisector, so the pair keeps facing each other even when the
  source walks around the clone. Live is unavailable while Mirror position is on, because the clone's
  position is derived from the plane. Fall back to the last valid plane on the same degenerate test
  (never NaN).

### 4.7 Mirror position (optional, per instance; requires a Fixed plane)

The clone's desired foot = the source's foot reflected across the plane in XY, at the source's foot
height. It is written into the **Studio model** (`inst.mFootGlobal`) and pushed with
`applyEntityTransform` from the post-pass, so:

- the foot-lock channel stays the single position owner, every other Studio writer reads the same
  value, and nothing calls `setGhostPosition` (which disables the foot lock, `llghostavatar.cpp:1052`);
- `setGhostFootPosition` applies immediately, so pose and position land in the **same frame**;
- turning Mirror position off leaves the clone **where it is**, because the model already holds the
  last reflected foot. No snap-back.

Mirror position is disabled (control greyed, status explains) for instances with a Studio motion mode
other than OFF (orbit, spin, path), and for instances in a locked group. Those modes already own
placement.

### 4.8 Why the pose stamp, not mirrored assets

Reflection is an automorphism of the rotation group: `S·(q₁q₂)·S = (S q₁ S)(S q₂ S)`, and slerp commutes
with it. So mirroring each keyframe asset and then blending gives the same result as blending and then
mirroring the final pose, for keyframe content. The two approaches differ only on procedural layers
(gaze, IK, Pose Polish, head turn, live poser edits). Only the pose stamp catches those, and True
Mirror already provides it.

---

## 5. Design

### 5.1 Enum and clamp

- `alghoststudio.h:110`: append `DRIVE_REFLECT` **after** `DRIVE_TRUE_MIRROR` (value 4) and extend the
  enum comment.
- `llghostavatar.cpp:1470`: widen the clamp's upper bound to `DRIVE_REFLECT`. **This is the trap:** if
  it's missed, everything compiles and Reflect silently runs as True Mirror.

### 5.2 One predicate per meaning, applied at every site

Add to `LLGhostAvatar`:

- `isPoseStampDriven()`, true for `TRUE_MIRROR || REFLECT`. Keep `isTrueMirrorDriven()` with its
  current meaning.

Add to `ALGhostStudio`:

- `stampOwnsFacing(inst)` = entity clone `&& (TRUE_MIRROR || REFLECT) && GhostTrueMirrorCopyFacing`.
  Keep `trueMirrorOwnsFacing` as an alias or replace it, but every call site must move to the new
  meaning.
- `reflectOwnsPosition(inst)` = REFLECT && Mirror position on.

Every site in §3 must be classified. **A missed site is a second writer on the clone's pose and fails
silently**, so the reviewer must re-grep, not trust this table:

| Site | Under REFLECT |
|---|---|
| `llghostavatar.cpp:1470` clamp | widen (§5.1) |
| `llghostavatar.cpp:1489-1494` pause release on leaving | release when leaving TRUE_MIRROR **or** REFLECT |
| `llghostavatar.cpp:1537-1559` entry suppression | same suppression as TRUE_MIRROR; also reset the reflect partner table and verify state |
| `llghostavatar.cpp:633` AutoAnimate learning presence | **exclude** REFLECT (learning pauses) |
| `llghostavatar.cpp:3191-3193` repeat-held animesh | same as TRUE_MIRROR (animesh ledger mirror continues; §6 L5) |
| `llghostavatar.cpp:3446-3449` animesh ledger | same as TRUE_MIRROR |
| `llactormover.cpp:4406` walk parked | `isPoseStampDriven()` |
| `llactormover.cpp:5793` gaze skip | `isPoseStampDriven()` |
| `alghoststudio.cpp:523-528` AutoAnimate Go | **exclude** REFLECT (returns false) |
| `alghoststudio.cpp:1679, 1924, 2002, 2076` facing writers | `stampOwnsFacing(inst)` |
| `alghoststudio.cpp:2044-2052` post-pass loop | include REFLECT; also run the plane update and Mirror position push here (§5.4, §4.7) |
| `alghoststudio.cpp:414-423`, `:1345-1352` freeze/resume | must round-trip REFLECT, including its plane state |
| `alpanelghoststudio.cpp:2208-2209` AutoAnim Go enable | exclude REFLECT |
| `alpanelghoststudio.cpp:2238-2239` dormant text | add `" (dormant: Reflect)"` |
| `alpanelghoststudio.cpp:1427, 1525` combo sync | value 4 must round-trip |
| `alchatcommand.cpp:820-840` | add `reflect`, and update **both** usage strings (`:785`, `:839`) |
| Studio placement writers that call `applyEntityTransform` (`alghoststudio.cpp:1956, 1972, 2149, 2287, 2331, 2780, 2798, 2893, 3602, 3641, 3693`) | harmless under §4.7 (they push the model value the post-pass wrote). The reviewer must confirm none of them **recomputes** `mFootGlobal` from formation/motion state while `reflectOwnsPosition`. |

### 5.3 The stamp

Generalise `applyTrueMirrorStamp` into a stamp that dispatches on the ghost's own mode:

- **TRUE_MIRROR must stay byte-identical.** Its branch is the existing code path, moved, not rewritten.
  The reviewer proves this by diff.
- **REFLECT** follows the same steps, with three differences:
  1. Root rotation `M · R_src · S` (§4.6) instead of a copy.
  2. Joints written via the partner table with the §4.2 rule.
  3. Face via §4.5.
- Then the same trailing `mRoot->updateWorldMatrixChildren()`.
- Same once-per-frame guard, same hold-last-pose on a missing source, same source resolution every
  frame.
- Non-finite guards on every quaternion, vector and plane value. A non-finite input skips that write;
  it never propagates (True Mirror already guards the root with `isFinite()`).

### 5.4 Plane state (per instance, session-only like the rest of `Instance`)

Add to `ALGhostStudio::Instance`:

- `mReflectPlaneMode` (Fixed / Live)
- `mReflectPlaneValid`
- `mReflectPlanePoint` (`LLVector3d`)
- `mReflectPlaneNormal` (horizontal unit `LLVector3`)
- `mReflectMirrorPosition` (bool, default off)

The plane is captured on entry to REFLECT if not valid, on **Face pair**, and on **Recapture plane**.
Live mode recomputes it in the post-pass before the stamp. The ghost reads the plane from the Studio
when stamping: pass it in, don't cache a second copy.

### 5.5 Face pair helper

Button, per selected entity clone (disabled for locked groups in this delivery, with a status
explanation):

1. Place the clone's foot at `source_foot + forward_source_horizontal × d`, where
   `d = GhostReflectFacePairDistance` (new setting, F32, default 2.0 m, range 0.5–10, step 0.1).
   Use the Studio's own placement path (`inst.mFootGlobal`, `applyEntityTransform`).
2. Capture a Fixed plane at the midpoint, with `n` = the source's horizontal forward.
3. Set the drive to REFLECT (via `setInstanceDriveMode`, so groups, UI and status follow the normal
   path).

Result: the clone faces the source immediately (§4.6 facing check).

### 5.6 UI (`panel_ghost_studio.xml`, both hosts)

- Combo: add `<combo_box.item label="Reflect" name="drive_reflect" value="4" />`. Update the combo
  tooltip to explain Reflect.
- A Reflect row, enabled only when the selected clone's mode is Reflect:
  - `Plane:` combo (Fixed / Live)
  - `Mirror position` check
  - `Face pair` button, with a distance spinner bound to `GhostReflectFacePairDistance` and a ↺ reset
    button (the weather-panel idiom)
  - `Recapture plane` button
  - a status text line: `Reflect: 59 pairs, plane fixed, mirroring` / `holding: source missing` /
    `Mirror position unavailable: path motion owns placement`
- **Reflow:** the row goes inside `pose_scroll_body` (height 790). Shift every widget below it and
  raise `pose_scroll_body`'s height by the row height. Prove there's no overlap and nothing clipped **by
  rect arithmetic in both hosts** (the M1 lesson from the light rig: the content grew, but the host
  wrappers kept stale heights and the bottom controls became unreachable).
- Multi-select: like `onDriveModeCommit`, apply to every member of a selected group.

### 5.7 Settings

- Reuse `GhostTrueMirrorCopyFacing` and `GhostTrueMirrorCopyFace` for Reflect. They mean "stamp the
  source's facing" and "stamp the face". Update both settings' `Comment` text to say they also govern
  Reflect.
- New: `GhostReflectFacePairDistance` (F32, 2.0).
- New: `GhostReflectVerify` (Boolean, default true): the periodic verdicts in §5.10. The entry verdict
  always logs.
- Plane and Mirror position are per-instance state, not settings.

### 5.8 Chat

`/ghostanim reflect [all|selected]` sets REFLECT. Add it to both usage strings (`alchatcommand.cpp:785` and `:839`).

### 5.9 Pure header

`indra/newview/alreflectmath.h`, header-only and pure (no viewer includes; the precedent is
`alcineliveproberefresh.h` and `algazemath.h`). It contains:

- `mirrorJointName(const std::string&) -> std::string` (§4.4)
- `reflectLocalRotation(q)` and `reflectLocalPosition(v)` (§4.2), on plain F32 arrays or the llmath
  types the test target can include
- `planeFromBisector(...)` with the degenerate fallback (§4.6)
- `reflectPointAcrossPlane(...)` and `reflectRootRotation(...)` (§4.6)
- `reflectFaceParam(id, weight) -> (dst_id, dst_weight)` (§4.5)

### 5.10 Instrumentation — the log states a verdict

Tag `[GhostReflect]`. Three outcomes are always distinct: **PASS**, **FAIL**, and **INCONCLUSIVE**
(the measurement itself can't be trusted).

1. **Entry verdict** (always logged):
   `[GhostReflect] clone <id> entered Reflect: pairs 59/59, centre 41, unresolved 0 -> PASS`.
   Any unresolved partner gives FAIL, with the joint names listed.
2. **Foreign-writer check** (every 5 s when `GhostReflectVerify` is on): at the start of a stamp,
   compare the clone's joint rotations with the values written by the previous stamp. Under a stamp
   mode nothing else may write them.
   - Any difference > 1e-4 gives `FAIL: joint <name> changed between stamps (second writer)`.
   - None gives PASS.
   - If the previous stamp was skipped (source missing), the result is INCONCLUSIVE.
   - This is the check that catches a missed §5.2 site.
3. **World-space symmetry check** (every 5 s): this uses the engine's own world matrices, so it is
   **not** a re-read of the values the stamp wrote. For probe joints {`mWristLeft`, `mWristRight`,
   `mAnkleLeft`, `mAnkleRight`, `mHead`, `mPelvis`}, compute each joint's world position in its own
   avatar's root frame. Expect `clone(P(J)) == S · source(J)` within 1 cm × uniform scale.
   - Max error under the tolerance gives PASS.
   - Over the tolerance gives FAIL, with the worst joint and its error in mm.
   - A source not built, or a clone whose appearance snapshot differs from the source (shape mismatch
     makes the comparison meaningless), gives INCONCLUSIVE, with the reason.
   - The implementer must verify which frame joint world positions use on a **scaled** clone. The light
     rig's master-presets work found that joint world positions exclude the outer render scale (see
     `doc/CINE_LIGHT_RIG_STATUS.md` on `feature/cine-light-rig`; re-locate the code on this base).
     Normalise accordingly, and state the choice in a comment.

### 5.11 TUT tests (`indra/newview/tests/alreflectmath_test.cpp`, registered like `algazemath_test`)

Each test must fail if the thing it tests is broken. Use literals, not the header's own symbols, for
expected values.

- **T1 Pairing:** an embedded literal list of all 159 skeleton names gives 59 pairs + 41 self. It's an
  involution (`P(P(J)) == J`), every partner is in the list, and there are spot literals (`mWristLeft`
  ↔ `mWristRight`, `L_CLAVICLE` ↔ `R_CLAVICLE`, `LEFT_PEC` ↔ `RIGHT_PEC`, `mFaceEyeAltLeft` ↔
  `mFaceEyeAltRight`, `mSpine1` ↔ `mSpine1`).
- **T2 The §4.1 discriminator:** +90° about Z applied to (0,−1,0) gives (1,0,0). The reflected rotation
  applied to (0,1,0) gives (1,0,0), and the unreflected one gives (−1,0,0). This must fail for a
  swap-only implementation.
- **T3 Behavioural reflection:** for ≥ 20 deterministic pseudo-random unit quaternions and vectors,
  `S·(q·v) == q'·(S·v)` within 1e-5. Cover all three axes, so a rule that negates the wrong component
  pair fails.
- **T4 Homomorphism:** `reflect(q₁q₂) == reflect(q₁)·reflect(q₂)`, and `reflect(reflect(q)) == q`
  exactly.
- **T5 Root and plane:** behavioural `world_clone(S·u) == M·world_source(u)` for several plane azimuths
  (0°, 37°, 90°, 179°, −135°) and source yaws, including ±180° wraps. The upright yaw formula agrees.
  The result has det +1 (orthonormal, finite).
- **T6 Facing check:** a source facing along `n` gives a clone facing −`n`.
- **T7 Bisector:** normal case; coincident within 0.1 m gives the forward fallback; NaN input gives the
  last valid plane, never NaN.
- **T8 Face params:** 58 ↔ 59 swap; 1206/1207 negate (values −0.7 and 1.3 as literals); other ids pass
  through unchanged.

---

## 6. Known limitations (stated, not hidden)

- **L1 — The geometry isn't mirrored.** A hair parting, a single strap, a tattoo or text stays on its
  side. That's correct for dance partners; a looking-glass mirror is the deferred geometry feature.
- **L2 — Asymmetric joint-position overrides get swapped.** Positions are reflected from the source's
  final local positions, which include mesh joint-position overrides. For a symmetric body this is
  exact. For a mesh whose left and right overrides differ, the left value lands on the right joint and
  the mesh may distort slightly. A future option is to reflect only the animated delta, which would
  need a rest-position reference LLJoint doesn't keep. Watch for this in-world.
- **L3 — One-sided expressions don't flip.** `Express_*` morphs have no mirrored twin.
- **L4 — Held props stay on the same hand.** Unrigged attachments ride their own joint, so a sword in
  the right hand follows the hand now doing the source's left-hand motion (open decision D3).
- **L5 — Animesh worn by the clone plays unmirrored.** They follow the ObjectAnimation ledger, exactly
  as under True Mirror.
- **L6 — Gaze at the camera doesn't survive.** The clone looks at the camera's mirror image. Mutual
  gaze works for free: if the source looks at the clone, the reflected target is the source.
- **L7 — Uneven ground.** Mirrored foot plants can float or sink. The clone's Pose Polish foot IK is
  paused under stamp modes.
- **L8 — Cost** is the same as True Mirror (a per-joint copy per clone per frame) plus a precomputed
  table lookup. It isn't meant for crowds; use Mirror for crowds.

---

## 7. Review focus for the adversarial reviewer (least sure first)

1. **Missed gate sites.** Re-grep `DRIVE_TRUE_MIRROR`, `isTrueMirrorDriven`, `trueMirrorOwnsFacing`
   and `mDriveMode` across `indra/newview`. Any site not classified in §5.2 is a must-fix. A missed
   writer is silent.
2. **The clamp** at `llghostavatar.cpp:1470`.
3. **TRUE_MIRROR byte-identity** after the stamp is generalised. Prove it by diff.
4. **Root rotation in LL's convention.** Do T5's behavioural assertions actually pin `M·R·S`, or would
   `S·R·M` or `M·R` also pass?
5. **Mirror position ownership.** Does any Studio path recompute `mFootGlobal` (formations, crowd
   placement, lock groups, motion) and fight the post-pass? Is there really no snap or double slam?
6. **The Live plane under motion.** A source walking straight through the clone's position: degenerate
   handling, no NaN, no 180° facing flip-flop at the crossing. If there is a flip, it must be stated.
7. **Freeze/resume and the Director switcher re-drive.** Does REFLECT round-trip with its plane state?
   Does the ghost-own-mode self-gate hold?
8. **The UI reflow** in both hosts, by rect arithmetic.
9. **The tests are real.** Would each fail if the code under test were broken? (Precedent: the light
   rig had a test that locked in a bug through several rounds.)

---

## 8. In-world test — one discriminating test, outcomes stated in advance

Setup: entity clone of yourself. Select it, click **Face pair** (2 m), and play any animation that
raises your **right** arm forward.

- **PASS:** the clone raises its **left** arm toward you, as a mirror image. The log shows
  `entered Reflect … PASS` and periodic `symmetry … PASS`.
- **FAIL-A** (names swapped, rotation not reflected): the clone's left arm swings **backward**.
- **FAIL-B** (rotation reflected, names not swapped): the clone's right arm moves, and wrongly.
- **FAIL-C** (clamp missed): the clone raises its **right** arm, and the combo still reads Reflect.
- **INCONCLUSIVE:** the log says why (source not built, shape mismatch).

Then:

- Turn 45° to your left: the clone turns 45° to its right.
- With Mirror position on, step toward the clone: it steps toward you.
- Switch the clone to **True Mirror**: it behaves exactly as before this delivery. That's the
  regression check.

---

## 9. Off-limits for the implementer

- `pipeline.cpp`, `pipeline.h`, any shader, any render target or GL state (none is needed; stop and
  report if you think otherwise).
- `fsposeranimator.*` and `fsjointpose.*`: read-only reference. Do not link the poser into the ghost.
- `indra/newview/character/*.xml`: the skeleton and visual-param definitions are inputs, not things
  to edit.
- The TRUE_MIRROR code path's behaviour (§5.3).
- Never build. Claude builds after review convergence.

---

## 10. Open decisions (defaults chosen; change before handing to Codex)

- **D1 — Mirror position default:** OFF (the clone stays where it was placed; only pose and facing
  mirror). Alternative: ON after Face pair.
- **D2 — Plane mode default:** Fixed. Alternative: Live, so pairs keep facing each other as the source
  walks around.
- **D3 — Held props:** deferred (L4). Pull it in if prop-heavy scenes (swords, cups, phones) are the
  main use. It becomes the riskiest part of the delivery, because it touches clone attachments.
- **D4 — Settings reuse:** reuse the two `GhostTrueMirror*` keys (fewer controls). Alternative:
  separate `GhostReflectCopyFacing`/`CopyFace` keys, so the two modes can be tuned independently.

# Real-Time Chirality Mirroring of a Clone's Pose — Deep-Research & Design

**A grounded design for flipping a Ghost Studio clone's live animation left-to-right, every frame.**

Status: written 2026-08-30 against the working tree at HEAD `b69eb6af71a` (branch
`feature/ultimate-diopter`). Line numbers are anchors, not addresses — the tree is in flux, so
they are marked `~` where the surrounding code is likely to drift. Every architectural claim below
was read out of the source; where I could not verify something from code it is called out as an
**assumption to confirm**, not asserted.

---

## 0. Executive summary

Ghost Studio spawns client-only duplicate avatars ("clones"/"ghosts", class `LLGhostAvatar`) and can
drive each one to reproduce a real avatar's live animation. This paper designs a **new per-clone
option that plays that pose flipped left-to-right in real time** — a right-handed wave becomes
left-handed, a lean-left becomes lean-right — continuously, every frame, on a live animating source.

The whole feature reduces to **one operator applied once per frame, after the motion controller poses
the clone's own skeleton**:

1. **Reflect** each joint's local rotation across the sagittal plane: `q = (w,x,y,z) → (w, −x, y, −z)`
   (negate `mQ[VX]` and `mQ[VZ]`; keep `mQ[VY]`, `mQ[VW]`). Derived in §4; **already shipping** in this
   repo's poser at `fsjointpose.h:303-309` and `fsposeranimator.cpp:536`.
2. **Swap** the reflected rotation onto the joint's left/right twin (`mShoulderLeft ↔ mShoulderRight`,
   the full Bento table in §4.4). **Midline** joints (`mPelvis`, spine, `mChest`, `mNeck`, `mHead`,
   tail…) reflect in place with no swap.
3. **Negate the Y component** of any *animated* joint translation (pelvis sway, hip shift), §4.5.

The clone's **placement/facing is untouched** — mirroring happens inside the clone's own root frame,
so it flips about the clone's own sagittal plane, in place (§4.6). Insertion point is a new virtual
`LLVOAvatar::applyPoseMirror()` called from `LLVOAvatar::updateCharacter()` immediately after
`updateMotions()` (`llvoavatar.cpp:~5722`), a sibling of the existing Pose-Polish and gaze post-motion
stages (§5).

### 0.1 ⚠ NAMING-COLLISION WARNING (read this first)

There is **already** a drive mode literally named `DRIVE_MIRROR` in this codebase, and **it does NOT
mean left/right flip.** Confirmed at `alghoststudio.h:101`:

```cpp
enum EDriveMode : S32 { DRIVE_MIRROR = 0, DRIVE_DIRECTED, DRIVE_FROZEN };
```

and its own header comment (`alghoststudio.h:96-100`) defines it as *"MIRROR = copy the source's live
animation state (default; the current behaviour)."* In this codebase **"mirror" means "the clone
mirrors (reflects/echoes) whatever its source is doing"** — a *copy*, not a chirality flip. The UI
label is likewise "Mirror" (`panel_ghost_studio.xml:218`), the runtime bridge is
`LLGhostAvatar::setEntityDriveMode` / `mEntityDriveMode` (`llghostavatar.cpp:568`,
`llghostavatar.h:193`), and the per-frame copy lives in `LLGhostAvatar::idleUpdate`
(`llghostavatar.cpp:2026-2051`).

**Do not reuse the word "Mirror" bare for this feature.** This document names the new concept
**chirality flip**:

| Concept | Name in this design | Existing collision to avoid |
|---|---|---|
| New L/R flip flag (Instance) | **`mFlipChirality`** (bool) | not `mDriveMode`, not any `*Mirror*` |
| New L/R flip flag (runtime avatar) | **`mEntityFlipChirality`** (bool) | not `mEntityDriveMode` |
| Studio setter | **`setInstanceFlipChirality`** | not `setInstanceDriveMode` |
| Runtime setter | **`setEntityFlipChirality`** | not `setEntityDriveMode` |
| Per-frame pass | **`applyPoseMirror()` / `applyChiralityFlip()`** | — |
| UI label | **"Mirror Pose (L/R)"** or **"Flip L/R"** | not the "Mirror" drive-mode item |

The feature is a **boolean modifier orthogonal to `EDriveMode`**, not a fourth drive mode. That is a
deliberate choice (§5.2): it composes with `DRIVE_MIRROR` (flip a live-copied pose) *and* with
`DRIVE_DIRECTED` (flip a locally-played asset), and it leaves `DRIVE_FROZEN` a flipped static pose.

---

## 1. Mental model

A Ghost Studio clone has two possible backings (see
`doc/GHOST_STUDIO_TRANSFORM_ARCHITECTURE_DEEP_RESEARCH.md` §1):

- **Overlay** (`BACKING_OVERLAY`) — a per-frame re-draw of the *source's already-skinned* rigged
  batches. It has **no skeleton of its own**; the vertices are already posed.
- **Entity** (`BACKING_ENTITY_CLONE`) — a genuine `LLVOAvatar` subclass (`LLGhostAvatar`) with a
  **cloned skeleton + attachments**, animated by its **own motion controller**.

The `EDriveMode` enum, `mAnimSpeed`, physics, loop mode, gaze — every per-clone *animation* control —
are **entity-only** and explicitly "Ignored for overlay instances" (`alghoststudio.h:100`). **The
chirality flip lives in the same family and is entity-only for the same reason** (§6.1): flipping an
overlay would mean reflecting an already-skinned vertex stream in the draw modelview, a different and
much messier mechanism (§6.1 has the verdict).

The key structural fact that makes this feature cheap: **a `DRIVE_MIRROR` clone does not receive
per-joint transforms from its source.** It copies the *set of animation asset UUIDs* the source is
playing and re-plays those **same assets on its own motion controller**
(`llghostavatar.cpp:2036-2051`, `synchronizeCloneAnimations` at `llghostavatar.cpp:~545`). So by the
time we want to flip, the clone has *already posed its own skeleton locally*. We flip **the clone's own
posed skeleton**, in place, after `updateMotions()` — we never touch the source, and we never need the
source's joints at all.

---

## 2. Problem statement & scope

**In scope.** A per-entity-clone toggle that, every rendered frame, reflects the clone's animation
pose across its sagittal (front-to-back vertical) plane: swap left/right limb motion, negate lateral
lean/twist, keep forward/back nod and up/down unchanged. Works on a live, continuously animating
source; is a selectable option alongside `DRIVE_MIRROR`/`DRIVE_DIRECTED`/`DRIVE_FROZEN`; stable to
toggle live; negligible cost.

**Explicitly NOT in scope — state honestly up front:**

- **Chiral meshes/textures do not flip.** We mirror the *animation* (bone rotations), applied onto the
  clone's own rest skeleton. We do **not** mirror the clone's geometry, UVs, or textures. If the
  source wears a jersey reading "SL", a parted hairstyle, a single glove, one shoulder tattoo, or an
  eyepatch, the clone's flipped pose will still show **un-flipped** text on the (now) other hand — the
  logo reads forwards, the parting stays on its modeled side. This is a fundamental limitation of
  pose-mirroring and cannot be fixed without reflecting the mesh (which breaks UVs and winding). We do
  not attempt it. (§6.3.)
- **Morph-based facial animation** (visemes, expressions driven by morph targets, not bones) cannot be
  bone-mirrored (§6.7). Asymmetric *bone*-based face motion (Bento face bones) **is** flipped.
- **Overlay clones** (§6.1): out of scope for v1; would need a separate reflect-matrix draw path.
- **The bind pose / custom asymmetric skeleton is not our job to reflect** (§6.2). We flip the pose
  *deltas the animation produces*, on top of whatever (possibly asymmetric) rest skeleton the clone
  already has. A clone built on an asymmetric avatar keeps its own asymmetry and merely performs the
  mirrored motion.

---

## 3. Current architecture (the pieces we compose with)

### 3.1 Drive modes and the entity bridge

| Symbol | Location | Role |
|---|---|---|
| `enum EDriveMode {DRIVE_MIRROR, DRIVE_DIRECTED, DRIVE_FROZEN}` | `alghoststudio.h:101` | the three existing modes; **`DRIVE_MIRROR` = copy source, not L/R flip** |
| `Instance::mDriveMode` (+ `mDirectedAnim`, `mAnimSpeed`, `mLoopMode`) | `alghoststudio.h:356-360` | per-clone authored drive state |
| `ALGhostStudio::setInstanceDriveMode` | `alghoststudio.cpp:2116-2137` | studio → runtime bridge; resolves `LLGhostAvatar`, calls `setEntityDriveMode` |
| `LLGhostAvatar::setEntityDriveMode` | `llghostavatar.cpp:568-620` | applies the mode to the clone's controller |
| `LLGhostAvatar::mEntityDriveMode` | `llghostavatar.h:193` | runtime copy (S32 to avoid a header cycle) |
| `ALPanelGhostStudio::onDriveModeCommit` | `alpanelghoststudio.cpp:2094-2116` | UI commit; fans out to every member of a selected group |
| `drive_mode_combo` | `panel_ghost_studio.xml:216-221` | the "Mirror / Directed / Frozen" combo |

This four-layer path — **XML control → panel commit → `setInstance*` studio bridge → `setEntity*`
runtime** — is the exact pattern the new flag will follow (§5.3). `setInstanceDriveMode` is a good
template: it validates `mKind == BACKING_ENTITY_CLONE`, resolves the live `LLGhostAvatar` through
`gObjectList`, and returns false for overlays (`alghoststudio.cpp:2120-2131`).

### 3.2 How a `DRIVE_MIRROR` clone actually poses itself

`LLGhostAvatar::idleUpdate` (`llghostavatar.cpp:2009`) runs *before* the base character update:

```cpp
if (mEntityDriveMode == ALGhostStudio::DRIVE_MIRROR) {
    LLVOAvatar* source = ...findObject(mAnimationSourceId)...;
    signaled_animation_map_t mirrored = source->mSignaledAnimations;   // 2036
    mirrored.erase(ANIM_AGENT_SIT_GROUND); ...
    if (... mCloneDesiredAnimations != mirrored)
        synchronizeCloneAnimations(mirrored);                          // 2050
}
```

`synchronizeCloneAnimations` (`llghostavatar.cpp:~545-566`) starts/stops motions on the **clone's own**
`LLCharacter` controller so its playing set matches the source's. **It copies animation UUIDs, never
joint transforms.** Then the base `LLVOAvatar::idleUpdate` → `updateCharacter` runs `updateMotions()`,
which blends those motions and writes every joint's local rotation on the clone's own skeleton. That
is the pose we will flip.

`DRIVE_DIRECTED` plays one local asset (`llghostavatar.cpp:2054-2084`); `DRIVE_FROZEN` holds via a
pause request (`llghostavatar.cpp:606-618`). In **all three cases the clone's skeleton is posed by its
own controller** — so a single post-`updateMotions` flip works identically for every drive mode.

### 3.3 The post-motion pipeline in `updateCharacter` — our insertion neighborhood

`LLVOAvatar::updateCharacter` (`llvoavatar.cpp`) runs, in order:

```cpp
updateRootPositionAndRotation(agent, speed, ...);   // ~5701  set mRoot world pos/rot (placement/facing)
...
updateMotions(LLCharacter::NORMAL_UPDATE);          // ~5721  MOTION CONTROLLER POSES THE SKELETON
mPosePolish.run(this, gFrameIntervalSeconds…);      // ~5728  [Machinima] continuity/grounding stage
if (!actor_mover.applyDirectorLookAt(this))         // ~5733  Director camera-facing look-at, OR
    actor_mover.applyGaze(this);                    // ~5735  procedural gaze (head/neck/eyes)
```

Two existing stages already establish the **"re-pose selected joints after `updateMotions`, every
frame"** pattern:

- **Pose Polish** (`mPosePolish.run`, `llvoavatar.cpp:5724-5728`) — post-blend transition/grounding
  repair, gated by `ALPolishEnabled`.
- **Gaze / Director look-at** (`applyGaze`/`applyDirectorLookAt`, `llvoavatar.cpp:5730-5736`). The
  header comment at `llactormover.cpp:5134-5142` states the contract explicitly: *"applyGaze() is
  called from LLVOAvatar::updateCharacter AFTER updateMotions() has posed the skeleton, so the override
  layers on the current anim pose … We never permanently corrupt the skeleton — the motion controller
  rebuilds it next frame."* This is precisely the lifecycle our flip needs.

The clone (`LLGhostAvatar`) overrides `idleUpdate` (`llghostavatar.h:108`) but **not**
`updateCharacter`, so the base `updateCharacter` — including all three post-motion stages — runs for
the clone unchanged. Our flip slots in as a fourth sibling (§5.1).

### 3.4 The skeleton — hierarchy, naming, coordinate convention

`avatar_skeleton.xml` (`num_bones="133" num_collision_volumes="26"`, referenced from
`avatar_lad.xml:7`). The joint API we use is on `LLJoint` (`lljoint.h`):

```cpp
const LLQuaternion& getRotation();  void setRotation(const LLQuaternion&);   // 249-250
const LLVector3&    getPosition();  void setPosition(const LLVector3&, ...); // 232-233
const std::string&  getName() const;                                        // 201
```

and the avatar exposes its animated joints (`llvoavatar.h:207-213`): `getJoint(name)`,
`getJoint(num)`, `getSkeletonJoint(num)`, `getSkeletonJointCount()`.

**Coordinate convention, read straight out of the skeleton rest offsets** (`pos="X Y Z"`):

| Joint | `pos` (X, Y, Z) | Reading |
|---|---|---|
| `mCollarLeft` | `-0.021  **+0.085**  0.165` | left side is **+Y** (`avatar_skeleton.xml:81`) |
| `mCollarRight` | `-0.021  **−0.085**  0.165` | right side is **−Y** (`:118`) |
| `mHipLeft` / `mHipRight` | `+0.127` / `−0.129` in Y | left **+Y**, right **−Y** (`:191`/`:179`) |
| `mEyeLeft` / `mEyeRight` | `+0.036` / `−0.036` in Y | left **+Y**, right **−Y** (`:25`/`:24`) |
| `mPelvis`, spine, `mChest`, `mNeck`, `mHead`, `mSkull`, tail, groin | `Y = 0` | **midline** (`:2-23,203-214`) |

So SL avatar space is **X = forward, Y = the avatar's LEFT, Z = up** (right-handed). The **sagittal
plane** (the plane that separates left from right) is the **X-Z plane**; the **left-right (lateral)
axis is Y**. A left-right mirror is a **reflection across the X-Z plane = negation of Y**.

**Every bone in the file has `rot="0.000 0.000 0.000"`.** This is load-bearing (§4.3): at rest, every
joint's local frame is *parallel to the root frame* (X-fwd / Y-left / Z-up). That is why a single
reflection formula, in the joint's own local quaternion, is correct for *every* joint uniformly —
we do not need per-joint axis remapping.

The L/R pairs are name-derivable by the substring **`Left` ↔ `Right`**, but three things make a
hand-maintained table safer than blind string surgery: (a) `mHandThumb1Left` etc. embed the side
mid-name, (b) some bones are midline with no twin, and (c) `mFaceForeheadCenter`/`mFaceNoseCenter`/…
are center bones sitting between Left/Right neighbours. This repo already ships exactly such a table —
see §4.4.

### 3.5 The precedent already in the tree: the Firestorm Poser

This fork ships the Firestorm Poser (`fsposeranimator.{h,cpp}`, `fsjointpose.h`,
`fsfloaterposer.cpp`). **It already contains a complete, tested L/R mirror implementation** — the same
math, the same joint-pair table — used for its "mirror this joint to its opposite" and "flip entire
pose" pose-editing features. Our design **reuses this**, lifted from a one-shot editor into a
per-frame pass. The relevant pieces:

| Poser symbol | Location | What it is |
|---|---|---|
| the mirror operator on a raw joint quat | `fsjointpose.h:303-309` (`reflectRotation`) | `mQ[VX]*=−1; mQ[VZ]*=−1;` — **exactly our operator** |
| operator on an applied delta | `fsposeranimator.cpp:536`, `:920` | `LLQuaternion(-q.mQ[VX], q.mQ[VY], -q.mQ[VZ], q.mQ[VW])` |
| reflect-one-joint + swap twin | `fsposeranimator.cpp:954-978` (`reflectJoint`) | reflect self, reflect twin, `swapRotationWith` |
| whole-body flip | `fsposeranimator.cpp:1019-1043` (`flipEntirePose`) | iterate table, skip `dontFlipOnMirror`, `reflectJoint` each |
| the L/R pair table | `fsposeranimator.h:219+` (`PoserJoints`) | 133-bone table with `mMirrorJointName` + `dontFlipOnMirror` |
| pair swap / mirror-from | `fsjointpose.h:166`, `:181` (`swapRotationWith`, `mirrorRotationFrom`) | exchange rotations between twins |

The one difference we must handle: the poser mirrors its **stored `FSJointPose`** (a base+delta pose
snapshot); we mirror the **live composed `LLJoint::getRotation()`** the motion controller just wrote,
every frame. The math is identical; the plumbing differs (§5).

---

## 4. The mirror operator — full derivation against SL's real convention

### 4.1 What "mirror across the sagittal plane" is, formally

The lateral axis is Y (§3.4). The reflection that flips left↔right is the improper orthogonal matrix

```
M = diag(1, −1, 1)          (negate Y; det M = −1; M = Mᵀ = M⁻¹)
```

A joint's animation output is a **local rotation** `R` (a proper rotation, quaternion `q`). Reflecting
the *whole posed body* across the plane and pushing the reflection down the (bilaterally symmetric)
chain turns each joint's local rotation into its **conjugate under M**:

```
R' = M · R · M⁻¹ = M · R · M          (proper, since det = (−1)(+1)(−1) = +1)
```

### 4.2 The quaternion algebra (worked, not hand-waved)

Let `q = (w, x, y, z)` produce rotation matrix `R`. With `M = diag(1,−1,1)`, `R' = M R M` negates
exactly those entries with **one** index equal to Y:

```
        ⎡ 1−2(y²+z²)    −2(xy−wz)     2(xz+wy) ⎤
R' = MRM = ⎢ −2(xy+wz)    1−2(x²+z²)    −2(yz−wx) ⎥
        ⎣  2(xz−wy)    −2(yz+wx)     1−2(x²+y²) ⎦
```

Solve for the quaternion `q' = (w',x',y',z')` whose matrix is `R'`. Matching the diagonal gives
`x'²=x², y'²=y², z'²=z², w'²=w²` (all magnitudes preserved — a pure sign problem). Matching the
off-diagonals gives six sign constraints:

```
w'z' = −wz ,  w'y' = +wy ,  w'x' = −wx ,  x'y' = −xy ,  x'z' = +xz ,  y'z' = −yz
```

Taking `w' = +w`, these are solved uniquely (up to the global q ≡ −q double cover) by

```
   ┌─────────────────────────────────────────────┐
   │   q'  =  ( w , −x , y , −z )                 │
   │   negate the X and Z parts; keep W and Y     │
   └─────────────────────────────────────────────┘
```

**Sanity checks against real motions:**

| Rotation | `q` | Mirror `q'` | Physical meaning | Correct? |
|---|---|---|---|---|
| Yaw (turn head left, about Z) | `(c,0,0,s)` | `(c,0,0,−s)` | mirror turns the other way | ✔ negated |
| Roll (tilt toward shoulder, about X) | `(c,s,0,0)` | `(c,−s,0,0)` | tilt-right ↔ tilt-left | ✔ negated |
| Pitch (nod forward, about Y) | `(c,0,s,0)` | `(c,0,s,0)` | a nod looks identical mirrored | ✔ unchanged |

Exactly right: a left-right mirror negates yaw and roll but leaves pitch alone.

### 4.3 Why one formula works for all 133 joints (the crux)

The formula `q'=(w,−x,y,−z)` operates in the joint's **own local frame**. It is correct for *every*
joint — not just the root — precisely because **every rest bone has `rot="0 0 0"`
(`avatar_skeleton.xml`, all bones)**, so each joint's local axes are parallel to the root's
X-fwd/Y-left/Z-up basis. Equivalently, the rest skeleton is bilaterally symmetric under M:
`restframe(twin(J)) = M · restframe(J) · M`. Under that symmetry, conjugating a joint's local
rotation by M — which in a frame parallel to the root is the component operation `(w,−x,y,−z)` — is the
correct reflection at *every* joint. (Note the bone *direction* — e.g. `mShoulderLeft`'s `end` points
down the +Y arm — is irrelevant; SL joint local frames are the root basis, not the bone-aligned frame.
This is why we negate X and Z, not "the two axes perpendicular to the bone".)

**This is not a derivation done in a vacuum — the repo's poser agrees byte-for-byte:**
`fsjointpose.h:303-309` mirrors a stored joint rotation as `mQ[VX]*=−1; mQ[VZ]*=−1;` and
`fsposeranimator.cpp:536` mirrors an applied rotation as
`LLQuaternion(-q.mQ[VX], q.mQ[VY], -q.mQ[VZ], q.mQ[VW])`. Since `LLQuaternion::mQ = {x,y,z,w}`
(`llquaternion.h:53`), those are `(w,−x,y,−z)` — identical to §4.2.

### 4.4 The left/right pair table and the midline set

Reflect-in-place is only half the job: after reflecting, a **left** joint's rotation must be **written
to the right** joint and vice versa. The pairing is the `PoserJoints` table (`fsposeranimator.h:219+`),
which our pass should share or mirror. Its shape per row is
`{ jointName, mirrorJointName, boneType, {children/CVs}, restPos, [restScale], [dontFlipOnMirror] }`.

**Swapped pairs (reflect both, then exchange):**

| Region | Left ↔ Right pairs |
|---|---|
| Shoulder girdle / arms | `mCollarLeft↔mCollarRight`, `mShoulderLeft↔mShoulderRight`, `mElbowLeft↔mElbowRight`, `mWristLeft↔mWristRight` (`fsposeranimator.h:226-233`) |
| Legs / feet | `mHipLeft↔mHipRight`, `mKneeLeft↔mKneeRight`, `mAnkleLeft↔mAnkleRight`, `mFootLeft↔mFootRight`, `mToeLeft↔mToeRight` (`:234-243`) |
| Bento hands (×5 fingers ×3 phalanges, ×2 hands) | `mHand{Thumb,Index,Middle,Ring,Pinky}{1,2,3}Left ↔ …Right` (`:311+`, both-hand rows) |
| Eyes | `mEyeLeft↔mEyeRight` (`:272-273`) |
| Bento face | forehead/eyebrow (outer/center/inner)/eyelid (upper/lower)/eyecorner/ear (1,2)/nose (L/R)/cheek (upper/lower)/lip (upper/lower/corner) Left↔Right (`:262-306`) |
| Bento wings | `mWing{1,2,3,4}Left ↔ …Right`, `mWing4FanLeft↔…Right` (`avatar_skeleton.xml:155-172`) — **not in `PoserJoints`; see note** |
| Bento hind limbs | `mHindLimb{1,2,3,4}Left ↔ …Right` (`avatar_skeleton.xml:216-229`) — **not in `PoserJoints`; see note** |

**Midline joints (reflect in place, no swap; `mirrorJointName == ""`):**
`mPelvis` (WHOLEAVATAR), `mTorso`, `mChest`, `mNeck`, `mHead`, `mSkull`, `mSpine1..4` (all Y=0),
`mGroin`, `mTail1..6`, `mFaceRoot`, `mFaceForeheadCenter`, `mFaceNoseCenter`, `mFaceNoseBase`,
`mFaceNoseBridge`, `mFaceJaw`, `mFaceChin`, `mFaceTeethUpper`, `mFaceTeethLower`,
`mFaceLipUpperCenter`, `mFaceLipLowerCenter`, `mFaceTongueBase`, `mFaceTongueTip`, `mFaceJawShaper`,
`mWingsRoot`, `mHindLimbsRoot` (`fsposeranimator.h` midline rows; `avatar_skeleton.xml` Y=0 bones).

The `dontFlipOnMirror==true` flag on **right-side rows** (`fsposeranimator.h:230-243,264-306` etc.)
is a **de-dup marker**: `flipEntirePose` iterates the whole table but skips those rows so each pair is
processed exactly once, from the left row, whose `reflectJoint` reflects+swaps both
(`fsposeranimator.cpp:1024-1042`). Our per-frame pass follows the same iterate-and-skip discipline.

> **Wings & hind-limbs note (assumption to confirm).** The Bento wing and hind-limb bones exist in
> `avatar_skeleton.xml:155-172,216-229` with clean `Left/Right` names and mirror-image Y offsets, but
> they are **absent from the poser's `PoserJoints` table** (which stops at the standard body+face+hands
> set). They are name-derivable, so the new pass should **extend the table** with these pairs
> (`mWing{1..4}Left↔Right`, `mWing4FanLeft↔Right`, `mHindLimb{1..4}Left↔Right`) plus their midline
> roots — otherwise a winged or digitigrade avatar's wings/hind legs will not mirror. Verify none are
> already covered by an alias before adding.

### 4.5 Translations (joint positions)

Most SL animations are rotation-only, but **`LLKeyframeMotion` does animate joint positions** —
confirmed at `llkeyframemotion.cpp:439` (`joint_state->setPosition(mPositionCurve.getValue(...))`) and
the pelvis-offset path at `:1878`. A position curve is almost always on `mPelvis` (a sideways sway or
lean shifts the root laterally). To mirror that, negate the **Y** component of the animated local
translation:

```
p = (px, py, pz)  →  p' = (px, −py, pz)          // negate the lateral component
```

applied via `joint->setPosition({px, −py, pz})` for any joint whose position the motion touched. For a
midline joint this makes a sway-left become a sway-right; for a swapped pair, the position travels with
the rotation swap. **P0 may ship rotation-only** (correct for the overwhelming majority of anims) and
add position mirroring in P1 (§8) — a lean that *translates* the pelvis is the only visible gap, and it
is rare.

### 4.6 The root, facing, and "flip in place"

The clone's **world placement and facing are not part of the animation** and must **not** be flipped:

- Placement/orientation live in `Instance::mFootGlobal` + `Instance::mRotation`
  (`alghoststudio.h:372-377`) and are written to `mRoot` by `updateRootPositionAndRotation`
  (`llvoavatar.cpp:5701`) / the studio's outer transform — *before* `updateMotions`. The animation
  poses joints **relative to** that already-placed root.
- Our pass runs **after** `updateMotions` and touches only **joint-local** rotations/positions below
  the root. Therefore the flip happens **entirely inside the clone's own root frame**, i.e. it mirrors
  about **the clone's own sagittal plane, in place** — exactly the behaviour asked for. The clone keeps
  standing where it stands and facing where it faces; only its pose flips. A right-hand wave becomes a
  left-hand wave **without the clone turning around** or teleporting.
- Any **yaw/roll the animation itself applies to `mPelvis`** (hip sway, a turning root motion) is a
  joint-local rotation and *is* reflected in place (negate x,z) — correct, because that lateral
  twist is part of the performance, not the placement. The placement `mRotation` is untouched.

This cleanly answers the root question in the brief: **mirror about the clone's own root, never the
world origin; do not touch the placement rotation.**

---

## 5. Integration design

### 5.1 Insertion point — a fourth post-motion stage

Add a **virtual no-op on `LLVOAvatar`**, overridden by `LLGhostAvatar`, and call it from
`LLVOAvatar::updateCharacter` immediately after `updateMotions()` and **before** Pose Polish and gaze:

```cpp
// llvoavatar.h
virtual void applyPoseMirror() {}                 // base: real avatars pay nothing

// llvoavatar.cpp, updateCharacter, right after updateMotions(...) (~5722):
    updateMotions(LLCharacter::NORMAL_UPDATE);     // ~5721
    applyPoseMirror();                             // NEW — flip the freshly-posed skeleton
    mPosePolish.run(this, gFrameIntervalSeconds.value());   // ~5728 grounds the MIRRORED pose
    if (!actor_mover.applyDirectorLookAt(this))    // ~5733 gaze aims at REAL targets on the mirrored body
        actor_mover.applyGaze(this);               // ~5735
```

`LLGhostAvatar::applyPoseMirror()` early-outs unless `mEntityFlipChirality` is set, then runs the
reflect+swap pass (§5.4). Real avatars, animesh, and non-flipped clones pay a single virtual dispatch
and an immediate return.

**Why here, and why this order:**

- **After `updateMotions`** — the skeleton is fully posed; `getRotation()` returns the live anim pose
  (same precondition the gaze layer relies on, `llactormover.cpp:5134-5142`). The motion controller
  rebuilds the skeleton next frame, so nothing is permanently corrupted — we re-flip every frame.
- **Before Pose Polish** — grounding/contact stabilization then operates on the *mirrored* pose, so
  the planted foot is the correct (swapped) one. (Polishing first, then flipping, would ground the
  un-mirrored foot and then swap it — subtly wrong on uneven contact.)
- **Before gaze / look-at** — gaze aims head/neck/eyes at **world-space** targets (camera, cast member;
  `llactormover.cpp` gaze targets). Those must **not** be mirrored, or the clone would look the wrong
  way. Running gaze *after* the flip means gaze **overwrites** the mirrored eye/head rotations with a
  correctly-aimed pose. Net: a clone with gaze on tracks the true target (eyes not mirrored); a clone
  with gaze off mirrors its eyes with the animation. Both are the desired behaviour (§6.6).

**Alternative considered:** route through `LLActorMover` like gaze (a mirror registry keyed by avatar
id). Rejected: chirality is per-Instance Ghost Studio state that already has a clean home on
`LLGhostAvatar`; a virtual method keeps the state and the pass together and avoids a per-frame map
lookup for every avatar.

### 5.2 Flag, not a fourth mode

`mFlipChirality` is a **boolean orthogonal to `EDriveMode`**, because it is meaningful in combination
with all three:

- `DRIVE_MIRROR` + flip → the clone live-copies the source and plays it mirrored (the headline case).
- `DRIVE_DIRECTED` + flip → a locally-played asset, mirrored (mirror any library animation).
- `DRIVE_FROZEN` + flip → holds a mirrored static pose (freeze, then flip, or flip, then freeze — both
  land on the same held mirrored pose since the flip re-asserts each frame regardless).

A fourth enum value would force mutual exclusivity with `DRIVE_DIRECTED` and duplicate the directed/loop
plumbing. The boolean avoids `EDriveMode` renumbering (persisted values stay stable) and is strictly
more expressive.

### 5.3 The four-layer plumbing (mirror the drive-mode path exactly)

```
Instance::mFlipChirality (bool)                         // alghoststudio.h, near :356
    ↑ persisted authored state; default false
ALGhostStudio::setInstanceFlipChirality(id, bool)       // new, model on setInstanceDriveMode :2116
    · validate mKind == BACKING_ENTITY_CLONE (overlay → return false)
    · resolve LLGhostAvatar via gObjectList, guard isDead()
    · inst->mFlipChirality = on;  ghost->setEntityFlipChirality(on);
LLGhostAvatar::setEntityFlipChirality(bool)             // new, model on setEntityDriveMode :568
    · mEntityFlipChirality = on;   (that's all — the pass reads it each frame)
LLGhostAvatar::mEntityFlipChirality (bool)              // llghostavatar.h, near :193
```

Re-apply on spawn/refresh alongside the existing drive state: `ALGhostStudio::applyEntityRuntimeState`
already re-pushes `mDriveMode` (`alghoststudio.cpp:401`), so add the `mFlipChirality` push there so a
refreshed/recovered clone keeps its flip. The `onEntityRuntimeReplaced` migration path
(`llactormover.h:602-630` for the walk state; the studio's equivalent) carries `Instance` fields
wholesale, so a persisted bool rides along for free.

### 5.4 The per-frame pass (`LLGhostAvatar::applyPoseMirror`)

```cpp
void LLGhostAvatar::applyPoseMirror()
{
    if (!mEntityFlipChirality || !mIsBuilt) return;

    // For each table row that is NOT a dontFlipOnMirror (right-side) duplicate:
    for (const auto& row : PoseMirrorTable) {          // shared with / derived from PoserJoints
        LLJoint* j = getJoint(row.name);
        if (!j) continue;                              // guard: missing on this skeleton
        LLQuaternion qs = reflectQ(j->getRotation());  // (w,−x,y,−z)

        if (row.twin.empty()) {                        // MIDLINE: reflect in place
            j->setRotation(qs);
            if (row.animatesPosition) j->setPosition(reflectY(j->getPosition()));  // P1
        } else {                                       // PAIR: reflect both, then swap
            LLJoint* t = getJoint(row.twin);
            if (!t) { j->setRotation(qs); continue; }  // twin missing → best-effort in place
            LLQuaternion qt = reflectQ(t->getRotation());
            j->setRotation(qt);                        // twin's reflected rot → this joint
            t->setRotation(qs);                        // this joint's reflected rot → twin
            if (row.animatesPosition) {                // P1: swap+reflect positions too
                LLVector3 ps = reflectY(j->getPosition()), pt = reflectY(t->getPosition());
                j->setPosition(pt); t->setPosition(ps);
            }
        }
    }
}
```

`reflectQ(q) = LLQuaternion(-q.mQ[VX], q.mQ[VY], -q.mQ[VZ], q.mQ[VW])` — the shipping operator
(`fsposeranimator.cpp:536`). `reflectY(p) = LLVector3(p.mV[VX], -p.mV[VY], p.mV[VZ])`. **Read both
twins' rotations before writing either** (the code above reads `qs` and `qt` up front) so the swap is
correct — writing `j` first and then reading `t` would swap a value we just clobbered. Because we only
process the left row of each pair (skip `dontFlipOnMirror`), each pair is touched once.

This is a de-novo pass rather than a call into `FSPoserAnimator::flipEntirePose` because the poser
operates on its own `FSPosingMotion`/`FSJointPose` snapshot (a pose *edit*), not the live per-frame
`LLJoint` state, and pulls in the whole poser-session machinery. The **operator and the table** are
shared/lifted; the driver loop is small and lives on the clone. Factor `reflectQ`, `reflectY`, and the
pair table into a tiny shared header (e.g. `alchiralitymirror.h`) that both the poser and the clone
pass include, so the table has one home.

### 5.5 Composition with the existing stages (ordering summary)

```
updateRootPositionAndRotation   (placement/facing — NOT flipped)          ~5701
        │
updateMotions                   (clone poses its own skeleton)            ~5721
        │
applyPoseMirror   ← NEW         (reflect+swap joint-local rot/pos)        ~5722
        │
mPosePolish.run                 (grounds/continuity on the MIRRORED pose) ~5728
        │
applyDirectorLookAt / applyGaze (aims head/eyes at REAL targets, wins)    ~5733/5735
```

---

## 6. Edge cases & limitations — each with a verdict

### 6.1 Overlay clones — **out of scope (v1)**
Overlay backing has no skeleton; it re-draws the source's already-skinned batches
(`drawGeometryGhost`, see the transform doc §6). Mirroring would mean inserting `M=diag(1,−1,1)` into
the draw modelview `T·R·S·M·T(−pivot)` **and** flipping front-face winding (a reflection reverses
triangle orientation → back-face culling would invert). That mirrors the *entire silhouette including
chiral textures backwards* — a different feature with different artifacts. The drive-mode family is
already entity-only (`alghoststudio.h:100`); chirality joins it. **Verdict: entity-clone only; overlay
disabled in UI, matching how the drive-mode combo is disabled for overlays
(`alpanelghoststudio.cpp:1224`).**

### 6.2 Asymmetric / custom skeletons — **handled (by design), stated honestly**
Many avatars have asymmetric bone offsets, deform sliders, or one-sided rigs. **We never reflect the
bind pose or the rest skeleton** — that is the clone's own, and it keeps it. We reflect the
*animation's joint-local rotation/position deltas* and apply them on top of the clone's own rest
skeleton. So a clone built on an asymmetric avatar keeps its asymmetry and merely performs the mirrored
*motion*. This is the correct and only sane choice; the one caveat is that if the rest skeleton is
strongly asymmetric, the mirrored motion rides that asymmetry (e.g. a longer left arm doing the right
arm's gesture) — inherent, not a bug. **Verdict: handled; documented caveat.**

### 6.3 Chiral mesh/texture — **out of scope, stated up front**
Text, logos, parted hair, single glove/earring, one-sided tattoos, an eyepatch: these are baked into
mesh/UV/texture and **cannot be flipped by a bone mirror**. The clone's flipped pose will present them
**un-flipped** (the "SL" logo still reads "SL", now on the other hand). This is intrinsic to
pose-mirroring and we do not attempt mesh reflection (it destroys UVs and winding). **Verdict:
out-of-scope; call it out in the tooltip so operators aren't surprised.**

### 6.4 Rigged-mesh attachments worn by the clone — **handled for free**
Rigged (skinned) attachments deform from the **same joints** we just flipped — they are skinned to
`mShoulderRight` etc., whose rotation now carries the mirrored-left motion. So worn rigged mesh
**follows the mirror automatically**, no extra work; a rigged jacket sleeves-swap with the arms. (Its
*texture* is still chiral per §6.3.) **Verdict: handled implicitly.**

### 6.5 Non-rigged attachments & attachment points — **handled; no CV mirroring needed**
A non-rigged attachment hangs off an **attachment point**, which is a child joint of some skeleton
joint. Its world transform is its parent joint's transform — which we flipped in place / swapped — so
it rides the mirrored limb (an object on the right-hand point travels as the right hand performs the
left hand's motion). We do **not** swap attachment points or collision volumes; the CVs
(`L_HAND/R_HAND`, `L_CLAVICLE/R_CLAVICLE`, … in `avatar_skeleton.xml`) are shape/physics/attach-anchor
geometry, **not animated by keyframe motions**, so they need no per-frame mirroring for the pose. **The
one visible asymmetry:** a HUD-less clone with a prop mounted on *only* the right hand will show the
prop on the right hand while the right hand does the left's gesture — usually what you want; if the goal
were "swap which hand holds the prop", that is an attachment-swap feature, not a pose mirror. **Verdict:
handled; CV mirroring not required; attachment-point swap is a deliberate non-goal.**

### 6.6 Eyes / gaze / look-at interaction — **handled by ordering**
`mEyeLeft/mEyeRight` are a swapped pair (`fsposeranimator.h:272-273`), so with gaze **off** they mirror
with the animation. With gaze **on**, `applyGaze`/`applyDirectorLookAt` run **after**
`applyPoseMirror` (§5.1) and re-aim head/neck/eyes at the true world target, **overwriting** the
mirrored eye pose — so a gazing clone keeps eye contact rather than looking the wrong way. The Ghost
Studio panel already exposes per-clone gaze (`panel_ghost_studio.xml:259-263`), so both can be on
together and the ordering makes them compose sensibly. **Verdict: handled; document that gaze wins for
head/eyes.**

### 6.7 Facial morph animation — **out of scope (bones only)**
Visemes and expression morphs are **morph-target** driven, not bone-driven; there is nothing to negate
in a quaternion. Bento **face-bone** motion (jaw, brow, cheek, lip corners — the L/R face rows in
`fsposeranimator.h:262-306`) **is** mirrored. A morph-based asymmetric wink or smirk will not flip.
**Verdict: bone face motion handled; morph face motion out-of-scope; note it.**

### 6.8 Continuity / toggle stability — **handled; watch the double-cover**
The pass re-derives the mirror **from the freshly-posed skeleton every frame**, storing no history, so
there is **no drift and no pop from accumulation**. Toggling on/off simply starts/stops applying it; the
next `updateMotions` restores the un-mirrored pose cleanly (the controller rebuilds the skeleton
regardless). Two guards:

- **Quaternion double cover.** `reflectQ` is a pure component negation, so if the source's `q` and `−q`
  alternate frame-to-frame (a known blend artifact), the mirrored output alternates in lockstep — no
  *extra* instability is introduced, and since we don't `nlerp` against a stored previous value there is
  nothing to flip-flop. If a **toggle transition** (ease in/out, like `GAZE_EASE_TIME`,
  `llactormover.cpp:5146`) is added in P3, that blend **must** hemisphere-align (`dot(qA,qB)<0 →
  negate`) before nlerp, exactly as the gaze/pose code already does elsewhere.
- **No permanent corruption.** Like gaze (`llactormover.cpp:5139-5142`), we only ever write post-motion;
  the motion controller owns the skeleton next frame.

**Verdict: stable by construction; transition blend is optional polish with a known correctness rule.**

### 6.9 Performance — **negligible; quantified**
Per flipped clone per frame: iterate ~one row per animated joint. Body+face+hands is ~**70 flipped
rows** (≈133 bones minus the `dontFlipOnMirror` right-duplicates and unanimated CVs); with wings +
hind-limbs, ~**80**. Each row: 1–2 `getJoint` (hash/array lookup), 1–2 `getRotation`, 2 sign negations,
2 `setRotation`. That is ~**a few hundred float ops + ~160 joint lookups per clone** — well under a
microsecond of arithmetic; the joint-map lookups dominate and can be **cached to `LLJoint*` on first
build** (the twins don't change) to make it a flat pointer walk. It **rides the existing per-frame joint
write** the motion controller already does (hundreds of joints blended), so it adds a low-single-digit
percentage to an already-present cost. For a 50-clone crowd that is ~4,000 joint writes/frame on top of
the many thousands `updateMotions` already performs across those same skeletons — immaterial. **Verdict: negligible; cache the twin
`LLJoint*` pointers and it disappears into the noise.**

---

## 7. UI / UX

The Pose tab of `panel_ghost_studio.xml` already clusters the entity-drive controls: `drive_mode_combo`
(`:216-221`), `chaos_check` (`:222`), `entity_look_combo` (`:227`), `anim_speed_spinner` (`:236`),
`physics_check` (`:239`), plus the per-clone gaze block (`:259+`).

**Add one checkbox** — `name="pose_mirror_lr_check"`, label **"Mirror Pose (L/R)"** (or "Flip L/R") —
in the drive cluster, e.g. next to `drive_mode_combo` at `top≈78` or beside `physics_check` at
`top≈108`. Tooltip must pre-empt the two confusions:

> *"Play this clone's animation flipped left-to-right in real time (a right-hand gesture becomes
> left-handed). Independent of the drive mode above — combine with Mirror, Directed, or Frozen. Flips
> the **pose** only; chiral textures/logos and one-sided mesh are not flipped, and morph-based facial
> expressions don't mirror. Entity clones only."*

Behavior, following the existing commit handlers:

- `onPoseMirrorCommit()` mirrors `onDriveModeCommit` (`alpanelghoststudio.cpp:2094-2116`): fan out to
  **every member of a selected group** via `selectedInstances()`, entity-only, calling
  `setInstanceFlipChirality`.
- **Enablement:** enable only for `BACKING_ENTITY_CLONE`, exactly like `mDriveModeCombo->setEnabled`
  (`alpanelghoststudio.cpp:1224`); grey out for overlays.
- **State refresh:** set the checkbox from `inst->mFlipChirality` on selection change, mirroring the
  `mDriveModeCombo->setValue(inst->mDriveMode)` refresh at `alpanelghoststudio.cpp:1398-1400`.

Keep the existing "Mirror" **drive-mode** label as-is (renaming it would churn saved scenes and is a
separate decision); the new checkbox's distinct label + tooltip is enough to disambiguate for
operators. Internally, the code names in §0.1 keep engineers straight.

---

## 8. Phased implementation plan (each phase independently testable)

**P0 — Core body-bone mirror (rotation only).**
Add `mFlipChirality`/`mEntityFlipChirality` + the four-layer setter path (§5.3); add virtual
`applyPoseMirror()` + call site (§5.1); implement the reflect+swap pass over the **body** rows
(shoulders→wrists, hips→toes, spine/chest/neck/head midline) using `reflectQ` and the shared table;
add the checkbox (§7). *Test:* source raises right hand → clone raises left; source leans left → clone
leans right; midline (head nod, spine) unchanged; toggling on/off is instant and pop-free.

**P1 — Bento hands + face + animated translations.**
Extend the pass to the hand (×30) and face rows; add wings + hind-limbs to the table (§4.4 note); add
the **position** mirror (§4.5) for joints whose motion animates translation (pelvis sway). *Test:* a
one-handed finger-count mirrors to the other hand; an asymmetric brow/lip-corner face-bone motion
flips; a sway-left animation sways right.

**P2 — Interaction with gaze / attachments / groups.**
Verify ordering vs. gaze (`applyGaze` overwrites eyes; §6.6); confirm rigged attachments follow and
non-rigged props ride the swapped limb (§6.4-6.5); confirm group fan-out (§7) and the
spawn/refresh/`onEntityRuntimeReplaced` re-apply (§5.3). *Test:* clone with gaze on keeps eye contact
while body mirrors; a prop-in-hand clone; a locked group all flip together; refresh a clone → flip
persists.

**P3 — Polish: toggle transition + pointer caching + tooltip/limits copy.**
Optional eased on/off blend with hemisphere-aligned nlerp (§6.8); cache twin `LLJoint*` pointers on
skeleton build (§6.9); finalize the limitations tooltip (§6.3, §6.7). *Test:* toggling eases smoothly;
profiler shows no measurable per-frame cost at 50 clones.

---

## 9. Test plan — concrete in-world scenarios

1. **Handedness.** Source plays a right-hand wave. Enable flip → clone waves with its **left** hand,
   still **facing the same way**, standing in the **same spot** (proves §4.6 in-place flip).
2. **One-sided gesture.** Source does a right-arm salute / point. Clone salutes/points with the left
   arm; the resting arm swaps correspondingly (proves the pair **swap**, not just per-joint reflect).
3. **Lateral lean / roll.** Source leans/tilts to their left. Clone leans to its right; a forward
   **nod** performed simultaneously stays a forward nod (proves yaw/roll negate, pitch preserved — §4.2
   sanity table).
4. **Midline invariance.** Source does a pure spine bow / head turn to camera with arms still. Head-turn
   yaw flips direction; the bow (pitch) is unchanged; no lateral jump of the pelvis (proves midline
   reflect-in-place).
5. **Live toggling.** With the source mid-loop, toggle the checkbox on and off repeatedly → clone flips
   and un-flips **without popping, drifting, or accumulating** (proves §6.8 statelessness).
6. **Bento hands (P1).** Source counts fingers on the right hand → clone counts on the left with correct
   finger correspondence.
7. **Gaze coexistence (P2).** Enable per-clone gaze + flip. Clone's **body** mirrors but its **eyes/head
   stay locked to camera** (proves gaze-after-mirror ordering, §6.6).
8. **Chiral honesty check.** Source wears a shirt with text. Confirm the clone's text reads **normally**
   (un-flipped) even though the pose is mirrored — documents the §6.3 limitation for the operator.
9. **Group + refresh (P2).** Lock 4 clones into a rigid group, flip via the group header → all four
   flip. Refresh one (appearance re-pull) → its flip persists (proves fan-out + re-apply, §5.3).
10. **Overlay guard.** Select an overlay clone → the checkbox is disabled (proves §6.1 entity-only).

---

## 10. Open questions & risks

- **Wings / hind-limbs table coverage (assumption to confirm, §4.4).** These pairs are in
  `avatar_skeleton.xml` but not `PoserJoints`; confirm no alias already covers them before extending
  the shared table, and test on a winged/digitigrade avatar.
- **Pelvis position animation frequency.** How often do in-use anims actually animate `mPelvis`
  translation vs. rotation-only? If rare, P0's rotation-only body flip already looks fully correct for
  most content; if common in the operators' library, prioritize the §4.5 position mirror into P0.
- **`dontFlipOnMirror` semantics parity.** The poser uses the flag purely for pair de-dup in
  `flipEntirePose` (`fsposeranimator.cpp:1026`); confirm our lifted table interprets it identically and
  that no *midline* row is accidentally marked (it isn't in the poser, but verify after any table
  extension).
- **Frozen-then-flip ordering.** `DRIVE_FROZEN` pauses the controller via a pause request
  (`llghostavatar.cpp:606-618`); confirm the paused skeleton still presents a stable
  `getRotation()` for `applyPoseMirror` to reflect each frame (expected: yes — the joints hold their
  last posed values). Test scenario 5 covers the toggling case; add a frozen-clone variant.
- **Animesh sub-avatars.** A clone can own animesh attachments as separate `LLControlAvatar`s
  (`llghostavatar.cpp:640-648`). Those are their own characters with their own skeletons; the flip pass
  runs on the **wearer**. Decide whether animesh companions should also flip (likely a separate,
  later toggle) — out of scope for v1, but note it so a winged-animesh case isn't mistaken for a bug.
- **Shared-header placement.** Putting `reflectQ`/`reflectY`/the pair table in a small shared header
  (§5.4) touches both the poser and the clone; confirm no include-cycle with `LLGhostAvatar` (the
  runtime already stores `mEntityDriveMode` as `S32` specifically to "avoid header cycle",
  `llghostavatar.h:193`) — keep the shared header dependency-free (plain names + `LLQuaternion`).

---

## 11. Appendix — file:line index (anchors, HEAD `b69eb6af71a`)

| Symbol / fact | Location |
|---|---|
| `EDriveMode` enum (**the naming collision**) | `alghoststudio.h:101` (+ comment `:96-100`) |
| `Instance::mDriveMode` & drive fields (add `mFlipChirality` here) | `alghoststudio.h:356-360` |
| `Instance::mFootGlobal` / `mRotation` (placement, not flipped) | `alghoststudio.h:372-377` |
| `setInstanceDriveMode` (template for `setInstanceFlipChirality`) | `alghoststudio.cpp:2116-2137` |
| entity runtime state re-push (add flip re-apply) | `alghoststudio.cpp:~401` |
| `LLGhostAvatar::setEntityDriveMode` (template for `setEntityFlipChirality`) | `llghostavatar.cpp:568-620` |
| `mEntityDriveMode` (add `mEntityFlipChirality` near here) | `llghostavatar.h:193` |
| `LLGhostAvatar::idleUpdate` — DRIVE_MIRROR copies **anim UUIDs**, not joints | `llghostavatar.cpp:2026-2051` |
| ghost overrides `idleUpdate`, not `updateCharacter` | `llghostavatar.h:108` |
| **insertion point** — `updateMotions` → (NEW `applyPoseMirror`) → PosePolish → gaze | `llvoavatar.cpp:~5721 / 5728 / 5733-5736` |
| gaze post-motion contract (the pattern to copy) | `llactormover.cpp:5134-5142` |
| `LLJoint` get/set rotation & position | `lljoint.h:249-250, 232-233` |
| avatar joint enumeration (`getJoint`, `getSkeletonJoint`) | `llvoavatar.h:207-213` |
| **skeleton** — L/R Y-offsets, all `rot=0`, 133 bones | `avatar_skeleton.xml:1, 24-25, 81, 118, 179, 191` |
| `LLQuaternion::mQ = {x,y,z,w}` | `llquaternion.h:53` |
| **the mirror operator** `mQ[VX]*=−1; mQ[VZ]*=−1` (raw joint) | `fsjointpose.h:303-309` |
| the operator on an applied rotation `(-x,y,-z,w)` | `fsposeranimator.cpp:536`, `:920` |
| `reflectJoint` (reflect + swap twin) | `fsposeranimator.cpp:954-978` |
| `flipEntirePose` (iterate table, skip `dontFlipOnMirror`) | `fsposeranimator.cpp:1019-1043` |
| **the L/R pair table** (`PoserJoints`, `mMirrorJointName`, `dontFlipOnMirror`) | `fsposeranimator.h:219-313+` |
| `swapRotationWith` / `mirrorRotationFrom` | `fsjointpose.h:166, 181` |
| anims **do** animate joint positions (`setPosition` from a curve) | `llkeyframemotion.cpp:439`, `:1878` |
| UI drive cluster (add the checkbox) | `panel_ghost_studio.xml:216-240` |
| drive combo enable-gate / value refresh (templates) | `alpanelghoststudio.cpp:1224, 1398-1400` |
| drive commit fan-out to group (template for `onPoseMirrorCommit`) | `alpanelghoststudio.cpp:2094-2116` |

---

*End. The whole feature is one operator — `q → (w, −x, y, −z)`, swap L↔R, keep midline — applied once
per frame after `updateMotions`, in the clone's own root frame. The math is already proven in this
repo's poser (`fsjointpose.h:303`); this design just lifts it from a one-shot pose edit into a
live per-frame stage, under a name (`mFlipChirality`) chosen so it can never be confused with the
pre-existing `DRIVE_MIRROR` copy mode.*

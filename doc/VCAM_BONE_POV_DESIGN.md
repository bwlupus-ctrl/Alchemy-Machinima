# Virtual Cam — bone-attached avatar/clone POV — design

**Status:** normative design. No source modified by this document. Resolves
`doc/VCAM_BONE_POV_SEED.md` (the seed) against verified code. Feeds a Codex
implementation brief.

## Review corrections (Opus adversarial pass, 2026-08-16 — NORMATIVE; override the body where they conflict)

Zero must-fix correctness errors; every cited line verified. Apply these before/while implementing:

- **UP-AXIS IS PROVEN, not inferred (upgrade §5.1 `up0` to PROVES; downgrade R2 to a nit; NO empirical
  check needed).** `LLHeadRotMotion::onUpdate` builds the head world rotation as
  `LLQuaternion(headLookAt, left, up)` with `up` from `root_up=(0,0,1)*rootRotWorld`
  (`llheadrotmotion.cpp:224, 245-247`); the eyes use the identical construction (`:413-416`). With the
  row-vector ctor (`llquaternion.cpp:95-103`) the head/eye LOCAL basis is exactly **X=forward,
  Y=left, Z=up**, so `(0,0,1)*R_head == head-up` and `(0,0,1)*R_eye == eye-up` hold.
- **S1 (should-fix) — the `fwd=+X / up=+Z` proof holds ONLY for spine-chain joints (head/eye/neck),**
  which inherit the pelvis frame (facing `alcinelightrig.cpp:1314`, gaze `llactormover.cpp:3089`). A
  CUSTOM joint (hand/wing/HUD/limb) has no such guarantee — its local +X runs down the bone. So:
  document custom-joint orientation as best-effort, and **default a custom joint to STABILIZED
  (position-only) aim unless the operator explicitly opts into full-follow.** Head/eye/neck keep
  full-follow as designed.
- **S2 (should-fix) — MUST use `LLJoint::getWorldPosition()` / `getWorldRotation()`** (lazy-recompute,
  `lljoint.cpp:743/813`), NEVER `getLastWorldPosition/getLastWorldRotation` (`:752/823`, cached, no
  recompute) — the latter reads a one-frame-stale pose despite correct tick ordering. State this in
  §4.1 and the OFF-LIMITS/DO list.
- **S3 (should-fix) — the FOV setter writes `mFixedVerticalFovRad`,** which the per-frame capture prep
  republishes into `mEffectiveVerticalFovRad` (`llprismlens.cpp:2734, 2788`) that the render actually
  reads (`:5601`). Do NOT write `mEffectiveVerticalFovRad` directly (next prep overwrites it) and do
  NOT gate the recompute behind a config-revision bump.
- **N1 (nit) — horizon-lock degeneracy fallback = HOLD LAST GOOD UP as the PRIMARY path** (not the
  parenthetical), so a straight-up look with a rolled head can't collapse `right = fwd % up0`. The
  render's own guard (`llprismlens.cpp:5591-5597`) prevents NaN but drops the frame.
- **N3 (nit, accepted) — an OLD binary that loads then RE-SAVES a new scene silently drops the
  additive `bone_pov` sub-map** (reparse→reserialize without the unknown key). Read-compat holds;
  round-trip-through-old data loss is accepted behaviour.
- **N4 (nit) — add one TUT assertion mirroring the render's re-derivation** (`forward=(0,0,-1)*q;
  up=(0,1,0)*q; right=fwd%up`, `llprismlens.cpp:5588-5589`) so a future render-side convention change
  trips the test.
- **Confirmed sound (no change):** the hook point + the-only-two render consumers
  (`llprismlens.cpp:5569-5570`, `:5227-5228`); idle() runs before display() with the skeleton already
  posed; `LLQuaternion(right,up,-fwd)` gives `(0,0,-1)*q==fwd`; clone joints/scale; stabilized/anchor-
  loss have no snap/NaN path; forcing `EFovMode::FIXED` breaks no other consumer.

**Evidence labels.** Every claim about existing code is cited `file:line`.
Design claims are labelled:
- **PROVES** — verified directly in code as cited.
- **IMPLIES** — strong inference from cited code plus one small, stated step.
- **INFERS** — assumption not fully pinned by code; flagged for the builder.

The seed's four fixed user decisions (both aim modes, configurable joint, both
roll modes, feature-complete config) are honoured verbatim. Where code
contradicts the seed it is flagged in §10.

---

## 1. Verified current model (behavioural truth)

### 1.1 The capture camera the driver must feed

- **PROVES** — A CAMERA_FEED capture is either object-anchored or virtual.
  `CameraSettings` holds `bool mVirtual`, `LLVector3 mVirtualPos` (AGENT-space
  eye), `LLQuaternion mVirtualRot` (`llprismlens.h:161-163`). The documented
  convention: `(0,0,-1)*mVirtualRot == view forward`, `(0,1,0)*mVirtualRot ==
  view up`; `mLocalEyeOffset` is NOT applied to a virtual camera
  (`llprismlens.h:152-163`).
- **PROVES** — Registration: `addVirtualCamera(handle, pos, rot, reason)` sets
  `mVirtual=true`, stores `pos`/`rot`, defaults the rest
  (`llprismlens.cpp:1839-1878`; public decl `llprismlens.h:451-453`).
- **PROVES** — The **exact per-frame read** of the virtual transform for the
  capture render is in `LLPrismLens::renderAuxiliaryView()`:
  ```
  rotation   = capture->mCamera.mVirtualRot;   // llprismlens.cpp:5569
  camera_eye = capture->mCamera.mVirtualPos;   // llprismlens.cpp:5570
  ```
  followed by `forward = (0,0,-1)*rotation`, `up = y_axis*rotation`,
  `right = forward % up`, re-orthonormalize (`:5588-5599`). A **second** read of
  the same fields drives the wireframe frustum guide
  (`renderCameraGuides()`, `:5227-5228`). Both are pure consumers.
- **PROVES** — `renderAuxiliaryView()` is called once per frame from
  `display()` at `llviewerdisplay.cpp:919`, inside the frame render, before the
  main StateSort (`:921-929`).

### 1.2 The frame ordering the driver depends on

- **PROVES** — Machinima per-frame controllers tick in `LLAppViewer::idle()`
  BEFORE `display()` runs. The light-rig manager ticks at
  `llappviewer.cpp:5525-5526`, alongside the Director switcher / anim switcher /
  local fog ticks (`:5515-5526`).
- **PROVES** — `gObjectList.update()` (which runs avatar character/skeleton
  updates) is ordered *above* these ticks — the comment at
  `llappviewer.cpp:5507-5510` states `ALObjectPathMover::update()` was moved to
  "just before `gObjectList.update()` above", i.e. the object/skeleton update
  precedes line 5525. **IMPLIES** — joint world transforms read at line 5525 are
  this frame's posed skeleton, and the light rig already relies on exactly that
  (`root->getWorldRotation()` at `alcinelightrig.cpp:1312-1316`).
- **Net ordering guarantee (PROVES + IMPLIES):** a tick placed in idle right
  after line 5526 reads fresh joint transforms and writes `mVirtualPos/
  mVirtualRot`; `renderAuxiliaryView()` later in the *same* `display()` reads
  them. **Write-before-read holds within the frame.**

### 1.3 Cast roster (read-only)

- **PROVES** — `LLDirectorCast` resolves subjects: `resolve(LLUUID::null)` = my
  avatar; `resolveSubjectA/B/C/D()` return `LLVOAvatar*` or nullptr when unset/
  dead (`lldirectorcast.h:85,100-103,90-91`). A stale id resolves to `nullptr`,
  never silently to self (`:83-85`).
- **PROVES** — The light rig consumes exactly this roster read-only via
  `resolveSlotAvatar()` / `gatherGroupMembers()`
  (`alcinelightrig.cpp:111-161,464-479`).
- **IMPLIES** — Clones appear in the roster as their `LLGhostAvatar` id: a ghost
  is a real cast member (added by id) and `resolve()` returns the `LLVOAvatar*`
  which may be an `LLGhostAvatar` (it derives from `LLVOAvatar`,
  `llghostavatar.h:51`). No special-casing needed at the roster boundary.

### 1.4 Clones expose the same joints and a scale override

- **PROVES** — `LLGhostAvatar : public LLVOAvatar` (`llghostavatar.h:51`);
  therefore `getJoint(name)` / `getRootJoint()` / `getPelvisToFoot()` /
  `getWorldPosition()` / `getWorldRotation()` are inherited and identical.
- **PROVES** — Clone scale: `LLVOAvatar::getUniformScale()` is `virtual` and
  returns `1.f` (`llvoavatar.h:266`); `LLGhostAvatar::getUniformScale()`
  overrides it to return `mEntityScale` (`llghostavatar.h:116`).
- **PROVES** — `getJoint(std::string_view)` is the joint lookup API
  (`llvoavatar.h:206`).

### 1.5 Reuse precedents (do not fork)

- **PROVES — foot-pivot clone scale.** `scaledPoint(avatar, point, scale)`
  scales a world point about the avatar's **foot** (root world pos minus
  `getPelvisToFoot()`), no-op at `scale==1` (`alcinelightrig.cpp:85-109`). The
  gaze code re-implements the identical invariant as
  `gazeRenderedJointPosition()` (`llactormover.cpp:3040-3057`). **Both are
  file-static** (anonymous namespace), so neither is callable across
  translation units — see §4.2 for how the driver honours "reuse the invariant,
  don't fork the logic" under that constraint.
- **PROVES — facing / basis precedent.** SL skeleton forward is local **+X** in
  the region frame: light rig computes
  `forward = (1,0,0)*root->getWorldRotation()`, `facing = atan2(fwd.Y, fwd.X)`
  (`alcinelightrig.cpp:1314-1316`). Gaze computes eye/head forward the same way:
  `(1,0,0)*joint->getWorldRotation()` (`llactormover.cpp:3089`).
- **PROVES — exponential position smoother.** Light rig: on first frame /
  `damping<=0` / time-reversal it snaps, else
  `alpha = 1 - exp(-dt/tau); centre += (target-centre)*alpha`
  (`alcinelightrig.cpp:1446-1466`). The reset flag `mHaveSmoothedCentre` is
  cleared on anchor / enable / slot change (`:443-462`).
- **PROVES — eyeline / eye-joint handling.** `currentAnimatedGazeDirection()`
  averages `(1,0,0)*getWorldRotation()` over `mEyeLeft`+`mEyeRight`, falling
  back to `mFaceEyeAltLeft/Right`, then `mHead` (`llactormover.cpp:3081-3106`).
  Joint names in use elsewhere: `mHead`, `mNeck`, `mEyeLeft`, `mEyeRight`
  (`:3094-3103,3233`). **IMPLIES** — the "eye" dropdown option reuses this
  averaging for a true eyeline.
- **PROVES — pure trim helper.** `ALGazeMath::eyelineOffsetDir(dir, up, yaw,
  pitch)` yaws/pitches a direction about `up` and `up%dir`, with a small-angle
  no-op and a degenerate-axis guard (`algazemath.h:67-85`). `chaseAlpha(dt,tau)`
  is the shared exponential alpha (`algazemath.h:21-24`).

### 1.6 The quaternion build is already canonical

- **PROVES** — "Snap to my view" builds `mVirtualRot` as
  `LLQuaternion(right, up, -forward)` from orthonormal camera axes
  (`llfloaterprismmanager.cpp:1236`, doc `:1218-1223`).
- **PROVES** — `LLQuaternion(x_axis, y_axis, z_axis)` sets those three vectors
  as the **rows** of a matrix and converts+normalizes
  (`llquaternion.cpp:95-103`). **IMPLIES** — for a row-vector transform,
  `(1,0,0)*q = x_axis`, `(0,1,0)*q = y_axis`, `(0,0,1)*q = z_axis`. Hence with
  `q = LLQuaternion(right, up, -forward)`: `(0,0,-1)*q = forward` and
  `(0,1,0)*q = up` — **exactly** the render-path convention at
  `llprismlens.cpp:5588-5589`. The driver builds `mVirtualRot` with this same
  constructor, so its output is byte-for-byte in the render's expected space.

### 1.7 Persistence vehicle

- **PROVES** — The Director console owns scene save/load. `saveScene()` copies
  `LLPrismLens::sceneData()["prism_captures"]`/`["prism_displays"]` into the
  scene (`llfloaterdirector.cpp:886-888`) and `loadScene()` applies via
  `LLPrismLens::applySceneData()` (`:1005`). `SCENE_VERSION` is stamped at
  `:885`.
- **PROVES** — Per-capture camera settings serialize in `sceneData()` as an
  item map, with the virtual transform already hand-serialized as `virtual`,
  `virtual_pos` (3-array), `virtual_rot` (4-array) at
  `llprismlens.cpp:3797-3808`, and parsed back at `:4079-4092`. An additive
  per-item sub-map (`bone_pov`) slots in beside them and round-trips through the
  existing Director save/load with **no new wiring** (see §7).

---

## 2. Resolution summary (A–F)

| # | Decision | One-line resolution |
|---|----------|---------------------|
| **A** | Driver & hook | New singleton `ALVCamBonePov` owns transient smoothing state; ticked in `LLAppViewer::idle()` immediately after the light-rig tick (`llappviewer.cpp:5526`); writes via a new lightweight registry setter that touches only `mVirtualPos/mVirtualRot/mFixedVerticalFovRad`. Config is POD in `CameraSettings`. Anchor-loss = **hold last written transform** (never snap/NaN). |
| **B** | Joint + clone scale | Dropdown → `mHead` / eye (avg `mEyeLeft`+`mEyeRight`) / `mNeck` / custom-by-name via `getJoint()`. Position = **foot-pivot** `scaledPoint(avatar, jointWorldPos, getUniformScale())` **+** `(localOffset * scale) * jointWorldRot`. Same invariant as the light rig; re-implemented as a shared pure helper (the originals are file-static). |
| **C** | Aim-mode math | Full-follow: `forward=(1,0,0)*R_joint`, `up=(0,0,1)*R_joint`, `right=fwd%up`, apply aim-trim to forward, roll-mode, build `LLQuaternion(right,up,-forward)`. Stabilized: drive **only** `mVirtualPos`; leave `mVirtualRot` untouched (operator keeps aim). Toggle hands off with no snap by seeding smoothing from the last written rot. |
| **D** | Roll | Horizon-lock (default): `right = fwd % worldUp; up = right % fwd`, guard `|fwd·worldUp|` near 1. Inherit: keep joint up (`(0,0,1)*R_joint`). |
| **E** | Smoothing + FOV | Position via light-rig exponential smoother; orientation via `LLQuaternion::slerp`. Snap on first frame / mode / anchor / joint change / time-reversal. FOV drives `mFixedVerticalFovRad` (mode forced FIXED — the only valid mode for a virtual cam); no separate prism FOV to compose. |
| **F** | UI + persistence | Controls live in the **Prism Manager** floater (owns the VCam), in the camera editor beneath the existing "Virtual camera" block. Persistence via an **additive `bone_pov` sub-map** in each `prism_captures` item — rides the existing Director scene round-trip, **no version bump**, plus one new `LLCachedControl`-style setting only if a global default is wanted (see §7). |

---

## 3. A — the driver and its hook

### 3.1 Owning object — recommendation

**Create a new small singleton `ALVCamBonePov`** (files `alvcambonepov.{h,cpp}`),
modelled on `ALCineLightRigManager` (`alcinelightrigmanager.cpp:405-445`).
Rationale:

- **PROVES** — `LLPrismLens` is a pure viewer-local registry that includes no
  avatar/joint headers and whose only job is capture/display bookkeeping and the
  render feed. Its header pulls `llmath`/`llquaternion`/`lluuid` only
  (`llprismlens.h:9-14`). Pushing `LLVOAvatar`/`LLJoint`/`LLDirectorCast`
  resolution into it would invert that boundary.
- **IMPLIES** — The seed's stated preference ("reuse the capture's existing
  virtual fields so the render path is UNCHANGED", seed A) is best served by a
  driver that *writes* the existing fields and leaves the *consumer*
  (`renderAuxiliaryView`) byte-identical.

**Split of responsibilities:**

- **Config (persistent, POD):** a `BonePovSettings` struct added to
  `CameraSettings` (§7.1) — default-constructed = disabled and byte-inert, like
  every other prism sub-struct (`llprismlens.h:117-164` establishes the
  default-inert idiom). No avatar pointers, so `llprismlens` stays joint-free.
- **Transient smoothing state (driver-owned):** `ALVCamBonePov` holds a
  `std::array<Smoother, MAX_CAPTURES>` keyed by `CaptureHandle.mId`, each with
  `LLVector3 mSmoothedPos; LLQuaternion mSmoothedRot; bool mHave; F64 mLastTime;`
  and a fingerprint of `{anchorSlot, jointSel, customName, aimMode}` to detect a
  reset condition. This mirrors the light rig's transient
  `mSmoothedCentre/mHaveSmoothedCentre` (`alcinelightrig.cpp:443-448`).

### 3.2 The exact per-frame hook

Add one line in `LLAppViewer::idle()` immediately after the light-rig tick:

```cpp
// llappviewer.cpp — insert after line 5526
ALVCamBonePov::instance().tick(
    LLPresentationTime::currentFrame().presentation_time);
```

**PROVES** this is before the read: the read is in `renderAuxiliaryView()`
(`llprismlens.cpp:5569-5570`) called from `display()`
(`llviewerdisplay.cpp:919`); `idle()` runs before `display()` each frame
(§1.2). Using the **same `presentation_time`** as the light rig
(`LLPresentationTime::currentFrame()`, `llappviewer.cpp:5525-5526`) keeps the
smoother time-base identical and correct under bullet-time/scrub.

**INFERS (builder check):** confirm no code between line 5526 and `display()`
re-poses the target skeleton in a way that would stale the read. The light rig
reads joints at the same site and is correct in production, so the risk is low;
if a future pipeline change moves skeleton posing later, relocate the `tick()`
call to `llviewerdisplay.cpp:918` (immediately before
`LLPrismLens::renderAuxiliaryView()`), which is strictly the latest legal write
point. Both sites satisfy write-before-read; idle is preferred for lockstep
parity with the other machinima ticks.

### 3.3 The write path (new lightweight setter)

Per-frame writes must **not** churn the configuration revision (the UI rebuilds
on `configurationRevision()` changes). Add to `llprismlens`:

```cpp
// declared in llprismlens.h beside setCameraSettings (:471)
bool setVirtualCameraTransform(const CaptureHandle& capture,
                               const LLVector3& pos, const LLQuaternion& rot,
                               F32 vertical_fov_rad, std::string* reason);
```

It resolves the slot, rejects non-`mVirtual` captures, validates finiteness /
unit-quaternion (mirroring the checks at `llprismlens.cpp:2737-2738,3360-3369`),
writes `mVirtualPos/mVirtualRot/mFixedVerticalFovRad`, and bumps the **runtime**
revision only (not `mRevision`). This is a registry write-side addition; it does
**not** alter how the render path *consumes* the fields, so §9's off-limits line
holds.

### 3.4 Graceful degradation (never snap / never NaN)

Priority order, each cited to a precedent:

1. **Anchor unresolved** (subject unset / left region / dead) — `resolve*()`
   returns `nullptr` (`lldirectorcast.h:83-85`). → **Do not write this frame.**
   The capture retains its last written `mVirtualPos/mVirtualRot`; the render
   reads a valid stale transform. Set the driver's `mHave=false` so re-resolve
   re-snaps rather than lerping from a stale pose. (Light-rig analogue: it
   `return`s early and clears smoothing on a null avatar,
   `alcinelightrig.cpp:1235-1245`.)
2. **Joint missing** (custom name not on this skeleton) — `getJoint()` returns
   `nullptr`. → Same as (1): hold last, `mHave=false`. Surface a one-line status
   in the UI ("joint 'X' not found on <subject>").
3. **Non-finite result** (`!pos.isFinite()` or `!rot.isFinite()`) — → discard
   this frame's compute, hold last. The setter also rejects non-finite as a
   second guard (§3.3).
4. **Feature disabled / capture not virtual** — write nothing; the operator's
   stored transform stands. Toggling enable OFF must leave the last transform in
   place (a body-cam freeze), not zero it.

**Never** fall back to "self view" or identity — that would snap the shot. Hold
is the only correct default and matches the seed's "must not snap/NaN".

---

## 4. B — joint resolution and clone scale

### 4.1 Joint dropdown → real joints

| Dropdown value | Resolution | Position source | Orientation source |
|----------------|-----------|-----------------|--------------------|
| `head` | `getJoint("mHead")` | `mHead` world pos | `mHead` world rot |
| `eye` (eyeline) | avg of `mEyeLeft`,`mEyeRight`; fallback `mFaceEyeAltLeft/Right`; fallback `mHead` | mean of the two eye world positions | mean eye forward (§4.3) + `mHead` up |
| `neck` | `getJoint("mNeck")` | `mNeck` world pos | `mNeck` world rot |
| `custom` | `getJoint(<name>)` from a text field | that joint's world pos | that joint's world rot |

- **PROVES** — all four names exist and are read this exact way by the gaze code
  (`llactormover.cpp:3094-3103,3233`) and `getJoint()` is the API
  (`llvoavatar.h:206`). **PROVES** — clones expose them identically (§1.4).
- **IMPLIES** — `head`/`neck`/`custom` take the joint's full world rotation for
  full-follow; `eye` composes a forward from the eyeline (eyes carry gaze yaw/
  pitch but no meaningful roll), taking **up** from `mHead` so roll is stable.

### 4.2 Clone-scale offset composition (the invariant, not a fork)

The finalized invariant (foot-pivot uniform scale) exists twice, both
file-static: `scaledPoint()` (`alcinelightrig.cpp:85-109`) and
`gazeRenderedJointPosition()` (`llactormover.cpp:3040-3057`). Because neither is
externally linkable, the driver **re-expresses the identical formula** in a new
shared pure-math header (§5.1 `boneScaledPoint`), TUT-tested against the same
expected numbers (§8). This satisfies "reuse the invariant, do not fork the
logic": it is one canonical formula, cited, with a regression test — not a
divergent second algorithm.

**Exact composition** (per frame, after joint resolve):

```
scale   = scale_aware ? sanitize(avatar->getUniformScale()) : 1        // clone scale
J_pos   = joint->getWorldPosition()                                    // rendered pose
J_rot   = joint->getWorldRotation()                                    // rendered pose
base    = boneScaledPoint(avatar, J_pos, scale)                        // foot-pivot scale
                    // = J_pos                       if scale == 1
                    // = foot + (J_pos - foot)*scale otherwise
                    //   foot = root->getWorldPosition(); foot.z -= max(0,getPelvisToFoot())
eye     = base + (localOffset * scale) * J_rot                         // offset in JOINT-LOCAL frame,
                                                                       // magnitude scaled by clone
```

- **PROVES** the foot expression: identical to `scaledPoint`
  (`alcinelightrig.cpp:97-108`) and `gazeRenderedJointPosition`
  (`llactormover.cpp:3049-3056`).
- **PROVES** the offset-scaling precedent: the light rig scales its Z offset by
  `subject_scale` (`alcinelightrig.cpp:1439-1440`). A 0.5× clone's 20 cm
  forward offset therefore becomes 10 cm world — the seed's stated invariant.
- **`localOffset * J_rot`** rotates the local offset (e.g. `+X` = forward to the
  lens) into agent space, so "forward to the lens" tracks head yaw
  automatically — **PROVES** by the same `vector * worldRotation` idiom at
  `llactormover.cpp:3089`.
- `sanitize()` clamps scale to a safe finite floor/ceil, mirroring the light
  rig's `sanitizeSubjectScale()` use (`alcinelightrig.cpp:1267,1332`). Handles
  the seed's 0.05× / 20× robustness cases.

**INFERS (builder):** for the `eye` selection, `base` uses the mean of the two
eye joints already run through `boneScaledPoint` individually then averaged (so
each is foot-scaled before the mean), matching how gaze scales each joint
position before use.

### 4.3 Eyeline forward (eye option)

```
fwd_eye = normalize( (1,0,0)*mEyeLeft->getWorldRotation()
                   + (1,0,0)*mEyeRight->getWorldRotation() )
```
**PROVES** — this is exactly `currentAnimatedGazeDirection()`
(`llactormover.cpp:3084-3105`), including the `mFaceEyeAlt*`→`mHead` fallback
chain and the `normVec() > 1e-4` validity gate.

---

## 5. C — aim-mode math (the basis-remap crux)

### 5.1 SL joint frame → camera frame (full-follow)

**Given** the anchor joint's world rotation `R_joint` (maps joint-local axes to
agent space). SL bone axes:

- **forward = local +X** — **PROVES** (`alcinelightrig.cpp:1314`,
  `llactormover.cpp:3089`).
- **up = local +Z** — **IMPLIES.** SL skeleton bones inherit the pelvis frame
  (X-forward, Y-left, Z-up); with forward=+X and left=+Y in a right-handed
  frame, up = forward×left = X×Y = +Z. No line asserts head-`+Z` directly, so
  this is inference from the frame's handedness; a wrong guess perturbs only the
  *inherit-roll* up vector, and **horizon-lock (the default) discards it
  entirely** (§6), so the exposure is bounded.

**The remap** (agent-space vectors):

```
fwd   = (1,0,0) * R_joint                 // nose direction   (PROVES)
up0   = (0,0,1) * R_joint                 // head-up          (IMPLIES)
fwd  := eyelineOffsetDir(fwd, up0, yawTrim, pitchTrim)   // aim trim (reuse gaze helper)
up   := roll-mode(up0, fwd)               // §6 horizon-lock or inherit
right = fwd % up
up   := right % fwd                       // re-orthonormalize (matches render :5598)
mVirtualRot = LLQuaternion(right, up, -fwd)     // §1.6: yields (0,0,-1)*rot==fwd
```

- **PROVES** — the final `LLQuaternion(right, up, -fwd)` is the identical
  construction the snap path uses (`llfloaterprismmanager.cpp:1236`) and the
  render re-derives (`llprismlens.cpp:5588-5589` + constructor
  `llquaternion.cpp:95-103`). This is why the driver's output drops into the
  render path with zero convention risk.
- **PROVES** — aim-trim reuses `ALGazeMath::eyelineOffsetDir` verbatim
  (`algazemath.h:67-85`); yaw about `up`, pitch about `up%fwd`, small-angle
  no-op, degenerate-axis guard already inside it.

`mVirtualPos = eye` from §4.2.

### 5.2 Stabilized mode

- Drive **only** `mVirtualPos` (from §4.2). **Do not** write `mVirtualRot` —
  pass the capture's *current* `mVirtualRot` straight back through the setter
  (or add an overload that omits rot). The operator continues to aim via the
  existing prism controls ("Snap to my view" and any future aim UI), which write
  `mVirtualRot` through the normal `setCameraSettings` path
  (`llfloaterprismmanager.cpp:1253-1259`).
- **Clean hand-off (no snap):**
  - full-follow → stabilized: stop writing rot; the last follow rot stays put →
    continuous.
  - stabilized → full-follow: on the mode-change frame, **seed the orientation
    smoother** `mSmoothedRot` from the current `mVirtualRot` and set `mHave=true`
    so the first follow frame slerps from where the operator left it rather than
    snapping. Detected via the driver's config fingerprint (§3.1).
- **Operator/driver contention:** in stabilized mode the driver never touches
  rot, so there is no fight. In full-follow the driver owns rot; the UI's
  orientation controls are disabled/greyed for a follow capture (§7.3) to make
  ownership legible. Position is always driver-owned while attached; the "Snap
  to my view" position write is ignored for an attached capture (or re-seeds the
  smoother if the operator uses it as a nudge — builder's call; recommend
  ignore-with-status).

---

## 6. D — roll handling

Compute `up` from `fwd` and the chosen mode, **after** aim-trim, **before** the
final `right/up` orthonormalization in §5.1.

### 6.1 Horizon-lock (default)

Re-level so the shot stays level as the head tilts:

```
worldUp = (0,0,1)
if (|fwd · worldUp| > 1 - 1e-3)          // fwd within ~2.5° of vertical: degenerate
    up = up0                             // fall back to inherited up (or last good up)
else
    right = fwd % worldUp
    right.normVec()
    up    = right % fwd
    up.normVec()
```

- The `fwd % worldUp` → `right % fwd` construction yields an up with **zero roll
  about the view axis** (up lies in the vertical plane containing fwd). This is
  the standard re-orthonormalization; **PROVES** it is the same shape the render
  guide already uses to rebuild `up` from `right` and `forward`
  (`llprismlens.cpp:5252,5598`), and the same `%`-based rebuild in
  `prismCurrentViewTransform` (`llfloaterprismmanager.cpp:1230-1232`).
- **Degeneracy guard:** when `fwd ≈ ±worldUp`, `fwd % worldUp → 0` and `right`
  collapses; the `1e-3` cosine threshold catches it and holds the previous
  up. **PROVES** the codebase already guards this exact collapse via
  `normVec() <= F_ALMOST_ZERO` early-outs (`llprismlens.cpp:5246-5251,5591-5597`).

### 6.2 Inherit roll

```
up = up0        // = (0,0,1)*R_joint, from §5.1
```
Keeps the joint's full tilt (raw first-person). The final `right = fwd % up; up
= right % fwd` still orthonormalizes against any fwd/up non-orthogonality
introduced by aim-trim.

---

## 7. E — smoothing and FOV

### 7.1 Position smoothing (reuse the light-rig smoother)

Per frame, with `tau = smoothing` seconds and `dt = presentation_time -
mLastTime`:

```
if (!mHave || tau <= 0 || mLastTime < 0 || presentation_time < mLastTime)
{
    mSmoothedPos = target_eye;           // SNAP
    mSmoothedRot = target_rot;
    mHave = true;
}
else
{
    alpha = 1 - exp(-dt / tau);          // ALGazeMath::chaseAlpha(dt,tau)
    mSmoothedPos += (target_eye - mSmoothedPos) * alpha;
    mSmoothedRot  = slerp(alpha, mSmoothedRot, target_rot);   // LLQuaternion::slerp
}
mLastTime = presentation_time;
```

- **PROVES** — the guard + `1-exp(-dt/tau)` + `+= delta*alpha` is the light
  rig's smoother verbatim (`alcinelightrig.cpp:1446-1466`); `chaseAlpha` is the
  shared pure form (`algazemath.h:21-24`).
- **Orientation** uses `LLQuaternion::slerp(alpha, from, to)` (INFERS the exact
  signature — builder verify against `llquaternion.h`; SL provides `slerp` and
  `nlerp`). One damping control drives both; **INFERS** a future split into
  `pos`/`rot` taus is trivial (two settings) — recommend one control to start,
  per seed "one damping control (or split)".
- **Snap conditions** (set `mHave=false` → next frame snaps): first attach,
  aim-mode change, roll-mode change, anchor change, joint change, custom-name
  change, time-reversal. All detected by the config fingerprint in §3.1, exactly
  as the light rig clears `mHaveSmoothedCentre` on `setAnchor`/`setGroup*`
  (`alcinelightrig.cpp:443-462`).

### 7.2 FOV

- Drive `settings.mFixedVerticalFovRad` from a UI spinner. **PROVES** — a
  virtual camera must be `EFovMode::FIXED`: FOLLOW_PROJECTOR is invalid and is
  soft-corrected for virtual cams (`llfloaterprismmanager.cpp:1251-1257,1329-1334`).
  So there is **no** second "prism FOV" to compose with — the bone FOV *is* the
  capture FOV.
- **Range:** the render/guide validity gate accepts `5°…175°`
  (`llprismlens.cpp:5259,5269-5270`); recommend a UI range of **10°…150°**
  vertical (comfortable machinima band) clamped into the validity window. The
  existing camera editor already exposes a vertical-FOV spinner in degrees
  (`llfloaterprismmanager.cpp:1304`), so reuse that control's convention
  (degrees in UI, `*DEG_TO_RAD` on commit).
- The driver passes `vertical_fov_rad` through `setVirtualCameraTransform`
  (§3.3) each frame so a live FOV change takes effect without a settings-commit
  round trip; the value originates from the persistent `BonePovSettings.mFovDeg`.

---

## 8. F — UI and persistence

### 8.1 Where the controls live

**Prism Manager floater** (`floater_prism_manager.xml` +
`llfloaterprismmanager.{h,cpp}`), in the camera editor beneath the existing
"Virtual camera (prim-free)" block (`floater_prism_manager.xml:368-370`). It
owns the VCam and already hosts the virtual-camera checkbox and "Snap to my
view" button (`llfloaterprismmanager.cpp:1239-1291`). This is where an operator
who just made a virtual cam expects the attach controls.

- **INFERS** — the light rig mirrors some controls into the Director console
  ("lockstep"). The VCam has **no** existing Director-console surface (unlike
  the light rig), and its bone attach is per-capture scene state, not a
  Director-global. Recommend **Prism-Manager-only** controls; the Director
  console already round-trips the persisted config via the scene (§8.3), so no
  Director UI is needed. If a Director quick-toggle is later wanted, it reads/
  writes the same `bone_pov` scene keys — additive, no rework.

### 8.2 Control list (match the light-rig idiom, with mini resets)

Gated visible only when the selected capture is `mVirtual`:

| Control | Widget | Backing field |
|---------|--------|---------------|
| Attach to skeleton (enable) | check_box | `mEnabled` |
| Anchor subject | combo (Me / A / B / C / D) | `mAnchorSlot` |
| Joint | combo (Head / Eye / Neck / Custom) | `mJointSel` |
| Custom joint name | line_editor (shown when Custom) | `mCustomJoint` |
| Aim mode | combo/toggle (Full-follow / Stabilized) | `mAimMode` (default full) |
| Roll mode | combo/toggle (Horizon-lock / Inherit) | `mRollMode` (default horizon) |
| Offset X/Y/Z (m, joint-local) | 3 spinners + mini "reset" | `mOffset` |
| Aim trim pitch/yaw (deg) | 2 spinners + mini "reset" | `mTrimPitchDeg/mTrimYawDeg` |
| FOV (deg) | spinner + mini "reset" | `mFovDeg` |
| Smoothing (s) | spinner + mini "reset" | `mSmoothingSec` |
| Scale-aware | check_box (default on) | `mScaleAware` |

- Mini per-row **reset** buttons match the light-rig UI idiom (seed F). A
  master "Reset attach" restores the whole struct to defaults.
- Commit path mirrors `onCommitCameraSettings()`
  (`llfloaterprismmanager.cpp:1293-1345`): read widgets → fill
  `settings.mBonePov` → `setCameraSettings()` (config commit bumps config
  revision → UI refresh). The per-frame *transform* still flows through the
  lightweight setter (§3.3); only the **config** goes through `setCameraSettings`.
- **§5.2 legibility:** when `mAimMode == full-follow`, grey the operator
  orientation affordances for this capture; when stabilized, grey nothing.

### 8.3 Persistence — additive, no version bump

**Storage:** add `BonePovSettings mBonePov;` to `CameraSettings`
(`llprismlens.h:125-164`), default-constructed = disabled and byte-inert (same
idiom as every sibling sub-struct there).

```cpp
struct BonePovSettings          // POD only — no avatar pointers
{
    bool  mEnabled     = false; // default OFF => byte-identical to today
    U8    mAnchorSlot  = 0;     // 0=Me,1=A,2=B,3=C,4=D
    U8    mJointSel    = 0;     // 0=Head,1=Eye,2=Neck,3=Custom
    U8    mAimMode     = 0;     // 0=FullFollow (default), 1=Stabilized
    U8    mRollMode    = 0;     // 0=HorizonLock (default), 1=Inherit
    bool  mScaleAware  = true;
    LLVector3 mOffset;          // metres, joint-local
    F32   mTrimPitchDeg = 0.f;
    F32   mTrimYawDeg   = 0.f;
    F32   mFovDeg       = 60.f;
    F32   mSmoothingSec = 0.15f;
    std::string mCustomJoint;   // used only when mJointSel==Custom
};
```

**Serialize** an additive `bone_pov` sub-map into each `prism_captures` item,
inserted at `llprismlens.cpp:3808` (right after `virtual_rot`), and parse it
back beside `:4079-4092`. Because it is **optional on read** (absent → defaults
→ disabled), pre-existing scenes round-trip unchanged and **`SCENE_VERSION` need
not bump** (`llfloaterdirector.cpp:885`). The Director save
(`llfloaterdirector.cpp:886-888`) copies `prism_captures` wholesale, so the
bone config rides along with **zero new save/load wiring**.

**New `gSavedSettings` keys:** none required for scene round-trip. Optionally add
one boolean `PrismBonePovScaleAwareDefault` (default true) if a global default
for new attaches is desired — a single additive setting, matching the light
rig's `CineLightRigScaleAware` (`alcinelightrig.cpp:1115-1116`). Recommend
**deferring even that**: the per-capture `mScaleAware` default of `true` covers
it.

---

## 9. Pure-helper math section (TUT-testable)

All four extracted into `alvcambonepovmath.h` (namespace `ALVCamBonePovMath`),
header-only pure functions like `algazemath.h`, so `tests/` can link them with
no viewer singletons. **Do not** call the file-static originals (§4.2); these
are the cited, tested canonical forms.

```cpp
// 9.1 foot-pivot clone scale (== alcinelightrig.cpp:85-109 invariant)
LLVector3 boneScaledPoint(const LLVector3& jointPos, const LLVector3& footPos,
                          F32 scale);
    // scale==1 -> jointPos; else footPos + (jointPos-footPos)*scale
    // caller supplies footPos = rootWorld; footPos.z -= max(0,pelvisToFoot)

// 9.2 offset composition
LLVector3 boneEye(const LLVector3& base, const LLVector3& localOffset,
                  F32 scale, const LLQuaternion& jointRot);
    // base + (localOffset*scale)*jointRot

// 9.3 full-follow basis remap -> camera quaternion (fwd=-Z, up=+Y)
LLQuaternion followRotation(const LLVector3& fwdIn, const LLVector3& upIn);
    // right=fwd%up; up=right%fwd; normalize; LLQuaternion(right,up,-fwd)

// 9.4 horizon-lock re-level (returns up; false if degenerate)
bool horizonLevelUp(const LLVector3& fwd, LLVector3& up_out);
    // worldUp=(0,0,1); if |fwd.worldUp|>1-1e-3 return false
    // else right=fwd%worldUp; up_out=right%fwd; normalize; return true

// 9.5 smoothing step (position); rot uses LLQuaternion::slerp inline
LLVector3 smoothStep(const LLVector3& cur, const LLVector3& target,
                     F32 dt, F32 tau);
    // tau<=0 -> target; else cur + (target-cur)*(1-exp(-dt/tau))
```

Aim-trim is **not** re-implemented — it calls `ALGazeMath::eyelineOffsetDir`
(`algazemath.h:67-85`) directly.

### 9.6 §5-style test plan (`tests/alvcambonepovmath_test.cpp`)

Every test asserts an **independent hand-computed expected value** (no
self-comparison, no "compute-then-compare-to-the-same-compute" tautology). Each
FAILS on the specific regression named.

1. **`boneScaledPoint` identity.** `scale=1`, arbitrary joint/foot → output ==
   jointPos exactly. *Fails if* the no-op branch is dropped.
2. **`boneScaledPoint` half-clone.** foot=(0,0,0), jointPos=(0,0,2), scale=0.5 →
   expect (0,0,1). scale=0.05 → expect (0,0,0.1). *Fails if* the pivot is the
   root/pelvis instead of the foot, or scaling is applied to the wrong term.
3. **`boneEye` offset rotates with joint.** base=(1,0,0),
   localOffset=(0.2,0,0), scale=1, jointRot = 90° about +Z (so local +X → agent
   +Y) → expect (1,0.2,0). *Fails if* offset is added in agent space (would give
   (1.2,0,0)) or not rotated.
4. **`boneEye` offset scales with clone.** same as (3) but scale=0.5 → expect
   (1,0.1,0). *Fails if* offset magnitude ignores clone scale.
5. **`followRotation` render round-trip.** Feed fwd=(1,0,0), up=(0,0,1); take
   `q=followRotation(...)`; assert `(0,0,-1)*q ≈ (1,0,0)` and `(0,1,0)*q ≈
   (0,0,1)` to 1e-4. *Fails if* the axis mapping or the `-fwd` sign regresses —
   this is the **basis-remap guard** and the single most important test.
6. **`followRotation` yaw case.** fwd=(0,1,0) (facing agent +Y), up=(0,0,1) →
   assert `(0,0,-1)*q ≈ (0,1,0)`. Independent of test 5's axis.
7. **`horizonLevelUp` levels roll.** fwd=(1,0,0), any tilted up=(0,1,1)
   normalized → expect up_out=(0,0,1), and `up_out·(0,0,1) > up_in·(0,0,1)`
   strictly (roll removed). *Fails if* the projection is wrong or returns the
   input.
8. **`horizonLevelUp` degeneracy.** fwd=(0,0,1) → returns false (caller holds
   last up). *Fails if* the guard is missing (would divide-by-~0 / NaN).
9. **`horizonLevelUp` orthogonality.** for a generic non-vertical fwd, assert
   `|up_out·fwd| < 1e-5`. *Fails if* re-orthonormalization regresses.
10. **`smoothStep` snap.** tau=0 → returns target exactly regardless of cur.
    *Fails if* the `tau<=0` snap branch regresses (would lerp from stale).
11. **`smoothStep` half-life.** cur=0, target=10, choose dt=tau → expect
    `10*(1-e^-1) ≈ 6.3212`. *Fails if* the alpha formula regresses (e.g. uses
    `dt*tau`).
12. **`smoothStep` convergence direction.** two steps move monotonically toward
    target and never overshoot. *Fails if* alpha exceeds 1 / sign error.
13. **eyeline forward (integration-style, pure inputs).** feed two eye rotations
    ±10° yaw about +Z; expect the averaged normalized forward to lie on the
    bisector (agent +X) to 1e-4. *Fails if* the average/normalize regresses.

---

## 10. §6-style file/function checklist

### Create
- `indra/newview/alvcambonepov.{h,cpp}` — `ALVCamBonePov` singleton: `tick()`,
  per-capture `Smoother`, joint resolve, calls into §9 helpers, writes via
  `setVirtualCameraTransform`.
- `indra/newview/alvcambonepovmath.h` — the five pure helpers (§9).
- `indra/newview/tests/alvcambonepovmath_test.cpp` — the §9.6 plan.

### Modify (additive, default-inert)
- `llprismlens.h` — add `struct BonePovSettings` and `BonePovSettings mBonePov`
  in `CameraSettings` (`:125-164`); declare `setVirtualCameraTransform` (near
  `:471`).
- `llprismlens.cpp` — implement `setVirtualCameraTransform` (runtime-revision
  bump only); serialize `bone_pov` at `:3808`; parse at `:4092`; validate in
  `validCameraSettings` (mirror `:3360-3369`).
- `llappviewer.cpp` — one `ALVCamBonePov::instance().tick(...)` call after
  `:5526`.
- `llfloaterprismmanager.{h,cpp}` — bone-attach controls + commit wiring in the
  camera editor (mirror `onCommitCameraSettings`, `:1293-1345`); grey operator
  orientation controls in full-follow (§5.2).
- `skins/default/xui/en/floater_prism_manager.xml` — controls under `:368-370`.
- `indra/newview/CMakeLists.txt` — add the new sources + test.

### NOT touched (see §11)
- `renderAuxiliaryView()` render/consume path and its guide reader — read-only
  precedent only (`llprismlens.cpp:5447-5599,5220-5253`).
- Prism composite shaders / pipeline / `llprimitive`.
- `lldirectorcast.*` — consumed read-only via `resolve*()`.
- `alcinelightrig*` / `algazemath.h` / lens-gaze — helpers *cited and reused*
  (`eyelineOffsetDir`, `chaseAlpha`), never modified. The foot-pivot invariant
  is re-expressed in §9.1, not edited in place (§4.2).

---

## 11. §7-style risks

| # | Risk | Severity | Mitigation |
|---|------|----------|------------|
| R1 | **Basis-remap wrong** (fwd/up/right or sign) → camera points sideways/upside-down | **Highest** | Build with the *identical* `LLQuaternion(right,up,-fwd)` the snap path uses (§1.6); test 5/6 assert the render round-trip on independent values. Reuse render's own re-orthonormalization shape. |
| R2 | **`up = +Z` assumption** (IMPLIES, §5.1) wrong for some skeleton | Medium | Only affects *inherit-roll*; horizon-lock (default) discards `up0`. Builder can confirm against one posed avatar; worst case is a roll offset in the non-default mode. |
| R3 | **Anchor/joint loss mid-shot** → snap or NaN | High | Hold-last + `mHave=false` (§3.4); setter rejects non-finite; never fall back to self/identity. |
| R4 | **Clone-scale offset** wrong pivot (root vs foot) or unscaled | Medium | §9.1 foot-pivot, tested (tests 2–4) against the same numbers the light rig/gaze produce; `sanitize` clamps 0.05×/20×. |
| R5 | **Smoothing snap** from stale transform on mode/anchor/joint switch | Medium | Fingerprint-driven `mHave=false` reset (§7.1); slerp seeded from current `mVirtualRot` on stabilized→follow (§5.2). |
| R6 | **Live mode toggles** (aim/roll/anchor) cause discontinuity | Medium | All are snap conditions (§7.1); orientation hand-off seeds the smoother (§5.2). |
| R7 | **Operator/driver contention** in stabilized mode | Medium | Driver writes only `mVirtualPos` in stabilized; only `mVirtualRot` untouched (§5.2). Full-follow greys operator aim UI (§8.2). |
| R8 | **Per-frame config churn** if written through `setCameraSettings` | Low | Dedicated `setVirtualCameraTransform` bumps runtime revision only (§3.3); config commits stay on the settings path. |
| R9 | **Hook ordering** vs skeleton pose | Low | Idle site is proven fresh (§1.2); fallback to the pre-`renderAuxiliaryView` site documented (§3.2). |
| R10 | **Scene compat** | Low | `bone_pov` optional-on-read, default-disabled → no version bump; pre-virtual scenes already round-trip (`llprismlens.cpp:3792-3794`). |

---

## 12. §9-style OFF-LIMITS

- **Prism capture RENDER path + shaders.** Drive the existing `mVirtualPos/
  mVirtualRot` only; do not change how `renderAuxiliaryView()`
  (`llprismlens.cpp:5556-5599`) or the composite shaders consume them. The new
  setter writes the same fields the render already reads.
- **Pipeline / `llprimitive` / shadow-light code.** Untouched.
- **`lldirectorcast.*` internals.** Read-only via `resolve()` /
  `resolveSubject*()`; no membership, subject, or transport mutation.
- **Light-rig subsystem (`alcinelightrig*`, `alcinelightrigmanager*`).** Read
  `scaledPoint`/smoother/facing as *pattern*; do not modify. The invariant is
  re-expressed in §9, not edited (§4.2 explains why: the originals are
  file-static and unlinkable).
- **Lens-gaze subsystem (`algazemath.h`, `alpanellensgaze.*`, actor-mover gaze
  block `llactormover.cpp:3010-3267`).** Reuse `eyelineOffsetDir`/`chaseAlpha`
  and the eye-averaging pattern; do not modify.

---

## 13. Anything in the seed I think is wrong

Nothing is overturned. Three refinements, none contradicting a fixed decision:

1. **Persistence — no new settings needed.** The seed floats "settings keys
   and/or the Director scene round-trip". Verified: the Director scene already
   copies `prism_captures` wholesale (`llfloaterdirector.cpp:886-888`), so a
   single additive `bone_pov` sub-map gives full round-trip with **zero** new
   `gSavedSettings` keys and **no version bump**. Recommend not adding settings
   at all (§8.3). This is *stronger* than the seed's "minimal new settings",
   not a contradiction.
2. **Write path.** The seed says "writes `mVirtualPos/mVirtualRot` … BEFORE the
   capture render reads them" — correct — but writing through the existing
   `setCameraSettings` would bump the configuration revision every frame and
   thrash the UI. The design adds a **runtime-only** setter (§3.3). This is an
   addition the seed did not specify but does not conflict with it, and it keeps
   the render consumer byte-identical.
3. **`up = +Z` is an inference, not proven.** The seed treats the joint→camera
   remap as "get the axis mapping right" and cites the `atan2` facing precedent
   — which proves **forward = +X** but not **up = +Z** (§5.1, R2). The design
   flags this explicitly and bounds the exposure to inherit-roll only. Worth a
   one-avatar empirical check during build; it does not change any user
   decision.

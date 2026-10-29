# Actor Gaze SL Animation Priority — Hybrid strict-yield gate: implementation plan

Companion to `docs/gaze_animation_priority_research.md`. This plan turns that research
into an ordered, buildable change list. **All file:line anchors below were re-verified
against the current working tree of `feature/cine-light-rig` (2026-08-22), not the
research doc's commit** — the doc's `llactormover.cpp` anchors are uniformly ~40–56
lines low (see "Doc corrections", item C3).

Goal: gaze gains a user-selectable SL animation priority (0–6) plus a **Legacy final**
default. Immediately before every gaze joint rotation write, gaze yields (skips the
write) when the highest non-additive rotation priority that actually reached that
joint in this frame's motion blend is **strictly greater** than the selected gaze
priority. Equal or lower priority: the existing write happens unchanged. Legacy final
(-1) never consults priority — byte-identical to today.

---

## 0. Verified architecture summary (current tree)

Frame order (`indra/newview/llvoavatar.cpp`):

- Director render-pose restore: `llvoavatar.cpp:5121-5124` (raw restore writes at
  `llactormover.cpp:3915-3925`, `setRotation` at 3921).
- LOD-minimal early-out: `llvoavatar.cpp:5148-5156` — `updateMotions(HIDDEN_UPDATE)` +
  `applyDirectorLookAt()` only; no `applyGaze()`, no new blend
  (`llcharacter.cpp:181-200` → `llmotioncontroller.cpp:943-956`).
- Motion update: `llvoavatar.cpp:5211-5224` → `LLMotionController::updateMotions()`
  (`llmotioncontroller.cpp:849-937`; `blendAndApply()` at 931; paused-without-force
  path at 911-914 skips the blend entirely; the disabled quantum path uses
  `blendAndCache()` at 927).
- Post-motion gaze arbitration: `llvoavatar.cpp:5226-5232` — `applyDirectorLookAt()`
  gets first refusal, else `applyGaze()`.

Both `applyGaze()` (`llactormover.cpp:3675`, call at 3795) and `applyDirectorLookAt()`
(`llactormover.cpp:4236`, call at 4683) funnel into the single
`LLActorMover::gazePaint()` (`llactormover.cpp:4722`). Inside `gazePaint()`:

- Ownership-scope resolution ("priority" today): `llactormover.cpp:5237-5248`
  (`gaze_priority` resolve 5237-5240, `override_head_eyes`/`override_upper_body`
  5241-5244, `priority_env` 5248).
- **Motion-program write block** (`gaze_motion_programs` gate, 5299): pelvis
  5522-5539 (`setRotation` 5531/5536), torso 5541-5563 (5554/5559), neck 5564-5611
  (5601/5607), head 5612-5660 (5651/5657), eyes via `applyMotorEye` lambda 5770-5802
  (`setRotation` 5791/5796), applied to `mEyeLeft`/`mEyeRight`/`mFaceEyeAltLeft`/
  `mFaceEyeAltRight` at 5799-5802.
- **Legacy write block**: pelvis 6101-6122 (6113/6118), torso 6123-6144 (6135/6140),
  neck 6146-6194 (6184/6190), head 6195-6248 (6238/6244), eyes via `applyEye` lambda
  6286-6347 (`setRotation` 6340/6345), applied to the same four eye joints 6348-6351.
- Blink/lid **visual params** (not joint rotations): 6354-6394.
- All smoothing / envelope / motor / micro-life state advances BEFORE the write
  blocks (direction smoothing 5150-5172, envelopes 5177-5248, dead-zone body-aim
  chase 5249-5287, `ALGazeMotor::step` 5366-5367, legacy applied-slew mirror
  5380-5435, natural breaks/micro-life 5970-6089), so skipping only `setRotation`
  automatically satisfies the "keep advancing state while yielding" requirement.

Motion-blend machinery (`indra/llcharacter/`):

- `LLJoint::JointPriority` (**not** `EJointPriority`): `lljoint.h:93-103`; values -1
  (`USE_MOTION_PRIORITY`), 0-4 named, 5-6 unnamed valid, 7 = `ADDITIVE_PRIORITY` =
  `LL_CHARACTER_MAX_PRIORITY` (`lljoint.h:54`).
- `LLJointStateBlender`: `llpose.h:83-101` (6 slots, `JSB_NUM_JOINT_STATES` at
  `llpose.h:81`; parallel `mPriorities[]` / `mAdditiveBlends[]` at 88-89).
  `addJointState` inserts descending by priority, equal keeps existing ahead
  (`llpose.cpp:196-233`). `blendJointStates` (`llpose.cpp:238-400`): zero-weight
  states skipped at 279-282 (exact `== 0.f`), additive composed separately 284-309,
  regular ROT weight-capped blend 353-369, transforms applied 386-390, `mJointStates`
  cleared at 392-399 (only when `apply_now`).
- `LLPoseBlender`: `llpose.h:105-137`; `mJointStateBlenderPool` (`llpose.h:110`) is a
  `std::map<LLJoint*, LLJointStateBlender*>` — **persistent across frames**, populated
  in `addMotion` (`llpose.cpp:471-505`, keyed by `LLJoint*` at 479-489, effective
  priority passed at 496 via `LLMotion::getJointPriority`, `llmotion.h:107-115`,
  which never returns `USE_MOTION_PRIORITY`), destroyed only in the destructor
  (`llpose.cpp:462-466`). `mActiveBlenders` is per-frame, cleared in `blendAndApply`
  (`llpose.cpp:510-521`).
- Controller owns one `LLPoseBlender mPoseBlender` per avatar
  (`llmotioncontroller.h:215`, protected); avatar access via
  `LLCharacter::getMotionController()` (`llcharacter.h:172`).

---

## 1. Step 1 — llpose observer (passive; no behavior change)

### 1.1 `LLJointStateBlender` fields (`indra/llcharacter/llpose.h:83-101`)

```cpp
// Observation of the last real blend: highest NON-ADDITIVE rotation priority
// that actually contributed to this joint, stamped with the pose blender's
// blend serial so a superseded observation (joint no longer animated) reads
// as invalid instead of blocking forever.
S32  mLastRegularRotPriority = LLJoint::USE_MOTION_PRIORITY;
bool mLastRegularRotValid    = false;
U32  mLastRegularRotSerial   = 0;
```

Initialize in the constructor next to the existing slot init (`llpose.cpp:180-186`).

### 1.2 Recording (`llpose.cpp:238-400`, `blendJointStates`)

- Extend the signature: `void blendJointStates(bool apply_now = true, U32 serial = 0);`
  (only `llpose.cpp` calls it — verified: call sites at 516 and 536 only).
- At the start of the state loop (before 271), init a local
  `S32 max_regular_rot = LLJoint::USE_MOTION_PRIORITY; bool found = false;`.
- Inside the existing loop, alongside the current usage/weight reads (276-277),
  record when ALL of:
  - `!mAdditiveBlends[joint_state_index]` (excludes additive P7 — matches the
    additive branch split at 284/310);
  - `current_usage & LLJointState::ROT`;
  - `current_weight > GAZE_OBS_ROT_WEIGHT_EPSILON` (see 1.4);
  then `max_regular_rot = llmax(max_regular_rot, mPriorities[joint_state_index]); found = true;`.
  `mPriorities[i]` is exactly what `addMotion` passed (`llpose.cpp:496`) — the
  resolved effective priority, never -1.
- **After** the loop but regardless of `found`, and before the `apply_now` clear at
  392-399: `mLastRegularRotPriority = max_regular_rot; mLastRegularRotValid = found;
  mLastRegularRotSerial = serial;`. Stamping even when `found == false` matters: a
  joint blended this frame with POS-only or additive-only contributions must read
  "no regular ROT contributor → allow", not resurrect an older observation.
- Record on BOTH `apply_now` paths (`blendAndApply` and the currently-disabled
  quantum `blendAndCache`, SL-763 — `llmotioncontroller.cpp:852-856`), so the
  observer stays correct if quantum is ever re-enabled.

### 1.3 `LLPoseBlender` serial + const query (`llpose.h:105-137`)

```cpp
U32 mBlendSerial = 0;   // increments once per real blend pass

// Returns true and sets priority_out only when `joint` received at least one
// non-additive rotation contribution in the MOST RECENT blend pass. False =>
// no data / stale / never animated => caller must allow the gaze write.
bool getLastRegularRotationPriority(const LLJoint* joint, S32& priority_out) const;
```

- `blendAndApply()` (`llpose.cpp:510-521`) and `blendAndCache()` (526-538): increment
  `mBlendSerial` first, pass it to each `blendJointStates(...)` call.
- Query implementation: `mJointStateBlenderPool.find(const_cast<LLJoint*>(joint))`;
  return `it != end && b->mLastRegularRotValid && b->mLastRegularRotSerial == mBlendSerial`.
- Serial semantics (this is the plan's one substantive addition over the research
  doc — see Doc corrections C2):
  - AO playing → each `blendAndApply` re-stamps the joint with the new serial → valid.
  - AO **stops** → next `blendAndApply` increments the serial but never touches that
    joint's blender → stale serial → query false → gaze allowed again. Without the
    serial, the last observation (e.g. P4) would suppress gaze on that joint forever.
  - **LOD-minimal frames** (`updateMotionsMinimal`, no blend) and **paused** frames
    (`llmotioncontroller.cpp:911-914`) don't advance the serial → the last real
    observation remains valid, matching the equally un-reblended skeleton pose.
  - Fresh avatar / never blended → pool miss or valid=false → allow.

### 1.4 Epsilon

`constexpr F32 GAZE_OBS_ROT_WEIGHT_EPSILON = 0.001f;` in `llpose.cpp` with a comment.
The blender itself only skips exact `0.f` (`llpose.cpp:279`); 0.001 matches the gaze
code's own activity thresholds (`wBody > 0.001f` etc., `llactormover.cpp:5516`) and
prevents an easing-out sliver-weight motion from strictly blocking gaze. Documented
divergence: a 1e-4-weight P6 motion fractionally touched the pose but will not block.

---

## 2. Step 2 — motion controller forwarder

`indra/llcharacter/llmotioncontroller.h`, public, next to `clearBlenders()`
(`llmotioncontroller.h:146`):

```cpp
// Narrow gaze-yield observer: true only when `joint` got a non-additive
// rotation contribution in the most recent real blend. Never reports the
// additive P7 channel. False => caller must allow (compatibility default).
bool getLastAppliedRegularRotationPriority(const LLJoint* joint, S32& priority) const
{ return mPoseBlender.getLastRegularRotationPriority(joint, priority); }
```

Reads only `mPoseBlender` (`llmotioncontroller.h:215`). Do **not** expose
`mJointSignature` (optimization masks; partial-weight motions bypass the merge —
`llmotioncontroller.cpp:595-638`) and do not add any active-motion-scan API.

---

## 3. Step 3 — llactormover gating

### 3.1 Model fields (`indra/newview/llactormover.h`)

- `GazeTarget` (`llactormover.h:337-384`): add
  `S32 mAnimPriorityOverride = -2;  // -2 inherit global, -1 Legacy final, 0..6 SL priority`
  next to `mGazePriorityOverride` (382).
- `Gaze` (`llactormover.h:934+`): add the same field next to `mGazePriorityOverride`
  (965), default -2.
- **Sentinel caution:** every other override in these structs uses -1 = inherit
  (e.g. 362-383). Here -1 is a *meaningful value* (Legacy final) so inherit is -2.
  Reset buttons must write -2, not -1 (see 5.3) — flagged as an easy copy-paste bug.

Propagate through all four existing copy/equality sites (verified):

| Site | File:line |
|---|---|
| `operator==` on GazeTarget | `llactormover.cpp:498` (add a line beside `mGazePriorityOverride`) |
| `setGazeTargetConfig` | `llactormover.cpp:910` |
| `getGazeTargetConfig` | `llactormover.cpp:1294` |
| Director copy into runtime gaze | `llactormover.cpp:4454` |

### 3.2 Resolve + clamp (once, in `gazePaint`)

Next to the existing ownership resolution at `llactormover.cpp:5237-5244`:

```cpp
static LLCachedControl<S32> gaze_anim_priority_global(
    gSavedSettings, "DirectorGazeAnimationPriority", -1);
const S32 gaze_anim_priority = g.mAnimPriorityOverride >= -1
    ? llclamp(g.mAnimPriorityOverride, -1, 6)
    : llclamp(static_cast<S32>(gaze_anim_priority_global), -1, 6);
```

-1 = Legacy final. Clamp ceiling is 6: 7 is `ADDITIVE_PRIORITY`
(`lljoint.h:54,102`), never a selectable normal priority.

### 3.3 Allowed-alpha helper

File-static in `llactormover.cpp` (near `currentGazePriority()`,
`llactormover.cpp:3565`), single authority for every write:

```cpp
// Hybrid strict-yield gate. Returns base_alpha unchanged when the write is
// allowed, 0.f when the joint must yield. Legacy final (-1) and any
// missing/stale observation return base_alpha (never suppress on stale).
static F32 gazeAllowedAlpha(LLVOAvatar* av, LLJoint* joint,
                            F32 base_alpha, S32 selected_priority)
{
    if (selected_priority < 0 || !av || !joint)
    {
        return base_alpha;                          // Legacy final / no data
    }
    S32 observed = 0;
    if (!av->getMotionController()
            .getLastAppliedRegularRotationPriority(joint, observed))
    {
        return base_alpha;                          // stale/missing => allow
    }
    return observed > selected_priority ? 0.f : base_alpha;
}
```

Because it returns `base_alpha` *unchanged* (same float) or exactly 0, the allowed
path is bit-identical to today, and Legacy final short-circuits before any query.

### 3.4 Application — every `setRotation` in BOTH write blocks

Pattern (identical at all ten sites): compute
`const F32 a = gazeAllowedAlpha(av, joint, <existing alpha expr>, gaze_anim_priority);`
then `if (a > 0.f) { joint->setRotation(nlerp(a, ... /* unchanged */)); }`. The
existing alpha expressions are `priority_env` on the ownership branches and
`wCueBody` / `wCueHead` / `wEye` on the blend branches. **Only the `setRotation` call
is skipped**; every surrounding computation stays.

| Joint | Motion-program block | Legacy block |
|---|---|---|
| mPelvis | `llactormover.cpp:5531` (owned) / `5536` (blend) | `6113` / `6118` |
| mTorso | `5554` / `5559` | `6135` / `6140` |
| mNeck | `5601` / `5607` | `6184` / `6190` |
| mHead | `5651` / `5657` | `6238` / `6244` |
| eyes ×4 | inside `applyMotorEye` `5791` / `5796` (called 5799-5802) | inside `applyEye` `6340` / `6345` (called 6348-6351) |

Details that must hold:

- **Eyes are gated per eye joint** — the query goes inside the lambdas with the
  lambda's own `eye` pointer, so a `.anim` that keys only `mEyeLeft` yields only that
  joint. Alt Bento eyes are independent joints and get independent decisions.
- **Legacy `applyEye` computes `lid_follow_pitch` / `have_lid_follow_pitch` inside
  the lambda** (`llactormover.cpp:6317-6323`) *before* its `setRotation`. The gate
  wraps only 6340/6345 — never early-return from the lambda — or lid-follow breaks
  when eyes yield. (Motor branch computes lid follow before the lambda, 5760-5762,
  so it is naturally safe.)
- **State keeps advancing while yielding**: all envelope/smoothing/motor/micro-life
  state is settled before the write blocks (see §0), so a joint that resumes after
  yield resumes from current state, not a stale pose — no extra work needed beyond
  skipping only `setRotation`.
- **VOR/parent-cancellation stay correct by construction**: the eye solves read
  `head->getWorldRotation()` *after* the head write-or-skip
  (`llactormover.cpp:5740`, `6276`), and the neck/head ownership math cancels the
  *actual* parent world rotation (`5596-5599`, `5646-5649`, `6180-6182`,
  `6234-6236`), so mixed allow/yield across the chain (torso yields, head allowed;
  head yields, eyes allowed) resolves correctly.
- **Do not gate**: the block-entry conditions (`5516`, `5564`, `5612`, `5725`,
  `6093`, `6146`, `6195`, `6256`) — entering the block and skipping the write is
  fine; the Director capture/restore bracket (`3841-3907`, restore 3909-3947) — when
  a joint yielded, capture == current, restore is a harmless identical write; the
  Director body-turn/root replant (`5465-5504`, `applyDirectorBodyTurn`); the
  blink/lid visual params (`6354-6394`).
- **LOD-minimal**: `applyDirectorLookAt` runs on skipped frames
  (`llvoavatar.cpp:5148-5156`) with no new blend; the serial scheme (§1.3) keeps the
  last real observation valid there, and any truly missing sample allows the write.

---

## 4. Step 4 — rename / migration (ownership scope)

### 4.1 What "Ownership scope" renames

`EGazePriority` / `GAZE_PRIORITY_*` describe joint-ownership scope, not SL priority
(`llactormover.h:327-335`, settings comment `settings.xml:5344`). Rename:

- `EGazePriority` → `EGazeOwnershipScope`; `GAZE_PRIORITY_BLEND/HEAD_EYES/UPPER_BODY`
  → `GAZE_SCOPE_BLEND/HEAD_EYES/UPPER_BODY`. Keep `using EGazePriority = EGazeOwnershipScope;`
  + enumerator aliases for one release if any out-of-tree code exists; otherwise a
  clean rename (grep shows no other consumers).
- `GazeTarget::mGazePriorityOverride` → `mOwnershipScopeOverride` (`llactormover.h:382`);
  `Gaze::mGazePriorityOverride` → same (`llactormover.h:965`).
- `currentGazePriority()` → `currentGazeOwnershipScope()` (`llactormover.cpp:3565-3574`).

### 4.2 Complete verified site list (grep-confirmed; the research doc missed two)

| # | Site | File:line |
|---|---|---|
| 1 | enum + both struct fields | `llactormover.h:330-335, 382, 965` |
| 2 | GazeTarget `operator==` | `llactormover.cpp:498` |
| 3 | `setGazeTargetConfig` copy | `llactormover.cpp:910` |
| 4 | `getGazeTargetConfig` copy | `llactormover.cpp:1294` |
| 5 | **`currentGazePriority()` itself** (doc §6.2 omitted) | `llactormover.cpp:3565-3574` |
| 6 | **Director capture torso-scope test** (doc §6.2 omitted) | `llactormover.cpp:3881-3886` |
| 7 | Director runtime copy | `llactormover.cpp:4454` |
| 8 | `gazePaint` resolution + flags | `llactormover.cpp:5237-5244` |
| 9 | Panel member + lookup | `alpanellensgaze.h:111`, `alpanellensgaze.cpp:290` |
| 10 | Panel commit wiring + handler | `alpanellensgaze.cpp:343`, `1119-1128`, decl `alpanellensgaze.h:64` |
| 11 | Panel reset button | `alpanellensgaze.cpp:486-491` |
| 12 | Panel enable + refresh | `alpanellensgaze.cpp:837`, `859-871` |
| 13 | XML control names/labels | `panel_lens_gaze.xml:90-104` |
| 14 | Settings key comment (and key, if Option B) | `settings.xml:5341-5351` |

Site 6 is behavioral, not just naming: torso capture is keyed to Upper-Body scope.
It stays keyed to **scope**, never to the new SL priority.

### 4.3 Settings key migration — two options

- **Option A (recommended, zero risk): keep the stored key name `DirectorGazePriority`.**
  Only its `Comment` (settings.xml:5344) and every C++/XML label change to
  "ownership scope". Saved user settings keep working with zero migration code; the
  0-2 values keep their exact meaning. The key name is an internal string users
  never see.
- **Option B (doc's proposal): add `DirectorGazeOwnershipScope`** (S32, persist,
  default 1) and one-time migrate: at `LLActorMover` construction (or first
  `currentGazeOwnershipScope()` call), if
  `gSavedSettings.getControl("DirectorGazeOwnershipScope")->isDefault()` and
  `!gSavedSettings.getControl("DirectorGazePriority")->isDefault()`, copy old → new.
  Keep the old key declared (deprecated comment) so old user files load without
  warnings. Never reinterpret the old 0-2 values as SL priority.

Per-actor state needs no migration: `Gaze`/`GazeTarget` are session-only (no LLSD
serialization exists — grep-verified), and the compiled-in performance presets
(`alpanellensgaze.cpp:96-171`) never touch the scope override.

---

## 5. Step 5 — settings + UI

### 5.1 New settings key (`indra/newview/app_settings/settings.xml`, insert after 5351)

```xml
<key>DirectorGazeAnimationPriority</key>
<map>
  <key>Comment</key>
  <string>[Director] SL animation priority for Actor Gaze: -1 = Legacy final (always apply, ignores animation priority; default), 0-6 = gaze yields per joint to animations whose effective joint rotation priority is HIGHER than this. Equal priority favors gaze.</string>
  <key>Persist</key><integer>1</integer>
  <key>Type</key><string>S32</string>
  <key>Value</key><integer>-1</integer>
</map>
```

(Plus `DirectorGazeOwnershipScope` if Option B.) Default -1 makes the whole feature a
no-op until opted in — the branch's byte-identical-default principle.

### 5.2 `panel_lens_gaze.xml` (rows currently at 90-104 / 106-129)

- Retitle the existing row: label `gaze_priority_lbl` (91) → **"Ownership scope"**;
  tooltip → "How completely gaze owns its joints when it is allowed to write: Blend
  (partial), Head+Eyes, or Upper body. Does not consult animation priority." Rename
  `gaze_priority_combo`/`reset_gaze_priority` → `gaze_scope_combo`/`reset_gaze_scope`
  together with sites 9-12 above (or keep the names under Option A minimalism —
  pick one; do not mix).
- Insert a new row after it (top=206; every later widget's `top` shifts +26):
  label **"SL anim priority"**, `gaze_anim_priority_combo`, items:
  `Legacy final (always apply)` = -1, `0 (Low)` … `4 (Highest named)`, `5`,
  `6 (Maximum normal)`; tooltip: "Gaze yields on each joint to animations above this
  priority. Equal priority favors gaze. Legacy final ignores animation priority."
  Plus `reset_gaze_anim_priority` refresh button (tooltip "Reset to inherit the
  global default").
- Bump the panel's own `height` attribute by +26, and in `floater_actor_gaze.xml`
  bump the embedded panel height (`floater_actor_gaze.xml:37`, 1346 → 1372) and the
  scroll-content panel height (`:27`, 1354 → 1380). The floater embeds the panel with
  no code of its own (`floater_actor_gaze.xml:24-42`) — no other floater change.

### 5.3 `alpanellensgaze.h/.cpp` — mirror the existing scope-row plumbing exactly

- `alpanellensgaze.h`: add `LLComboBox* mAnimPriority = nullptr;` (near 111) and
  `void onAnimPriorityCommit();` (near 64).
- `postBuild`: `getChild` (pattern at 290), commit callback (pattern at 343), reset
  callback (pattern at 486-491) — **reset writes `mAnimPriorityOverride = -2`**
  (inherit), NOT -1 (that would force Legacy final).
- `onAnimPriorityCommit()` (pattern at 1119-1128): clamp the combo value to
  [-1, 6], `editGazeTargetsFor(editActors(), ...)` writing the per-actor override.
- Refresh (pattern at 859-871): resolved display =
  `slot_target.mAnimPriorityOverride >= -1 ? clamp(override, -1, 6) :
  clamp(gSavedSettings.getS32("DirectorGazeAnimationPriority"), -1, 6)`. Note the
  condition is `>= -1`, not `>= 0` as the scope row uses — -1 is a real selection
  here. Enable with `have_slot` (pattern at 837).
- Per-actor vs global matches the existing control: the combo edits the selected
  actors' override; the global is the settings key (Debug Settings), shown when the
  override is inherit; reset returns to inherit.
- Keep the two controls fully independent: SL priority never widens ownership scope.

---

## 6. What stays untouched

- **Blink/lid visual params** (`llactormover.cpp:6354-6394`, capture/restore params
  at 3902-3906 / restore_param 3927-3938): visual parameters, not `LLJointState`
  rotations — outside SL joint-priority semantics by definition. Document this.
- **Director root/body replanting** (`applyDirectorBodyTurn`, motor-branch
  reintegration `llactormover.cpp:5465-5504`; root capture 3875-3878): root
  ownership, separate feature; unchanged in v1.
- Director capture/restore bracket and its LOD release-gate call
  (`llvoavatar.cpp:5148-5156`).
- `LLMotionController` blending itself: the observer only reads inside
  `blendJointStates`; ordering, weights, signatures, and the six-slot limit are
  untouched. No new `LLMotion` is created (Path A explicitly rejected).
- The additive channel: additive states are excluded from the observation, so a
  poser/additive P7 motion never reads as a blocker.

---

## 7. Build/verify order, risks, test matrix

### 7.1 Ordered, buildable steps

1. **llcharacter observer** — `llpose.h`, `llpose.cpp`, `llmotioncontroller.h`
   (Steps 1-2). Builds alone; purely passive bookkeeping. Runtime behavior
   byte-identical (a few stores per blended joint per frame).
2. **Gate wiring, default-off** — `settings.xml` (new key), `llactormover.h`
   (fields), `llactormover.cpp` (§3.1-3.4). Builds alone. With the default -1 and
   overrides at -2, `gazeAllowedAlpha` short-circuits before any query — every write
   uses the same float alpha as today → **expected byte-identical default**.
3. **Rename/migration** — §4 sites 1-14. Mechanical; compile-verified by the
   compiler finding every use (fields/enum renames break the build until all sites
   are converted — use the table, then grep `GazePriority` to confirm zero
   stragglers outside the deprecated alias/settings key).
4. **UI** — §5.2-5.3. Builds alone.
5. **Tests** — §7.3; then the frame-capture legacy A/B.

Files touched, complete list: `indra/llcharacter/llpose.h`, `llpose.cpp`,
`llmotioncontroller.h`; `indra/newview/llactormover.h`, `llactormover.cpp`,
`alpanellensgaze.h`, `alpanellensgaze.cpp`,
`app_settings/settings.xml`,
`skins/default/xui/en/panel_lens_gaze.xml`,
`skins/default/xui/en/floater_actor_gaze.xml`; tests under
`indra/llcharacter/tests/` (new `llposeobserver_test.cpp` beside
`lljoint_test.cpp`) and `indra/newview/tests/` (beside the six existing
`algaze*_test.cpp` suites).

### 7.2 Risks + mitigations

| Risk | Mitigation |
|---|---|
| Stale observation suppresses gaze forever after the AO stops (joint never re-blended) | Blend-serial stamp (§1.3): superseded observation → query false → allow. This is the plan's required addition over the research doc. |
| Strict yield drops gaze during a higher-priority motion's ease-in/out (partial weight) | Documented conservative v1 behavior; the observation records the priority regardless of weight ≥ epsilon. An approximate attenuation mode can come later. |
| Equal-priority tie always favors gaze (not SL start-order) | Documented in tooltip/help; strict `>` comparison; only Path A could do true tie lifecycle. |
| Additive P7 mistaken for a normal blocker | `mAdditiveBlends[]` excluded at the record site; unit test 5. |
| A write site missed (10 `setRotation` sites, 4 eye joints ×2 blocks) | Single `gazeAllowedAlpha` helper; the site table in §3.4 is the code-review checklist; mixed per-joint test cases. |
| Legacy applyEye lid-follow broken by gating the lambda instead of the write | Gate exactly `llactormover.cpp:6340/6345`; keep 6317-6323 unconditional; blink regression test. |
| -1/-2 sentinel confusion (inherit vs Legacy final) | -2 inherit documented at the field; reset writes -2; refresh condition `>= -1`; policy unit test for both sentinels. |
| Settings rename breaks saved scenes | Option A keeps the stored key; Option B migrates via `isDefault()` once and never reinterprets 0-2 as SL priority. |
| Zero/sliver-weight state blocks gaze | 0.001 epsilon at the record site (§1.4). |
| Only some parents yield → child dragged/double-applied | Per-joint gating + existing actual-parent cancellation (verified §3.4); mixed parent/child matrix rows. |
| Quantum blend path (SL-763) re-enabled later | Observer records on both `blendAndApply` and `blendAndCache` paths with the same serial. |
| Embedded-panel layout clipping after the new row | Heights bumped in all three places (§5.2); visual check of the floater at min size. |

### 7.3 Test matrix

Unit — pose observer (`indra/llcharacter/tests/`, new suite):
1. Full-weight P4 + P3 ROT states → records P4, valid.
2. Partial-weight P4 (0.4) + P3 → still records P4 (strict-yield source).
3. Zero-weight and sub-epsilon states → not recorded; POS-only / non-ROT → valid=false stamp.
4. Additive P7 + normal P2 → records P2 only.
5. >6 candidate states → observation reflects only the six retained slots.
6. Serial: blend joint at serial N, blend *other* joints at serial N+1 → query for
   the first joint returns false (stale); re-blend → true again; no blend at all
   (minimal/paused emulation) → previous observation still returned.
7. `blendAndCache` path records identically.

Unit — gate policy (`indra/newview/tests/`, beside `algazepolicy_test.cpp`):
- Legacy final (-1) returns base alpha without a query; observed P4 vs selected P3 →
  0; selected P6 vs observed 0-6 → base; missing/stale sample → base; sentinel -2
  resolves to the global; clamps at both ends (7 → 6, -5 → -1... per §3.2 clamp).
- Run the six existing gaze suites (`algazemath/motor/policy/recruit/noise/blink`)
  unchanged.

In-world matrix (per research doc §8.2, all still valid): joint-keyed test anims at
P2-P6; for each gaze priority P0-P6 verify lower→apply, equal→gaze wins, higher→
yield; mixed per-joint `.anim` (torso P4, head P2, gaze P3 → torso yields, head
follows gaze); an AO omitting head/neck never blocks those joints; all three
ownership scopes affect ownership strength only, never the comparison; additive
poser never blocks at any selected priority.

Transitions/regression (research doc §8.3, plus the new cases this plan adds):
- **AO stop while yielding**: gaze must resume the frame after the stopped motion
  leaves the blend (serial supersession), with no snap (state kept advancing).
- **Paused controller** and **LOD-minimal → detailed** round trips: decision stable,
  no stale suppression, no unrecovered Director capture.
- Both solver branches (motion-program gate on/off), classic + Bento alt eyes,
  per-actor `applyGaze` and Director `applyDirectorLookAt`.
- VOR/parent cancellation with head-blocked-eyes-allowed and torso-blocked-head-allowed.
- **Legacy default A/B**: with `DirectorGazeAnimationPriority = -1`, frame-captured
  joint quaternions must match the pre-change build byte-for-byte (the helper
  returns the original float alpha before any query).

---

## 8. Doc corrections (ranked)

- **C1 (must fix — incomplete rename enumeration).** Research §6.2 lists the
  copy/equality/resolution sites but omits **`currentGazePriority()` itself**
  (`llactormover.cpp:3565-3574`) and the **Director capture torso-scope test**
  (`llactormover.cpp:3881-3886`). Both consume the ownership scope and must be
  renamed; the capture site is behavioral (torso capture keyed to Upper-Body scope).
  The panel sites additionally include `alpanellensgaze.cpp:290/343/837` beyond the
  ranges the doc cites.
- **C2 (must fix — under-specified observation lifetime).** §6.1 says "on the
  LOD-minimal path... retaining the last observation matches the skeleton pose" and
  gestures at an "optional" generation counter. Retention alone is wrong in the
  common case where the blocking motion **stops**: that joint's blender is never
  visited again, so a recorded P4 would suppress gaze indefinitely. The blend-serial
  stamp (§1.3) is mandatory, not optional; it simultaneously yields the correct
  retain-on-minimal/paused behavior and the correct expire-on-next-blend behavior.
- **C3 (fix line anchors — systematic drift).** All `llactormover.cpp` anchors in the
  doc are ~40-56 lines low relative to the current tree (researched at 3341fc06):
  gaze arbitration is at `llvoavatar.cpp:5226-5232` (doc: 5223-5231); ownership
  resolution at `llactormover.cpp:5237-5244` (doc: 5188-5199); write blocks at
  ~5516-5803 and ~6091-6352 (doc: 5482-5747, 6064-6296); Director copy at 4454
  (doc: 4384-4422). Design conclusions are unaffected; raw line-following is not safe.
- **C4 (under-specified — legacy eye lambda).** The doc's "skip only `setRotation`"
  rule needs the concrete caveat that legacy `applyEye` computes
  `lid_follow_pitch`/`have_lid_follow_pitch` inside the lambda
  (`llactormover.cpp:6317-6323`) before its write; the gate must wrap only
  6340/6345, never early-return.
- **C5 (under-specified — epsilon).** The blender skips only exact `0.f`
  (`llpose.cpp:279`); the doc's "small, documented epsilon" needs a value. This plan
  fixes 0.001f and documents the (benign) divergence.
- **C6 (under-specified — paused controller).** `updateMotions` skips the blend when
  paused without force (`llmotioncontroller.cpp:911-914`); the doc never covers it.
  The serial scheme handles it (observation retained, matching the frozen pose).
- **C7 (minor — sentinel collision).** The doc proposes -2/-1/0..6 but doesn't flag
  that -1 = inherit everywhere else in `GazeTarget` (and the existing reset button
  writes -1, `alpanellensgaze.cpp:490`); the new reset must write -2. Cheap to get
  wrong, cheap to test.
- **C8 (minor — naming).** The priority type is `LLJoint::JointPriority`
  (`lljoint.h:93-103`), not `EJointPriority` (the doc itself already corrects the
  task's spelling; reconfirmed).
- **C9 (minor — migration mechanics).** "Alias or initialize once" is stated without
  a mechanism; §4.3 supplies both a zero-risk Option A (keep the stored key name)
  and a concrete `isDefault()`-based Option B.

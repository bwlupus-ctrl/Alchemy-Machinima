# Stepped Animation: the Animation engine, the fixed Display-only engine, and the keyframe-editor foundation (rev 1, 10/14/2026)

Supersedes the *engine* part of `doc/STEPPED_ANIMATION_DESIGN.md` (rev 2, "the overlay"). Everything else in that doc (page, settings, presets, scopes, per-actor overrides, the `alsteppedanim` pure core, the clock grid, adaptive rate, boil, impact hold, smear, ease, Roto Ink match, actor cap) **stays** and is referenced here as "SAD §n".

Implementation follows the repo `CLAUDE.md` process: Codex implements, Opus adversarial review to 0 must-fix, one fresh build at the end.

## 0. Decisions

| # | Decision | Source |
|---|---|---|
| P1 | **Two engines, one feature.** `StepAnimEngine` = 0 **Animation** (new, physical, **default**) or 1 **Display only** (the shipped overlay, kept and fixed). Both share the clock, scopes, overrides, presets, effects and the `alsteppedanim` core. Only the application point differs. | user, 10/14 |
| P2 | **Animation engine = step the animation itself.** Keyframe-family motions are evaluated only on ticks, at the exact grid time; everything that is not re-evaluated keeps its tick output. A per-avatar **latch** at the end of `updateCharacter` makes the whole drawn skeleton bitwise constant between ticks. Every reader of the skeleton (attachments, rigged palettes, cameras, ghosts, particles, picking) sees the stepped pose. There is no render-window restore. | user, 10/14 |
| P3 | **The motion lifecycle clock is never quantized.** `mAnimTime` keeps advancing exactly as stock. Only the *sample* time handed to the stepped motions is stepped. This keeps every server-visible event (self's `ANIM_REQUEST_STOP`) at stock timing. | this design (§2.3) |
| P4 | **Per-avatar playhead** (`LLAnimPlayhead`, owned by each `LLMotionController`) is the one clock that stepping, pause, step ±1, seek and the future editor all drive. Stepping = quantizing the playhead advance. | user, 10/14 |
| P5 | **Keyframe editor foundation** (data model, edit layer, local playback, export path) is designed here (§9). Phase 1 builds only the engine and the playhead API. Phase 2 = scrub UI. Phase 3 = key editing and export. | user, 10/14 |
| P6 | **Never both engines on one avatar.** Engine changes take effect at a frame boundary with a clean release (§2.8). | user, 10/14 |
| P7 | **The Display-only engine is fixed, not retired.** The attachment glitch and the "real-time avatar" double are root-caused in §1 and fixed in §10. Its use case: the camera (Bone Lock, VCam bone POV) and anything reading the skeleton in idle stay smooth while the drawn body steps. | user, 10/14 |
| P8 | **Viewer-local only.** Nothing new is sent to the server. The animation *set* is unchanged; only evaluation timing changes. | user |

---

## 1. How the shipped overlay fails (the Display-only engine)

### 1.1 The structural cause
The overlay keeps two poses per frame: the smooth pose (all of idle) and the held pose (only inside the render window, `ALSteppedAnimRuntime::applyHeld` at the end of `idle()`, `llappviewer.cpp:5780-5783`, then `endFrameRestore` after display, `llappviewer.cpp:1656-1659`). Every consumer of the skeleton must therefore be classified as "inside" or "outside" the window, and anything that **computes** from the skeleton outside the window and **draws** inside it shows the smooth, real-time pose on top of or next to the held body. The overlay design enumerated joints, palettes, rigid attachment matrices and name tags (SAD §3), but missed the derived state below.

### 1.2 Attachment glitch: root causes (code-cited)

**G1. Animesh attachments render from the smooth pose, and their own animation is never stepped (most likely the reported glitch).**
- An animesh attachment is drawn by its own `LLControlAvatar` skeleton. Its root is placed on the wearer's attach point by `LLControlAvatar::matchVolumeTransform()` (`llcontrolavatar.cpp:159`): `attach->getWorldPosition()/getWorldRotation()` (`:189-190`) → `mRoot->setWorldPosition/Rotation` (`:196-197`).
- That runs in the control avatar's own `updateRootPositionAndRotation` during `gObjectList.update` (`llvoavatar.cpp:5079-5083`), i.e. in idle, **before** `applyHeld`, so it reads the wearer's **smooth** attach point. Its skeleton world matrices are then built from that smooth root (`updateCharacter` → `mRoot->updateWorldMatrixChildren()`, `llvoavatar.cpp:5875`).
- Inside the window the only re-match is `LLDrawable::updateXform` (`lldrawable.cpp:727-734`), which calls `matchVolumeTransform()` but **never updates the control avatar's child world matrices**. Skin palettes read the cached joint world matrices (`LLJoint::getWorldMatrix4a`, `llskinningutil.cpp:153`), so the animesh is skinned at the smooth attach point while the body is drawn held.
- The overlay never processes the control avatar together with its wearer: `collectActors` walks `LLCharacter::sInstances` (`alsteppedanimruntime.cpp:176`) and resolves each control avatar as an independent actor. In Whole cast scope it is not a cast member, so it animates fully smoothly. In Everyone in view it steps with its **own** seed and phase (`:181-182`), on different frames from its wearer. The repo already learned this lesson for ghosts ("multi-animesh desync", `llghostavatar.h:129-133`).
- Visible as: animesh tails, wings, ears, pets and held animesh props swim or jitter off a held body. This is exactly "attachments glitching".

**G2. The restore re-syncs rigid attachments to a pose that was never drawn; idle readers then use it.**
- `restoreOverlay` calls `syncStepAnimAttachmentMatrices()` after restoring the smooth joints (`alsteppedanimruntime.cpp:595-601`; implementation `llvoavatar.cpp:11579-11595`). Between frames every rigid attachment drawable therefore carries the **smooth** transform of a frame that rendered the held one.
- Stock invariant broken: attachment drawable xforms are only recomputed by `gPipeline.updateMove` in `display()`, so between frames they always hold **the last rendered** transform.
- Idle consumers that read them: the particle simulation (`LLWorld::updateParticles`, `llappviewer.cpp:5663`) takes source positions from `getRenderPosition()` (`llviewerpartsource.cpp:160, 603, 644`), so particles are emitted from where the attachment was **not** drawn. Floating text (`updateText`, `llvoavatar.cpp` idleUpdateMisc), camera or DoF focus on an attachment, and the Ghost Studio "frozen" non-rigged attachment matrices (`alghoststudio.cpp` R2-2 block after `:3290`) are affected the same way.

**G3. A held frame can draw attachments at a matrix the overlay never synced (hardening).**
- `writeOverlay` returns early without syncing when the render pose equals the smooth pose (`alsteppedanimruntime.cpp:487-493`, the plain capture frame). Correctness then depends on `idleUpdateMisc` having queued an `updateMove` for every attachment this frame (`llvoavatar.cpp:3154-3200`, only when `detailed_update` and `visible || !(bridge && radius < 2)`). For a visible stepped actor that holds, but the overlay should not depend on it.

### 1.3 The "real-time avatar" double: every draw path audited

User report: "you have to hide the true realtime avatar somehow, or mask the actual motion one." Below, every path that can put a stepped avatar's pixels on screen, what it reads at draw time, and whether that is the held pose under the overlay.

| # | Draw path | Reads at draw time | Built where | Held? |
|---|---|---|---|---|
| D1 | Rigged mesh (bodies, heads, clothes, hair) | `updateSkinInfoMatrixPalette` per (avatar, skin hash), cached per `gFrameCount` (`llvoavatar.cpp:11477-11534`); uploaded in the rigged pushers (`lldrawpool.cpp:2168, 2203, 2240`) | `display()`, after `gFrameCount++` (`llviewerdisplay.cpp:624`) | **Yes.** Traced with and without an idle consumer: an idle build lands on the old frame id and is rebuilt in display from the held joints; `invalidateSkinPalettesForOverlay` / `AfterOverlay` (`llvoavatar.cpp:11538-11577`) keep the previous palette = last rendered hold. |
| D2 | Rigged velocity (TAA, motion blur, ReShade motion vectors) | `mLastGLMp` when `mLastFrame == gFrameCount-1` (`lldrawpool.cpp:1805-1828`) | display | **Yes**, zero on held frames, the real snap on step frames. |
| D3 | System avatar mesh | joint matrices at render (`llviewerjointmesh.cpp:178-205`) | display | **Yes.** |
| D4 | Rigid attachments (prims, unrigged mesh) | `model_mat = &drawable->getRenderMatrix()` (`llvovolume.cpp:5798`), a **pointer** to the root drawable's world matrix (`lldrawable.cpp:232-235`; child prims use the parent's) | `updateMove` in display + `syncStepAnimAttachmentMatrices` | **Yes** inside the window; G2/G3 affect idle readers only. |
| D5 | Rigid-attachment velocity | `mLastModelMatrix`, written at draw (`lldrawpool.cpp:1617-1630`) | display | **Yes.** |
| D6 | **Animesh attachments** | control avatar palettes (D1 path) from the control avatar's own skeleton | skeleton in **idle** from the smooth attach point (G1) | **No: smooth.** |
| D7 | Flexi prims | geometry rebuilt in display (`llflexibleobject.cpp:753`), skipped on held frames (`:771-772`) | display | Yes. |
| D8 | **Particles from attachments** | particle positions | **idle** (`llappviewer.cpp:5663`) from G2 matrices | **No: smooth (previous frame).** |
| D9 | Shadows, reflection probes, water reflection, VCam / Prism captures, snapshots, selection outline | the same draw infos and palettes | inside `display()` and the post-display snapshot pass (`llappviewer.cpp:1628-1650`) | Yes. |
| D10 | Impostors | impostor texture | display | Stepped actors are never impostors (`eligible`, `alsteppedanimruntime.cpp:144-151`). No double. |
| D11 | **Ghost Studio copies of a stepped actor** (cadence "Live" at a pose rate, Frozen) | palettes **copied** into `mCadencePalettes` / `mFrozenPalettes` | **idle**, `ALGhostStudio::updatePerFrame` (`llappviewer.cpp:5542`) via `alghoststudio.cpp:1325-1327, 3269-3272`, before `applyHeld` | **No: smooth copies.** (SAD §17.3 accepted this; it is a visible real-time double of the actor.) |
| D12 | TRUE-mirror clones | stamped from the source's smooth pose in idle (`llappviewer.cpp:5554`), then stepped by inheritance (`alsteppedanimruntime.cpp:209-228`) | idle + window | Yes, same frames as the source. |
| D13 | Faces with `StepAnimFace` off (the default) | Bento face bones from the smooth pose (`composeRender`, SAD §3.2) | window | **Smooth by design.** On a held head this reads as "the real motion showing through". |
| D14 | Motion blur (`BDMergeMotionBlur`), viewer TAA, ReShade TAAU / RTGI / AO temporal accumulation | velocity (D2/D5) + history | display / present | Not a smooth pose, but on each **step frame** blur draws a streak spanning the in-between motion and temporal history fades the previous pose over several frames: a second, moving-looking image 12 times a second. Identical under both engines. |

Anything drawn twice: D11 (a deliberate copy, but smooth), D12 (copy, held) and D14 (temporal ghost). No impostor-plus-live double exists.

**Verdict.** The held body itself (D1-D5, D7, D9) is drawn held. The real-time image comes from the derived state computed in idle: **D6 animesh, D8 particles, D11 Ghost Studio copies**, plus **D13 face smooth by default** and **D14 temporal post on step frames**. Static analysis cannot rank these for the user's scene; the provenance instrument (§12, R9) records, for every draw of a stepped avatar, which pose serial its palette or matrix was built from, and names the offending path in one in-world session.

The fixes are in §10. The Animation engine has none of these failure modes by construction: on a held frame there is no smooth pose anywhere in the process to draw.

---

## 2. The Animation engine

### 2.1 Frame timeline for a stepped avatar

```
LLAppViewer::idle()
  LLPresentationTime::tick()                     llappviewer.cpp:5216 (frozen clock t)
  ALSteppedAnimRuntime::beginFrame()             :5222  plan: per actor {tick?, backdate, roles, held classes}
                                                        engine 0 -> writes each stepped controller's playhead plan
  ALGhostStudio::updatePerFrame(), ALDanceBall::tick()   :5542-5544
  gObjectList.update()                           :5549
    LLVOAvatar::idleUpdate -> updateCharacter    llvoavatar.cpp:2942 -> :5685
      [A] stepLatchRestore(this)                 NEW, top of updateCharacter, before restoreDirectorLookAtPose (:5710)
      restoreDirectorLookAtPose                  :5710 (stock, LIFO after ours)
      computeNeedsUpdate / LOD early-out         :5731-5745  -> on early-out: [A'] stepLatchReapply(this)
      updateRootPositionAndRotation              :5795 (stock, smooth root)
      updateMotions(NORMAL_UPDATE)               :5815 -> LLMotionController::updateMotions
        [B] playhead branch: lifecycle at mAnimTime (stock); keyframe-family motions
            evaluated only on ticks at the grid sample time; frozen otherwise; blend every frame
      Pose Polish                                :5840  [C] ticks only, dt = sample delta
      Director look-at / gaze                    :5846  every frame (stock)
      ground-sit offset, updateHeadOffset        :5850-5869
      [D] stepLatchApply(this)                   NEW, right before mRoot->updateWorldMatrixChildren() (:5875)
      mRoot->updateWorldMatrixChildren()         :5875 (stock: world matrices of the latched pose)
    idleUpdateMisc -> attachment moves queued    llvoavatar.cpp:3139 (read the latched skeleton)
    idleUpdateLipSync                            :2971 (gated to ticks when Face is held, existing hook :3369)
  ALGhostStudio::updatePostObjectList()          :5554 (TRUE-mirror stamp copies the latched pose)
  [E] ALSteppedAnimRuntime::postObjectList()     NEW: animesh followers re-match (§2.6)
  camera dispatch, recorders, particles, LOD, audio (all see the latched pose)
  (engine 1 only) applyHeld()                    :5780
display()                                        everything draws the latched pose
(engine 1 only) endFrameRestore()                :1656
```

Between [D] of frame N and [A] of frame N+1 the latched pose is **the** pose of the avatar for every reader in the process. [A] runs immediately before the avatar's own re-evaluation, which needs the un-latched values to keep its integrators stock (§2.5).

### 2.2 The clock (shared core, unchanged)
- Time source `t`: presentation time (`StepAnimClock` 0) or wall time (1) from the frozen `LLTemporalFrameContext` (SAD §2.1).
- Grid: `tickIndex(t) = floor(t·r + φ + kTickEps)` from absolute `t` (SAD §2.2, `ALSteppedAnim::tickIndex`). No accumulator, no drift. Rate changes re-grid (SAD §2.2). Phase (Random / Locked / Manual / Roto match, SAD §2.3), adaptive hysteresis and dwell (SAD §5), scopes and overrides (SAD §4), actor cap (SAD §16.1): all from `ALSteppedAnim::resolveActor`, `planFrame`, `applyActorCap` as today.
- New core helper: `F64 ALSteppedAnim::boundaryTime(S64 tick, F64 rate, F64 phase)` = `(tick − φ) / r`, the exact grid time of a tick (inverse of `tickIndex`, tested T13).

### 2.3 Sample time vs lifecycle time (the controller change)

`LLMotionController` keeps two times:
- **Lifecycle time** `mAnimTime` (and `mLastTime`): advanced every frame exactly as stock (`llmotioncontroller.cpp:880-919`). Used for start/stop/activation timestamps, deactivation, `mSendStopTimestamp` and `requestStopMotion`. For self, `requestStopMotion` → `LLAgent::requestStopMotion` → `sendAnimationRequest(ANIM_REQUEST_STOP)` (`llvoavatarself.cpp:839-845`, `llagent.cpp:2760-2771`), which is why this clock must stay stock (P3).
- **Sample time** `mSampleTime` (and `mLastSampleTime`): the time stepped motions are evaluated at. Live: equals `mAnimTime`. Stepped: on a tick frame,

```
backdate   = t_now − boundaryTime(tick)                       // clock seconds, in [0, one frame)
mSampleTime = clamp(mAnimTime − backdate · mTimeFactor · animPerClock,
                    mLastSampleTime, mAnimTime)
animPerClock = sGlobalTimeFactor / clockScale                 // clockScale = effective_scale (capture clock) or 1 (real clock); no tick when clockScale == 0
```

So a tick samples the animation at the **exact grid instant**, not "the first frame after the boundary" (the overlay's limit, SAD §2.4/§6.4). Held poses are therefore identical at 60, 144 or 24 fps render (up to the sub-microsecond skew between `LLFrameTimer` and the presentation timer, re-anchored every tick, never accumulated). `mTimeFactor` covers Ghost Studio retiming (`llghostavatar.cpp:1772-1788`) and the agent's 0.2 factor (`llagent.cpp:294`).

**Why not the existing fixed-step mode.** `setTimeStep` (`llmotioncontroller.cpp:266-286`) re-floors activation/stop/send-stop timestamps to the quantum (changes server stop timing), and `updateMotions` in quantum mode **interpolates** joints toward a pose cached one quantum *ahead* (`:888-919`, `blendAndCache` `:943`, `LLPoseBlender::interpolate` `llpose.cpp:615`): smooth motion at a lower evaluation rate, not held poses. It is disabled (SL-763, comment `:867-871`); its only caller `LLVOAvatar::updateTimeStep` (`llvoavatar.cpp:4928-4956`) is commented out (`:5759`). Rule: a non-LIVE playhead requires `mTimeStep == 0` (asserted); the quantum path is not touched.

### 2.4 `updateMotions` with a playhead

```cpp
void LLMotionController::updateMotions(bool force_update)
{
    ... stock lines up to and including the mAnimTime advance (llmotioncontroller.cpp:865-919) ...
    updateLoadingMotions();
    resetJointSignatures();
    if (mPaused && !force_update)          { updateIdleActiveMotions(); }           // stock pause wins
    else if (mPlayhead.isLive())           { /* stock lines :931-948, unchanged */ }
    else                                   { updateMotionsPlayhead(force_update); }  // NEW
    mHasRunOnce = true;
}
```

The stock path is literally unchanged; the only new instruction on it is the `isLive()` test (a byte compare on a member). **Option off = byte-identical by construction.**

`updateMotionsPlayhead()` (new, `llmotioncontroller.cpp`):
1. **Lifecycle pass** at lifecycle time for every active motion whose role this frame is STEP: `updateIdleMotion(m)` (`:521-560`), the same function the stock paused path uses: deactivation after ease-out, residual-weight capture, native timed stop with `requestStopMotion`. Server-visible events happen at stock times.
2. **Additive, then regular** via `updateMotionsByTypePlayhead(type, plan)`, a sibling of `updateMotionsByType` (`:578-815`) with the same joint-signature filter (`:600-652`) and the same weight formulas, differing per motion:
   - **SMOOTH role**: the stock body, at `mAnimTime/mLastTime`, including its in-line lifecycle.
   - **STEP role, tick frame**: LOD fade, ease weights and `onUpdate(mSampleTime − activation)` computed with `mSampleTime/mLastSampleTime`; **no** lifecycle side effects (step 1 owns them). If `onUpdate` returns false (a motion stopping itself, `:796-806`), the stop is queued and executed by the lifecycle helper at lifecycle time.
   - **STEP role, hold frame**: no weight change, no `onUpdate`. Its `LLPose` keeps the tick's joint states and weight.
   - **STEP motion activated since the last tick** (activation timestamp > lifecycle time of the last tick): weight forced to 0 on hold frames, so a motion started mid-hold appears on the next tick, deterministically. (`activateMotionInstance` calls `onUpdate(0)` immediately, `:1026`; that output is not blended until the tick.)
   - Every motion is still `mPoseBlender.addMotion`ed (`:810`).
3. `mPoseBlender.blendAndApply()` **every frame**. The blend must run every frame because the post-motion layers (Pose Polish, gaze, Director look-at) compose onto a freshly blended pose: Pose Polish is additive over current rotations and accumulates on a stale pose (the documented paused-editing drift, `llvoavatar.cpp:5822-5839`), and gaze "re-asserts" on the current joints (`llactormover.cpp:6018-6019`). On a hold frame with only STEP contributors the blend output equals the tick output, except where blend weights sum below 1 (the blender then starts from the joint's current value, `llpose.cpp:270, 389`). The latch (§2.5) makes the final result exact regardless.

A maintenance rule for the duplicated function: test P1b (§12) runs both functions on the same fixture with the plan "every frame is a tick at live time, all roles SMOOTH" and requires bitwise-equal joints, so a future edit to one that is not mirrored in the other fails the suite.

### 2.5 The latch (`ALStepLatch`, viewer side)

Per stepped avatar: `PoseBuffer T` (tick snapshot), `R` (render pose = T + effects), `U` (the values the latch replaced), written mask, override epoch, skeleton serial. Joint set and classes exactly as SAD §3.2 (`mSkeleton` + collision volumes + `mRoot`; classes ROOT/BODY/FACE/GAZE/PHYS; held classes from `StepAnimWhatSteps/Face/Gaze/Flexi`). Reuses the core's `PoseBuffer`, `composeRender`, `restoreValue`, `bitEqual`.

- **[D] Apply** (end of `updateCharacter`, before `mRoot->updateWorldMatrixChildren()` at `llvoavatar.cpp:5875`):
  - Tick frame: read the final pose `S` (after motions, Pose Polish, look-at/gaze, ground-sit offset). `T = composeCapture(S, …)` (ease, §5). `R = T` + boil + smear for this tick.
  - Hold frame: `R` unchanged.
  - Write `R` for held classes **only where it differs bitwise** from the current value, saving the current value in `U` and marking the slot. Collision-volume scale only for PHYS; skeleton scale never (SAD §3.2).
- **[A] Restore** (top of the next `updateCharacter`, before `restoreDirectorLookAtPose`, LIFO): for every marked slot, if the current value still equals `R` bitwise, write `U` back; otherwise keep the foreign value (SAD §3.4 ownership, `ALSteppedAnim::restoreValue`). Drop `T` if the avatar's `mJointOverrideEpoch` (`llvoavatar.cpp:2332, 7701, 7875, 8130, 8159`) or skeleton serial changed.
- **[A'] LOD early-out** (`llvoavatar.cpp:5740-5745`): the avatar is not re-evaluated this frame, so after restore the latch is re-applied as-is (the held pose persists, like stock's "keep the last pose").
- Why the restore is needed at all: the avatar's own integrators must see the values they produced. `updateOrientation` slerps the root from its current world rotation (`llvoavatar.cpp:4917`), the blender starts from current joint values when weights sum below 1, and Director look-at restores its own saved values (`llactormover.cpp:6232-6279`). Restoring first keeps all of them stock.
- What it is not: the overlay restored after display so that idle consumers saw the smooth pose. The latch is restored only inside the avatar's own update and re-applied before anything else can read it. No consumer outside that function ever sees `U`.
- Writes between [D] and [A] by other systems: mesh-load override callbacks (epoch bump, handled), appearance `updateVisualParams` (writes joint scale and positions; positions are latched only where an animation changed them, so a bitwise-equal slot is never written and never contested), the Poser (excluded while editing, SAD §10.1).

Velocity and TAA need nothing: on a hold frame every latched slot gets the same bits, the root gets the same bits, `updateWorldMatrixChildren` is deterministic, so joint world matrices, rigged palettes (`mGLMp == mLastGLMp`) and attachment matrices are bitwise identical to the previous frame. Zero motion vectors. Step frames carry the real snap. The overlay's palette-invalidation logic (SAD §7.2) is not used by this engine.

### 2.6 Followers: animesh attachments and ghosts
- **Animesh attachments** (control avatars of the wearer's animated attachments): never resolved as independent actors under either engine. A control avatar whose root volume `isAttachment()` inherits the wearer's assignment, seed, rate, phase and decision (like ghosts, `ALSteppedAnim::ghostMayInherit`), so its own animations step on the same ticks.
- **[E] `postObjectList()`** (after `ALGhostStudio::updatePostObjectList`, `llappviewer.cpp:5554`): for every stepped wearer's animesh attachment, `cav->matchVolumeTransform()` then `cav->getRootJoint()->updateWorldMatrixChildren()`. Object-list order may update the control avatar before its wearer (stock one-frame lag); with stepping, that lag would show as a one-frame pop at every tick. The re-match makes the animesh root exactly follow the wearer's latched attach point on the same frame.
- **TRUE-mirror clones** (`isTrueMirrorDriven`, `llghostavatar.h:164`): **no playhead of their own**; the stamp (`applyTrueMirrorStamp`, after `gObjectList.update`) copies the source's latched pose, so the clone steps on the source's frames, bitwise.
- **Ghosts with an animation source** that is stepped: inherit the source's assignment (existing `collectActors` rule, `alsteppedanimruntime.cpp:209-228`), get their own playhead with the source's seed and phase.

### 2.7 Engine selector and shared core
- `StepAnimEngine` is read in `readConfig()` into `Config::engine` (new field, sanitized `std::clamp(v, 0, 1)`).
- Shared: `beginFrame()` (config, scope resolution, ghost/animesh inheritance, actor cap, `planFrame`), `decisionFor()` (flexi, particles, spin, lip-sync hooks keep working unchanged for both engines), effects math, Roto match, presets, page.
- Engine 0: `beginFrame` writes each stepped actor's playhead plan; [A]/[D]/[E] run; `applyHeld`/`endFrameRestore` find no VISUAL entries and do nothing.
- Engine 1: playheads stay LIVE, latch empty, `applyHeld`/`endFrameRestore` as today plus the §10 fixes.
- Entry state: `Entry::mEngine ∈ {NONE, ANIMATION, DISPLAY}`. Assert in `applyHeld`: an entry with engine DISPLAY has a LIVE playhead and an empty latch. Assert in [D]: engine ANIMATION never has overlay writes.

### 2.8 Switching rules (mid-session, mid-animation)
All switches take effect at a frame boundary, resolved in `beginFrame` (before any avatar update).

| From → to | What happens |
|---|---|
| off → Animation | Actor gets a playhead in STEPPED mode; its first frame is a forced tick (`planFrame` capture, SAD §2.2). |
| Animation → off / Display, actor leaves scope, cap drops it, override = Never | `beginFrame` sets the playhead LIVE and marks the latch "release". At the avatar's next `updateCharacter`: [A] restores, [D] does not re-apply, the latch entry is freed. That frame's evaluation is stock (STEP motions evaluated at live time: a jump from the last tick to now, which is the correct smooth pose). Display engine may take the actor **the same frame**: its `applyHeld` runs after `gObjectList.update`, by which time the latch has been released. If the avatar is not updated that frame (LOD early-out), the Display engine skips it until the latch is gone (assert above). |
| Display → Animation | The overlay never survives a frame (closed by `endFrameRestore` before the next input). `beginFrame` drops the overlay's held state for the actor and attaches the playhead (forced tick). |
| Master off | As "Animation → off" for all actors; `sActive` false after the last release is processed; an avatar never updated again keeps its last latched pose (a real pose, released whenever it next updates). |
| Teleport / region crossing | Root-holding latches dropped (restore, then recapture), as the overlay's SAD §3.3 step 2. |

---

## 3. Per-motion-type table

Roles: **STEP** = evaluated only on ticks at the grid sample time, frozen in between. **SMOOTH** = evaluated every frame at lifecycle time; its output is still held by the latch when its joint class is held. Post-motion layers are in the lower half.

| Motion / layer | Registered / code | Role | Held output via | Scrubbable (§8) | Reason |
|---|---|---|---|---|---|
| Asset animations (AO, dances, poses, gestures, Bento face/hand anims) | `LLKeyframeMotion` (`llkeyframemotion.cpp:768-860`) | STEP | blend + latch | **Yes** (curves, loop map, ease weights); constraints and loop-seam repair excluded | The drawn performance. Curve sampling is a pure function of time (`RotationCurve::getValue` `:255`, `PositionCurve::getValue` `:336`). |
| Stand / walk / run / crouch-walk / turn / fall | `LLKeyframeStandMotion`, `LLKeyframeWalkMotion`, `LLKeyframeFallMotion` (`llvoavatar.cpp:1280-1315`) | STEP | blend + latch | No (IK and walk-phase state) | Drawn. Their time deltas come from consecutive sample times, so phase integration stays correct at tick rate. |
| Sit (built-in) | `LLKeyframeMotion` (`:1328`) | STEP | | Yes | |
| Poser posing motion | `FSPosingMotion : LLKeyframeMotion` (`fsposingmotion.h:42`, registered `fsposeranimator.cpp:1728`) | STEP | | Static (targets, not time) | Avatar excluded while being edited (SAD §10.1). |
| Dance Ball / Actor Mover external-sample motions | presentation control `setExternalSampleTime` (`llmotion.h:147-156`; `aldanceball.cpp:1289-1367`, `llactormover.cpp:237, 357`) | STEP | | Owner-driven | `onUpdate` uses the owner's sample of the tick frame (≤ one frame from the grid; open item O7). |
| AutoAnimate, Ghost AutoAnimate | start keyframe motions | STEP | | Yes | |
| Breathing | `LLBreatheMotionRot` (`llvoavatar.cpp:409-505`, `sinf(rate·time)`) | STEP | | **Yes** (pure) | Drawn; pure in time. |
| Body noise | `LLBodyNoiseMotion` (`llvoavatar.cpp:303-395`, `noise2(time)`) | STEP | | **Yes** (pure) | |
| Hand pose (morphs, system hands) | `LLHandMotion` | STEP | params constant between ticks | No | Reads the "Hand Pose" animation data, which keyframes set at ticks (`llkeyframemotion.cpp:875-889`). |
| Emotes (system face morphs) | `LLEmote` (`:1282-1300`) | STEP if `StepAnimFace`, else SMOOTH | params constant between ticks | No | Face toggle. |
| Avatar physics (breast/belly/butt bounce) | `LLPhysicsMotionController` (`:1320`, `llphysicsmotion.cpp`) | STEP if `StepAnimFlexi`, else SMOOTH | params / PHYS class | No | STEP: integrates the tick interval in ≤0.05 s sub-steps (`llphysicsmotion.cpp:49, 577`), dt cap 1 s (`:509`), output held; reacts to the stepped body with follow-through. SMOOTH: jiggles every frame on the held body. |
| Head follows look-at | `LLHeadRotMotion` (`llheadrotmotion.cpp:198`) | SMOOTH | GAZE class if `StepAnimGaze` | No | Uses frame-based `LLSmoothInterpolation` (`llcriticaldamp.cpp:84`); at tick rate it would track 5x slower. |
| Eyes, saccades, blink | `LLEyeMotion` (`llheadrotmotion.cpp:480`) | SMOOTH | GAZE class (eye joints); blink morph not held | No | Same reason. |
| Editing arm, targeting | `LLEditingMotion`, `LLTargetingMotion` | SMOOTH | BODY class | No | Frame-based smoothing (`lleditingmotion.cpp`, `lltargetingmotion.cpp`). Editing also pauses the wearer (stock pause wins). |
| Walk adjust, fly adjust, pelvis fix | `LLWalkAdjustMotion`, `LLFlyAdjustMotion` (`llkeyframewalkmotion.h:156, 206`), `LLPelvisFixMotion` (`llvoavatar.cpp:523`) | SMOOTH | BODY/ROOT classes | No | Speed-coupled foot-slide repair, frame-based smoothing. |
| Null / DND | `LLNullMotion` | n/a | | | |
| **Post-motion layers** | | | | | |
| Pose Polish (inertia, contact, secondary, loop-seam) | `ALPosePolish::run(av, dt)` (`llvoavatar.cpp:5608`, called `:5840`) | **Ticks only**, `dt = sample-time delta` | latch | No (stateful) | Drawn. Run every frame, its inertia stage would treat every tick as a discontinuity and lag the captured pose. At ticks it sees a fresh blend and the true elapsed time. |
| Director look-at | `applyDirectorLookAt` (`llactormover.cpp:6560`) | every frame | GAZE class | No | Has its own restore (LIFO after ours). |
| Gaze layer + ALGazeMotor | `applyGaze` (`llactormover.cpp:5939`) | every frame | GAZE class if `StepAnimGaze` | No | Envelope advances once per frame on the presentation delta (`:6018-6035`). |
| Lip-sync morphs | `idleUpdateLipSync` (`llvoavatar.cpp:3368`) | ticks only when Face held (existing hook `:3369-3375`) | | No | Unchanged. |
| Ground-sit offset, client height | `llvoavatar.cpp:5850-5866` | every frame | ROOT class | | |
| Mouselook head scale | `updateMouselookHeadBoneScale` | every frame | never held (scale) | | Self in mouselook is excluded anyway. |

Notes:
- Role is a function of (motion kind, config). New `virtual LLMotion::EStepKind getStepKind() const` (default `STEP_KIND_PROCEDURAL_SMOOTH`), overridden by `LLKeyframeMotion` (KEYFRAME), `LLBreatheMotionRot`/`LLBodyNoiseMotion` (PURE_PROCEDURAL), `LLHandMotion` (HAND), `LLEmote` (FACE_MORPH), `LLPhysicsMotionController` (PHYSICS). The playhead plan carries a role mask over kinds.
- Known caveat (risk 4): keyframe **constraints** (`applyConstraint`, `llkeyframemotion.cpp:1580`) smooth with frame-based interpolants (`:1700, :1704, :1753`); evaluated per tick they converge more slowly. Constraints are rare in user animations (mostly built-in sits). Accepted for Phase 1, measured in R8.

---

## 4. Root and position

- **Pose + position** (default): the latch holds `mRoot`'s world position and rotation (class ROOT), captured at the tick after every root writer: `updateRootPositionAndRotation` (sim interpolation, Actor Mover traversal, turns), the ground-sit offset (`llvoavatar.cpp:5850-5866`) and client height. On hold frames the root is re-written with the tick value. The avatar **object** position (`getPositionAgent`, the network-interpolated position) is never touched.
- Since attachments are parented to joints (`llviewerjointattachment.cpp:115`), rigged mesh skins to joints, and animesh followers are re-matched in [E], **everything attached moves with the latched root. Attachments cannot diverge from it.** The avatar drawable (`mDrawable`, smooth object position) keeps feeding culling extents, LOD/pixel area, voice and audio positions, and the agent camera's follow target: all within one step of the drawn body (spatial extents are padded for animation; SAD §3.1 order decision).
- Name tags place from `mLastRootPos` cached before `updateCharacter` (`llvoavatar.cpp:2931-2933`), i.e. last frame's latched root: tags step with the body. Ghost nameplates anchor on ghost roots the same way. Neither engine-0 path needs the overlay's label shift (SAD §3.7).
- **Sitting on an object**: root not held (the body rides the seat or vehicle smoothly; the pose still steps). Holding the root there would detach the avatar from a moving seat. Ground sit holds normally.
- **Pose only**: ROOT not held; the body travels smoothly, limbs step. Feet slide between ticks by design.
- Restore before `updateRootPositionAndRotation` (§2.5) keeps `updateOrientation`'s slerp (`llvoavatar.cpp:4917`) on the smooth track, so the smooth root never accumulates lag from the hold.
- Position stepping never alters server state: the agent position is the simulator's; the viewer only decides where the skeleton is drawn.

---

## 5. The effects (Animation engine)

The Animation engine must honour `StepAnimParticles` (particle motion/age debt) and `StepAnimTextureAnim` (attachment texture holds evaluated at current time on ticks).

All effect math is the shared core (SAD §6). The application point is the latch: `T` and `R` are computed at the tick and written on every frame of that tick, so attachments, cameras and clones follow them.

| Effect | Animation engine |
|---|---|
| **Boil** | `R_rot = T_rot · boilRotation(seed, b, j, A)` for held BODY/GAZE/FACE joints (SAD §6.1), `b` from the base-rate grid. Deterministic per (actor, b, j). |
| **Impact hold** | `observe()` fed with pose-space effector velocities (below). On trigger: the next tick is forced at the impact pose, then `ImpactTicks` grid ticks are suppressed: the playhead plans no tick, so STEP motions stay frozen and the latch holds. The lifecycle clock keeps running, so the release jumps to the current pose, as SAD §6.2. |
| **Smear** | On a tick, from `T_prev → T`, per limb chain (SAD §6.3): lagged rotation and direction-weighted local scale, written by the latch as part of `R` for that tick only. Scale slots go through the ownership rule. |
| **Ease** | Needs poses inside the tick window. **Sub-step probes**: when Adaptive, Impact or Ease is on, at each of the `M = 4` sub-step grid times of the current tick window the playhead evaluates the STEP keyframe motions with the pure path (`sampleAt`, §8.3) into a scratch buffer and runs FK over the five effector chains in pelvis space (no joint writes, no motion state touched). The sub-step speeds feed `observe()`; the lowest-speed sub-step's scratch pose is the ease candidate `C`; at capture `T = slerp/lerp(S, C, easeWeight)` for BODY/GAZE/FACE (SAD §6.4). Probes are taken **at the exact sub-step times**, so ease is identical at any render rate (stronger than SAD §6.4's "same choice" guarantee). |
| **Adaptive** | Same probes give limb speeds at 4× the tick rate (48 Hz at 12 fps), enough for the ≥33 ms window and τ 50 ms smoother (SAD §5). Without probes (all three effects off) nothing measures speed, and adaptive is off. |

**Rejected: effects as an additive high-priority `LLMotion`.** It would be blended under Pose Polish (additive deltas on top), overwritten on GAZE joints by the gaze layer, and fight user animations at priority 6. The latch is the last writer before world matrices, so its result is exactly what everything reads.

Flexi, particles and attachment spin keep their existing tick deferrals (`llflexibleobject.cpp:771`, `llviewerpartsim.cpp:296/840-851`, `llviewerobject.cpp:2540-2542`), driven by `decisionFor()`, under both engines.

---

## 6. Interactions

| System | Animation engine | Display-only engine |
|---|---|---|
| Camera Bone Lock, VCam bone POV | Read joints in the idle camera dispatch (after `gObjectList.update`): **follow the stepped pose** (accepted, user 10/14). With `StepAnimExcludeCameraMounted` on (default) the mounted actor stays smooth. | Camera stays smooth while the body steps: **the reason to choose Display only.** |
| Agent / flycam / path / CineCam cameras | Follow the avatar object position (smooth). The body steps relative to a smooth camera. | Same. |
| Take recorder (`LLFlycamRecorder`) | Records camera keyframes only (`llflycamrecorder.h:65-71`). A camera locked to a stepped bone records stepped motion, because that is what the camera did. No avatar pose is recorded anywhere today. Future pose recording should store animation state + playhead times + step settings, not poses, so stepping is re-applied on playback (§9.7). | Records smooth. |
| Ghosts following live, TRUE mirror | Inherit (§2.6). The mirror copies the latched pose. | Inherit (existing). |
| Ghost Studio copies (cadence, frozen) | Snapshots in `updatePerFrame` read last frame's latched pose: stepped, one frame late (stock lag). | Fixed in §10 (snapshot after `applyHeld`). |
| Actor Mover traversal | Written in `updateRootPositionAndRotation`, before [D]: held in Pose + position. | As SAD. |
| Sitting avatars | Pose steps; root follows the seat (§4). | Root held (SAD); unchanged. |
| AO | Unchanged: the AO sends start/stop requests; stepping changes only evaluation timing. Lifecycle stays stock (P3). | Same. |
| Lip-sync | Ticks only when Face held (existing hook). | Same. |
| Other residents (Everyone in view) | Their client-side playback steps locally; nothing is sent. | Same. |
| Poser | Excluded while the floater edits the avatar (`FSFloaterPoser::isEditingAvatar`). | Same. |
| Selection / attachment editing | Stock pause (`requestPause`, `llcharacter.cpp:490-493`) wins over the playhead; Pose Polish reset path unchanged. | Same. |
| Picking, hover, build tools | See the drawn (latched) pose. | See the smooth pose (up to one step off). |
| Temporal World clock | At 0x no ticks: the latch holds; lifecycle also frozen (`sGlobalTimeFactor` 0). | Same. |
| Impostors, animation preview, UI avatars | Excluded (SAD §10.2). | Same. |
| Velocity / TAA / ReShade motion vectors | Natural zero on held frames (§2.5). | Existing palette logic (SAD §7.2). |

---

## 7. Performance

Model per Bento avatar per frame (stock costs are estimates to be replaced by Tracy numbers in R8; ratios are what matter):

| Item | Stock | Animation engine, rate r on render f (hold fraction h = 1 − r/f) | Display-only engine |
|---|---|---|---|
| Keyframe-family `onUpdate` (~55% of `updateMotions`) | E | E·(1−h) | E |
| Blend (~30%) | B | B (every frame) | B |
| SMOOTH procedurals (~15%) | P | P | P |
| Pose Polish (when enabled) | Q | Q·(1−h) | Q |
| Latch (copy ~160 slots at ticks, compare/write + restore) | – | ≈ 4-8 µs | – |
| Probes (only with Adaptive/Impact/Ease) | – | 3 pure keyframe samples + 5-chain FK per tick ≈ 0.6·E·(1−h) | – |
| Overlay apply + restore + attachment syncs + palette bookkeeping | – | – | 40-75 µs (SAD §16.2) |
| Flexi, particles, spin on held frames | full | skipped | skipped |

Worked example, `updateMotions` = 40 µs (E 22, B 12, P 6), Pose Polish 10 µs, 12 fps steps at 60 fps render (h = 0.8), no probes:
- Stock: 50 µs. Animation engine: 22·0.2 + 12 + 6 + 10·0.2 + 6 ≈ **30 µs (−40%)**. Display only: ≈ 50 + 55 = **105 µs**.
- 24 actors: stock ≈ 1.2 ms, Animation ≈ 0.72 ms, Display only ≈ 2.5 ms. Twos at 24 on 60 fps (h = 0.6): Animation ≈ 37 µs/actor (−26%).
- Worst case (rate ≥ render fps, h = 0): Animation ≈ stock + latch (+10%).
- Memory: playhead ~64 B per controller (all avatars); latch 3 PoseBuffers ≈ 21 KB per stepped actor (vs 5 for the overlay).

Further saving (Phase 1.5, not in scope): on hold frames with every class held and no SMOOTH contributors, skip the blend and the post layers entirely. It needs a "post layers frozen" contract for Pose Polish and gaze, so it is deferred.

---

## 8. The playhead API (Phase 1 implements; Phase 2 adds the UI)

### 8.1 Types (`indra/llcharacter/llanimplayhead.h`, no viewer dependencies)

```cpp
class LLAnimPlayhead
{
public:
    enum EMode : U8 { LIVE = 0, STEPPED = 1, PAUSED = 2, SCRUB = 3 };
    enum ERoleBit : U16 {                        // motion kinds evaluated as STEP this frame
        ROLE_KEYFRAME = 1, ROLE_PURE_PROCEDURAL = 2, ROLE_HAND = 4,
        ROLE_FACE_MORPH = 8, ROLE_PHYSICS = 16, ROLE_ALL_PROCEDURAL = 32 /* freeze-frame */ };

    struct Plan                                  // written by the viewer runtime each frame, read by updateMotions
    {
        EMode mode = LIVE;
        bool  evaluate = false;                  // tick / seek / step this frame
        F64   backdateClock = 0.0;               // STEPPED: clock seconds since the tick boundary
        F64   animPerClock = 1.0;                // sGlobalTimeFactor / clockScale
        F32   seekSampleTime = 0.f;              // SCRUB: absolute controller-domain sample time
        U16   stepRoles = 0;
        bool  postLayersAtTicksOnly = true;      // Pose Polish cadence (viewer reads this)
    };

    bool  isLive() const { return mPlan.mode == LIVE; }
    void  setPlan(const Plan& p);                // asserts the controller's mTimeStep == 0 when !LIVE
    const Plan& plan() const { return mPlan; }

    // results of the last update, for the runtime / UI
    F32   sampleTime() const;                    // controller-domain time the STEP motions show
    F32   lastTickLifecycleTime() const;
    bool  evaluatedThisFrame() const;
private:
    Plan mPlan; F32 mSampleTime = 0.f, mLastSampleTime = 0.f, mLastTickLifecycle = 0.f; bool mEvaluated = false;
};
```

`LLMotionController` owns `LLAnimPlayhead mPlayhead` (value member: no lifetime coupling, LIVE by default) and exposes `LLAnimPlayhead& getPlayhead()`. `LLCharacter` forwards `getAnimPlayhead()`.

### 8.2 Viewer-side controller (`ALAnimPlayheadControl`, in `alsteppedanimruntime.h/.cpp`)

```cpp
class ALAnimPlayheadControl            // per avatar, keyed by avatar UUID in the runtime; never retains pointers
{
public:
    void play();                       // back to LIVE, or STEPPED if stepping is assigned; sample time re-syncs to lifecycle (now)
    void pause();                      // PAUSED: no evaluation; STEP roles frozen; ROLE_ALL_PROCEDURAL optional (freeze-frame)
    void stepTicks(S32 n);             // PAUSED/SCRUB: seek to boundaryTime(currentTick + n) on the actor's grid (n may be negative)
    void seek(F32 sample_time);        // SCRUB: evaluate once at sample_time, then hold
    void seekMotionLocal(const LLUUID& motion, F32 local_time);   // sample = activation + local_time
    F64  currentTickTime() const;      // grid time of the shown pose
    bool isScrubbable(const LLMotion*) const;
};
```

Stepping is the STEPPED mode of the same object: `beginFrame` fills `Plan{STEPPED, evaluate = decision.capture, backdate, …}`. Pause/seek from a future UI override the stepped plan for that avatar (the UI wins; stepping resumes on `play()`), so the two can never fight.

### 8.3 Deterministic evaluation and reverse seeking
- **Forward** (`sample ≥ last sample`): the normal STEP evaluation; stateful motions integrate correctly because their deltas come from consecutive sample times.
- **Reverse or random access** (`SCRUB`): only the **scrubbable subset** is evaluated, through new pure entry points; everything else is frozen at its last output (default) or, with ROLE_ALL_PROCEDURAL cleared, left live.
  - `bool LLMotion::canSampleAt() const` (default false) and `virtual void LLMotion::sampleAt(F32 local_time)` (writes the motion's joint states as a pure function of time; no member state read or written besides the joint states).
  - `LLKeyframeMotion::sampleAt`: loop mapping as in `onUpdate` (`llkeyframemotion.cpp:785-819`, non-stopped branch only: the stopped branch reads `mLastUpdateTime`), then `applyKeyframes` (`:845`, body `:865`). **Excluded**: loop-seam repair (`:780-852`, PosePolish M4 state) and constraints (`:855`, frame-smoothed state). The hand-pose animation data write in `applyKeyframes` is idempotent.
  - `LLBreatheMotionRot`, `LLBodyNoiseMotion`: their `onUpdate` is already pure (`llvoavatar.cpp:375-394, 496-505`).
  - Weights in SCRUB use the pure function `scrubWeight(m, t)` = ease-in `cubic_step((t − act)/easeIn)`, ease-out `cubic_step(1 − (t − stop)/easeOut)`, × fade × presentation weight. The stock residual-weight capture (`llmotioncontroller.cpp:717, 770`) depends on history and is not used in SCRUB.
  - Not scrubbable: walk/stand/fall (IK and phase state), physics, head-rot/eye, editing/targeting, walk/fly adjust, pelvis fix, posing (target-driven), external-sample motions (owner-driven), Pose Polish, gaze, look-at.
- **Lifecycle during pause/scrub** keeps running at stock time (P3). Looping animations (nearly all AO, dance and pose content) never end, so they stay scrubbable. A non-looping server animation that ends lifecycle-wise is deactivated as in stock. Editing must not suppress that for self (it would desync the server's animation list), so the editor scrubs a **local copy** (§9.4).
- **Seek determinism contract** (tests P6/P7): for the scrubbable subset, `seek(t)` yields bitwise the same joint states regardless of the order or direction of previous seeks.

---

## 9. Keyframe editor foundation (Phase 2 scrub UI, Phase 3 editing and export)

Goal (user, 10/14): Blender-like editing of uploaded SL animations in the viewer. Normally an uploaded `.anim` cannot be modified.

### 9.1 What the viewer already has
- Decoded `.anim` data per asset: `LLKeyframeMotion::JointMotionList` (`llkeyframemotion.h:523-551`): per-joint `JointMotion` with `RotationCurve`/`PositionCurve`/`ScaleCurve` (`std::map<F32, Key>`, `:450-500`), joint priority and usage, `mDuration`, `mLoop`, `mLoopInPoint/OutPoint`, `mEaseIn/OutDuration`, `mBasePriority`, `mHandPose`, `mMaxPriority`, `mEmoteName`, constraints. Shared across every avatar via `LLKeyframeDataCache` (`:589-602`).
- Serialization: `serialize(LLDataPacker&)` / `deserialize(...)` (`:169-170`), `dumpToFile` (`llkeyframemotion.cpp:2849`).
- Local motion precedent: BVH preview creates a motion under a local UUID, deserializes into it, and removes the cache entry afterwards (`llfloaterbvhpreview.cpp:249-267, 1011`). The Poser registers a local motion class per avatar (`fsposeranimator.cpp:1728`) and writes BVH (`fsfloaterposer.cpp` SaveToBVH, `:3059-3152`).
- Per-instance, owner-scoped presentation controls (`llmotion.h:130-190`): weight (to silence the original), external sample clock, owner-managed lifetime, joint exclusion. Clearing on deactivate is already handled.

### 9.2 Data model (pure, `indra/llcharacter/llanimclip.h`)

```cpp
struct LLAnimClip                                  // editable, value-semantic copy of one .anim
{
    LLUUID mSourceAsset;                           // original UUID (never mutated, never re-sent)
    F32 mDuration, mLoopIn, mLoopOut, mEaseIn, mEaseOut; bool mLoop;
    S32 mBasePriority, mHandPose; std::string mEmoteName;
    struct RotKey { F32 t; LLQuaternion q; };  struct PosKey { F32 t; LLVector3 p; };
    struct Track  { std::string joint; S32 priority; std::vector<RotKey> rot; std::vector<PosKey> pos; };
    std::vector<Track> mTracks;                    // keys sorted by t, times in seconds
    LLSD mConstraintsPassthrough;                  // carried through unchanged; not editable in Phase 3
};
bool LLKeyframeMotion::exportClip(LLAnimClip& out) const;                 // read-only on the shared list
static LLKeyframeMotion::JointMotionList* LLKeyframeMotion::buildList(const LLAnimClip&);  // for local playback
bool LLKeyframeMotion::serializeClip(const LLAnimClip&, LLDataPacker&);   // .anim bytes (export, Phase 3)
```

The shared `JointMotionList` for a source asset is **never** edited: another avatar playing the same asset would change too.

### 9.3 Edit layer (non-destructive)

`LLAnimEditLayer` = ordered list of operations over a clip, each invertible (undo comes later). Undo is not in scope.
- Keys: `SetKey(track, channel, t, value)`, `InsertKey`, `DeleteKey`, `MoveKey(t0 → t1)`.
- Tracks: `JointOverride(track, channel, REPLACE | ADDITIVE | MUTE)`, `AddTrack(joint)` (animate a joint the source did not), per-track priority.
- Time: `Retime(curve)`: monotone piecewise-linear map clip time → source time (speed ramps, holds); `Trim(in, out)`; `Loop(in, out, on)`; `Ease(in, out)`.
- Global: `BasePriority`, `HandPose`.
- `LLAnimClip bake(const LLAnimClip& source, const LLAnimEditLayer& edits)` is pure. Evaluation, preview and export all consume the baked clip, so **what you preview is what you export**.

### 9.4 Local playback (viewer-local by construction)
1. Bake → `buildList` → register under a **local** UUID (hash of source UUID + edit-layer revision) in `LLKeyframeDataCache`, as the BVH preview does.
2. `startMotion(localId)` on the avatar. A local `startMotion` never messages the simulator; only `LLAgent::sendAnimationRequest` does. The edited copy claims presentation control with owner-managed lifetime (it never ends while being edited) and the external sample clock (the editor's playhead time).
3. The original server-driven instance stays active (the server still lists the original UUID: AO and stop logic unchanged) and is silenced with `setPresentationWeight(owner, 0)`.
4. Re-bake on each edit commit (an animation is at most ~130 joints × keys: microseconds); swap the cache entry; `purgeMotionInstances(oldLocalId)` (`llmotioncontroller.h:124-132`) and `removeKeyframeData` on release.
5. Scrubbing the edit = setting the external sample time; stepping the edit = the same playhead quantizing that time. One clock.

### 9.5 Export (Phase 3, later)
- `.anim`: `serializeClip` → file (and the existing upload path for re-upload). Times and rotations are re-quantized to the `.anim` U16 encoding (lossy at the format's precision; a round-trip test pins it).
- BVH: reuse the Poser's writer.
- **Permission gate** (open question Q4): proposed: export only animations the user **owns with full permissions** (creator or full-perm inventory item). Editing for local playback of any animation playing on any avatar is viewer-local and creates no copy.

### 9.6 What Phase 1 must already contain (so Phases 2-3 need no engine rewrite)
1. The lifecycle/sample split and the playhead branch in `LLMotionController` (§2.3-2.4).
2. `LLAnimPlayhead` + `ALAnimPlayheadControl` with pause / step ±n / seek / seekMotionLocal, unit-tested (no UI).
3. `getStepKind`, `canSampleAt`, `sampleAt` (pure keyframe sampling, also used by the ease/adaptive probes).
4. The latch with ownership (the editor's freeze-frame uses the same hold).
5. `LLAnimClip` with `exportClip` + `buildList` and a round-trip test (decode → clip → list → sample equals the original at 1000 random times, bitwise). `bake`, the edit ops and `serializeClip` are Phase 3.

### 9.7 Which motions take part in scrubbing
Scrubbable: keyframe animations (minus constraints and seam repair), breathing, body noise, edited local clips. Stays live or frozen (UI choice, default frozen): physics, head/eye look-at, gaze layer, Director look-at, editing/targeting, walk/fly adjust, pelvis fix, stand/walk IK, Pose Polish, external-sample owners (Dance Ball, Actor Mover clips are sampled by their owners; Phase 2 can route their sample through the playhead).

---

## 10. Display-only engine: fixes, and what changes in the overlay code

Nothing is deleted (P1). Changes:

| Fix | Change |
|---|---|
| **F1 animesh followers (G1, D6)** | `collectActors` never resolves an attached control avatar as an actor; it inherits its wearer's assignment and decision (§2.6). In `applyHeld`, after each wearer's write: for each animesh attachment, `cav->matchVolumeTransform()`, overlay its skeleton with the wearer's decision (its own capture buffers, same capture frames), `cav->getRootJoint()->updateWorldMatrixChildren()`, `invalidateSkinPalettesForOverlay()` on the control avatar. `endFrameRestore` restores followers **before** wearers. |
| **F2 no restore-time attachment re-sync (G2, D8)** | `restoreOverlay` stops calling `syncStepAnimAttachmentMatrices()` (`alsteppedanimruntime.cpp:595-601`). Attachment drawables keep the last **rendered** transform until the next display, the stock invariant. Particles, floating text and focus targets read what was drawn. (The joints are still restored, so Bone Lock and VCam bone POV stay smooth: the Display-only use case is preserved.) |
| **F3 always sync on write frames (G3)** | `writeOverlay` calls `syncStepAnimAttachmentMatrices()` for every stepped actor every frame, including the no-write capture frame. Cost: one matrix compose per attachment prim. |
| **F4 Ghost Studio snapshots (D11)** | New hook `ALGhostStudio::onStepOverlayApplied()` at the end of `applyHeld`, inside the window: cadence and frozen palette refreshes due this frame for a **stepped** source run there instead of in `updatePerFrame`, so the copies are taken from the held palettes. |
| **F5 face default (D13)** | Page hint on "Face & lip-sync": "Off: the face keeps moving in real time on a held head." No default change (open question Q2). |
| **F6 temporal post (D14)** | Not a viewer bug: a step frame carries a real snap. Page hint on the Stepped card: motion blur and temporal filters (TAA, ReShade TAAU/RTGI) smear each step. Q3 asks whether to add "suppress motion blur on step frames". |
| **F7 provenance instrument** | `StepAnimDebugProvenance` (debug, not persisted): each avatar keeps a pose serial incremented by every overlay write and restore; palette builds (`updateSkinInfoMatrixPalette`), attachment xform updates and particle source reads record the serial they used; at upload or use time the serial is compared to the held serial; mismatches are logged per path (D1-D14 ids). Used by R9. |

Shared code extracted for both engines (no behaviour change): config read, scope/cap/inheritance, `planFrame`, `decisionFor`, effects composition (`composeCapture`, boil, smear), Roto match.

---

## 11. Settings

### 11.1 New
| Setting | Type | Default | Values | Persist | Consumer clamp | Tracked by presets |
|---|---|---|---|---|---|---|
| `StepAnimEngine` | S32 | 0 | 0 Animation, 1 Display only | 1 | `std::clamp(v, 0, 1)` | **Yes** (30 tracked) |
| `StepAnimDebugProvenance` | Boolean | 0 | debug | 0 | bool | No |

Page: segmented row `sa_engine` in the first card ("Stepped animation"), directly under "Applies to". Labels "Animation" / "Display only". Hint: "Animation steps the animation itself: attachments, cameras and clones all follow the stepped pose. Display only steps what you see: cameras mounted on a bone and anything reading the skeleton stay smooth." Wiring per SAD §15 (segmented rows bound to S32, kit_lint `segmented`/`radio_group` rule).

All 14 built-in presets write `StepAnimEngine = 0`. T12 becomes "each preset sets all 30".

### 11.2 Kept, with engine-specific meaning
- `StepAnimWhatSteps`: Pose + position = latched root (Animation; seated → pose only, §4) or overlay root (Display).
- `StepAnimFlexi` ("Hair, cloth & flexi"): in Animation also selects the physics role (STEP when on).
- `StepAnimFace`: Animation also steps `LLEmote` morphs.
- `StepAnimGaze`: GAZE class held by the latch (Animation) or overlay (Display); head-rot/eye/gaze always evaluated every frame.
- `StepAnimExcludeCameraMounted`: unchanged (excludes the actor in both engines).
- `StepAnimIncludeGhosts` (Boolean, Persist 1, default 0): the Animation engine **must honour it**, as must Display only. Independent-animation Ghost Studio clones join every Scope when on, subject to Smooth overrides, posing/edit and camera-mounted exclusions, and `StepAnimMaxAvatars`. Mirror/copy ghosts retain source inheritance. Shared Config/readConfig and scope resolution own it; it is tracked but presets never write it. The kit toggle is directly under Scope, labelled "Include ghost clones" with help "Step Ghost Studio clones even when they are outside the scope above."
- `StepAnimMaxAvatars`, all rate/adaptive/effect/clock/phase/Roto settings: unchanged.

### 11.3 Removed
None. Per-actor `SteppedOverride` unchanged (Q1 asks about a per-actor engine).

---

## 12. Unit tests and runtime checks

### 12.1 Unit tests
Existing `alsteppedanim_test.cpp` T1-T12 stay (T12 → 30 tracked settings). New:

| Test | File | Proves |
|---|---|---|
| T13 `boundaryTime` | `alsteppedanim_test.cpp` | `tickIndex(boundaryTime(k)) == k` for k over ±10⁷ ticks at every rate/phase; backdate in [0, frame). |
| P1a stock path untouched | `llcharacter/tests/llanimplayhead_test.cpp` (new `LL_ADD_INTEGRATION_TEST`, like `llmotionpresentation`, `llcharacter/CMakeLists.txt:81`) | A fixture controller (test seam `advanceAndUpdate(F32 delta)` split out of `updateMotions`, the timer read stays in `updateMotions`) with synthetic motions and an in-memory `.anim`: LIVE playhead output is bitwise equal to a golden captured from the unmodified function over 10⁴ jittered frames. |
| P1b sibling equivalence | same | `updateMotionsByTypePlayhead` with "tick every frame, sample = lifecycle, all roles SMOOTH" equals `updateMotionsByType` bitwise. |
| P2 hold | same | STEPPED: STEP-role `onUpdate` count == tick count; STEP joint states bitwise constant between ticks; SMOOTH-role motions updated every frame. |
| P3 grid sampling | same | 60 and 144 fps jittered streams produce the same sample-time sequence (±1 ulp F32) and bitwise-equal keyframe outputs at every tick. |
| P4 lifecycle neutrality | same | A recording mock `LLCharacter::requestStopMotion` sees identical calls at identical lifecycle times with stepping on and off (non-loop with ease-out, loop stopped by request, motion stopping itself). |
| P5 activation/deactivation | same | A motion started mid-hold contributes 0 until the next tick; a motion deactivated mid-hold changes nothing until the next tick (with the latch fixture). |
| P6 seek/step/pause | same | `seek(t1); seek(t0)` == `seek(t0)` bitwise for the scrubbable subset; `stepTicks(+1)`/`(−1)` move exactly one grid tick; `pause` holds; `play` re-syncs to lifecycle. |
| P7 `sampleAt` purity | same | Random-order sampling of a looping and a non-looping clip equals forward sampling bitwise; seam repair and constraints untouched. |
| P8 clip round-trip | same | `.anim` → `exportClip` → `buildList` → `sampleAt` equals the source at 1000 random times bitwise. |
| L1 latch ownership | `alsteppedanim_test.cpp` (pure `restoreValue` cases) + runtime R3 | Untouched slots restore; foreign writes stay; epoch change drops `T`. |

### 12.2 Runtime checks (self-test and wiring audit)
- **R1 option off = byte-identical (both engines).**
  - R1a static gate (prebuild grep): every new call site is behind `ALSteppedAnimRuntime::sActive`, an empty-latch test or `mPlayhead.isLive()`.
  - R1b master off, 600 frames of live animation, Dance Ball, impostors, shadows, probes, VCam bone POV, one snapshot: every counter 0 (playheads non-LIVE, latch writes, probes, overlay writes, follower re-matches, provenance mismatches).
  - R1c World clock 0x: hash (SAD §13 R1 hash + playhead sample times) for 30 frames off; enable Animation with every option on; disable; after release the 30-frame hashes equal the first set. Repeat for Display only, and for Animation → Display → Animation.
- **R2 attachments exactly follow held bones (Animation).** On every held frame for every stepped actor, at display time after `updateMove`: (a) every rigid attachment root drawable world matrix == compose(attach-point world, local) bitwise and == previous frame's; (b) every animesh follower's `mRoot` world == compose(attach point, object local) and its palettes `mGLMp == mLastGLMp`; (c) every rigged palette `mGLMp == mLastGLMp` (zero velocity); (d) particle source position == attachment drawable position. Zero failures over 600 frames, with an animesh tail, a particle emitter, rigged hair and a rigid collar worn.
- **R3 held invariance (Animation).** Hash of held-class joint locals, root and joint world matrices is constant between ticks for 600 frames with every option on; foreign writes logged.
- **R4 engine exclusivity.** Asserts of §2.7 never fire across 50 random engine/master/scope toggles while animating.
- **R5 server neutrality.** Message log over a scripted 5-minute session (AO, non-looping gestures, sit/stand): the sequence of `AgentAnimation` messages is identical with stepping off, Animation and Display; stop timestamps within one frame.
- **R6 velocity.** With `BDMergeVelocityDebug` on, held frames show neutral grey on stepped actors (both engines); step frames show the snap.
- **R7 page.** `sa_engine` value written, read back, consumer clamp honoured; every preset applies 30 values; "modified" after one tracked edit.
- **R8 benchmark (Tracy).** Zones `StepAnim::playhead`, `StepAnim::latch`, `StepAnim::probes`, `StepAnim::followers`, plus the overlay zones. 24 and 50 Bento actors, Everyone in view, 12 fps on 60 fps. Pass: Animation engine total avatar update time ≤ 0.85× stock without probes and ≤ 1.0× stock with probes; Display only per SAD R5; master off ≤ 0.01 ms.
- **R9 Display-only double/real-time audit.** Scene: animesh tail, particle attachment, Ghost Studio cadence copy, Face off, motion blur on and off, ReShade on and off. With `StepAnimDebugProvenance`: before the §10 fixes the log names D6/D8/D11; after them zero mismatches; screenshot comparison per the visual match gate.

---

## 13. Risks, in order

1. **The playhead branch in `LLMotionController`** (shared hot path for every avatar). Mitigated by leaving the stock lines untouched (§2.4), P1a/P1b and R1. The duplicated `updateMotionsByTypePlayhead` must track future edits (P1b catches drift).
2. **Latch ownership and post-layer interplay** (Pose Polish additive deltas, gaze, Director restore LIFO, ground-sit offset, appearance writes between [D] and [A], LOD early-out). Mitigated by the restore-before-anything order, bitwise-differ-only writes, epoch drop, R3.
3. **Pose Polish at ticks only** changes its behaviour for stepped actors (inertia/contact see 12 Hz input). Needs in-world tuning; fallback is running it every frame with its output latched (accepting lag).
4. **Frame-based smoothing inside STEP motions** (keyframe constraints, `llkeyframemotion.cpp:1700-1734`) converges per tick, not per frame. Rare content; measured.
5. **Animesh follower re-match** relies on `matchVolumeTransform` being idempotent and cheap; it already runs from two sites (`llvoavatar.cpp:5082`, `lldrawable.cpp:733`).
6. **Probe cost and correctness** (pure sampling + FK must match the real pose closely enough for adaptive thresholds). Probes ignore procedural layers by design.
7. **Display-only fix F2** changes what idle readers see for attachments (last rendered instead of smooth); check camera focus on attachments.
8. **External-sample owners** (Dance Ball) are captured at the tick frame, not the exact grid time (≤ one frame).

---

## 14. Open questions for the user

- **Q1** Per-actor engine: should an override let one actor use Display only (e.g. the Bone Lock subject) while the rest use Animation? (Default proposal: no, one global engine; `StepAnimExcludeCameraMounted` already keeps a mounted actor smooth.)
- **Q2** Face default: keep "Face & lip-sync" off by default (face moves in real time on a held head), or turn it on for the Animation engine?
- **Q3** Add "No motion blur on step frames" (suppress `BDMergeMotionBlur` for stepped actors on step frames), or leave blur as is (SAD D6 removed blur features)?
- **Q4** Export permission: allow `.anim`/BVH export only for animations you own with full permissions? Local editing and playback of anything is viewer-local.
- **Q5** Pose Polish on stepped actors: run at ticks (proposed, crisp poses) or every frame with its output latched (softer, eased poses)?
- **Q6** During a future scrub/pause, should gaze, look-at and physics freeze with the body (proposed default: freeze) or keep moving?
- **Q7** Is "first frame after the grid boundary" acceptable for Dance Ball synchronized dancers, or should Dance Ball sample its clips through the playhead's grid time (more work)?

---

## 15. Work packages (Phase 1, one build at the end)

| WP | Files | Content |
|---|---|---|
| WP1 core | `alsteppedanim.h/.cpp`, `tests/alsteppedanim_test.cpp` | `Config::engine`, `boundaryTime`, tracked list 30, preset values, T12/T13. |
| WP2 playhead | `llcharacter/llanimplayhead.h/.cpp` (new), `llmotioncontroller.h/.cpp`, `llmotion.h/.cpp`, `llkeyframemotion.h/.cpp`, `llcharacter.h`, `llemote`, `llhandmotion`, `llphysicsmotion` (kind only), `llvoavatar.cpp` (breathe/noise kinds), `llcharacter/tests/llanimplayhead_test.cpp`, `llcharacter/CMakeLists.txt` | §2.3-2.4, `getStepKind`, `canSampleAt/sampleAt`, `LLAnimClip` + `exportClip/buildList`, P1-P8. |
| WP3 runtime | `alsteppedanimruntime.h/.cpp`, `llvoavatar.cpp/.h` (hooks [A] [A'] [C] [D]), `llappviewer.cpp` ([E]) | Engine dispatch, latch, followers, probes, effects at the latch, `ALAnimPlayheadControl`, switching. |
| WP4 Display-only fixes | `alsteppedanimruntime.cpp`, `llvoavatar.cpp`, `alghoststudio.cpp/.h` | F1-F4, F7. |
| WP5 page and settings | `settings.xml`, `alpanelsteppedanim.cpp`, kit JSON (`look_group/stepped_tab`), `llfloaterdirector.cpp` provider, `kit_lint.py` rule | `sa_engine` row, hints F5/F6, preset provider 30 values. |
| WP6 checks | self-test / wiring audit | R1-R9. |

OFF-LIMITS for the implementer unless listed: the stock lines of `LLMotionController::updateMotions` / `updateMotionsByType`, `LLPoseBlender`, `lldrawpool.cpp`, shaders, `BDMergeVelocity*`, the quantum time-step path.

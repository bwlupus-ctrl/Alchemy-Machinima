# Stepped Animation: engine design (rev 2, 10/14/2026)

Rev 2 answers the Codex design review (NEEDS CHANGES). Resolution map in §18.

Plan: `doc/PLAN_STEPPED_ANIMATION_2026-10-13.md` (approved). Page data: `doc/director_kit/stepped_tab_mockup_data.json`.

## 0. Decisions and scope changes

| # | Decision | Source |
|---|---|---|
| D1 | Page `look_group/stepped_tab`, label "Stepped", next to Roto Ink and Tron. | user default |
| D2 | Default "what steps" = Pose + position. | user default |
| D3 | Default scope = Whole cast, with per-actor overrides. | user default |
| D4 | **Phase 2 is IN this build.** Smear frames and ease spacing are fully designed, have settings, page rows and unit tests. | user scope change, 10/14 |
| D5 | **All scopes are built:** Selected actor, Whole cast, Everyone in view, and the Mixed rate mode (target rate vs background rate). | user scope change, 10/14 |
| D6 | **No motion blur feature.** Plan Card 6 ("Motion blur & TAA on steps") and the hard-cut / real / partial modes and their settings are removed. No velocity-buffer feature work. §7 shows held frames get zero rigged motion with no velocity code, and names the one correctness-only change. | user scope change, 10/14 |
| D7 | The hold is a **render-window-only overlay**. It is written at the very end of `idle()` and restored right after `display()` and the snapshot pass, before the frame's sleep and before the next frame's input, mainloop listeners and coroutines. Nothing outside the render window (input, UI commits, coroutines, the Poser, simulation, recorders, ghosts) ever sees the held pose. Joints, attachment drawable matrices and name-tag positions are all restored together (§3). | this design (rev 2) |

Changes from the plan, with reasons:
- **Card 6 removed** (D6). The page has 6 cards, not 7.
- **"Attachments & particles stepped" becomes "Attachment FX".** Attachments are parented to joint xforms (`llviewerjointattachment.cpp:115`), so they always ride the held skeleton; a toggle that let them float smoothly off a held body would be meaningless. The toggle covers what attachments animate *themselves*: particles and scripted spin (llTargetOmega).
- **The Actor picker is always visible**, not only in Selected-actor scope: it is also how you choose whose per-actor override to edit and the Mixed target.
- **Presets drop the blur extras** ("hard-cut blur", "no motion blur", "hard cut") because of D6. The phase 2 extras were added where they fit the style (§12).
- **System-avatar morph faces** (LLEmote `express_*` morphs and blink morphs on non-Bento heads) are **not** held. Bento face bones and voice lip-sync are held (§8). Reason: holding morphs needs a CPU re-morph twice per frame, and `LLEmote` lives in llcharacter with no settings access. Machinima heads are Bento.
- **Exact twos in a 24 fps *screen recording* is not guaranteed** (§2.4). The clock is exact; a real-time screen recorder samples frames on its own timer.

## 1. Where the frame goes today (anchors)

Per viewer iteration (`LLAppViewer::frame` loop body, then `LLAppViewer::idle`, `llappviewer.cpp:5194`):

0. **Before `idle()`** (`llappviewer.cpp:1500-1549`): native window events, `gatherInput()` (UI clicks, drags and commits, e.g. the Poser, whose joint-frame math reads joint **world** rotations, `fsposeranimator.cpp:1118-1139`), `mainloop.post(newFrame)` listeners and `llcoro::suspend()` (coroutines); then joystick/keyboard/mouse scans (`:1574-1577`).
1. `LLPresentationTime::instance().tick()` (`llappviewer.cpp:5209`) freezes the frame's `LLTemporalFrameContext` (`lltemporalframecontext.h`): `presentation_time` (the scaled cinematic clock) and `wall_time` (unscaled monotonic). It is the only writer of `LLMotionController::sGlobalTimeFactor` (`llpresentationtime.h:21`).
2. `ALTron::latchFrame()` (`:5214`), path mover, `ALGhostStudio::updatePerFrame()`, `ALDanceBall::instance().tick()` (`:5533`).
3. `gObjectList.update()` → each `LLVOAvatar::idleUpdate` (`llvoavatar.cpp:2775`) → `updateCharacter()` (`llvoavatar.cpp:2937` calls `:5670`):
   - `actor_mover.restoreDirectorLookAtPose(this)` (`:5695`): undoes last frame's render-only look-at pose (the same pattern this design reuses).
   - `computeNeedsUpdate()` (`:5716`); LOD-skipped non-self avatars do `updateMotions(HIDDEN_UPDATE)` and return (`:5727`).
   - `updateRootPositionAndRotation()` (`:5780`) sets `mRoot` world position and rotation.
   - `updateMotions(NORMAL_UPDATE)` (`:5800`) → `LLCharacter::updateMotions` (`llcharacter.cpp:181`) → `mMotionController.updateMotions(force)` (`llcharacter.cpp:197`) → `LLMotionController::updateMotions` (`llmotioncontroller.cpp:865`). Animation time advances by `delta * mTimeFactor * sGlobalTimeFactor` (`:887`); `mPoseBlender.blendAndApply()` (`:947`) writes joint local rotations and positions. Joints with no active motion keep whatever was last written.
   - `mPosePolish.run()` (`:5825`), then `applyDirectorLookAt` or `applyGaze` (`:5831/5833`), the ground-sit offset, `updateHeadOffset()` (`:5854`), and `mRoot->updateWorldMatrixChildren()` (`:5860`); `mNeedsSkin = true` (`:5865`).
   - Back in `idleUpdate`: `idleUpdateMisc()` (`:2962`) queues attachment drawable moves (`updateMoveDampedAsync`/`updateMoveNormalAsync`, around `llvoavatar.cpp:3135+`); `idleUpdateLipSync()` (`:2966`, body at `:3363`) writes the `Lipsync_Ooh/Aah` morphs.
4. `ALGhostStudio::instance().updatePostObjectList()` (`:5543`): TRUE-mirror clones copy their source's final pose (`LLGhostAvatar::applyTrueMirrorStamp`, `llghostavatar.cpp:1572`).
5. Camera dispatch (recorder, path camera, CineCam incl. Bone Lock, flycam, agent camera) then `LLFlycamRecorder::instance().onIdleFrame()` (`:5738`), then `gObjectList.updateApparentAngles()` (`:5749`, LOD/visibility), `LLAvatarRenderInfoAccountant::idle()` (`:5753`) and the audio update (`:5755-5767`); `idle()` ends with the `mQuitRequested` / `idleShutdown` block (`:5772-5776`).
6. `display()` (`llappviewer.cpp:1628`): `gPipeline.updateMove` applies the queued attachment moves (`LLDrawable::updateXform`, `lldrawable.cpp:612`), geometry rebuilds run flexi (`LLVolumeImplFlexible::doUpdateGeometry`, `llflexibleobject.cpp:753` → `doFlexibleUpdate`, `:429`), `rebuildPriorityGroups` runs mesh-load callbacks (`gMeshRepo.notifyLoadedMeshes()`, `pipeline.cpp:4835`), draw pools build skin palettes, the UI draws, and the buffers swap (`llviewerdisplay.cpp:232/463/1915`).
7. After `display()`: the snapshot pass (`LLFloaterSnapshot::update()`, `LLFloaterSimpleSnapshot::update()`, optional probe update, `llappviewer.cpp:1630-1645`) and the stats recorder (`:1647-1650`), then "Main:Sleep" (`:1656`).

### 1.1 Skinning and the previous-frame palettes
- **Rigged mesh:** `LLVOAvatar::updateSkinInfoMatrixPalette` (`llvoavatar.cpp:11459`) rebuilds a palette once per `gFrameCount` per skin hash from `LLJoint::getWorldMatrix4a()` (`LLSkinningUtil::initSkinningMatrixPalette`, `llskinningutil.cpp:128`, read at `:153`). Before rebuilding, it swaps the outgoing palette into `mLastGLMp` and stamps `mLastFrame` (`llvoavatar.cpp:11473`; struct at `llvoavatar.h:905-937`). The velocity draw (`lldrawpool.cpp:1805-1828`) uploads `AVATAR_LAST_MATRIX` = `mLastGLMp` only when `mLastFrame == gFrameCount - 1`, else the current palette (zero motion).
- **System avatar mesh:** `llviewerjointmesh.cpp:178-205` keeps `mLastMatrixPalette` and rolls it once per frame in the velocity pass (`mLastVelocityPaletteFrame`).
- **Velocity pass:** `LLPipeline::renderGeomVelocity` (`pipeline.cpp:8105`, header comment `:8091-8103`), gated by `BDMergeVelocityBuffer` (settings.xml:9851, `pipeline.cpp:2658`); rigged polygon offset `BDMergeVelocityRiggedBias` (settings.xml:9862, read and clamped `[-16,0]` in `getVelocityRiggedBias`, `lldrawpool.cpp:136`); alpha stage `BDMergeVelocityAlpha` (settings.xml:9873, `pipeline.cpp:8220`). Rigged motion "lives entirely in the palettes" (`lldrawpool.cpp:1834`).
- **Rigid attachments:** drawable xform is parented to the joint (`llviewerjointattachment.cpp:115`), resolved in `display()` by `updateMove`.

## 2. The stepped clock

### 2.1 Time source
- **Capture time** (`StepAnimClock` = 0): `LLPresentationTime::currentFrame().presentation_time` (`llpresentationtime.h:52`). This is the clock the motion controllers already run on through `sGlobalTimeFactor`, so at 0.25x a stepped actor gets 12 steps per *presentation* second. Sped up 4x in post, that is exactly 12 per film second. At 0x (paused) the clock does not advance, so nothing re-captures.
- **Real time** (`StepAnimClock` = 1): `currentFrame().wall_time` (unscaled monotonic). 12 steps per real second whatever the world speed.
- Both are read from the one frozen frame context, so every consumer in a frame sees the same `t` (F64).

### 2.2 Grid, not accumulator (no drift)
For an actor with rate `r` (steps/s) and phase `φ` (fraction of a step, `[0,1)`):

```
tickIndex(t) = floor(t * r + φ + kTickEps)        kTickEps = 1e-6 (in step units)
```

computed from the absolute `t` every frame, never by accumulating `dt`. So the stepped phase cannot drift from the smooth timeline: after any number of seconds, the tick index equals the analytic one. `kTickEps` makes boundaries sampled at exactly `n/24` land on the right side despite F64 rounding (`2/24*12` evaluates to `0.999…`).

A **capture** happens on the first frame whose `tickIndex` differs from the last captured one. The held pose is the smooth pose of that capture frame, so at capture frames held == smooth exactly (§13 test T3).

**Rate changes** (adaptive switch, slider edit, hit-stop release) re-grid: the next boundary is `nextBoundary(t, r_new, φ)`; the index sequence keeps increasing (the core stores `mLastCaptureTick` in a rate-tagged form: `(rate_bits, index)`; any change of rate forces a compare on the new grid, and a capture happens only when the new grid's boundary is crossed). `ALSteppedAnim::Clock` owns this (§11).

### 2.3 Per-actor phase
- **Random** (default): `φ = unitHash(actorSeed, "phase")`, `actorSeed` = 64-bit hash of the canonical actor UUID (`LLDirectorCast::canonicalActorId`, `lldirectorcast.h`). Stable across sessions and replays.
- **Locked together:** `φ = 0` for everyone.
- **Manual phase:** `φ = StepAnimPhase` for everyone.
- **Match Roto Ink** adds a global phase term so ink redraws and pose steps land on the same frames (§9).
- TRUE-mirror ghosts and ghosts whose animation source is a stepped actor use their **source's** seed, rate and phase (§10.1), so a clone steps on the same frames as its source.

### 2.4 Capture exactness
Pass criterion "twos at 24 fps capture = the pose changes exactly every 2nd frame" holds for the clock: a frame stream at `t_n = t0 + n/24` gives a capture on every second frame (test T1, over one hour of frames). In the viewer that stream exists when frames are rendered at fixed presentation steps of 1/24, which is the reserved `LLTemporalMode::FIXED_FRAME_CAPTURE` (`lltemporalframecontext.h`; not yet wired, `TEMPORAL_CAPTURE_WORLD_TIME_SCALE_BRIEF.md:7`). With a real-time screen recorder (OBS at 24 fps over a 60/144 fps viewer) the clock is still exact, but the recorder samples on its own timer, so a boundary near a sample can split 2-2 as 1-3. Rendering at 0.5x World clock and speeding up in post halves that error. The design needs no change when fixed-frame capture lands.

At 60 or 144 fps render, an actor on 12 fps captures exactly 12 times per clock second (±1 over any window; test T2).

## 3. The joint-transform overlay (cache and re-apply)

### 3.1 Placement (D7): the render window
Three new calls in `llappviewer.cpp`, each first checking a static bool:

| Call | Where | What |
|---|---|---|
| `ALSteppedAnimRuntime::beginFrame()` | right after `ALTron::latchFrame()` (`:5214`), before the path mover, Ghost Studio and Dance Ball | **plan only**: read config, resolve scope (§4), `planFrame` per stepped actor (§11). Lip-sync, particles and attachment spin run later in `idle()` and query this plan. No pose writes. |
| `ALSteppedAnimRuntime::applyHeld()` | the **last statement of `idle()`'s main body**: after the audio update (`:5767`) and before the `mQuitRequested` block (`:5772`) | opens the window: for each stepped, drawn actor, read the smooth pose, observe limb motion, capture on capture frames, write the render pose (§3.3), sync attachment matrices (§3.6) and shift name tags (§3.7). |
| `ALSteppedAnimRuntime::endFrameRestore()` | in `LLAppViewer::frame()`, **immediately after the display branch** (after the snapshot pass and the stats recorder, `:1630-1650`), before "Main:Sleep" (`:1656`). Unconditional (it also runs when `display()` was skipped), gated only on `sHasOverlay`. | closes the window: restores joints, attachment matrices and name tags (§3.4). |

**Order decision (review MAJOR 3).** `applyHeld` runs **after** `gObjectList.updateApparentAngles()` (`:5749`), the render-info accountant (`:5753`) and audio, so LOD, pixel area, visibility, render accounting and voice positions all use the smooth roots. The held body differs from the smooth one by at most one step of motion; avatar spatial extents are already padded for animation, so culling against smooth extents is safe. If a fast actor in Pose + position is ever culled at a screen edge, the fix is to union the held root into `updateSpatialExtents` (`llvoavatar.cpp:1407`), not to move `applyHeld` earlier.

**What sees what.**
- Inside the window (end of `idle()` → display → swap → snapshot pass): `updateMove` (attachments), flexi rebuilds, skin palettes, shadows, reflection probes, outlines, VCam renders, snapshots and the UI draw. All render the held pose.
- Outside the window: the next frame's input, UI commits and drags (the Poser), `mainloop` listeners, coroutines, joystick/keyboard, `idle()` simulation, recorders, ghosts, the camera dispatch, LOD and audio. All see the smooth pose. This closes review BLOCKER 1: the Poser's world-rotation reads (`fsposeranimator.cpp:1118-1139`) happen in input handling or idle, both outside the window. The open-Poser exclusion (§10.1) stays, so a Poser floater *drawing* inside the window never shows a held value for the avatar it edits.
- Picking (hover/click, run in input handling) tests the smooth pose, which can differ from the drawn held pose by up to one step of motion. Accepted.

### 3.2 What is cached
Per actor, `ALSteppedAnim::PoseBuffer` (§11) over:
- every joint in `LLAvatarAppearance::mSkeleton` (`llavatarappearance.h:176`; mPelvis at index 0, all Bento bones),
- every `mCollisionVolumes[i]` (`llavatarappearance.h:364`), which carry avatar-physics deformation,
- `mRoot` (`llavatarappearance.h:144`; not in mSkeleton, parentless, so local == world).

Per joint: local rotation (`LLJoint::getRotation/setRotation`, `lljoint.h:249/250`), local position (`getPosition/setPosition(pos, false)`, `lljoint.h:232/233`; `false` = no override re-application, `lljoint.cpp:333`), local scale (`lljoint.h:258/259`).

Skeleton **scale is never held** (it comes from the shape, and from `updateMouselookHeadBoneScale`, `llvoavatar.cpp:5892`); only collision-volume scale (physics) is held. Scale is written only by smear frames (§6.3).

Joint classes (computed once per skeleton build by `classifyJoints`, §11): `ROOT`, `BODY`, `FACE` (the `mFaceRoot` subtree, minus gaze joints), `GAZE` (`mNeck`, `mHead`, `mEyeLeft`, `mEyeRight`, `mFaceEyeAltLeft`, `mFaceEyeAltRight`: the joints `LLActorMover::restoreDirectorLookAtPose` and the gaze layer own, `llactormover.cpp:6262-6271`), `PHYS` (collision volumes).

Held classes for an actor:

| Class | Held when |
|---|---|
| ROOT (position + rotation) | what = Pose + position |
| BODY (incl. mPelvis local position: hip bounce) | always |
| GAZE | `StepAnimGaze` |
| FACE | `StepAnimFace` |
| PHYS | `StepAnimFlexi` |

Non-held classes are rendered from the smooth pose of the same frame. Gaze smooth means the head and eyes are fully smooth (animation and gaze), riding the held chest.

### 3.3 applyHeld, per actor (opens the window)
1. Resolve the avatar through `gObjectList.findObject(id)` every frame (never retained, the Ghost Studio rule at `llghostavatar.cpp:1588`). Skip dead, unbuilt, impostored, `mSpecialRenderMode != 0`, UI avatars, and avatars not drawn this frame. Apply the actor cap (§16.1).
2. If `mLastSkeletonSerialNum` (`llvoavatar.h:498`), the joint count or the avatar's new `mJointOverrideEpoch` (§3.4) changed since the last capture, drop that actor's held state. Separately, if the **agent's** region handle or agent-space global origin changed (teleport/region crossing), drop every Pose + position held state so roots, attachments and labels are recaptured in the rebased coordinate system.
3. Read the smooth pose `S` into the buffer.
4. `observe(state, effectors(S), t)` (§5, §6).
5. If `decision.capture`: `H_prev = H`; `H = composeCapture(S, …)` (ease, §6.4; smear flag, §6.3).
6. Compose the render pose `R`: held classes from `H`, the rest from `S`; boil (§6.1) on BODY/GAZE/FACE rotations; smear (§6.3) on smear ticks.
7. Write `R` only where it differs bitwise from the current joint value; record each written value in `W` and the avatar's `mJointOverrideEpoch` at write time.
8. If anything was written: `mRoot->updateWorldMatrixChildren()`, `mNeedsSkin = true`, `invalidateSkinPalettesForOverlay()` (§7.2), `syncAttachmentMatrices()` (§3.6), and, when ROOT is held, the name-tag shift (§3.7).

On a plain capture frame with no boil or smear, `R == S`, nothing is written, and the cost is the read.

### 3.4 endFrameRestore, with ownership (closes the window)
For each actor with a non-empty `W`:
- For every written slot, if the joint's current value is bitwise equal to `W`, write the saved smooth `S` back; otherwise **leave it**: something wrote inside the window (a mesh-load callback from `gMeshRepo.notifyLoadedMeshes()` in `rebuildPriorityGroups`, `pipeline.cpp:4835`, which reaches `LLVOAvatar::addAttachmentOverridesForObject`, `llvoavatar.cpp:7858`, via `LLVOVolume::notifyMeshLoaded`, `llvovolume.cpp:1353`; or a UI draw).
- **Override epoch (correctness only).** New `U32 LLVOAvatar::mJointOverrideEpoch`, bumped in `addAttachmentOverridesForObject` (`:7858`), both `removeAttachmentOverridesForObject` overloads (`:8112`, `:8140`), `rebuildAttachmentOverrides` (`:7685`) and `resetSkeleton`. An epoch change does not by itself transfer ownership of every position/scale slot: each written rotation, position and scale is still restored when its current bits equal `W`, while a slot changed by an in-window writer is retained. This also guarantees collision-volume and pelvis/face positions, plus smear scale (purely an overlay write), cannot leak out of the render window. The epoch change drops the held state after restore so the next frame recaptures the rebuilt rig.
- Then `mRoot->updateWorldMatrixChildren()`, `syncAttachmentMatrices()` (§3.6), name-tag restore (§3.7), clear `W`.

This makes the overlay invisible outside the render window, including for joints no motion writes (otherwise a motion that ended between ticks would leave a joint stuck at the held value).

**Review BLOCKER 2 (confirmed covered).** `restoreMeshData()` → `updateJointLODs()` (`llvoavatar.cpp:2490-2500`) is called from the visibility update inside `idleUpdate` (`:6176`), which is outside the window, so it sees the smooth pose. The mesh-load callbacks that *do* run inside the window (`pipeline.cpp:4835`) are handled by the ownership rule and the override epoch above. Appearance messages and `updateVisualParams` from the network arrive in `idle()`, outside the window.

### 3.5 Pose only vs Pose + position
- **Pose:** ROOT not held. The body travels smoothly through space, limbs step. Rigged velocity on held frames is the real body translation (correct, not ghosting).
- **Pose + position:** ROOT world position and rotation held too, so traversal (sim interpolation, Actor Mover walks, turns) steps. Held frames are bitwise identical in joint world matrices (§7).

### 3.6 Attachment matrices (review MAJOR 5)
Rigid attachment drawables are parented to joint xforms (`llviewerjointattachment.cpp:115-128`) and normally re-resolved only when `gPipeline.updateMove` processes a queued move in `display()` (queued by `idleUpdateMisc`, `llvoavatar.cpp:3135+`, and only on detailed-update frames). Two problems follow if nothing else is done: after the window closes, the cached drawable matrices would stay held until the next display; and an avatar with no queued move this frame would draw its attachments at the smooth pose on a held body.

`syncAttachmentMatrices(av)` is called by `applyHeld` (after writing the held pose) and by `endFrameRestore` (after restoring the smooth pose). For every attachment point it refreshes the linkset root matrix first; unchanged roots skip the linkset. Changed linksets refresh child matrices in parent-before-child order and queue only the root with `gPipeline.markMoved`, avoiding child `updateXform` rebuilds for spinning prims. The ordinary `mMovedList` is drained earlier in idle, before `applyHeld`; display drains `mMovedBridge`, so this explicit matrix refresh is what makes the held pose visible in the current render window.

Result: inside the window, attachments render at the held pose; outside it, everything that reads attachment matrices sees smooth ones: cameras or focus targeted at an attachment (CineCam/flycam/DoF focus on an object, run in the idle camera dispatch), build and selection tools, manipulators and picking (input handling). Cost: one matrix compose per prim in each changed attachment linkset (§16).

### 3.7 Name tags and nameplates follow the held root (review MAJOR 4)
Only when ROOT is held (Pose + position); in Pose mode the body travels smoothly and the labels already match.
- **Avatar name tag and voice dots.** `idleUpdateNameTag` places `mNameText` at `root_pos_last` (`llvoavatar.cpp:3658`), computed by `idleCalcNameTagPosition(mLastRootPos)` (`:4045`) from the root cached before `updateCharacter` (`:2931-2933`); the voice visualizer is placed at `:3102`. In `applyHeld`, `delta = heldRoot − smoothRoot` (agent space). Both HUD objects are shifted by `delta` (`setPositionAgent(current + delta)`, current from `gAgent.getPosAgentFromGlobal(getPositionGlobal())`), and the shifted and original positions are recorded. `endFrameRestore` puts the original back when the object's position still equals the shifted one (same ownership rule).
- **Ghost nameplates.** `ALGhostStudio::updateNameplates()` (`alghoststudio.cpp:1871`, called at `:2007` in `updatePerFrame`, before avatar evaluation) builds labels from `nameplateAnchor` and hands them to `ALGhostNameplates::update` (tags in `mTags`, `alghostnameplates.h:54`). New `ALGhostNameplates::applyOffsets(fn)` / `restoreOffsets()`: `applyHeld` calls it with `fn(key)` = the held-root delta of that ghost instance's avatar (zero if it is not held), and `endFrameRestore` undoes it with the same ownership rule.
- Name tags are therefore drawn with the stepped body, while every idle-time consumer of `mLastRootPos` and the nameplate anchors keeps the smooth values.

## 4. Scopes and the Mixed mode

`resolveActor(config, facts)` (§11) is pure. Facts per avatar: `inCast`, `isSelf`, `isSelectedActor`, `override`, `ghostSource`, exclusions (§10).

Selected actor = `LLDirectorCast::mSteppedActorId` (new, scene-saved, null = You; §14.2). Picking "Selected cast" in the picker stores the actor selected in the cast list at that moment.

Whole cast = `LLDirectorCast::getIds()`; empty cast means You (same rule as `getRoster`, `lldirectorcast.h`). Everyone in view = every `LLVOAvatar` (residents, animesh, ghosts) that is drawn and not impostored this frame.

| Scope | Fixed / Adaptive | Mixed |
|---|---|---|
| Selected actor | only the selected actor, at Rate | selected actor at Rate; rest of the cast at Background |
| Whole cast | every cast member at Rate | selected actor at Rate; other cast at Background |
| Everyone in view | every drawn avatar at Rate | selected actor at Rate; everyone else in view at Background |

Then per-actor overrides, which beat the scope table:
- `Never stepped`: not stepped.
- `Own settings`: stepped at its own rate (and own "what steps" unless Inherit), even outside the scope.
- Mixed role `Target` / `Background` forces the role; `Auto` uses the table.

Background rate below 2 means smooth (not stepped). Adaptive applies to Rate (the slow rate) for every actor stepped at Rate; in Mixed, background actors are fixed.

## 5. Adaptive rate

Limb speed is measured in **pose space**, so walking translation does not count:
- effectors: `mWristLeft`, `mWristRight`, `mAnkleLeft`, `mAnkleRight`, `mHead`;
- `p_e = (world(e) - world(mPelvis)) * ~worldRot(mRoot)`, from the **smooth** pose at applyHeld;
- velocity per effector over a window of at least 33 ms from a 16-sample ring buffer (frame-rate independent; at 144 fps it spans ~5 frames);
- `speed = max_e |v_e|`, then an exponential smoother with τ = 50 ms.

States `SLOW` (Rate) and `FAST` (Fast rate). **Strict comparisons with enforced minimums (review MAJOR 6):** switch to FAST only when `speed > T`; back to SLOW only when `speed < T·(1−H)` with `H ≥ 0.05` (clamp, §14.1), so the two thresholds are always at least 5% apart and a speed exactly at T can never alternate. A switch is allowed only after the current state has lasted `dwell = max(StepAnimAdaptMinDwell, 1 / currentRate)` seconds (`StepAnimAdaptMinDwell ≥ 0.05`; the `1/rate` floor guarantees at least one full step; when the current rate is smooth the floor is 0.05 s). Decisions made at applyHeld of frame f take effect in `planFrame` of frame f+1 (one frame of latency, so every consumer in a frame agrees). Entering FAST forces a capture on the next frame. Fast rate below 2 = smooth: every frame captures.

Test T5 covers threshold crossing, a speed dithering inside the hysteresis band (exactly one transition), a speed held exactly at T with H and dwell at their minimums (zero transitions after the first), and dwell.

## 6. Character effects

### 6.1 Pose boil
- Boil index `b = floor(tickIndex_base / StepAnimBoilTicks)`, where `tickIndex_base` is the actor's tick on its **base-rate** grid (Rate and φ), so adaptive FAST frames do not boil at render rate.
- Per joint j in BODY ∪ GAZE ∪ FACE (never ROOT, never PHYS): axis = normalized vector from three hashes `unitHash(actorSeed, b, j, 0..2)·2−1` (re-drawn deterministically if degenerate); angle = `(2·unitHash(actorSeed, b, j, 3) − 1) · A`, `A = StepAnimBoilAmount` degrees. `R_rot = H_rot * boilQuat`.
- `unitHash` = splitmix64 → `[0,1)` F32, the same construction as `ALGazeNoise::unitHash` (`algazenoise.h`), but private to the core so it stays standalone.
- Deterministic per (actor, b, j); `|angle| ≤ A` exactly; identical on every held frame of the same b (test T6).

### 6.2 Hold on impact (hit-stop)
- Per effector, per frame: `decel = (|v_prev| − |v_now|) / dt_window`, using the same windowed velocities as §5.
- Trigger when `decel ≥ StepAnimImpactThreshold` and `|v_prev| ≥ 1.0 m/s`, and the actor is not already holding or in cool-down.
- On trigger at frame f: frame f+1 is a forced capture (the impact pose), then the next `StepAnimImpactTicks` grid captures are suppressed (the held pose stays), then normal capturing resumes. Cool-down: no re-trigger until one normal capture after release.
- Animation time keeps running (plan anchor: only the applied pose is held), so the release jumps to the current pose, as on a hit-stop drawn on twos. The hold is measured in grid ticks of the actor's current rate.

### 6.3 Smear frames (phase 2, in this build)
- On a capture, per limb chain (L arm: `mShoulderLeft`, `mElbowLeft`; R arm; L leg: `mHipLeft`, `mKneeLeft`; R leg), compute the chain end-effector's pose-space displacement `d` between `H_prev` and the new `H`, and its speed `d / Δt_tick`.
- **Guards:** no smear for a chain if its rest length `L` (sum of the chain's child local offsets) is below 0.01 m, if any bone in it is below 0.005 m, if `d < 1e-4` m, or if `Δt_tick ≤ 0`.
- If speed > `StepAnimSmearThreshold`, this tick is a **smear tick** for that chain: for the chain joints,
  - rotation `= slerp(H_prev, H, 1 − 0.5·s)` with `s = StepAnimSmearStrength` (the in-between lags toward the old pose);
  - **total chain stretch** `K = 1 + 0.6·s·clamp(d / L, 0, 1)`, `K ∈ [1, 1.6]`, shared across the `n` chain joints as `k = K^(1/n)` each, so the compounded stretch never exceeds 1.6 (not 1.6² = 2.56);
  - **direction:** `LLJoint` scale is per local axis (no shear), so the stretch follows the displacement as closely as axis-aligned scale allows: express the unit displacement `û` in each joint's local frame and scale each local axis `i` by `1 + (k − 1)·|û_i|`. A limb moving along its length stretches along the bone; a limb swinging sideways widens across it, as a drawn smear does. Applied on top of the smooth scale.
- The smear lasts one tick; the next capture shows the plain pose. `s = 0` is exactly the identity (test T8).
- Scale writes go through `W`, so `endFrameRestore` restores the smooth scale.

### 6.4 Ease spacing (phase 2, in this build)
- **Cadence-independent selection (review minor).** Each tick window is divided into `M = 4` fixed sub-steps on the clock grid (`tickIndex·4 + j`, from the same absolute `t`, not from frame count). The limb tracker's speed is evaluated **at each sub-step time** by linear interpolation between the bracketing frames, so the same motion gives the same per-sub-step speeds at 60 or 144 fps. The candidate sub-step is the one with the lowest speed; its pose `C` is the smooth pose of the first rendered frame at or after that sub-step time (stored when that frame arrives; one PoseBuffer copy per sub-step at most).
- At capture: `w = a · clamp(1 − s_min / max(s_cap, ε), 0, 1)` with `s_min` and `s_cap` the sub-step speeds (the candidate's and the capture sub-step's), `a = StepAnimEaseAmount`; `H = slerp/lerp(S, C, w)` for BODY/GAZE/FACE (ROOT and PHYS take S). Then the window resets.
- `a = 0` gives `H == S` bitwise (test T9). Held poses lean toward the most recent extreme, so poses read as snappier key poses.
- Scope of the guarantee: the *choice* (sub-step and weight) is identical at 60 and 144 fps. The candidate pose itself can only come from a rendered frame, so it is the pose at most one frame after the sub-step time, exactly like the base capture (§2.2), which also lands on the first frame after a boundary. Bitwise-identical held poses across frame rates exist only under fixed-frame capture (§2.4). Test T9 asserts the identical choice.

## 7. Velocity and TAA on held frames (no feature, correctness only)

### 7.1 Does holding give previous = current naturally? Yes, with one condition
- Rigged palettes are rebuilt each frame from joint world matrices (§1.1). On a held frame N+1 the overlay writes the same local values `H` it rendered at N, and (in Pose + position) the same root. `updateWorldMatrixChildren` is deterministic, so the world matrices, and therefore `mGLMp`, are **bitwise equal** to frame N's, which the swap at `llvoavatar.cpp:11473` made `mLastGLMp`. The contiguity check (`lldrawpool.cpp:1818`) passes, so the velocity draw outputs zero rigged motion. No velocity code is needed.
- System avatar mesh: `llviewerjointmesh.cpp:178-205` builds `mat` from the same joints at render time; `mLastMatrixPalette` is last frame's `mat`, so it is zero too.
- Rigid attachments: model matrix from the joint-parented xform (`syncAttachmentMatrices` in `applyHeld`, then `updateMove`) → unchanged → zero.
- Pose only: the root moves smoothly, so velocity is the real body translation. That is correct motion, not a ghost trail.
- Step frames carry the real snap as velocity, which is what TAA needs to reproject correctly (anyone with `BDMergeMotionBlur` on will see the snap blurred; no option changes that, per D6).

### 7.2 The one correctness-only change
**The condition:** the palette for render frame N must be built *after* `applyHeld`, and its previous palette must be the pose actually rendered in N-1. `applyHeld` runs in idle while `gFrameCount` still names the previous render; `display()` increments it before palette construction. Idle-time consumers (`ALGhostStudio` cadence/frozen palettes and `LLActorMover` ghost batches) therefore build smooth palettes under the old frame id, not the about-to-render id.

**Smallest fix (correctness only, not a feature):**
- `MatrixPaletteCache` gets an explicit `mOverlayFrame`, set by `applyHeld` to `gFrameCount + 1`, the frame that `display()` is about to render. Comparisons never mistake the previous render's cache for the upcoming one.
- After render-window restore, a cache that was built for `mOverlayFrame == gFrameCount` first saves that held palette as `mLastGLMp`/`mLastFrame`, then invalidates `mGLMp` with `mRebuildNoRoll`. Thus any idle-time reader rebuilds and receives the smooth pose without replacing the last actually rendered pose.
- At the next overlay build, if `mLastFrame == gFrameCount - 1`, the smooth idle palette is not rolled into `mLastGLMp`; the saved rendered palette stays contiguous. Held frames therefore compare held-to-held (zero velocity), while step/moving frames compare the last rendered hold to the new rendered pose (real velocity).
- Both invalidation calls are reached only for active stepped actors. With the feature off no overlay frame is tagged, no palette is invalidated, and the added cache-branch predicates remain false.

No change to `lldrawpool.cpp`, `llviewerjointmesh.cpp`, the shaders, or any `BDMergeVelocity*` setting.

## 8. Flexi, physics, hair, attachments, lip-sync

- **Attachments:** always held with their joint (drawable xform parented to the joint, resolved in `display()`).
- **Rigged hair and clothes:** skinned to held joints, so held. Bento hair bones driven by animations are BODY joints.
- **Avatar physics** (`LLPhysicsMotion` writes driven visual params, `llphysicsmotion.cpp:783`, which move collision volumes): held through the PHYS class when `StepAnimFlexi` is on; smooth when off. The physics simulation itself keeps integrating every frame (simulation is never touched).
- **Flexi prims:** in `LLVolumeImplFlexible::doUpdateGeometry` (`llflexibleobject.cpp:753`), the existing attachment-to-avatar walk (`:758-773`, used for the impostor skip) gains: if `ALSteppedAnimRuntime::sActive` and the avatar is stepped, `StepAnimFlexi` is on and this is not a capture frame → `return true` (no update). `doFlexibleUpdate`'s `mTimer.getElapsedTimeAndResetF32()` (`:462`) is not reset on skipped frames, so the capture frame integrates the whole interval (capped at 0.2 s by the existing clamp).
- **Attachment FX** (`StepAnimAttachFX`):
  - Particle sources: in `LLViewerPartSim::updateSimulation`, a held source accumulates `dt` and skips `update(dt)`; on capture it emits using the accumulated interval. Source emission and scripted spin are independent of particle motion.
  - Spin: in `LLViewerObject::idleUpdate` (`llviewerobject.cpp:2532-2539`), for attachments of a stepped actor, `omega_dt` is accumulated in a runtime side map (keyed by object id) on held frames and applied as one `applyAngularVelocity(sum)` on capture.
- **Step particles** (`StepAnimParticles`, default off): particles sourced by a stepped avatar or its attachments hold position, velocity, age, colour, scale and glow while `decision && hold && !capture`. Each particle carries skipped time and consumes it once on the next tick, including offscreen groups; lifetime expiry happens by that tick. They remain drawn while held, and follow-source re-anchoring still uses the held attachment. HUD and world sources are unaffected. Catch-up uses at most 16 force substeps of at most 1/30 s; longer holds retain the remaining ballistic time and full age. Disabling or leaving the stepped assignment invalidates debt without replay.
- **Step texture animation** (`StepAnimTextureAnim`, default off): texture transforms on attachments, including worn animesh, hold between ticks. The animation clock continues through holds (including presentation-time drive), so each tick evaluates the current frame/offset/rotation without drift. HUD attachments and non-attachments are unaffected; release resumes at current time.
- **Lip-sync:** with `StepAnimFace` on, `idleUpdateLipSync` (`llvoavatar.cpp:3363`) runs only on capture frames for a stepped actor (the morph weights persist between). It runs before applyHeld, which is why the frame plan is made in beginFrame.
- **Face** = FACE joints (Bento face). System `express_*`/blink morphs are not held (§0).
- Flexi, Attachment FX and lip-sync are **simulation deferrals**, not overlays: they update less often by design, so their stepped state is visible outside the render window. That is the intended look, and it never touches joints.

## 9. Match Roto Ink redraw

Roto Ink's redraw is `floor(rotoClock * CineOutlineSketchFPS)` in the shader (`cineOutlineF.glsl:254`), with `CineOutlineSketchFPS` clamped `[0.5, 60]` (`pipeline.cpp:18391`) and `CineOutlineMotionStep` clamped `[0, 3]` → `{0, 12, 8, 6}` fps (`pipeline.cpp:18406-18410`); settings at settings.xml:328 and :412; Roto page rows `ri_sketch_fps_slider` (2 to 24) and `ri_motion_step_combo` (setting `CineOutlineMotionStep`). The Roto clock is `mRotoAnimClock`, accumulated from presentation time and scaled by `CineOutlineAnimSpeed` (`pipeline.cpp:18753-18790`).

`StepAnimMatchRotoInk` (a settings listener in the runtime, active only while it and `StepAnimEnabled` are on) re-writes on any change of `StepAnimRate`, `StepAnimMode`, `StepAnimEnabled` or itself:
- `CineOutlineSketchFPS = clamp(StepAnimRate, 2, 24)` (the Roto page range, inside the consumer clamp);
- `CineOutlineMotionStep = 1 / 2 / 3` when the rate is 12 / 8 / 6 (±0.01); otherwise left unchanged;
- it never turns Roto Ink or its sketch on, and turning Match off leaves the Roto values as they are.

Phase: new const accessor `LLPipeline::getRotoAnimClockOffset()` returns `mRotoAnimClock − presentation_time` from the last render. While Match is on, the global phase gets `+ frac(offset · rate)` so Locked (and Manual) actors' steps land on the ink's redraw frames when Roto speed is 1. Random offsets stay per actor, by design. The Roto page shows "modified" after a Match write, as expected.

## 10. Exclusions and interactions

### 10.1 Interactions
| System | Behaviour |
|---|---|
| **Camera, flycam, path camera, CineCam** | Never stepped. They run before applyHeld and read the smooth skeleton. |
| **Own avatar in mouselook** | Excluded while `StepAnimExcludeCameraMounted` (default on). |
| **Attached cameras** | Excluded while `StepAnimExcludeCameraMounted`: the CineCam Bone Lock subject and VCam bone-POV anchors. `collectActors` takes one `LLPrismLens::RegistrySnapshot` per frame, resolves its active bone-POV anchors once into a set, then tests avatars against that set; it never copies the registry per avatar. When the exclusion is off, these actors step and the camera stays smooth. |
| **Ghosts** | A TRUE-mirror clone (`isTrueMirrorDriven`, `llghostavatar.h:164`) is stamped from its source's smooth pose after `gObjectList.update` (`llappviewer.cpp:5543`). It uses its source's seed, rate, phase and plan, so its captures match the source frame for frame. Other ghosts whose `mAnimationSourceId` is a stepped actor follow the source the same way. Otherwise ghosts are ordinary avatars (stepped in Everyone in view). `endFrameRestore` restores ghosts too, so the stamp and a paused clone controller never see held values; ghost nameplates follow the held root (§3.7). |
| **Ghost AutoAnimate** | Plays motions; stepped like any motion. |
| **Dance Ball** | Forces motions locally (`aldanceball.cpp`); stepped like any motion. Dance Ball unison wants **Locked** offsets (Random desyncs visible steps). Non-cast dancers step only in Everyone in view. |
| **Poser** | An avatar being edited in an open Poser (`FSPoserAnimator::isPosingAvatar`, `fsposeranimator.h:450`; expose `FSFloaterPoser::isEditingAvatar(const LLVOAvatar*)`) is excluded, so the Poser's own draw (inside the render window) never shows a held value for it. Its commits and drags run in input handling, outside the window, so they always read the smooth pose (`fsposeranimator.cpp:1118-1139`). Once closed, the posed actor steps normally. |
| **Actor Mover** | Writes the root (traversal) and gaze before applyHeld, so Pose + position steps the walk; the gaze toggle decides the head. Director look-at keeps its own render-only restore at `llvoavatar.cpp:5695`; ours runs before it (at the end of the previous frame) and restores the post-look-at smooth values, which is the state stock leaves there. |
| **Pose Polish** | Runs on the fresh smooth blend (`:5825`); unaffected because the restore makes its inputs stock. |
| **Take recorder** | `LLFlycamRecorder` samples the camera at `onIdleFrame` (`:5738`), before applyHeld: takes store smooth data. Gaze cues, paths and scene data are smooth. Stepping is playback/render only. |
| **Temporal (World clock)** | Capture time follows it (§2.1). At 0x nothing re-captures. |
| **Actor FX / Tron / Roto Ink** | Independent render looks; they draw whatever pose is held. Actor FX "Reset style" must keep `mStepped` (§14.2). |

### 10.2 Always excluded
Dead or unbuilt avatars, impostors, `mSpecialRenderMode != 0` (animation preview, `llvoavatar.cpp:5793`), UI avatars, avatars not drawn this frame.

## 11. Pure core: `indra/newview/alsteppedanim.h/.cpp`

No viewer globals, no settings, no `LLVOAvatar`. Depends only on llcommon/llmath (`LLQuaternion`, `LLVector3`). Unit-tested with TUT, registered like `alflycamweighted`: `LL_ADD_INTEGRATION_TEST(alsteppedanim "alsteppedanim.cpp" "${test_libs}" "${test_project}")` (`newview/CMakeLists.txt` ~2836), test file `indra/newview/tests/alsteppedanim_test.cpp`.

```cpp
namespace ALSteppedAnim
{
    enum EScope  : S32 { SCOPE_SELECTED = 0, SCOPE_CAST = 1, SCOPE_VIEW = 2 };
    enum EMode   : S32 { MODE_FIXED = 0, MODE_ADAPTIVE = 1, MODE_MIXED = 2 };
    enum EWhat   : S32 { WHAT_POSE = 0, WHAT_POSE_POSITION = 1 };
    enum EClock  : S32 { CLOCK_CAPTURE = 0, CLOCK_REAL = 1 };
    enum EOffset : S32 { OFFSET_RANDOM = 0, OFFSET_LOCKED = 1, OFFSET_MANUAL = 2 };
    enum EJointClass : U8 { JC_ROOT = 1, JC_BODY = 2, JC_FACE = 4, JC_GAZE = 8, JC_PHYS = 16 };

    constexpr F64 kTickEps = 1e-6;

    struct Config            // every global setting, already sanitized
    {
        bool enabled = false; S32 scope = SCOPE_CAST; bool excludeCameraMounted = true; S32 maxAvatars = 24;
        S32 mode = MODE_FIXED; F32 rate = 12.f; F32 fastRate = 24.f;
        F32 adaptThreshold = 1.5f; F32 adaptHysteresis = 0.3f; F32 adaptMinDwell = 0.25f;
        F32 backgroundRate = 8.f; S32 what = WHAT_POSE_POSITION;
        bool face = true; bool gaze = true; bool flexi = true; bool attachFX = false;
        S32 clock = CLOCK_CAPTURE; S32 offset = OFFSET_RANDOM; F32 phase = 0.f; S32 baseFps = 24;
        bool boil = false; F32 boilAmount = 0.6f; S32 boilTicks = 2;
        bool impact = false; S32 impactTicks = 3; F32 impactThreshold = 25.f;
        bool smear = false; F32 smearStrength = 0.5f; F32 smearThreshold = 2.5f;
        bool ease = false; F32 easeAmount = 0.5f;
        bool matchRoto = false;
    };
    Config sanitize(const Config& raw);          // applies exactly the §14.1 clamps
    F32    effectiveRate(F32 rate_or_smooth);    // < 2 -> 0 (smooth), else clamp [2,30]
    F64    effectiveDwell(F32 min_dwell, F32 current_rate);  // max(min_dwell, 1/rate); 0.05 when smooth

    struct ActorOverride { S32 mode = 0; F32 rate = 12.f; S32 what = -1; S32 role = 0; };
    ActorOverride sanitize(const ActorOverride& raw);     // §14.2 clamps

    struct ActorFacts
    {
        U64  seed = 0;            // hash of the canonical actor id (or ghost source's)
        bool inCast = false, isSelf = false, isSelected = false, drawn = false;
        bool excludedAlways = false;     // §10.2
        bool cameraMounted = false;      // mouselook self, Bone Lock subject, bone-POV anchor
        bool posing = false;             // open Poser editing this avatar
        F32  cameraDistance = 0.f;       // for the actor cap (§16.1)
        ActorOverride ov;
    };
    struct Assignment
    {
        bool stepped = false; bool adaptive = false;
        F32 rate = 0.f;          // grid rate (0 = smooth)
        F32 fastRate = 0.f;      // adaptive FAST rate (0 = smooth)
        S32 what = WHAT_POSE_POSITION; F64 phase = 0.0; U8 heldClasses = 0;
    };
    Assignment resolveActor(const Config&, const ActorFacts&, F64 roto_phase);
    // §16.1: applies maxAvatars; selected, overridden and cast actors first, then nearest
    void       applyActorCap(const Config&, const std::vector<ActorFacts>&, std::vector<Assignment>& inout);
    U8         heldClasses(const Config&, S32 what);
    F64        actorPhase(const Config&, U64 seed, F64 roto_phase);  // [0,1)

    // clock
    S64  tickIndex(F64 t, F64 rate, F64 phase);
    F64  nextBoundary(F64 t, F64 rate, F64 phase);

    // limb tracking (pose-space effector positions supplied by the runtime)
    constexpr U32 kEffectors = 5;   // wristL, wristR, ankleL, ankleR, head
    struct LimbFrame { F64 t; LLVector3 p[kEffectors]; };
    struct LimbTracker   // 16-sample ring, >= 33 ms velocity window, tau 50 ms smoother
    {
        void  push(const LimbFrame&);
        F32   speed() const;                 // smoothed max effector speed, m/s
        F32   decel(U32 e) const;            // m/s^2, windowed
        F32   prevSpeed(U32 e) const;
        void  reset();
    };

    struct ActorState
    {
        LimbTracker limbs;
        // clock
        bool  hasCapture = false; F64 captureRate = 0.0; S64 captureTick = 0;
        // adaptive
        bool  fast = false; F64 stateSince = 0.0; bool forceCapture = false;
        // impact
        S32   impactHoldLeft = 0; bool impactCooldown = false; bool impactPending = false;
        // ease (sub-step grid, §6.4)
        static constexpr U32 kEaseSubsteps = 4;
        F32   subSpeed[kEaseSubsteps] = {}; bool subSeen[kEaseSubsteps] = {};
        S32   candidateSub = -1; bool candidatePending = false;
        // smear
        bool  smearTick = false;
    };

    struct FrameDecision
    {
        bool capture = false;    // take a new held pose this frame
        bool hold = false;       // render the held pose (false = render smooth)
        S64  tick = 0;           // current grid tick (actor's current rate)
        S64  boilIndex = 0;      // §6.1
        F32  rate = 0.f;         // rate in force this frame
    };
    FrameDecision planFrame(ActorState&, const Assignment&, const Config&, F64 t);
    void          observe(ActorState&, const Config&, const LimbFrame&);  // adaptive + impact + ease window

    // effects
    LLQuaternion boilRotation(U64 seed, S64 boilIndex, U32 joint, F32 amplitude_deg);
    struct SmearChain { F32 lagT = 1.f; F32 stretch = 1.f; bool active = false; };
    // total chain stretch K in [1,1.6]; per joint k = K^(1/n); inactive on the §6.3 guards
    SmearChain   smearFor(F32 displacement, F32 tick_dt, F32 chain_len, F32 min_bone_len,
                          U32 n_joints, F32 strength, F32 threshold);
    LLVector3    smearAxisScale(F32 k, const LLVector3& dir_local);   // 1 + (k-1)|u_i| per axis
    F32          easeWeight(F32 amount, F32 s_min, F32 s_cap);
    // speed at a sub-step time by linear interpolation between bracketing frames
    F32          speedAt(const LimbTracker&, F64 t_sub);
    F32          unitHash(U64 seed, S64 a, U32 b, U32 c);    // splitmix64 -> [0,1)

    // pose buffers and ownership
    struct JointLocal { LLQuaternion rot; LLVector3 pos; LLVector3 scale; };
    struct PoseBuffer { std::vector<JointLocal> joints; LLVector3 rootPos; LLQuaternion rootRot; };
    struct JointDesc  { std::string name; S32 parent; bool collisionVolume; };
    std::vector<U8> classifyJoints(const std::vector<JointDesc>&);   // EJointClass per joint
    void composeRender(const PoseBuffer& smooth, const PoseBuffer& held, const std::vector<U8>& classes,
                       U8 held_mask, PoseBuffer& out);
    // restore rule (§3.4): returns the value endFrameRestore must write, or current if foreign-owned;
    // Every component still equal to our write restores; foreign component writes stay.
    // epoch_changed invalidates the held capture after this ownership decision.
    JointLocal restoreValue(const JointLocal& current, const JointLocal& written, const JointLocal& smooth,
                            bool epoch_changed);
    bool       bitEqual(const JointLocal&, const JointLocal&);

    // Roto Ink match (§9)
    struct RotoMatch { F32 sketchFps; S32 motionStep; bool setMotionStep; };
    RotoMatch mapRotoInk(F32 rate);

    // presets (§12): built-in table, data only
    struct Preset { const char* id; const char* label; Config values; };
    const std::vector<Preset>& builtInPresets();
    const std::vector<std::string>& trackedSettings();   // §12.1, 32 names
}
```

Viewer side (not pure): `alsteppedanimruntime.h/.cpp`, singleton `ALSteppedAnimRuntime` with `static bool sActive` (master on) and `static bool sHasOverlay` (render/palette window open, including a zero-write capture frame). It owns the per-actor map `LLUUID → {ActorState, PoseBuffer S, H, H_prev, C, W, classes, skeletonSerial, overrideEpoch, nameTagShift}`, `beginFrame()` (plan only), `applyHeld()` (opens the window), `endFrameRestore()` (closes it), `syncAttachmentMatrices()`, the query `const FrameDecision* decisionFor(const LLVOAvatar*)` (used by flexi, particles, spin, lip-sync), the exclusion probes, the Roto Match listener and a debug counter block. The page is `alpanelsteppedanim.h/.cpp` (`root_class` `panel_stepped_anim`).

## 12. Presets

Provider key **`stepped_anim`**, registered in `registerLookPresetProviders()` (`llfloaterdirector.cpp:531`) next to `roto_ink` and `tron_look` (`:540-542`). Unlike `ALLookPresetProvider` (`:412`, built-ins only), it supports user Save/Delete with an `ALKitSnapshotStore("stepped_anim", presetSettings())`, the shape of `ALProjectorShaftPresetProvider` (`:486`) and `RigRimPresetProvider` (`alpanelrigrim.cpp:32-66`): built-ins `b:<id>`, user presets `u:<name>`. `apply("b:<id>")` writes the 29 preset settings from the table, then `StepAnimEnabled = true` (the master is not tracked, as for Rig Rim). Presets never touch per-actor overrides or the selected actor.

"Modified" is the kit strip's comparison of the tracked settings against the values captured at apply (`ALKitPresetStrip::captureApplied` / `isModified`, `alkitpresets.cpp:409/436`).

A non-UI persisted `StepAnimParticlesMigrated` marker (Boolean, default 0) migrates the shipped `StepAnimAttachFX` particle hold once after settings load: enable `StepAnimParticles` when Attachment FX was enabled, then mark migration complete. Later opt-outs are preserved.

### 12.1 Tracked settings (32)
`StepAnimScope, StepAnimIncludeGhosts, StepAnimMode, StepAnimRate, StepAnimFastRate, StepAnimAdaptThreshold, StepAnimAdaptHysteresis, StepAnimAdaptMinDwell, StepAnimBackgroundRate, StepAnimWhatSteps, StepAnimFace, StepAnimGaze, StepAnimFlexi, StepAnimAttachFX, StepAnimParticles, StepAnimTextureAnim, StepAnimClock, StepAnimOffsetMode, StepAnimPhase, StepAnimBaseFPS, StepAnimBoil, StepAnimBoilAmount, StepAnimBoilTicks, StepAnimImpactHold, StepAnimImpactTicks, StepAnimImpactThreshold, StepAnimSmear, StepAnimSmearStrength, StepAnimSmearThreshold, StepAnimEase, StepAnimEaseAmount, StepAnimMatchRotoInk`.

Not tracked: `StepAnimEnabled` (master), `StepAnimExcludeCameraMounted` (safety), `StepAnimMaxAvatars` (performance), `StepAnimDebugVerify` (debug).

### 12.2 Values
Presets write the 29 style settings. Built-in and saved presets exclude `StepAnimParticles`, `StepAnimTextureAnim` and `StepAnimIncludeGhosts`, preserving the user's independent choices. **Any preset setting not listed in a row = its default (§14.1).**

| # | id | Label | Values that differ from the defaults |
|---|---|---|---|
| 1 | `anime_twos` | Anime on twos | Ease 1, EaseAmount 0.3 |
| 2 | `anime_threes` | Anime on threes (limited) | Rate 8, Ease 1, EaseAmount 0.3 |
| 3 | `kpdh` | KPop Demon Hunters | Mode 1, Rate 12, FastRate 24, ImpactHold 1, ImpactTicks 2, ImpactThreshold 30, Smear 1, SmearStrength 0.4, Ease 1, EaseAmount 0.4, MatchRotoInk 1 |
| 4 | `spiderverse` | Spider-Verse mixed | Scope 2, Mode 2, Rate 12, BackgroundRate 8, Smear 1, SmearStrength 0.6, SmearThreshold 2.0 |
| 5 | `stopmotion` | Stop-motion (Laika/Aardman) | OffsetMode 1, Face 1, Boil 1, BoilAmount 0.6, BoilTicks 2 |
| 6 | `claymation` | Claymation boil | OffsetMode 1, Face 1, Boil 1, BoilAmount 1.5, BoilTicks 1 |
| 7 | `harryhausen` | Harryhausen creature | Scope 0, Boil 1, BoilAmount 0.3, BoilTicks 2 |
| 8 | `rotoscope` | Rotoscope (A Scanner Darkly style) | Face 1, MatchRotoInk 1 |
| 9 | `undercrank` | Silent film undercrank | Rate 16, OffsetMode 1 |
| 10 | `puppet` | Puppet / marionette | Rate 10, WhatSteps 0, Face 0, Gaze 0, Boil 1, BoilAmount 0.3, BoilTicks 2 |
| 11 | `retro_sprite` | Retro sprite | Rate 6, OffsetMode 1 |
| 12 | `strobe` | Choppy dream / strobe | Rate 4, OffsetMode 1 |
| 13 | `hitstop` | Hit-stop action | Rate 24, ImpactHold 1, ImpactTicks 4, ImpactThreshold 20, Smear 1, SmearStrength 0.5 |
| 14 | `custom` | Custom (start from defaults) | none (all defaults) |

Booleans as 0/1. All values sit inside the §14.1 clamps (test T12 checks `sanitize(preset) == preset`).

## 13. Unit tests (`alsteppedanim_test.cpp`) and runtime checks

| Test | What it proves | Plan pass criterion |
|---|---|---|
| T1 exact twos | `t_n = t0 + n/24` (t0 ∈ {0, 0.37, 1e5}), rate 12, φ 0: capture on every 2nd frame for n < 24·3600; also threes at 8. | Exact rates |
| T2 render-rate independence | 60 and 144 fps with ±20% dt jitter over 10 s: 120 ± 1 captures; each gap ≤ one frame from 1/12 s. | Exact rates |
| T3 no drift | 1 hour of accumulated jittered dt: `captureTick == tickIndex(t_analytic)`; at every capture the held "pose" (= capture time scalar) equals that frame's smooth value bitwise. | No drift |
| T4 phase | Random φ is deterministic per seed and spread over [0,1); Locked = 0; Manual = setting; Roto term adds `frac(offset·rate)`. | |
| T5 adaptive | A speed ramp above T switches to FAST once; dithering inside `[T(1−H), T]` gives no extra transitions; speed held **exactly at T** with H = 0.05 and dwell = 0.05 (the minimums) gives zero transitions over 10⁴ frames; `sanitize` lifts H = 0 and dwell = 0 to 0.05; dwell blocks a switch before `max(MinDwell, 1/rate)`; FAST forces one capture; Fast rate < 2 captures every frame. | Adaptive |
| T6 boil | Same (seed, b, j) → identical quaternion bits; 10⁵ samples have angle ≤ A (+1e-5); different seeds differ; b changes only every `BoilTicks` base ticks; FAST frames do not change b. | Pose boil |
| T7 impact | A spike (|v| 4 m/s → 0 in 30 ms) triggers: next frame captures, then exactly `ImpactTicks` grid captures are suppressed; no re-trigger during hold or before one normal capture; gentle reversal (6 m/s²) never triggers. | |
| T8 smear | Below threshold: inactive, lag 1, stretch 1; above: `lagT = 1 − 0.5s`, total chain stretch `Π k_j ≤ 1.6` for n = 1, 2, 3 joints, monotonic in displacement; `smearAxisScale` is 1 on axes orthogonal to the motion and k along it; zero-length bone, chain < 1 cm, d < 1e-4 or tick_dt ≤ 0 → inactive (no NaN); s = 0 is identity bitwise. | phase 2 |
| T9 ease | amount 0 → `H == S` bitwise; weight ≤ amount; the same synthetic motion sampled at 60 and at 144 fps (jittered) picks the **same candidate sub-step and the same weight** (±1e-4); window resets on capture. | phase 2 |
| T10 scope table | Every row of the §4 table plus override precedence (Never beats scope, Own beats scope, roles), exclusions, disabled config → `stepped == false` for every fact combination. | Option off |
| T11 classes + restore | `classifyJoints` on the real skeleton name/parent list (fixture from `avatar_skeleton.xml`): mFaceRoot subtree = FACE, alt eyes = GAZE, collision volumes = PHYS. `restoreValue` restores untouched owned rotation/position/scale even after an epoch change and keeps foreign component writes; palette helpers prove the about-to-render id and contiguous previous-overlay rules; `composeRender` takes held classes from H, others from S. | |
| T12 presets/config | 14 presets, unique ids, each sets all 29 tracked values, `sanitize(p) == p`; `sanitize` maps NaN/out-of-range to the §14.1 clamps; `effectiveRate(1.9) == 0`. `mapRotoInk`: 12→(12,1), 8→(8,2), 6→(6,3), 10→(10, unchanged), 30→(24, unchanged). | Page presets |

Runtime (login screen and in-world, in the self-test and the wiring audit):
- **R1 option off = byte-identical** (review minor: expanded). The debug hash `H(av)` = FNV-1a over every joint local (rot, pos, scale) incl. collision volumes and `mRoot`, every `MatrixPaletteCache` `mGLMp`/`mLastGLMp`/`mLastFrame`, the system mesh `mLastMatrixPalette`, every attachment drawable world matrix, and the name-tag/voice-dot agent positions. It is exposed by a debug console command, per avatar, per frame, and sampled at three points: top of `idle()`, end of `idle()` and end of frame.
  - **R1a static.** Every call site of the runtime (`llappviewer.cpp` ×3, flexi, particles ×2, spin, lip-sync, palette flag) starts with `if (ALSteppedAnimRuntime::sActive)` or `sHasOverlay`. A grep gate in the prebuild list fails the build otherwise.
  - **R1b off, live animation.** `StepAnimEnabled` off for 600 frames in a scene with live animations, a Dance Ball, impostored avatars, shadows, reflection probes, a VCam bone-POV capture and one snapshot taken. Every runtime counter stays 0 (actors resolved, joints written, palettes invalidated, attachment syncs, name-tag shifts, flexi skips, particle/spin deferrals, lip-sync skips). Off-path code is never entered, so live output is stock by construction.
  - **R1c enable → disable, frozen.** World clock 0x (motions frozen, deterministic): hash all avatars at end of `idle()` for 30 frames with stepping off; enable with every option on; disable; after the next `endFrameRestore`, `sHasOverlay == false` and the 30-frame hashes equal the first set bitwise (incl. palettes and attachment matrices, so the auxiliary passes render the same input).
  - **R1d async writers during the window.** While stepping, wear and remove a rigged mesh with joint offsets (mesh-load callbacks run inside the window, `pipeline.cpp:4835`), change shape, and receive an appearance update. Then disable. Calling `rebuildAttachmentOverrides()` (`llvoavatar.cpp:7685`) and `updateVisualParams()` must not change the joint-position hash: no override or shape write was reverted.
  - **R1e impostors and auxiliary passes.** Impostored avatars never get a runtime entry (counter). With stepping on, the hash at end of `idle()` (inside the window) is the one the shadow, probe, VCam and snapshot passes render from; at the top of the next `idle()` it equals the smooth hash (R2).
- **R2 smooth invariance (live).** `StepAnimDebugVerify` (debug, not persisted): right after `endFrameRestore` and again at the top of the next `idle()`, compare every slot the overlay wrote against the saved smooth value. Foreign writes (current ≠ W at restore) are logged, not errors. Assert zero unexpected differences over 600 frames of live animation with every option on.
- **R3 held velocity zero.** With `BDMergeVelocityBuffer` and `BDMergeVelocityDebug` on, a held actor in Pose + position shows neutral grey (zero) on held frames; step frames show the snap. Visual check for TAA: no ghost trail on held poses.
- **R4 page.** Every row runtime-proven (value written, read back, consumer clamp honoured); every preset applies exactly §12.2 and shows "modified" after one tracked edit.
- **R5 benchmark (acceptance, review MAJOR 7).** Tracy zones `StepAnim::beginFrame`, `StepAnim::applyHeld`, `StepAnim::endFrameRestore`, `StepAnim::attachmentsApply`, `StepAnim::attachmentsRestore`, and the counter `paletteRebuilds`. Scene: 50 Bento avatars in view (typical outfits, ~30 attachments each, rigid and rigged), Everyone in view, `StepAnimMaxAvatars` 50, every option on, Release build on the user's machine. Pass: the sum of the zones has mean ≤ 2.5 ms and p95 ≤ 4.0 ms per frame; at the default cap of 24, mean ≤ 1.0 ms; with the master off, ≤ 0.01 ms; `paletteRebuilds` = 0 when no actor is a ghost source.

## 14. Settings

### 14.1 Global (`indra/newview/app_settings/settings.xml`, comments prefixed `[StepAnim]`)
Consumer clamp = what the runtime's `readConfig()` must apply before `ALSteppedAnim::sanitize` (identical clamps; NaN → default). Page row ranges equal these.

| Setting | Type | Default | Range / values | Persist | Consumer clamp |
|---|---|---|---|---|---|
| StepAnimEnabled | Boolean | 0 | on/off | 1 | bool |
| StepAnimScope | S32 | 1 | 0 Selected actor, 1 Whole cast, 2 Everyone in view | 1 | `std::clamp(v, 0, 2)` |
| StepAnimIncludeGhosts | Boolean | 0 | Step Ghost Studio clones with their own animation regardless of Scope | 1 | bool |
| StepAnimExcludeCameraMounted | Boolean | 1 | | 1 | bool |
| StepAnimMaxAvatars | S32 | 24 | 1 to 100 actors | 1 | `std::clamp(v, 1, 100)` |
| StepAnimMode | S32 | 0 | 0 Fixed, 1 Adaptive, 2 Mixed | 1 | `std::clamp(v, 0, 2)` |
| StepAnimRate | F32 | 12.0 | 2 to 30 fps | 1 | `llclamp(finite_or(v, 12), 2, 30)` |
| StepAnimFastRate | F32 | 24.0 | 0 to 30 fps; < 2 = smooth | 1 | `v = finite_or(v, 24); v < 2 ? 0 : llclamp(v, 2, 30)` |
| StepAnimAdaptThreshold | F32 | 1.5 | 0.1 to 10 m/s | 1 | `llclamp(finite_or(v, 1.5), 0.1, 10)` |
| StepAnimAdaptHysteresis | F32 | 0.3 | 0.05 to 0.9 | 1 | `llclamp(finite_or(v, 0.3), 0.05, 0.9)` |
| StepAnimAdaptMinDwell | F32 | 0.25 | 0.05 to 2 s (runtime floor: one step, §5) | 1 | `llclamp(finite_or(v, 0.25), 0.05, 2)` |
| StepAnimBackgroundRate | F32 | 8.0 | 0 to 30 fps; < 2 = smooth | 1 | `v = finite_or(v, 8); v < 2 ? 0 : llclamp(v, 2, 30)` |
| StepAnimWhatSteps | S32 | 1 | 0 Pose, 1 Pose + position | 1 | `std::clamp(v, 0, 1)` |
| StepAnimFace | Boolean | 1 | | 1 | bool |
| StepAnimGaze | Boolean | 1 | | 1 | bool |
| StepAnimFlexi | Boolean | 1 | | 1 | bool |
| StepAnimAttachFX | Boolean | 0 | | 1 | bool |
| StepAnimParticles | Boolean | 0 | | 1 | bool |
| StepAnimTextureAnim | Boolean | 0 | | 1 | bool |
| StepAnimClock | S32 | 0 | 0 Capture time, 1 Real time | 1 | `std::clamp(v, 0, 1)` |
| StepAnimOffsetMode | S32 | 0 | 0 Random, 1 Locked, 2 Manual | 1 | `std::clamp(v, 0, 2)` |
| StepAnimPhase | F32 | 0.0 | 0 to 1 step | 1 | `llclamp(finite_or(v, 0), 0, 1)`, then `1 → 0` |
| StepAnimBaseFPS | S32 | 24 | 24, 25, 30 | 1 | `v < 25 ? 24 : (v < 28 ? 25 : 30)` |
| StepAnimBoil | Boolean | 0 | | 1 | bool |
| StepAnimBoilAmount | F32 | 0.6 | 0 to 3 deg | 1 | `llclamp(finite_or(v, 0.6), 0, 3)` |
| StepAnimBoilTicks | S32 | 2 | 1 to 8 steps | 1 | `std::clamp(v, 1, 8)` |
| StepAnimImpactHold | Boolean | 0 | | 1 | bool |
| StepAnimImpactTicks | S32 | 3 | 1 to 12 steps | 1 | `std::clamp(v, 1, 12)` |
| StepAnimImpactThreshold | F32 | 25.0 | 5 to 200 m/s² | 1 | `llclamp(finite_or(v, 25), 5, 200)` |
| StepAnimSmear | Boolean | 0 | | 1 | bool |
| StepAnimSmearStrength | F32 | 0.5 | 0 to 1 | 1 | `llclamp(finite_or(v, 0.5), 0, 1)` |
| StepAnimSmearThreshold | F32 | 2.5 | 0.5 to 10 m/s | 1 | `llclamp(finite_or(v, 2.5), 0.5, 10)` |
| StepAnimEase | Boolean | 0 | | 1 | bool |
| StepAnimEaseAmount | F32 | 0.5 | 0 to 1 | 1 | `llclamp(finite_or(v, 0.5), 0, 1)` |
| StepAnimMatchRotoInk | Boolean | 0 | | 1 | bool |
| StepAnimDebugVerify | Boolean | 0 | debug only, not on the page | 0 | bool |

Page-row notes: `sa_base_fps` is a combo with `iv` 24/25/30; `sa_rate` steps 0.5 so 12.5 (twos at 25) and 15 (twos at 30) are reachable; the fast and background sliders step 1, so "0 or 1" reads as smooth.

### 14.2 Per-actor: `LLDirectorCast::ActorStyle` (`lldirectorcast.h:169`)
New nested struct, so it saves wherever Actor FX saves:

```cpp
struct SteppedOverride
{
    S32 mMode = 0;      // 0 inherit, 1 never stepped, 2 own settings
    F32 mRate = 12.f;   // used when mMode == 2
    S32 mWhat = -1;     // -1 inherit, 0 pose, 1 pose + position
    S32 mRole = 0;      // Mixed: 0 auto, 1 target, 2 background
};
SteppedOverride mStepped;   // in ActorStyle
```

| Field | Default | Clamp (in `sanitizeActorStyle`, `lldirectorcast.cpp:99`) |
|---|---|---|
| mMode | 0 | `std::clamp(v, 0, 2)` |
| mRate | 12 | `llclamp(isfinite(v) ? v : 12, 2, 30)` |
| mWhat | -1 | `std::clamp(v, -1, 1)` |
| mRole | 0 | `std::clamp(v, 0, 2)` |

Scene save: `writeActorStyle` (`lldirectorcast.cpp:148`) writes `data["stepped"] = {mode, rate, what, role}` **only when non-default**, so existing scenes and users who never touch it save byte-identical files; `readActorStyle` (`:183`) reads it when present. Members save through `sceneData()` (`:2130`, `e["actor_style"]` at `:2155`); You through `self_actor_style` (`:2228`); loads at `:2356` and `:2399` in `applySceneData` (`:2259`).

Scene-level (not a setting): `LLDirectorCast::mSteppedActorId` (LLUUID, null = You), saved by `sceneData()` as `"stepped_actor"` and read in `applySceneData`. Cleared when that member is removed from the cast.

Edits go through `setActorStyle`, which ignores non-cast ids (`lldirectorcast.h`), so overrides exist only for cast members and You. The Actor FX "Reset style" handler must copy the existing `mStepped` into the reset style.

## 15. Page wiring summary (for the implementer)
- `kit_pages.json` p0 entry `look_group/stepped_tab`: `v1_sources []`, `root_class "panel_stepped_anim"`, `root_name "stepped_tab_logic"`, `preset {"mode": "provider", "provider": "stepped_anim"}`, `who_adapter "stepped_anim"`, empty `selector_settings`, `card_enables`, `add_rows`, `label_names`.
- `show_if` conditions the panel adapter evaluates: `adaptive` (Mode == 1), `mixed` (Mode == 2), `manual_phase` (OffsetMode == 2), `actor_own` (picked actor's override mode == 2).
- Code rows and handlers (the JSON's `handler` key, informational like `src`): `sa_actor_target` onActorTargetCommit (writes `mSteppedActorId`), `sa_override_list` onOverrideListSelect, `sa_actor_override` / `sa_actor_rate` / `sa_actor_steps` / `sa_actor_role` (write the picked actor's `ActorStyle::mStepped` with the §14.2 clamps; read back on actor change), `sa_override_clear` onOverrideClear, `sa_rate_chips` onRateChip (writes `StepAnimRate = BaseFPS / n`; read-back highlights the chip whose `BaseFPS/n` equals the rate within 0.001, else none), readouts `sa_status`, `sa_rate_readout`, `sa_speed_meter`.
- Segmented rows bound to S32 settings (`sa_mode`, `sa_what`) are the first setting-bound `radio_group`s in a kit page. The wiring audit must prove the radio payload reaches the S32 control as an integer and reads back (`LLRadioGroup::getValue` returns the item payload, `llradiogroup.cpp:370`).
- **kit_lint gap (review minor, confirmed).** `KIND_TYPES` and `TAG_TYPES` in `scripts/code_tools/kit_lint.py:103-117` have no entry for `segmented` / `radio_group`, so a segmented row bound to a wrong-typed setting would pass the lint. The implementation must add `"segmented": ("S32", "U32", "String")` to `KIND_TYPES` and `"radio_group": ("S32", "U32", "String")` to `TAG_TYPES`, plus a lint fixture for each.
- `sa_include_ghosts` (toggle, `StepAnimIncludeGhosts`) sits directly under Scope: "Include ghost clones"; help: "Step Ghost Studio clones even when they are outside the scope above." Default off. With it on, clones with independent animation join every scope; per-actor Smooth, posing/edit, camera-mounted exclusion and the actor cap still win. Mirror/copy ghosts keep inheriting their source. Both engines, including the Animation engine, must honour this option. It is tracked for config/history, but built-in and saved presets must never write it.
- `sa_max_avatars` (number, `StepAnimMaxAvatars`, advanced) is in Card 1.

## 16. Cost and bounds

### 16.1 Actor cap
`StepAnimMaxAvatars` (default 24, 1 to 100) bounds the per-frame work. `applyActorCap` keeps, in order: the selected actor, actors with an Own-settings override, cast members (cast order), then other avatars by ascending camera distance, until the cap. Actors beyond the cap render smooth for that frame, and the status row says how many were skipped. Impostored avatars never count (they are excluded, §10.2), which is the LOD cutoff.

### 16.2 Per-actor cost
Per stepped, drawn actor per frame (Bento skeleton ≈ 160 joints incl. 26 collision volumes; A = rigid attachment prims):
- `applyHeld`: read 160 locals (~7 KB), 5 effector transforms, hash/slerp ops; on held frames one `updateWorldMatrixChildren` (~160 matrix composes). `attachmentsApply` compares each linkset root and only traverses changed linksets, updating child matrices without queuing them. About 20 to 35 µs at A ≈ 30.
- `endFrameRestore`: compare + write of the written slots, one `updateWorldMatrixChildren`; `attachmentsRestore` has the same root-only queue and unchanged-linkset skip. About 12 to 25 µs.
- Palette rebuilds: 0 extra unless the actor is a ghost source (§7.2).
- Boil/smear/ease: ≤ 160 quaternion ops on the frames they apply; ease copies at most one buffer per sub-step (4 per step).
- Flexi and particles: **save** CPU on held frames.
- Memory: 5 PoseBuffers per actor (~35 KB).

Expected after root-only attachment queuing: 24 actors ≈ 0.7 to 0.9 ms, 50 actors ≈ 1.6 to 2.2 ms per frame on the main thread. Tracy reports `StepAnim::attachmentsApply` and `StepAnim::attachmentsRestore` separately. Acceptance is the R5 benchmark (§13): 50 actors mean ≤ 2.5 ms, p95 ≤ 4.0 ms; 24 actors mean ≤ 1.0 ms; off ≤ 0.01 ms. If R5 fails, the first lever is to write only joints whose held value differs from smooth (animated joints only), then lower the default cap. Off: three static-bool tests per frame in `llappviewer.cpp`, and one per flexi / particle source / lip-sync / attachment-spin call site.

## 17. Risks, in order
1. **Overlay ownership and restore** (§3.4). The window is now closed before anything outside rendering runs, so a leak needs a writer *inside* the window: mesh-load callbacks (handled by the ownership rule and the override epoch) or a UI `draw()` that writes pose state (none known; the Poser is excluded). R1d and R2 are the guards.
2. **Attachment matrix sync** (§3.6): any attachment whose world matrix is not derived purely from its parent xform must be skipped. HUD attachments sit on a joint that never moves, so syncing them recomputes the same matrix; skipping them is only an optimization. An attachment being edited pauses its wearer's animation (`llvoavatar.cpp:5811-5819`), and the edit tools run outside the window, so they see smooth matrices.
3. **Palette pre-build** (§7.2): any new idle-time `updateSkinInfoMatrixPalette` caller is covered by the invalidation; caches that copy `mGLMp` before `applyHeld` (Ghost Studio cadence/frozen palettes) keep a smooth copy for their own purpose, which is acceptable.
4. **Region/origin shifts** in Pose + position: held agent-space roots go stale; §3.3 step 2 drops state.
5. **Cost in crowds** (§16): bounded by the cap; proven by R5.
6. **Adaptive, hit-stop and smear tuning** on real animations (plan step 6).
7. **Gaze/face visual mismatch** when one is smooth and the body stepped: by design, each is a toggle.

## 18. Rev 2: Codex review resolution

| Finding | Verified | Resolution |
|---|---|---|
| BLOCKER 1: input, UI commits, mainloop listeners and coroutines (`llappviewer.cpp:1500-1549`) ran before the old idle-top restore; Poser world reads (`fsposeranimator.cpp:1118-1139`) | Yes | Restore moved to `endFrameRestore()`, right after display and the snapshot pass (`:1630-1650`), before Sleep and the next frame's input (§3.1, D7). The hold exists only inside the render window. |
| BLOCKER 2: callbacks while held; `restoreMeshData()` → `updateJointLODs()` (`llvoavatar.cpp:2490-2500`) | Partly | `restoreMeshData` runs from the idle visibility update (`:6176`), outside the window: covered. The real in-window writer is mesh-load callbacks in display (`pipeline.cpp:4835` → `addAttachmentOverridesForObject`, `llvoavatar.cpp:7858`): covered by the ownership rule plus the new override epoch (§3.4). Test R1d. |
| MAJOR 3: `applyHeld` before `updateApparentAngles` / render accounting (`:5747-5753`) | Yes | `applyHeld` moved to the end of `idle()`, after LOD, accounting and audio; LOD and visibility use smooth roots (§3.1, order decision). |
| MAJOR 4: name tags use smooth `mLastRootPos` (`llvoavatar.cpp:2931-2933`, `:3658`); ghost nameplates update early (`alghoststudio.cpp:1871/2007`) | Yes | Labels follow the held root: render-window shift of `mNameText`, the voice dots and ghost nameplates by `heldRoot − smoothRoot`, restored with ownership (§3.7). |
| MAJOR 5: attachment matrices stay held across display → next input (`llviewerjointattachment.cpp:115-128`) | Yes | `syncAttachmentMatrices()` in both `applyHeld` and `endFrameRestore`; cameras and tools targeting attachments read smooth matrices outside the window (§3.6). |
| MAJOR 6: hysteresis/dwell may be 0, so a speed exactly at T alternates | Yes | Strict `>` / `<`; `StepAnimAdaptHysteresis` ≥ 0.05; `StepAnimAdaptMinDwell` ≥ 0.05 with a runtime floor of one step (§5, §14.1); page ranges updated; T5 extended. |
| MAJOR 7: 50-avatar cost; invalidation could rebuild every per-skin cache (`llvoavatar.cpp:11459-11510`) | Partly | Invalidation only marks entries already built this frame (idle-time ghost consumers), never all skins (§7.2). Added the `StepAnimMaxAvatars` cap (§16.1), Tracy zones and the R5 acceptance benchmark. "Rebuild only on tick changes" is **rejected**: held frames need the held palette every frame, because the window is reopened every frame. The bound comes from marking only pre-built entries, not from skipping frames. |
| Minor: smear compounding (2.56×), direction, zero-length bones | Yes | Total chain stretch ≤ 1.6 shared as `K^(1/n)`; direction-weighted axis scale (no shear in `LLJoint`); length guards (§6.3, T8). |
| Minor: ease cadence-independent, identical at 60 and 144 fps | Partly | The selection now runs on a fixed sub-step grid (cadence-independent choice and weight, T9). **Bitwise-identical held poses are rejected as unachievable:** poses exist only on rendered frames, and the base capture (§2.2) already lands on the first frame after a boundary, so 60 vs 144 fps differ by up to one frame in any design short of fixed-frame capture (§2.4). |
| Minor: expand the byte-identical OFF proof | Yes | R1a to R1e: static guard gate, live-animation counters, frozen enable → disable hash, async writers, impostors and auxiliary passes (§13). |
| Minor: kit_lint has no segmented/radio_group setting-type rule (`kit_lint.py:103-117`) | Yes | Implementation must add it (§15). |

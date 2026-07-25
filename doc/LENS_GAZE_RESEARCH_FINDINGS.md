# Lens gaze (head/eye camera tracking) — research findings

Deep research pass, 2026-07-25. Target feature: *"a mode where the flycam is a trackable target and
the head and eyes will realistically look into the center of the camera, overriding any head and eye
joint priority"* — for **entity clones, the user's own avatar, AND other residents**.

**Phase 0 of the plan below is IMPLEMENTED and BUILT (exe 06:44, commit `57026a985ac`), untested
in-world.** Phases 1-4 remain.

---

## Headline: ~80% of the feature already existed

`indra/newview/llactormover.cpp:2639-2916` (`LLActorMover::applyGaze()` / `gazePaint()`, declared
`llactormover.h:305-350`, config struct `llactormover.h:798-813`) is a **post-blend procedural gaze
system** — head/neck/torso/eye override ported from `LLHeadRotMotion`/`LLEyeMotion` math — with a
`GAZE_CAMERA` mode that already aims at `LLViewerCamera::getInstance()->getOrigin()`
(`llactormover.cpp:2738-2741`).

It is called from `LLVOAvatar::updateCharacter()` at **`llvoavatar.cpp:5156`**, i.e. for **every**
avatar class, and because it runs *after* `updateMotions()` it **sidesteps the joint priority system
entirely**.

The only thing blocking it: it painted only while the actor was under an active, non-suspended
`LLActorMover` **Move** (`llactormover.cpp:2653-2666`, guard at `~2732`). Phase 0 removed that gate
for the camera/cast/point modes.

## The priority problem, and why post-blend wins

How priority actually resolves: `LLJointState` defaults to `USE_MOTION_PRIORITY`
(`lljointstate.h:70`); `LLMotion::getJointPriority()` resolves it (`llmotion.h:107-115`); uploaded
`.anim` assets carry per-joint priority (`llkeyframemotion.cpp:1552-1566`, base at `:1308`);
`LLPoseBlender::addMotion()` feeds it to `LLJointStateBlender::addJointState()` (`llpose.cpp:496`),
which **insertion-sorts by priority, ties broken toward the more recently started motion**
(`llpose.cpp:204-233`).

**The crux:** `LLHeadRotMotion` and `LLEyeMotion` both run at `MEDIUM_PRIORITY` = 1
(`llheadrotmotion.h:82`, `:174`). Typical AO/uploaded animations are priority 2-4. **Any ordinary AO
already beats stock head tracking** — which is why SL head tracking so often appears to do nothing.

| Approach | Verdict |
|---|---|
| High-priority custom `LLMotion` | Needs `HIGHEST_PRIORITY` (4); a priority-4 upload still **ties**, and ties go to most-recently-started, so a re-signalled AO can steal the head mid-shot. **Rejected** |
| **Post-blend joint override** | Priority irrelevant — writes the final pose. Non-destructive: `blendAndApply()` rebuilds from `getRotation()` each frame, so easing weight to 0 returns cleanly. **CHOSEN** |
| Elevated-priority `LLHeadRotMotion` | This fork *does* have `LLMotion::setPriorityOverride()` (`llmotion.h:99-115`), but inherits the `"LookAtPoint"` dependency, the 25000px eye LOD gate, and the tie problem. **Rejected as primary** (useful escape hatch) |

**The answer does not differ across actor classes** — post-blend override is agnostic to what
produced the pose. That justifies one unified mechanism:

| | Clone | Self | Other resident |
|---|---|---|---|
| Competing for head/eyes | nothing (`mEnableDefaultMotions=false`) | `LLHeadRotMotion`+`LLEyeMotion`+AO | same, sim-driven |
| Post-blend override wins | yes | yes | yes |
| Extra work needed | none | none | none |

Notably, **no suppression of the default motions is needed for real avatars** — they keep running,
which is *good*: you inherit blink and saccades for free (blinking is a visual-param morph at
`llheadrotmotion.cpp:524-525` that a joint override never touches).

## BD "Cinematic Head Tracking" is the INVERSE of this feature

`UseCinematicCamera` (`llagentcamera.cpp:226`, consumed `:1599-1609`, `:1638-1650`) makes the
**camera follow the head** — third-person focus nudges with the head, camera up-vector inherits head
roll. Same for `CameraFollowJoint`/B6 Bone Camera (`:1590-1597`). It does not aim the head at the
camera, is not extensible into it, and is an active feedback hazard (see R1).

## Client-only safety — confirmed

`LLHUDEffectLookAt`'s wire format (`llhudeffectlookat.cpp:271-309`) carries source avatar UUID,
target object UUID, target position, and a type byte. **No head or eye rotation is ever
transmitted** — head/eye orientation is 100% viewer-side inference. `LLJoint::setRotation()` has no
network path. Other residents see nothing.

**State that must never be written** (the leak paths):
1. `gAgentCamera.mLookAt` / any `LLHUDEffectLookAt` — especially anything reaching
   `setNeedsSendToSim(true)` (`llhudeffectlookat.cpp:432, 482, 541, 717`).
2. `LLCharacter::setAnimationData("LookAtPoint", …)` — not transmitted, but it is *shared* input to
   `LLHeadRotMotion`, `LLEyeMotion` and `LLTargetingMotion` (`lltargetingmotion.cpp:110`), and it is
   a raw pointer owned by the HUD effect (dangling risk).
3. `startMotion`/`stopMotion` on a **real** avatar (for self this can reach `LLAgent`'s send path).
4. `mSignaledAnimations` / `processAnimationStateChanges()` on real avatars.

Post-blend override touches none of these.

## Camera targeting

The idle dispatch (`llappviewer.cpp:5456-5490`) is a strict if/else chain — agent-pilot → flycam
recorder → path camera → cinematic → flycam → agent camera — and **every** branch writes
`LLViewerCamera::getInstance()->setOrigin()`. So `getOrigin()` is **unconditionally authoritative**;
no need to query the cinematic camera or flycam separately.

- **Aim at the origin**, not a forward offset — `LLViewerCamera`'s origin *is* the notional entrance
  pupil. For an intentional off-lens eyeline, use a small **angular** offset in the head-local frame,
  never a positional one.
- **One-frame lag by construction:** avatar update (`llappviewer.cpp:5328`) runs *before* the camera
  dispatch (`:5456`), so gaze reads last frame's camera. ~16ms at 60fps — invisible, and a **free
  damper** against feedback. **Do not "fix" this by reordering.**

## Realism: what exists vs what to add

Already in `gazePaint` (ported from the built-ins): anatomical torso→neck→head distribution
(`:2855-2879`); eyes carrying the **residual** after the head turn, so they lead on small angles and
go near-zero once the head arrives (`:2881-2915`); yaw/pitch clamps with roll zeroed; clamp-and-hold
for targets behind the actor (`:2844-2853`).

Gaps, in priority order:
1. **Dead zone** — neither the built-in nor `gazePaint` has one; without ~3-5° of no-op a
   near-centred camera produces continuous micro-corrections that read as a servo.
2. **Torso lag differential** — `gazePaint` uses one weight for torso/neck/head, while
   `LLHeadRotMotion` uses different half-lives (torso 0.27s vs head 0.15s,
   `llheadrotmotion.cpp:50-51`). Restoring the differential is what makes the chest trail the head.
3. **Behind-the-actor policy** — currently holds at 72° and stares sideways forever; machinima wants
   a selectable break-off (ease weight to zero past ~120° and let the animation reclaim the head).
4. **Blink/jitter on clones** — real avatars get these free from `LLEyeMotion`; clones have none
   (`mEnableDefaultMotions=false`), so a tracking clone has **dead, unblinking eyes** — a strong
   uncanny-valley tell in close-ups.

## Composition with existing features

- **Body-yaw "keep facing"** (`ALGhostStudio::aimInstanceAt` → `setYaw` → `mRotation`) composes
  cleanly and must **not** be merged: `gazePaint` computes `head_rot_local` in the **root frame**, so
  if the body already faces the camera the head override is near-identity; if not, the head takes up
  the difference within its clamps.
- **Frozen/paused clones:** `DRIVE_FROZEN` → `requestPause()` → `updateMotions()` takes the paused
  branch and never calls `blendAndApply()` (`llmotioncontroller.cpp:911-933`), but `applyGaze` still
  runs and still tracks. **Recommended as intended behaviour** — a frozen tableau whose heads follow
  the camera is arguably the best use of the feature — with an explicit "freeze gaze too" option.
- **Per-clone anim speed** does not affect gaze, which advances on real `gFrameIntervalSeconds`
  (`llactormover.cpp:2675`). Correct — a head turning at half speed because the body anim is slowed
  would look wrong.
- **Do NOT start `ANIM_AGENT_HEAD_ROT` on clones** — it drives `mTorso` toward root-forward every
  frame (`llheadrotmotion.cpp:250-253`), exactly the fight `llghostavatar.cpp:240-241` warns about.
- **DO consider starting only `ANIM_AGENT_EYE`** on clones (the `setEntityPhysicsEnabled` pattern,
  `llghostavatar.cpp:224-248`) purely for blink morphs and saccades — safe because the gaze layer
  overwrites eye *joints* downstream, so `LLEyeMotion` contributes only its morphs.

---

## Phased plan

- **Phase 0 — decouple gaze from the Move gate.** ✅ **DONE** (commit `57026a985ac`) — the whole
  feature, minimally. Ship and validate before anything else.
- **Phase 1 — targeting polish.** Dead zone; torso/head lag differential; selectable
  behind-the-actor policy.
- **Phase 2 — clone life signals.** Per-clone `ANIM_AGENT_EYE` so tracking clones blink.
- **Phase 3 — breadth and UI.** Promote gaze out of the Path tab into the shared panel (superset
  rule); "all selected look at lens"; a `/ghostlens`-style chat command.
- **Phase 4 — safety interlocks.** Bone-lock feedback guard (R1); optional eyeline angular offset.

## Risks

- **R1 — Bone Lock feedback loop (highest).** `CinematicCamJoint` defaults to **`"mHead"`**
  (`llcinematiccamera.cpp:215, 245`). Bone Lock mounts the camera on the target's head; if that
  avatar also has camera-gaze, head turns → camera moves → head chases. The one-frame lag and
  smoothing may only slow divergence. **Mitigation: hard interlock suppressing gaze on the active
  Bone Lock target.** (Addressed in Phase 0's implementation — verify in-world.)
- **R2 — Mesh heads (medium-high, needs in-world proof).** Fitted-mesh heads *usually* weight to
  `mHead` with eyes on the Bento alts `mFaceEyeAlt*` (which `gazePaint` drives, `:2913-2914`), but
  some use custom eye rigs or parent eye meshes elsewhere. **Not determinable from viewer source** —
  it is third-party content behaviour. Test against LeLutka/Catwa/Genus-class heads. System avatars
  are safe (`avatar_lad.xml:167`).
- **R3 — Impostors on real avatars (medium).** `updateCharacter()` early-outs at
  `llvoavatar.cpp:5078-5082` when `!needs_update && !isSelf()`, so gaze is skipped for
  throttled/impostored avatars and nothing marks the impostor dirty on head-angle change — a distant
  tracked resident shows a **stale head angle**. Clones are immune (`mUpdatePeriod = 1`,
  `llvoavatar.cpp:4564-4570`). Mitigation: pin `mUpdatePeriod = 1` for gaze-enabled real avatars.
- **R4 — Per-frame cost (low).** Early-outs on empty map / single miss (`:2641-2649`); ~6 cached
  `getJoint()` hits + ~10 quaternion ops per painting actor. Sub-millisecond at 50 clones.
- **R5 — Non-standard skeletons (low).** Each joint is guarded individually, so animesh/partial
  skeletons degrade rather than crash.
- **R6 — Etiquette on other residents (policy).** Purely local and invisible to them, but turning
  bystanders' heads toward the lens deserves a visible "N avatars overridden" affordance.

## Not determinable from static reading
1. **Whether it looks convincing.** The gaze math has presumably only ever been seen *during a walk*;
   it has never been evaluated as a held close-up, which is the demanding case.
2. **Frame ordering** of `ALGhostStudio::updatePerFrame()` (body yaw) vs `LLVOAvatar::updateCharacter()`
   (gaze) — the math composes regardless, but an inversion could cause chatter with a fast camera.
3. **Mesh-head eye-joint conformance** (R2).
4. **Whether Bone Lock + gaze actually diverges or merely wobbles.**
5. **Whether `applyGaze` has ever been validated in-world at all** — treat the gaze layer as
   code-complete but unproven. (The shipped "look-at / keep facing" feature is the separate
   *body-yaw* path, `doc/GHOST_LOOK_AT_BRIEF.md`.)

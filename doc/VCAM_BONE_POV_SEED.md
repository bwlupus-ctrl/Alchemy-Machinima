# Virtual Cam — bone-attached avatar/clone POV — design seed

**Status:** seed for a design pass. No source modified. Captured 2026-08-16.
Feeds `doc/VCAM_BONE_POV_DESIGN.md` -> Codex -> review -> build.

Attach the prim-free Virtual Cam to an avatar or clone and drive it from that subject's SKELETON so a
shot can ride a first-person POV off their bones (head/eye), with a full config surface. Motivated by
machinima: filming *through* a cast member's eyes, or a stabilized body-mounted POV, without rezzing a
camera prim.

## User decisions (2026-08-16, fixed)
1. **Aim follow — BOTH, toggled, default full-follow.** Full head-follow (camera orientation rides the
   anchor joint's rotation — true first-person, turns/bobs with animation) AND a stabilized mode
   (camera rides the joint POSITION but the operator keeps manual aim — smooth body-cam). Default:
   full-follow.
2. **POV point — CONFIGURABLE JOINT.** A dropdown selects the anchor joint (head / eye / neck / a
   named custom joint), plus a positional offset (below). Not hard-wired to one bone.
3. **Roll — BOTH, toggled, default horizon-lock.** Horizon-lock (keep the shot level as the head tilts
   — film standard) AND inherit-roll (full head tilt — raw FP). Default: horizon-lock.
4. **Feature-complete config** ("with options to configure of course"): anchor subject, joint,
   aim-mode, roll-mode, positional offset (XYZ, bone/local space), aim trim (pitch/yaw rotational
   offset), FOV, smoothing/damping, clone-scale awareness. One delivery.

## Verified current model (read from code — cite file:line in the design)
- **The Virtual Cam is prim-free** in `LLPrismLens`: a capture's camera is either OBJECT-attached
  (`mCameraObjectId` + `mLocalEyeOffset`) or VIRTUAL (`mVirtual=true`, `mVirtualPos` eye position in
  AGENT space, `mVirtualRot` orientation with **fwd = local −Z, up = local +Y**) —
  `llprismlens.h:146-163`. `mLocalEyeOffset` is NOT applied to a virtual camera (`:159-160`).
- **Registration:** `addVirtualCamera(capture, pos, ...)` sets `mCamera.mVirtual=true` and stores the
  transform (`llprismlens.h:451`, `:450`). "Snap to my view" stores `getOrigin()` verbatim (`:156`).
- **Consumers:** `llfloaterprismmanager.*` (UI), `llviewerdisplay.cpp` / `pipeline.*` (the capture
  render derives eye+orientation from the stored transform). The design must cite the exact per-frame
  point where `mVirtualPos/mVirtualRot` are read so the bone driver writes them BEFORE that read.
- **Cast roster:** `LLDirectorCast` resolves {You, A, B, C, D} -> `LLVOAvatar*`; clones are
  `LLGhostAvatar` (client-only, `mEntityScale` for clone scale). Same roster the light rig and lens
  gaze consume.
- **Joint machinery (reuse, do not fork):** `LLVOAvatar::getJoint(name)` / `mHeadp` etc. ->
  `LLJoint::getWorldPosition()` / `getWorldRotation()`; the finalized foot-pivot clone-scale map
  `scaledPoint(avatar, point, scale)` and `getUniformScale()` (virtual; `LLGhostAvatar` override ->
  `mEntityScale`) from the light rig; the eye-joint / eyeline handling from lens gaze (`algazemath`,
  ANIM_AGENT_EYE). Clone-scale offsets must use the SAME foot-pivot approach (user's standing
  "account for clone scaling" mandate).

## What the design pass must decide

### A. The driver & where it hooks
- A per-frame driver that, when the capture's camera is in BONE-POV mode, computes eye position +
  orientation from the anchor subject's selected joint (+ offsets, +smoothing) and writes
  `mVirtualPos`/`mVirtualRot` (AGENT space, fwd=−Z/up=+Y) BEFORE the capture render reads them. Decide
  the exact hook (a tick alongside the light-rig/lens-gaze ticks in `llviewerdisplay`/`llappviewer`,
  ordered before the prism capture) and the owning object (extend `LLPrismLens` capture state vs a new
  small driver like the light-rig controller). Prefer reusing the capture's existing virtual fields so
  the render path is UNCHANGED.
- Lost/unresolved anchor (avatar unloaded/left, joint missing): decide graceful behavior — hold last
  transform, or fall back to the stored static virtual transform, or disable the attach. Must not
  snap/NaN the camera.

### B. Joint resolution (configurable) + clone scale
- Map the joint dropdown to actual joints across avatars AND clones (`LLGhostAvatar`): head, eye
  (eyeline), neck, custom-by-name. Eye should give a true eyeline (reuse lens-gaze's eye handling).
- The positional offset is in the joint's LOCAL frame (so "forward to the lens" tracks head yaw).
  Clone-scale: the offset distance must scale with the subject via the finalized foot-pivot
  `scaledPoint`/`getUniformScale()` so a 0.5x clone's 20 cm offset is 10 cm in world — same invariant
  the light rig uses. Specify the exact composition.

### C. Aim-mode math (full-follow vs stabilized)
- **Full-follow:** orientation = joint world rotation remapped into the camera's fwd=−Z/up=+Y
  convention, plus the aim-trim (pitch/yaw) and roll-mode (below). Specify the basis remap from the
  SL joint frame to the camera frame (this is the subtle bit — get the axis mapping right, cite the
  light-rig facing/`atan2` precedent).
- **Stabilized:** position rides the joint; orientation stays operator-controlled (the stored
  `mVirtualRot` is NOT overwritten each frame — only `mVirtualPos` is driven). Decide how the operator
  still aims (existing prism controls) and that the toggle cleanly hands orientation back and forth.

### D. Roll handling (horizon-lock vs inherit)
- **Horizon-lock:** after computing the follow orientation, re-level so up ≈ world up (remove roll
  about the view forward) — specify the exact re-orthonormalization (project up onto the plane ⟂ fwd,
  guard the fwd≈world-up degeneracy). **Inherit:** keep the joint's roll. Default horizon-lock.

### E. Smoothing / FOV
- Smoothing/damping on position AND orientation to tame animation jitter (reuse the light rig's
  exponential centre smoother pattern; orientation via slerp). One damping control (or split pos/rot).
  Snap on first frame / mode change (no lerp from a stale transform).
- FOV: drive the capture's lens FOV from a config control; decide the range and how it composes with
  the existing prism lens FOV (if any).

### F. UI + persistence
- Where the controls live (the Prism Manager floater `floater_prism_manager.xml` and/or the Director
  console — decide; likely Prism Manager since it owns the VCam, mirrored to Director if that matches
  the light-rig lockstep pattern). Controls: anchor subject combo, joint combo, aim-mode toggle,
  roll-mode toggle, offset XYZ, aim trim, FOV, smoothing, enable/attach toggle, plus mini reset
  buttons (match the light-rig idiom). Persistence: settings keys and/or the Director scene round-trip
  (decide; a bone-attach is session/scene state — prefer additive scene keys, minimal new settings).

## Constraints
- Label PROVES/IMPLIES/INFERS; cite file:line. No source modified by the design.
- Ship-whole: driver + joint resolution + both aim modes + both roll modes + offset/trim/FOV/smoothing
  + clone-scale + UI + persistence in one delivery. Defer nothing silently.
- REUSE the finalized machinery: `scaledPoint`/`getUniformScale()` clone-scale (do NOT re-derive), the
  lens-gaze eye handling, `LLDirectorCast` roster (read-only). Do NOT fork joint math.
- OFF-LIMITS (unless the design justifies): the prism capture RENDER path and shaders (drive the
  existing `mVirtualPos/mVirtualRot`, don't change how they're consumed); `llprimitive`/pipeline
  shadow/light code; `lldirectorcast.*` internals (consumed read-only); the light-rig and lens-gaze
  subsystems (read their helpers, don't modify).
- Forward-facing robustness: 0.05x clone + 1.0 avatar; anchor leaving mid-shot; a missing custom
  joint; toggling aim/roll mode live; switching anchor live; the operator also driving the prism.
- Concrete enough to become a Codex brief: named fields/functions/settings/UI + an OFF-LIMITS list.

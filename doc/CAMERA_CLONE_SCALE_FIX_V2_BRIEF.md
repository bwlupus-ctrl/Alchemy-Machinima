# Camera-clone-scale fix v2 — the v1 fix has ZERO in-world effect (diagnose, don't re-guess)

## Status: v1 FAILED in-world
The v1 fix (doc/CAMERA_CLONE_SCALE_FIX_BRIEF.md) compiled and self-reviewed to 0 must-fix,
but **in-world it does nothing**. Verbatim user report:

> "The bone offset camera fix is a failure. Changing the size of the clone in realtime has
> NO impact on the position of the bone cam. The other camera presets also fail to note the
> scaling bone offsets, in reference to the ghost entities ONLY."

So: **every** cinematic preset (bone-lock and the rest) ignores an entity clone's live scale.
Bone Lock is `MODE_BONE_LOCK = 1`, the DEFAULT mode.

## What v1 changed (all present in the tree now)
- `llvoavatar.h`: `virtual F32 getUniformScale() const { return 1.f; }`
- `llghostavatar.h`: `F32 getUniformScale() const override { return mEntityScale; }`
- `llcinematiccamera.cpp`: helpers `cc_subjectBase()` / `cc_scaleAboutSubjectBase()`; scaled
  `resolveAnchor()`, `patternOTS()`, `patternTwoShot()`; and a post-dispatch block in
  `updateCamera()` (~line 1066) that, when `av->getUniformScale() != 1.f`, scales `focus`/`pos`
  about the subject base for the `default`/bone-lock cases.

## Claude's verified static trace (this is why v1 SHOULD have worked — but doesn't)
1. Scale is live on the avatar: `ALGhostStudio::setInstanceScale()` (alghoststudio.cpp:375)
   -> for `BACKING_ENTITY_CLONE` it does `gObjectList.findObject(inst->mEntityId)` ->
   `ghost->setEntityScale(scale)` -> `mEntityScale = clamp(scale)` (llghostavatar.cpp:216).
   The clone visibly resizes, so `mEntityScale` is the live value.
2. `updateCamera()` uses `LLVOAvatar* av = resolveTarget()` (llcinematiccamera.cpp:973).
   `resolveTarget()` -> `LLDirectorCast::resolveSubjectA()` -> `resolve(mSubjectA)`, which was
   previously fixed to return the CLONE for Director subjects (returns the ghost via
   `findObject` or `ALGhostStudio::resolveEntityClone()`; that returns the SAME
   `gObjectList.findObject(mEntityId)` object). So `av` should be the scaled `LLGhostAvatar`.
3. `MODE_BONE_LOCK` -> `patternBoneLock(av,...)` sets `pos`; then the scale block runs; then
   `pos` -> smoothing (bypassed for bone-lock) -> `cam->setOrigin(out_pos)` with no
   re-derivation from the unscaled joint. So a fired scale block WOULD move the camera.

**Conclusion:** for the symptom to be "no movement at all," `av->getUniformScale()` must be
returning **1.0** at llcinematiccamera.cpp:1070 (the `if (subject_scale != 1.f)` guard), even
though `mEntityScale` is live on the clone. Find out WHY. Do not just re-scale more offsets.

## STEP 1 — INSTRUMENT (required; you cannot see the render, so get real data)
Add temporary, clearly-tagged logging so the user can run one take and report the values.
Right after `av = resolveTarget()` in `updateCamera()`, and again at the scale block, log:
- `mode`
- `av->getID()`
- `av->isGhostAvatar()`
- the dynamic type (e.g. `typeid(*av).name()` or an `LLGhostAvatar* dynamic_cast` bool)
- `av->getUniformScale()`
- `cc_subjectBase(av)`, and `pos`/`focus` before and after `cc_scaleAboutSubjectBase`

Use `LL_INFOS("GhostCineScale")` throttled (e.g. once per ~30 frames or on scale-change) so it
does not spam. This single log line tells us definitively whether `av` is the ghost and what
scale the camera sees. Keep it easy for Claude to remove later (one tag).

## STEP 2 — AUDIT the resolution + dispatch, then FIX the real disconnect
Leading hypotheses to confirm/refute with the audit (and the log):
- **H1** `resolveTarget()/resolveSubjectA()/resolve()` returns a non-ghost (e.g. the SOURCE
  avatar, or a control avatar for an animesh clone) whose `getUniformScale()` is the base 1.f.
  Check what `mSubjectA` actually holds for an entity clone and which object `resolve()`
  returns for it. Confirm `isGhostAvatar()` is true for the entity clone (note the header
  comment: LLGhostAvatar does NOT override isGhostAvatar()).
- **H2** The scaled `LLGhostAvatar` is not the object `resolveTarget()` returns (two-object /
  identity mismatch between the mEntityId object and the resolved subject).
- **H3** `getUniformScale()` returns `mEntityScale` but in this call path `mEntityScale` is not
  the live value (stale copy / wrong instance).

Then fix so that, for an entity clone set as Director Subject A, the bone cam and all presets
track the clone's live scale about its foot/root pivot. Keep the v1 changes that are correct.

## Constraints & expectations
- Do NOT regress the scale-1 path (real avatars/self must be byte-identical).
- This is machinima RENDER behavior you cannot verify. **Instrument first; the user's in-world
  run + the log is the truth, not your review confidence.** A second blind "PASS" that fails
  in-world is the specific failure we are trying to avoid.
- Self-review to 0 must-fix on code correctness, but explicitly report: the confirmed cause
  (from the audit), the exact line where `subject_scale` was collapsing to 1.0 (or why `av` was
  wrong), the diagnostic tag added, and what the user should look for in the log.

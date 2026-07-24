# Director Console cinematic camera ignores entity-clone uniform scale

## Symptom (user-reported)
In the Director Console, the cinematic camera correctly **targets** an entity
clone set as Subject A, but the **framing does not scale with the clone's size**.
The camera behaves as if the clone's uniform scale were always 1.0: a clone
scaled up is framed too close / camera ends up inside it, and the focus point
sits at the wrong elevation (head focus lands at scale-1 height, not the
visually-scaled head). User's own read: "it always seems to assume the entity
scale is 1, so I think it has to do with the offsets."

## Scale mechanism (context — this is already how clones scale visually)
- Entity-clone uniform scale lives on `LLGhostAvatar::mEntityScale` (default
  `1.f`), set via `LLGhostAvatar::setEntityScale()`.
  - `indra/newview/llghostavatar.h:109` (setter), `:157` (`mEntityScale`).
  - There is currently **no public getter**.
- The clone is scaled **visually** by an OUTER render transform:
  `outer->mScale = mEntityScale` in `indra/newview/llghostavatar.cpp` (~line
  177–183). Full rationale in `doc/AVATAR_UNIFORM_SCALE_DEEP_RESEARCH.md`.
- The camera resolves its subject via
  `LLCinematicCamera::resolveTarget()` -> `LLDirectorCast::resolveSubjectA()`,
  which returns the (possibly ghost) `LLVOAvatar`.

## Root-cause hypothesis (verify against the code; do not assume)
The camera samples the subject from **scale-1 skeleton geometry** and uses
**absolute meter** distances/offsets tuned for a scale-1 avatar, so nothing
reflects `mEntityScale`:
- `indra/newview/llcinematiccamera.cpp`:
  - `resolveAnchor()` (~175): `pos = joint->getWorldPosition()`, fallback
    `av->getPositionAgent() + LLVector3(0,0,1)`.
  - `patternBoneLock()` (~209): same joint sampling + mount offset
    `LLVector3(off_fwd, off_left, off_up)` (the `CinematicCamBoneOffset*`).
  - Every pattern generator uses fixed-meter framing:
    `CinematicCamHoverDistance` (2.5), `CinematicCamSweepDistance` (3),
    `CinematicCamHeroDistance`, `CinematicCamOrbitRadius/Height`,
    `CinematicCamWhipDistance`, `CinematicCamArcDistance`,
    `CinematicCamLongDistance`, `CinematicCamECUDistance`,
    `CinematicCamPedestalDistance`, `CinematicCamLeadDistance`, etc., plus the
    `+LLVector3(0,0,1)` head-height guess and per-pattern `height` terms.

Because the visual scale is an outer render transform, these logical positions
and fixed offsets never see `mEntityScale`.

## CRITICAL thing to determine first
Does `LLJoint::getWorldPosition()` on a **clone's** joints already include the
outer `mEntityScale`, or is the scale applied purely downstream at render time?
This decides whether the **focus point itself** must be scaled/repositioned, or
only the **offsets and framing distances**. Inspect exactly how `outer`/`mScale`
propagates (or does not) into joint world transforms vs. render.

## Fix goal
A clone of uniform scale `N` should be framed the way a scale-1 avatar `N`x
larger would be: focus tracks the visually-scaled head, and all subject-relative
distances/heights/offsets scale by `N`.

Suggested shape (adjust to what the code actually needs):
1. Add a generic accessor so the camera can query any subject's uniform scale:
   `virtual F32 LLVOAvatar::getUniformScale() const { return 1.f; }` and override
   in `LLGhostAvatar` to return `mEntityScale`. (Name/placement your call — the
   point is the camera must not special-case ghost casting inline if avoidable.)
2. In the cinematic camera framing, read `subjectScale` from the resolved target
   and multiply the subject-relative geometry by it: the anchor/focus elevation
   offset, the bone-lock mount offset, and the per-pattern framing
   distances/heights. Scale **about the clone's base/pivot** (foot), matching how
   the outer transform pivots — not about the region origin.
3. If `getWorldPosition()` does NOT already include the outer scale, also lift the
   focus point from the base by the scaled head height so it tracks the rendered
   head.

## Constraints
- **Do not regress the scale-1 path.** With `subjectScale == 1.f`
  (normal avatars, self, Subject B pairing), behavior must be byte-identical to
  today.
- Match existing code style in these files.
- Two-shot (`patternTwoShot`, Subject B) and any midpoint framing should also
  behave sanely when one or both bodies are scaled clones.
- After implementing, run your own adversarial review and **converge to 0
  must-fix** before returning. Report: the root cause you confirmed (esp. the
  `getWorldPosition()`-includes-scale question), the files/functions changed, and
  any offset you deliberately left unscaled with the reasoning.

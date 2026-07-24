# Local avatar scale — Phase 1 (real avatars) + high-scale glitch fixes + probe cleanup

## Context (all confirmed working in-world as of commit 82317ba7028)
Entity-clone uniform scaling works via a **client-only, foot-pivoted OUTER RENDER
TRANSFORM** on `LLGhostAvatar` (`mEntityScale`, `llclientoutertransform.h`, matrix
built in `llghostavatar.cpp` ~168-216 pivoting about `foot = mRoot->getWorldPosition();
foot.z -= getPelvisToFoot()`). The cinematic camera now frames scaled clones correctly
(`getUniformScale()` + foot-pivoted `cc_scaleAboutSubjectBase`). User wants to (1) apply
this same local scaling to REAL avatars via a slider, and (2) fix two glitches that appear
at high scale. Phase 2 (a growth/shrink ANIMATION system) comes later — design for it now.

This is one coherent task because Parts B/C/D all sit on the same generalization (Part B).

## Part A — strip the diagnostic probe (housekeeping)
Remove the temporary `LL_INFOS("GhostCineScale")` instrumentation from
`llcinematiccamera.cpp` (the `resolve` log near line ~983 and the `scale-block` log near
~1111). The camera fix is confirmed; keep the actual foot-pivot fix. Leave nothing behind.

## Part B — generalize client scale from LLGhostAvatar to LLVOAvatar (Phase 1 foundation)
Move the client-only uniform-scale state + outer-transform application UP to the
`LLVOAvatar` base so ANY avatar can carry it:
- The scale field + `getUniformScale()` (already virtual on LLVOAvatar returning 1.f) become
  backed by a real base-class field; `LLGhostAvatar::mEntityScale` unifies with it (the ghost
  keeps behaving exactly as today).
- Move the outer-transform build/update (the foot-pivot matrix) to LLVOAvatar so a real avatar
  gets the same foot-planted scaling. Keep `LLGhostAvatar`'s clone-specific attachment stamping
  (`stampEntityOuterTransform`) but have it apply to the real attachment prims of a scaled real
  avatar too (a scaled real avatar must scale its worn attachments).
- **Default scale 1.0 = complete no-op**: a normal, unscaled avatar must be byte-identical to
  today (no outer transform enabled, no extent/LOD change). Guard everything on `scale != 1.f`.
- **Client-only invariant**: never send anything to the sim; this only alters local render.
  A scaled real avatar's true server size/position/hitbox/sit-target are unchanged (cosmetic,
  local, like the clones).
- Expose a single `setLocalScale(F32)` (name your call) as the ONE entry point — Phase 2's
  animator will drive it per-frame, so keep it cheap and idempotent.

## Part C — real-avatar scale UI (Phase 1)
- **Avatar context menu**: a "Local Scale" control (slider or submenu) to set any nearby
  avatar's local scale, including your own. Session-only; resets to 1.0.
- **Director Console**: mirror it (a scale control/row) per the superset rule
  ([[director-console-superset-rule]]) — anything the menu can do, the console can too.
- Reuse the foot-pivoted apply so real avatars scale feet-planted, same as clones.

## Part D — fix the two high-scale glitches (now that scale is a first-class avatar property)
Both are the deferred scale-polish gaps; both should key off the avatar's local scale so they
fix clones AND scaled real avatars:
1. **LOD coarsening** — a scaled-up avatar/attachments render low-poly up close because LOD is
   chosen from UN-scaled size. Factor the local scale into the LOD distance/radius so apparent
   size drives detail (bigger => higher LOD, as if closer). Touch `LLVOVolume::calcLOD()` (there
   is already a clone override that follows the source LOD — generalize it to also account for
   the outer scale). Verify normal (scale 1) LOD is unchanged.
2. **Geometry vanishing at high scale** — objects/attachments disappear because spatial extents
   / bounding boxes are computed at scale 1, so scaled geometry falls outside them and gets
   frustum/occlusion-culled. Make the cull extents **scale-aware**: grow the avatar's and its
   attachments' spatial extents by the local scale about the foot pivot (the same pivot the
   render uses), and trigger an extent update when the scale changes. Touch the extent path
   (`calculateSpatialExtents`/`setSpatialExtents`/`setNeedsExtentUpdate`) and
   `llspatialpartition` as needed. Verify normal avatars are untouched.

## Constraints & expectations
- Default scale 1.0 path must be byte-identical for normal avatars AND existing clones.
- Client-only; nothing to the sim.
- Known-acceptable gaps may remain (name-tag floats at true height; click/selection uses real
  geometry) — but call out any you leave.
- Parts A/B/C are mechanical/plumbing; **Parts D (LOD + culling) are RENDER-correctness you
  CANNOT see** — implement carefully, the user verifies in-world, and expect an iteration pass.
  Do not report render correctness as verified; only that it compiles and self-reviews clean.
- Self-review to 0 must-fix. Report: files changed per part, the single scale entry point's
  name/signature (for Phase 2), any gaps left, and confirm the scale-1 no-op path.

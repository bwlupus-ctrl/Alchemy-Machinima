# Scaled-avatar culling / extents — geometry vanishes at high scale (clones AND real avatars)

## Symptom
At high local scale, parts of a scaled avatar VANISH. Latest in-world report on a real
self-avatar at scale ~5: "more parts grew, **head disappeared**." Earlier, on entity clones:
"some objects occasionally disappear at high scale." Same underlying defect, both actor kinds.
Geometry that renders is correctly positioned/scaled — the problem is that some drawables are
being CULLED (or binned wrong), not mis-transformed.

## Architecture recap
Local uniform scale is a client-only **foot-pivoted OUTER RENDER TRANSFORM**
(`LLClientOuterTransform`, `mCurrent`/`mInverse`, `mFootPivot`, `mScale`, `mEnabled`), stored on
`LLVOAvatar` (`mLocalScale`, `setLocalScale()`), stamped onto attachment trees, and consumed at
draw time via `LLDrawInfo::mOuterTransform` (`lldrawpool.cpp` `applyModelMatrix`) and
`LLScopedAvatarOuterModelView` in `LLVOAvatar::renderSkinned()`.

Already correct (do not redo):
- `LLVOAvatar::updateLocalScaleTransform()` runs **per frame** from `idleUpdate()` while scaled
  (llvoavatar.cpp ~2858), so `mFootPivot` stays fresh as the avatar moves.
- `LLDrawable::updateSpatialExtents()` (lldrawable.cpp ~1057-1080) DOES scale the drawable's
  extents about `outer->mFootPivot` when the transform is enabled.

## Confirmed gaps to fix (Claude's audit — verify each, then fix)

### G1. Bin radius is NOT scale-aware  (prime suspect for the vanishing head)
Immediately after scaling the extents, `LLDrawable::updateSpatialExtents()` calls
`updateBinRadius()`, which does `setBinRadius(llmin(mVObjp->getBinRadius(), 256.f))` — an
**unscaled** value. Bin radius drives octree bin placement/size, so a 5x avatar can be filed in a
bin sized for a 1x avatar and get culled or mis-tested. There is **no `getBinRadius()` override on
`LLVOAvatar`**, and `LLVOVolume::getBinRadius()` (llvovolume.cpp ~4607) is not scale-aware either.
Fix: make the effective bin radius account for the owning avatar's local scale, for BOTH the
avatar drawable and its (rigged and static) attachment volumes. Note llvovolume.cpp ~1522 has a
commented-out `//radius = avatar->getBinRadius();` — rigged volumes deriving radius from the
avatar is relevant context.

### G2. Cached `mLastAnimExtents` are UNSCALED, and other consumers read them directly
`LLVOAvatar::updateSpatialExtents()` (llvoavatar.cpp ~1521) caches **unscaled** extents in
`mLastAnimExtents` (and shifts them by pelvis delta when `mNeedsExtentUpdate` is false). That is
fine for the drawable path, because `LLDrawable::updateSpatialExtents()` applies the outer scale
afterwards — BUT any other consumer that reads `mLastAnimExtents` / `getLastAnimExtents()`
directly gets **true-size** extents for a scaled avatar. Audit every consumer (impostor sizing and
update decisions, occlusion queries, visibility/`mVisibilityPreference`, camera/bounding tests,
`LLVOAvatar::getPixelArea()` paths) and make them scale-aware — or expose a scaled accessor and
use it. This is a strong candidate for a rigged mesh HEAD disappearing while the body renders.

### G3. Occlusion / frustum culling boxes and pixel area
Make sure the occlusion-query bounding box and any pixel-area / apparent-size computation used for
culling, impostoring, and LOD reflect the scaled size. A scaled-up avatar must not be occlusion-
culled using a true-size box, and must not be impostored/LOD-dropped as if it were small.
(Related: the Director research doc notes motion updates are gated by pixel-area thresholds —
`doc/DIRECTOR_ANIMATION_CONTROL_DEEP_RESEARCH.md` §23.7.)

### G4. Independently-culled attachment bridges
Attachments with their own spatial bridge are culled separately from the avatar. Confirm their
bridge extents/bin radius also account for the wearer's scale, and are invalidated when scale or
pivot changes.

## Instrumentation (INCLUDE IT — this is render-invisible to you)
A previous blind fix round failed; a probe cracked the last one in a single iteration. Add a
throttled, clearly-tagged `LL_INFOS("ScaleCull")` probe (once per ~30 frames, only while an avatar
is scaled) reporting per scaled avatar and per attachment drawable: object id / name, is-rigged,
local scale, foot pivot, the extents BEFORE and AFTER outer scaling, the bin radius used, whether
the drawable is currently visible/culled, and (if reachable) the occlusion state. Enough that the
user can run one take at scale 5 and the log shows which drawable is being culled and on what
number. Keep it easy to strip later (single tag).

## Constraints
- **Scale 1.0 must be byte-identical** for normal avatars, normal attachments, and existing clones.
- Client-only; nothing to the simulator.
- **REGRESSION CHECK:** the previous change made `resolve_outer_transform()` reject
  disabled/unit-scale transforms. Entity-clone scaling was CONFIRMED WORKING before that change —
  re-verify by inspection that clones still bind their outer transform correctly, and call out any
  risk you find.
- This is RENDER correctness you CANNOT verify. Implement carefully, self-review to 0 must-fix, and
  report: each gap confirmed/refuted, the fix per gap, the probe tag, and what the user should look
  for in the log. Do NOT report render correctness as verified.

# Scaled avatar vanishes on MOVE / ANIMATION — stale-extent throttle amplified by scale

## In-world report (precise, and it pins the cause)
> "I tried local scale, it did grow the avatar in full, but as soon as I moved the head and body
> disappeared, but if I shrink back to default they reappear. If I regrow, it's fine till an
> animation plays."

So: **static scaling now works** (the rebuild/binding fix landed correctly). The failure is
triggered by **pelvis movement and by pose change**, and is fully reversible at scale 1.0.

## Confirmed root cause
`LLVOAvatar::updateSpatialExtents()` (llvoavatar.cpp ~1585) only recomputes TRUE extents when
`mNeedsExtentUpdate` is set:

```cpp
if (mNeedsExtentUpdate) { calculateSpatialExtents(newMin,newMax); ... }
else {
    LLVector3 shift = mPelvisp->getWorldPosition() - mLastAnimBasePos;
    mLastAnimExtents[0] += shift;   // TRANSLATE the cached box only
    mLastAnimExtents[1] += shift;
}
```

and `mNeedsExtentUpdate` is a **throttled, per-avatar-staggered flag** (llvoavatar.cpp ~3011):

```cpp
mNeedsExtentUpdate = ((thisFrame + mID.mData[0]) % upd_freq == 0);
```

`upd_freq` grows with avatar count. So on most frames the box is merely **translated by the pelvis
delta**, which silently assumes the POSE did not change. Stock viewers tolerate this because the
box carries slack relative to a 1x avatar.

`LLDrawable::updateSpatialExtents()` (lldrawable.cpp ~1057) then scales that box about the foot
pivot:

```cpp
scaled_min = outer->mFootPivot + outer->mScale * (scaled_min - outer->mFootPivot);
```

**The staleness error is multiplied by `mScale` along with everything else.** At scale 5 a stale or
pose-outdated box diverges ~5x further from the real geometry, the drawable/attachments fall
outside their own bounds, and frustum/octree culling drops them — head and body vanish. Returning
to 1.0 disables the multiplication, so the stock slack is again sufficient and they reappear.

This is a *different* defect from the previously fixed ones (draw-batch binding, bin radius,
`getLastAnimExtents()` consumers) — those were about the transform not being applied or read.
This one is about the extents being **stale/approximated**, then amplified.

## Fix
While an avatar has an ENABLED outer scale (`hasClientOuterTransform()` true, i.e. scale != 1):
1. **Do not use the throttled/shift-approximated path.** Force `mNeedsExtentUpdate = true` every
   frame for that avatar so `calculateSpatialExtents()` produces exact, current-pose extents before
   the scale multiplication. Only locally-scaled avatars pay this cost — there are normally zero or
   one, so the perf impact is negligible and it is strictly correct.
   - Best done where the throttle is computed (llvoavatar.cpp ~3002-3012): keep stock behavior for
     unscaled avatars, force true when scaled.
2. **Additionally consider padding** the scaled extents by a small proportional margin to absorb
   any residual intra-frame staleness (e.g. attachments updating on a different tick). Padding is a
   cheap belt-and-braces measure; do not use it as a substitute for (1).
3. Verify the same reasoning for **attachment drawables and their spatial bridges** — if any of them
   have their own throttled/cached extent path, they need the same treatment while the wearer is
   scaled. The head is very likely a rigged mesh attachment, and it is the reported casualty.
4. Re-check the **ordering** within a frame: `updateLocalScaleTransform()` runs from
   `LLVOAvatar::idleUpdate()` and refreshes `outer->mFootPivot` from the current root position.
   If `LLDrawable::updateSpatialExtents()` can run BEFORE that in the same frame, extents get scaled
   about the PREVIOUS frame's pivot while the render uses the current one — a divergence that also
   only appears while moving. Confirm the order, and if it is wrong or not guaranteed, make the
   extent path use the same pivot the render will use.

## Constraints
- Scale 1.0 must remain **byte-identical** — normal avatars keep the stock throttle untouched.
- Client-only; nothing to the simulator.
- Keep the previously landed fixes intact (draw-batch rebuild binding, scale-aware bin radius,
  scaled `getLastAnimExtents()`, pixel area/impostor, spatial bridges).
- The `ScaleCull` probe is still in the build — extend it if useful to log whether the extents came
  from the exact or the shifted path, so a follow-up run can confirm.
- RENDER correctness you cannot verify: implement carefully, self-review to 0 must-fix, and report
  which of (1)-(4) you applied, the frame-ordering finding, and confirm the unscaled path is
  untouched. Do NOT claim render correctness verified.

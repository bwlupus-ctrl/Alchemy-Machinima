# Cine Light Rig — Multi-Anchor (group-as-unit) — design seed

**Status:** seed for a design pass. No source modified. Captured 2026-08-16.
Feeds `doc/CINE_LIGHT_RIG_MULTIANCHOR_DESIGN.md` -> Codex -> Opus review -> build.

The Cinematic Light Rig (base + scale + mirror + enhancements + track-mode) is built/reviewed. This
adds a multi-anchor mode so the rig can light a GROUP as one unit — motivated by the Virtual Cam
feature, where a VCam frames several cast members a single-avatar anchor doesn't cover.

## User decision (2026-08-16, fixed)
**Light the group as one unit.** The rig anchors to a SET of subjects (from the Director cast —
Subjects A/B/C/D + self), centres on their midpoint / bounding centre, and lights them as one rig.
NOT per-light anchoring, NOT anchor-to-the-VCam. Scale and facing are derived from the group.

## Verified current model (read from code — cite file:line in the design)
- Single anchor today: `mAnchor` is one `LLUUID`; `LLDirectorCast::instance().resolve(mAnchor)` -> one
  `LLVOAvatar` (`alcinelightrig.cpp` setAnchor/tick). Scene stores `data["anchor"] = mAnchor`.
- The centre (scale-aware, FINAL): chest joint (track-mode 0) or render-pos+1.2 (track-mode 1), foot-
  pivot-scaled via `scaledPoint(avatar, point, subject_scale)`; then OffsetZ×scale; then damping
  (`alcinelightrig.cpp` ~1090-1131). `subject_scale = avatar->getUniformScale()`.
- The facing (mirror, FINAL): `atan2(fwd.Y,fwd.X)` of `<1,0,0>*root->getWorldRotation()`
  (`alcinelightrig.cpp` ~997-1007) — one avatar's body facing.
- The Director cast: `LLDirectorCast` owns the roster + Subjects A/B/C/D (shared with LLActorMover).
  The rig's anchor combo picks ONE cast member or "You".
- VCam: a free camera at a stored world transform (`LLPrismLens` mVirtual*), not subject-targeted —
  so "light what the VCam frames" means "light multiple cast subjects", which is this feature.

## What the design pass must decide

### A. The anchor-set model & selection
- How is the SET stored and selected? Options: multi-select of cast subjects (A/B/C/D + self); a
  "Group" mode that uses all current cast members; or N explicit anchor slots. Recommend one.
  Keep a SINGLE-member set == today's exact behaviour (bitwise), so single-anchor is unchanged and is
  the default. Decide the member cap (all cast? a small N like 4?).
- Scene round-trip: the anchor SET persists (today's single-UUID scene field must still load — an old
  scene's single anchor becomes a one-member set; forward-compat, no version bump if possible).

### B. The group CENTRE (the core geometry decision)
- Centroid of member centres vs bounding-box centre. Consider the two-shot (two subjects apart): the
  midpoint lights both symmetrically; the bounds centre + auto-radius covers spread better. Decide and
  justify. Each member's centre uses its OWN track-mode point (chest vs body) and its own foot-pivot
  scale — so the group centre is the average/bounds of already-scale-correct per-member points.
- **TRACK-MODE INTEGRATION (user emphasis — do not lose this).** The existing track-mode toggle
  (`CineLightRigTrackMode`: 0 Skeleton = relative/chest joint, 1 Body-stable = approximate/render-pos)
  MUST compose with the group. The group centre is the midpoint/bounds of each member's per-member
  point computed UNDER THE CURRENT TRACK-MODE — i.e. Skeleton -> average/bounds of each member's chest
  joint; Body-stable -> average/bounds of each member's body position. So "relative vs approximate"
  works per member and then aggregates. DECIDE: does track-mode stay GLOBAL (one mode applied to every
  member — recommended for v1, simplest, matches the single control) or become PER-MEMBER? If global,
  a group in Body-stable ignores every member's animated bob (the whole point of the toggle) and the
  group centre is stable; in Skeleton it tracks each member's chest. State this explicitly and confirm
  a single-member group under either track-mode is bitwise-identical to today.
- Lost/unloaded/out-of-range member: recompute the centre from the RESOLVED members only; if none
  resolve, go dark (as today). A member entering/leaving must not jump the rig (damping already
  smooths the centre; confirm it still applies to the group centre).

### C. Group SCALE / RADIUS (the hard problem)
- Single-subject `getUniformScale()` does not apply to a group. Decide how the group maps to the rig
  geometry so the lights actually COVER the whole group:
  - Option 1: group scale = a function of member scales (avg/max) used exactly as today's subject_scale
    (rig orbits the centroid at radius×groupScale) — simple but ignores member SEPARATION (a wide
    two-shot of two 1.0 avatars would be under-covered).
  - Option 2: derive an effective radius from the group's spatial EXTENT (bounding radius of the member
    centres + their individual reach) so the orbit expands to encompass the spread. More correct for
    spread-out groups; must still respect the 20 m projector-reach cap (design input) and the exposure
    rule (EV reads NOMINAL radius — do not let group-extent drift exposure, same trap as clone scale).
  - Recommend and specify the exact formula, its clamps, and how it composes with the existing
    scale-aware exposure/headroom rules (the SA-9 "geometry scales, EV nominal" invariant must hold).

### D. Group FACING (for mirror)
- A two-shot has no single facing. Decide the group facing for the mirror reflection axis: average of
  member facings; the dominant/first member; a fixed/world facing for multi-member groups; or the VCam
  facing. Note the mirror is FINAL code (reflect across facing) — single-member must stay exactly
  today's per-avatar facing. State what a 2+ member group uses and why (average facing is the obvious
  candidate but degenerates for back-to-back subjects — address that).

### E. UI
- The anchor combo becomes a multi-select (checkbytes for cast A/B/C/D + self, or a group toggle +
  member picker). Specify the control, and the panel reflow (the M1 lesson: grow the panel AND both
  wrapper heights). Show the resolved member count / a group indicator.

### F. Interaction with the FINALS (careful)
- The group centre/scale/facing REPLACE the single-subject versions inside the scale-aware and mirror
  code. Single-member == today's behaviour bitwise (the fast path). The per-member points still use the
  finalized `scaledPoint` foot-pivot. Do NOT touch `scaledPoint`, the exposure math, or the mirror
  reflection formula — only how the centre/scale/facing INPUTS are computed (one member vs group).
- Emitter lifetime, damping, transitions, FX, gobos, master temp — all unchanged; they consume the
  centre/scale the same way.

## Constraints
- Label PROVES/IMPLIES/INFERS; cite file:line. No source modified by the design.
- Ship-whole: anchor-set model + group centre + group scale + group facing + UI + scene in one
  delivery. Defer nothing silently.
- OFF-LIMITS: pipeline/shaders/llprimitive; the finalized scaledPoint/exposure/mirror-reflection math
  (inputs only); lldirectorcast.* (consumed — READ the cast roster API, do not rewrite it); the
  gobo/colour/preset/track-mode code (unchanged). No version bump if achievable.
- Forward-facing robustness: a group with 0 resolved members, mixed scales (0.05x clone + 1.0 avatar),
  a member leaving mid-shot, an old single-anchor scene — all must degrade sanely.
- Concrete enough to become a Codex brief: named fields/functions/settings/UI + an OFF-LIMITS list.

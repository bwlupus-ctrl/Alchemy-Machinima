# Cine Light Rig — Object Targeting (light a prim, not just an avatar)

## Goal
Let a Cinematic Light Rig instance target an in-world OBJECT (prim/linkset) instead of an
avatar, so props, vehicles, and set pieces can be lit. v1 scope (user decisions):
- **"Target Selected Object" button** in the rig panel: select a prim in-world, click to
  anchor the CURRENTLY-SELECTED rig slot to that object. A **Clear** returns the slot to its
  avatar. (No dropdown scan, no UUID paste.)
- **Single object per slot.** A slot targets exactly ONE object, framed from its bounding box.
  The multi-subject GROUP stays avatar-only — when a slot has an object target, group mode is
  bypassed for that slot.

## Current architecture (what this fits into)
- The rig is avatar-typed: `resolveSlotAvatar()` -> `LLVOAvatar*` (alcinelightrig.cpp:549);
  `tickShared` (~:1550) resolves an avatar (single or group_members[0]), computes a track point
  and a `subject_scale` (avatar `getUniformScale()`), then everything downstream reduces to a
  smoothed **centre** (`mSmoothedCentre`, agent->global at ~:1811) + **subject scale**
  (`mSmoothedScale`), which `render()` + `applyFrame()` consume. Object targeting produces the
  SAME (centre, scale) from an object and joins that common path.

## Data model (additive, per-instance)
- Add `LLUUID mObjectTarget` to `ALCineLightRig` (and `mObjectTarget` to the per-slot blob
  `ALCineLightRigParamBlob`). Null = normal avatar behavior (unchanged). Non-null = target that
  object.
- It is PER INSTANCE (per slot SELF/A/B/C/D), stored/restored with the other blob fields; the
  anchor dropdown still selects which slot you are editing.

## Tick branch (alcinelightrig.cpp tickShared, at the avatar-resolve point ~:1570)
Before resolving the avatar/group, check `mObjectTarget`:
```
if (mObjectTarget.notNull())
{
    LLViewerObject* obj = gObjectList.findObject(mObjectTarget);
    if (obj && !obj->isDead())
    {
        // OBJECT PATH — single subject, no group, no bone/track avatar logic.
        centre_agent   = <object bounding-box centre in AGENT coords>;
        subject_scale  = sanitizeSubjectScale(<object bounding radius> / AVATAR_REF_RADIUS);
        // then jump to the common "have centre + scale -> smooth -> render -> applyFrame" path,
        // exactly as the single-avatar branch does after it has centre_agent + subject_scale.
    }
    else
    {
        // Dead / out-of-range / not-yet-loaded target: behave like "no subject" —
        // destroyEmitters(), mHaveSmoothedCentre=false, return (same as the !avatar branch).
        // Do NOT silently fall back to the avatar; the user chose an object.
    }
}
else { ...existing avatar/group path unchanged... }
```
Details:
- **Centre**: prefer the object's world bounding-box CENTRE (root + linkset children), not just
  the root prim position, so off-centre linksets frame correctly. Use the existing
  bounding-box API (e.g. `LLViewerObject::getBoundingBoxAgent()` / the select-mgr bbox helper);
  convert to global via `gAgent.getPosGlobalFromAgent(centre_agent)` like the avatar path.
- **Scale mapping**: map the object's bounding RADIUS (half of the max bbox dimension, or the
  bbox diagonal/2) to the rig's subject-scale convention where a default ~2 m avatar reads ~1.0.
  Define `constexpr F32 AVATAR_REF_RADIUS` (~1.0 m, i.e. half a 2 m avatar) and
  `subject_scale = clamp(object_radius / AVATAR_REF_RADIUS, SUBJECT_SCALE_MIN, SUBJECT_SCALE_MAX)`.
  Reuse `sanitizeSubjectScale`. This makes a 2 m prop read ~1.0 (framed like an avatar), a 8 m
  vehicle ~4.0, a 0.5 m prop ~0.25 — all handled by the existing scale-aware radius math.
- **Track mode / offsets**: the avatar path adds head-height offsets (+1.65 etc.); the object
  path must NOT — the bbox centre already IS the framing point. Use the bbox centre verbatim.
- **Smoothing**: reuse the existing `mSmoothedCentre` damping so a moving object (vehicle) is
  tracked smoothly, identical to avatar tracking.
- Keep `resolveSlotAvatar()` for the avatar path untouched.

## UI (panel_cine_light_rig.xml + alpanelcinelightrig.*)
- Near the Anchor row, add: a **"Target Object"** button, a **"Clear"** button, and a small
  read-only label showing the current target ("— avatar —" when null, else the object's name or
  short key).
- **Target Object** handler: get the selected root object via
  `LLSelectMgr::getInstance()->getSelection()->getPrimaryObject()` (root it with `getRootEdit()`);
  if null, briefly note "select an object first"; else set the SELECTED instance's object target
  (`ALCineLightRigManager::instance().selected().setObjectTarget(id)`), refresh the label, and
  invalidate so the tick picks it up. Reject targeting an avatar's object or an attachment (must
  be a real in-world object) — flag with the label if rejected.
- **Clear** handler: set the selected instance's object target to null; label -> "— avatar —".
- Refresh the label + button state on instance switch (like the Easy/Manual refresh). When an
  object target is set, the group checkboxes for that slot may be shown disabled with a hint
  ("Object target: group is avatar-only") — optional, but do at least disable group's effect.
- The anchor-dropdown label for a slot may append "(object)" when that slot has a target.

## Persistence (additive; SCENE_VERSION already 4)
- Blob: add the object target to `ALCineLightRigParamBlob` and to `blobMapWellFormed`'s key list
  (a UUID key, e.g. `CineLightRigObjectTarget`), round-tripping through `toSettingsStore`/
  `fromSettingsStore` and the director-scene block, additively.
- On load, if the stored target UUID does not resolve to a live object, keep it stored but behave
  as dead-target (no lights) until it resolves, OR treat as cleared — choose the graceful one:
  KEEP the UUID (so returning to the region re-lights it) but render nothing while unresolved.
  Never fail the scene load over a missing object.

## Tests
- Pure/model: the scale mapping (`objectRadiusToSubjectScale`) as a pure helper with unit tests
  (2 m -> ~1.0, 8 m -> ~4.0, clamp at min/max, degenerate 0 -> min).
- Persistence round-trip for the new blob field (set -> toSettingsStore -> fromSettingsStore).
- Do not disturb existing avatar-path tests.

## Out of scope (v1)
- Objects in the multi-subject group; per-face targeting; auto-scan object dropdown; following an
  object across regions. Attachments (worn objects) are out — target the wearer avatar instead.

# Client-side avatar height (local vertical offset) — design

Status: DESIGN v4.1 (2026-10-05), DESIGN ONLY. No source edits, builds or commits. Next: Codex round 5.

**v4.1 changes (Codex round 4):**
- `LLControlAvatar::computeNeedsUpdate()` OVERWRITES `mNeedsImpostorUpdate` with the wearer's value.
  So `refreshClientHeight()` returns a `changed` flag, computed BEFORE the call at llvoavatar.cpp:5689,
  and is applied AFTER it: `needs_update = computeNeedsUpdate() || changed`, and on `changed`
  `mNeedsImpostorUpdate = true` is re-asserted. This happens before the early return at :5697. The v4
  test also asserts that the impostor flag survives the control-avatar overwrite.
- Slider wheel (cross-doc, SLIDER_WHEEL_GLOBAL_DESIGN): the Local offset slider keeps its 5 mm
  `increment`, but its wheel step follows the GLOBAL default rule: 25 mm per notch, Shift 5 mm, Ctrl
  250 mm. It does not set `wheel_adjust="true"`.

**v4 changes (Codex round 3):** refresh and change detection track the EFFECTIVE offset, not only the
avatar's own value.
- `refreshClientHeight()` caches `mClientHeightEffectiveZ` and adds the wearer key to the stamp.
- For a worn control avatar it reads the wearer's value straight from the STORE, via the wearer's key
  (`keyForAvatar(getAttachedAvatar())`). It never uses the wearer's cached `mClientHeightZ`, so the
  result does not depend on avatar update order.
- It compares that value with the cached previous effective value. On a change it sets
  `mNeedsImpostorUpdate`, calls `mPosePolish.reset()` and forces `needs_update` once. This happens before
  `computeNeedsUpdate()` at llvoavatar.cpp:5689, so the `LLControlAvatar::computeNeedsUpdate` early-out
  (llcontrolavatar.cpp:405-443) cannot skip it.
- `applyOverride` and `getBaseRootPosition` read the cached effective value.
- New regression test: a throttled worn animesh whose own offset is unchanged and whose wearer offset
  changes reports exactly one forced update, and applies wearer + own in that frame.

**v3 changes (Codex round 2):**
- `mLastRootPos` stays VISUAL. A new `mLastBaseRootPos` cache feeds the getOffObject fallback (P0-1).
- sitOnObject/getOffObject apply clientZ DIRECTLY (P0-2).
- Worn animesh uses an EFFECTIVE offset (own + wearer) in the mover and base-root paths (P0-3).
- The revision is checked BEFORE the LOD early-out (P1-4).
- `slamPosition` needs no term (teleport suppresses rendering).
The v2 note "mLastRootPos stored as base" is superseded.

**v2 changes (Codex round 1):** worn animesh now gets its own world-Z term at llcontrolavatar.cpp:194
(P0-1). The Ghost key is stamped by Studio and also re-resolved on instance revision (P0-2).
alghoststudio.cpp:1806 is reclassified as visual, and :13875 is added, giving 13 base / 9 visual
(P0-3, P1-4). The transient root writers are audited and `mLastRootPos` is stored as base (P1-5).
`disconnectViewer` now clears the store (P1-6). The sit offset is world-vertical (P1-7). Regression
tests are added (P2-8).

## Goal
Reuse the Set Hover Height floater to give ANY avatar (you, residents, Ghost clones, animesh) a
vertical offset that only this viewer renders. Session only, never saved, never sent. Offset, not scale.

## 1. How stock hover works today (verified)
- `LLCharacter::mHoverOffset` + `get/setHoverOffset()` (llcharacter.h:288). `LLVOAvatarSelf::setHoverOffset`
  (llvoavatarself.cpp:2934) also sends it to the server; others receive it in appearance
  (`mHoverOffsetWasSet`, llvoavatar.cpp:11355). Clamp `MIN/MAX_HOVER_Z` = -3/+3 m (llvoavatar.cpp:145).
- Floater (llfloaterhoverheight.cpp): drag = `setHoverOffset(z,false)` (local preview); mouse-up/text
  commit writes `AvatarHoverOffsetZ`, whose listener (llviewercontrol.cpp:192) sends. Slider is
  region-gated (`avatarHoverHeightEnabled`); the menu item is gated the same way (`LLEnableHoverHeight`,
  llviewermenu.cpp:4707).
- The offset reaches the render root in `LLVOAvatar::updateRootPositionAndRotation`:
  - standing / flying: `root_pos += getHoverOffset()` (llvoavatar.cpp:5021), AFTER `mInAir` is computed
    and skipped for ghosts;
  - sitting on an object: `pos += getHoverOffset() * mDrawable->getRotation()` (5090);
  - ground sit: re-added post-motion (5812).
  Then `LLActorMover::applyOverride(this)` (5100) may replace the root.
- "Bind Camera view" = `HoverHeightAffectsCamera`: `LLAgentCamera::getAvatarRootPosition()`
  (llagentcamera.cpp:2509) subtracts the hover from `mRoot` when it is off. Self only.

## 2. Mechanism (recommended): one more term next to hover, at every root writer
Nothing reads a "local offset" today, and no single choke point exists: the root is rewritten by the
stock path, `LLControlAvatar::matchVolumeTransform` (also called from `LLDrawable::updateXform`), the
Actor Mover override and the Ghost foot lock. A post-pass on `mRoot` would be overwritten or
double-applied. So the offset is a term each writer adds to the root it computes, never to a root it reads.

New store `ALClientHeight` (LLSingleton, alclientheight.h/.cpp) over a pure model `alclientheightmodel.h`:
`std::map<LLUUID,F32>`, clamp +/-2 m, finite-only, `U64 revision()` bumped only on real change, change
signal, `set/get/reset/resetAll`. Key = the Light Response convention
(`ALAvatarLightResponse::keyForAvatar`, alavatarlightresponse.cpp:112): self -> gAgentID, clone -> Ghost
Studio `Instance::mId` (stable across runtime respawn), animesh -> linkset root (`getActorFxOwnerId`),
resident -> avatar id. Factor that function into a shared helper; do not change Light Response behaviour.

`LLVOAvatar` gets `F32 getClientHeightZ()`, which caches `mClientHeightZ`, `mClientHeightKey`, and a
stamp `{storeRev, ghostInstRev}`. It re-resolves in any of these cases:
(a) the store revision changes;
(b) for ghosts, `ALGhostStudio::getInstanceRevision()` changes; it is bumped at alghoststudio.cpp:458,
    AFTER `mEntityId` is published;
(c) the cached key is null (never treated as final);
(d) for control avatars, `getActorFxOwnerId()` differs from the cached key, which happens when mRootVolp
    or the linkset root arrives late.
Ghost spawn timing: `applyEntityRuntimeState()` places the foot before `inst.mEntityId = new_runtime`
(alghoststudio.cpp:453-457), so `findInstanceByRuntime` misses on that first slam. Studio therefore stamps
the stable id onto the clone (`LLGhostAvatar::setClientHeightKey(inst.mId)`) BEFORE
`applyEntityRuntimeState`. The resolver prefers that stamped key, and (b) is the fallback. A replacement
runtime is a new object with a fresh cache, so it picks up the existing stable-key value on its first
foot lock. The pure resolver decision (`needsResolve(stamp, revs, cachedKey, liveKeyHint)`) lives in
the model header so it can be unit-tested. An empty store returns 0 with one branch. On a value change
the resolver sets `mNeedsImpostorUpdate = true` and calls `mPosePolish.reset()` to drop stale foot locks.

Throttled avatars (v3): `refreshClientHeight()` (the stamp compare plus resolve) runs in `updateCharacter`
BEFORE `computeNeedsUpdate()` (llvoavatar.cpp:5689). If the value changed, it forces `needs_update` for that
one frame, so the early return at :5697-5704 cannot hold a LOD-skipped or impostored avatar at its old
height. The root writers then read the cached value.

Effective offset for worn animesh (v3): `getEffectiveClientHeightZ()` = own clientZ, plus the wearer's
clientZ when the control avatar is worn (`getAttachedAvatar()`). The :194 writer adds only the OWN term,
because the attach joint already carries the wearer's offset. Paths that rebuild the root from
un-offset mover space use the EFFECTIVE term: `applyOverride` (:4608, re-run from :259) and
`getBaseRootPosition()`. For every other avatar, effective == own.

Apply sites (all render-root only; `setPositionAgent`, agent position and messages are never touched):
| Site | Change |
|---|---|
| llvoavatar.cpp:5021 standing/flying | `+= (0,0,clientZ)` next to hover, still after `mInAir`; skipped for ghosts like hover |
| llvoavatar.cpp:5090 sit on object | after the stock rotated hover term, add WORLD-vertical `(0,0,clientZ) * ~seatWorldRot` (root is seat-local here), so the camera's world-Z subtraction matches exactly |
| llvoavatar.cpp:5812 ground sit | add to `off_z` (world Z) |
| llcontrolavatar.cpp:246 animesh (unworn) | `mRoot->setPosition(vol_pos + fixup + clientZ)` |
| llcontrolavatar.cpp:194 animesh (worn) | `mRoot->setWorldPosition(obj_pos + joint_pos + (0,0,clientZ))`; the wearer's inherited offset (via the joint) and the animesh's own offset STACK |
| llactormover.cpp:4608 `applyOverride` | `setWorldPosition(mv.mCurPos + effectiveClientZ)` (mover space stays un-offset) |
| llvoavatar.cpp:9253 `sitOnObject` | `mRoot->setPosition(getPosition() + (0,0,clientZ) * ~seatWorldRot)` (seat-local) |
| llvoavatar.cpp:9313 `getOffObject` | `mRoot->setPosition(cur_position_world + (0,0,clientZ))` (world; parent is now null) |
| llghostavatar.cpp:1100 `applyDesiredGhostFootPosition` | `root_agent.z += clientZ` (ghost object pos = visual root by contract; redundancy guard still holds) |
| llagentcamera.cpp:2512 | Bind off subtracts hover AND self clientZ |

Composition rule: Actor Mover paths, marks and Ghost placements are authored in BASE space (no local
offset). Add `LLVOAvatar::getBaseRootPosition()` = `mRoot` world minus clientZ. Capture sites that read the
rendered root to author state switch to it, or the offset double-counts (the follower hold would
re-capture every walk):
- capture -> base (13): llactormover.cpp 2134, 2321, 2362, 4295, 4927, 5232, 5555; lldirectorcast.cpp
  2003, 2036; llfloaterdirector.cpp 1821; alghoststudio.cpp 102; alpanelghoststudio.cpp 2706, 2740.
- visual -> keep the rendered root (9): llactormover.cpp 5454 (debug), 5469, 11902, 13472, 13875
  (impostor centre-minus-foot; both terms carry the offset, so it cancels), 14532; alghoststudio.cpp 1806
  (paired with the visual head at :1813-1816, and the delta is used at :1838), 3270 (palette anchor);
  llvoavatar.cpp 11960.
Verify by counting (CLAUDE.md rule 6): 22 `getRootJoint()->getWorldPosition` sites before = 13
`getBaseRootPosition` + 9 unchanged after. Put the list in a header comment on `getBaseRootPosition`, so
future captures pick a side.
Transient root writers (v3):
- `sitOnObject` and `getOffObject` carry the term (table above), because they run outside the avatar
  update (llvoavatar.cpp:8702/8714, llviewerobject.cpp:503, llagent.cpp:4160) and LOD-skipped avatars
  never reach the root writer that frame.
- `slamPosition` (:4161) adds nothing: rendering is suppressed during the teleport display.
- `mLastRootPos` (:2927) stays the VISUAL root. It feeds name tags and voice (:2813, :2949).
- A new `mLastBaseRootPos` (`mLastRootPos` minus the effective clientZ, captured at the same line) replaces
  `mLastRootPos` only in getOffObject's standoff fallback (:9294-9300). That fallback becomes the
  drawable's position, so it must be base; the root write that follows re-adds clientZ.
TRUE_MIRROR copies joint locals and root rotation, never root position (llghostavatar.cpp:1629), so
source and clone keep independent offsets. Pose Polish never writes the root; feet raised above the
ground fail its plant gate and it idles, which is correct.
Followers on a leader path ride the leader's spline with their OWN offset (mover space is base).

## 3. "Me"
Two sliders, one meaning each. "Hover height (everyone sees)" is the existing `HoverHeightSlider`,
unchanged: same callbacks, region gate and server send. It is enabled only when Me is selected.
"Local offset (only you see)" is the new `ClientHeightSlider`, the same widget for every target,
including Me. Bind Camera stays a Me-only row and covers both terms. The stock slider never moves when
another avatar is selected, so nobody can change their server hover by accident.

## 4. Selector: combo (chosen) over an "add avatar" list
A combo needs no picker floater, list persistence or remove UX, and lets the floater keep its small
shape. Contents, rebuilt only in `onOpen`, in the combo prearrange (drop-down opening) callback, after
Reset all, and from an avatar-name-cache callback (UI rules 1-3; no draw override, so rule 5 needs no
zone; if one is added it may only compare `ALGhostStudio::getInstanceRevision()` and the store revision
and set a dirty flag):
`Me` / residents from `LLWorld::getAvatars` (display name via `LLAvatarNameCache`, short id until it
resolves) / animesh from `LLCharacter::sInstances` (control, not dead; worn ones labelled "(worn)") /
clones from `ALGhostStudio::getInstances()` (`mName`, then `mSourceLabel`) / stored keys not present,
labelled "(away)". Rows with a non-zero offset get a marker. The floater key may carry a UUID
(`showInstance("edit_hover_height", id)`); optionally add "Local Height..." to menu_avatar_other.xml.
`LLEnableHoverHeight` must no longer gate the whole floater: keep the menu always enabled and gate only
the stock slider.

## 5. Slider, reset, layout
`ClientHeightSlider`: -2.000..+2.000 m, `increment="0.005"`, 3 decimals, editable text. Live write on drag
(the store only, which is cheap). Buttons: `Reset` (selected -> 0) and `Reset all` (clears the store).
Status text: "N avatars adjusted", updated from the store change signal with `setTextIfChanged`. Layout:
fixed small floater, rows positioned with `top_pad`. There is no scroll document, so rule 6 does not
apply; the floater height grows once. Translated XMLs only override labels, so the new widgets fall back
to English. Slider wheel: the slider keeps `increment="0.005"` (5 mm, for drag and text), but the
wheel follows the global default rule from doc/SLIDER_WHEEL_GLOBAL_DESIGN.md: 25 mm per notch, Shift 5 mm,
Ctrl 250 mm. It does not set `wheel_adjust="true"`. Check that a wheel step sends a commit
the slider handler honours, and that the stock hover slider's wheel commit still reaches `onFinalCommit`
(server send). If not, the wheel would preview without sending.

## 6. Edge cases
- Leaves, teleports, derezzes: the entry is kept by key for the session, shown "(away)", and re-applied
  on return through the per-avatar revision cache. Never saved. `LLAppViewer::disconnectViewer`
  (llappviewer.cpp, next to `LLActorMover::resetEditSession()` at :6125) calls
  `ALClientHeight::resetAll()` when an instance exists, so offsets cannot leak across accounts.
- Sitting: applies as a world-vertical lift (unlike stock hover, which tilts with the seat). Ground sit
  uses the post-motion path. Flying applies.
  `mInAir` and footsteps keep using the un-offset root.
- Camera: self follows Bind Camera. For others, nothing binds the agent camera. Focusing the menu on an
  avatar targets its sim position, so it is off by the offset (accepted; the Director and cine cameras read
  joints and follow).
- Animesh: keyed by linkset root; a relink loses the entry. Worn animesh: the wearer's offset carries it
  through the attach joint, and its own offset stacks on top (written at :194).
- Ghost clone vs source: separate keys and separate rows. A clone spawned at the source copies the BASE
  foot, so it does not inherit the source's offset. `setGhostPosition` (no foot lock) is not offset; it is
  a legacy path.
- Impostors are re-baked on change. Culling uses joint-based extents, so check at +/-2 m.
- Never server: the store has no message or settings hook. Self clientZ never enters `setHoverOffset`.

## Files to change
New: `alclientheightmodel.h`, `alclientheight.h/.cpp`, `tests/alclientheightmodel_test.cpp`, and the
shared key helper. Edit: `llfloaterhoverheight.cpp/.h` (target, second slider, resets, combo refresh),
`floater_edit_hover_height.xml` (en), `llvoavatar.h/.cpp` (cache, `getClientHeightZ`, `getBaseRootPosition`,
3 sites, `mLastRootPos`), `llcontrolavatar.cpp` (:194 + :246), `llactormover.cpp` (applyOverride + 7
captures), `llghostavatar.h/.cpp` (foot lock, `setClientHeightKey`), `lldirectorcast.cpp`,
`llfloaterdirector.cpp`, `alghoststudio.cpp` (:102 capture, key stamp before `applyEntityRuntimeState`),
`alpanelghoststudio.cpp`, `llagentcamera.cpp`, `llappviewer.cpp` (disconnect clear), `llviewermenu.cpp`
(enable), `alavatarlightresponse.cpp` (use the shared key helper), CMakeLists (new files + test).

## Tests (model + resolver)
- Store: clamp at +/-2 and reject NaN/Inf; revision bumps only on a real change; reset of an absent key
  does not bump; resetAll empties and bumps once; keys stay independent (clone vs source); a set to 0
  erases the entry; an empty store is the fast path; `clearSession()` (the disconnect path) empties it.
- `needsResolve`:
  - Late animesh ownership: the cached key is the control avatar's own id and the live hint becomes the
    linkset root, so it re-resolves and picks up the stored value.
  - Ghost runtime replacement with an existing stable-key value: a fresh cache plus the stamped key
    resolve on the first call; a null cached key always re-resolves; a ghost instance-revision bump
    re-resolves.
- Effective offset: unworn = own; worn = own + wearer; wearer 0 -> own.
- v4: a throttled worn animesh with its own offset unchanged and its wearer offset changed reports exactly
  one forced update, and the effective value equals wearer + own in that frame. A detach (wearer key
  becomes null) also counts as a change.
- Pre-early-out refresh: a revision change on a throttled avatar reports "changed" exactly once.
- Classification: a source count check in the review checklist (PowerShell): exactly 9
  `getRootJoint()->getWorldPosition` and 13 `getBaseRootPosition` in the 8 listed files.

## In-world checklist
1. Me: the stock slider still sends (an alt sees it), Local offset is invisible to the alt, and Bind
   Camera on/off frames correctly with both.
2. Resident +1.5 m standing, walking, flying, sitting on a prim, ground sit: attachments, name tag and
   shadow follow; no foot jitter with Pose Polish on.
3. Animesh, unworn and worn; a Ghost clone in FROZEN/MIRROR/TRUE_MIRROR; a source and clone with
   different offsets.
4. Actor Mover: straight move, path walk, a follower, Reset-to-Marks with an offset set BEFORE authoring.
   No double lift and no creep.
5. Actor Mask and Rig Rim on an offset actor; an impostored distant avatar; culling at the screen edge at
   +/-2 m.
6. TP away and back (the entry persists; no one-frame drop at TP, sit-down or stand-up), relog as a
   different account (all gone), Reset and Reset all. A sitting avatar on a tilted seat lifts straight up.
   Clone Refresh/respawn keeps its offset on the first frame. Name tag and voice dots follow the offset
   avatar. A distant/impostored avatar updates promptly on a slider change. A worn animesh walked by the
   Actor Mover keeps wearer + own offset.
7. Tracy: no new hot zone, and the floater open does no per-frame work.

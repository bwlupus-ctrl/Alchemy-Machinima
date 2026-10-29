# UNIFIED POSE / ANIM SWITCHBOARD — IMPLEMENTATION SPEC (for Codex `--write`)

**On disk:** `I:\alchemy-machinima\doc\UNIFIED_POSE_ANIM_SWITCHBOARD_SPEC.md`
**Run mode:** Codex **`--write` (implement)**, one section-group per run. Produce **committed code**, not prose.
**Shell:** Windows **PowerShell 5.1** (`Select-String`, `;`, `if ($?)`; no `grep`, no `&&`).
**Build:** Claude builds Release (`--target alchemy-bin`), never Codex. All builds need explicit user OK.
**Grounding tips:** committed fork tip is the source of truth; verify EVERY symbol/signature with
`git grep <sym>` before you call it. Line numbers below are hints (2026-08-12), not contracts.

---

## 0. OBJECTIVE
Make each of the Director Anim Switchboard's 12 slots hold **either** an animation asset (today's
behavior) **or** a saved poser **pose**, and punch the right one onto the slot's target
(Self / Cast / Ghosts). Add a "send this pose to a slot" bridge from the poser's **Poses** list,
mirroring the existing anim bridge. **One unified board. Backward compatible** with existing
anim-only banks. Poses are applied **locally only** (client-side, like the poser's own posing and the
anim board's local playback) — never sent to the sim.

Non-goals: no new pose *engine* (reuse `FSPoserAnimator` / `FSPosingMotion`); no server broadcast;
no upload-to-asset. A "pose slot" references a **local pose file**, not a UUID.

---

## 1. THE TWO SUBSYSTEMS (verified integration points)

### Switchboard (engine + panel + bridge — all exist)
- `indra/newview/aldirectoranimswitcher.h` — `ALDirectorAnimSwitcher`:
  `struct Slot { bool mEnabled; LLUUID mAnimID; std::string mLabel; S32 mTarget; S32 mPriority;
  F32 mSpeed; bool mLoop; bool mSnapOnCut; };`, `enum ETarget {TARGET_CAST=0,TARGET_SELF=1,TARGET_GHOSTS=2}`,
  `SLOT_COUNT` (12), `static std::vector<Slot> loadBank(); static void saveBank(const std::vector<Slot>&);`,
  `bool punch(S32); void stopAll(); void tick(F64);` (private `applySlot`, `applySlotToAvatar`,
  `getTargetAvatars`, `defaultBank`, `sanitizeSlot`).
- `indra/newview/aldirectoranimswitcher.cpp` — `loadBank`/`saveBank` (LLSD in setting
  `DirectorAnimSwitcherBank`, `BANK_VERSION=1`, keys enabled/anim/label/target/priority/speed/loop/snap
  ~lines 88-175); `getTargetAvatars` (~198, resolves `LLDirectorCast::getIds()`+`resolve()`, handles
  control avatars); `applySlotToAvatar` (~238: ghost branch = `ALGhostStudio` directed-anim ledger;
  else = `av->startMotion(slot.mAnimID)`); `applySlot`/`tick`.
- `indra/newview/alpaneldirectoranimswitcher.h/.cpp` + `skins/default/xui/en/panel_director_anim_switcher.xml`
  — the 12-slot panel: per-slot editor (UUID line editor, From Explorer, label, target, priority, speed,
  loop, snap, use-in-auto), `handleDragAndDrop` (DAD_ANIMATION), draw()-rate bank diffing. Embedded in
  `skins/default/xui/en/floater_director.xml` (`director_anim_switcher_embedded`, ~line 1214).
- Poser→board anim bridge already added: `fsfloaterposer.*` `onAnimSendToSwitchboard()` /
  `refreshSwitchboardSlotCombo()` + controls `poser_anim_slot_combo`/`poser_anim_to_switchboard` in the
  **Anims** tab of `floater_fs_poser.xml`.

### Poser (business layer is UI-free; the floater owns file IO + permission gates)
- `indra/newview/fsposeranimator.h/.cpp` — `FSPoserAnimator` (business layer, **no UI**). Operates on a
  per-avatar `FSPosingMotion` obtained via private `getPosingMotion(LLVOAvatar*)`. Key API:
  `bool tryPosingAvatar(LLVOAvatar*)`, `void stopPosingAvatar(LLVOAvatar*)`, `bool isPosingAvatar(LLVOAvatar*) const`,
  `void setPosingAvatarJoint(...)`, `void loadJointRotation/Position/Scale(...)`,
  `void setRotationIsWorldLocked/Mirrored(...)`, `bool loadPosingState(LLVOAvatar*, bool ignoreOwnership, LLSD)`,
  `void setAllAvatarStartingRotationsToZero(LLVOAvatar*)`, `const FSPoserJoint* getPoserJointByName(const std::string&)`.
  **Posing state is keyed by avatar (in FSPosingMotion), NOT by the FSPoserAnimator instance** — so a
  *separate* `FSPoserAnimator` (owned by the switchboard) manipulates the SAME per-avatar motion. This is
  what makes the engine integration clean.
- `indra/newview/fsfloaterposer.h/.cpp`:
  - `bool loadPoseFromXml(LLVOAvatar* avatar, const std::string& poseFileName, E_LoadPoseMethods loadMethod)`
    (~1288). **Its body is already UI-free**: it uses only `mPoserAnimator`, `gDirUtilp`, `LLSD`, and
    `gSavedSettings.getBOOL("FSPoserLoadBlackDragonFormat")`. **Precondition (line ~1295):
    `mPoserAnimator.isPosingAvatar(avatar)` must be true** — the caller must `tryPosingAvatar` first.
  - Pose files: `LL_PATH_USER_SETTINGS` + `POSE_SAVE_SUBDIRECTORY="poses"` + `<name>` +
    `POSE_INTERNAL_FORMAT_FILE_EXT=".xml"`. Enumerated by `refreshPoseScroll(LLScrollListCtrl*, subDir)` (~423).
  - `enum E_LoadPoseMethods { ROTATIONS=1, POSITIONS=2, SCALES=3, ROTATIONS_AND_POSITIONS=4,
    ROTATIONS_AND_SCALES=5, POSITIONS_AND_SCALES=6, ROT_POS_AND_SCALES=7, HAND_RIGHT=8, HAND_LEFT=9,
    FACE_ONLY=10, SELECTIVE=11, SELECTIVE_ROT=12 }` (in `fsfloaterposer.h`).
  - Permission gates (floater methods): `bool couldAnimateAvatar(LLVOAvatar*)`,
    `bool havePermissionToAnimateAvatar(LLVOAvatar*)` (self + control avatars you own — includes ghosts),
    `bool havePermissionToAnimateOtherAvatar(LLVOAvatar*)` (other residents, permission-gated).
- Ghost drive system: `indra/newview/llghostavatar.h` + `ALGhostStudio` — `setEntityDriveMode(mode, id)`,
  modes `DRIVE_MIRROR` / `DRIVE_DIRECTED` / `DRIVE_FROZEN` (verify exact names/signature in-tree; the anim
  board already calls `ghost->setEntityDriveMode(ALGhostStudio::DRIVE_DIRECTED, slot.mAnimID)` in
  `applySlotToAvatar`). A ghost is re-driven every frame by its mode — see §5.

---

## 2. DATA MODEL — unify the Slot (aldirectoranimswitcher.h/.cpp)
Add to `ALDirectorAnimSwitcher`:
```
enum EKind : S32 { KIND_ANIM = 0, KIND_POSE = 1 };
```
Add to `struct Slot`:
```
S32         mKind         = KIND_ANIM;            // anim asset vs local pose file
std::string mPoseName;                            // pose file basename (no extension), KIND_POSE only
S32         mPoseLoadMethod = 7 /*ROT_POS_AND_SCALES*/; // E_LoadPoseMethods, KIND_POSE only
```
Serialization (`loadBank`/`saveBank`) — **do NOT bump `BANK_VERSION`** (keep 1). Add optional keys read
defensively so existing anim-only banks load unchanged:
- `saveBank`: also write `item["kind"] = slot.mKind; item["pose"] = slot.mPoseName;
  item["poseload"] = slot.mPoseLoadMethod;`
- `loadBank`: `if (item.has("kind")) bank[i].mKind = item["kind"].asInteger();` (absent ⇒ KIND_ANIM);
  same for `pose` (string) and `poseload` (int).
- `sanitizeSlot`: clamp `mKind` to {KIND_ANIM,KIND_POSE}; clamp `mPoseLoadMethod` to the valid
  `E_LoadPoseMethods` range [1,12]; `utf8str_symbol_truncate(mPoseName, ...)` (reuse the label pattern);
  a KIND_POSE slot with empty `mPoseName` is treated as empty (renders "(empty)", never applies).
`displayLabel(const Slot&)` (panel) and `refreshButtons` label: KIND_POSE ⇒ label else `mPoseName`
(prefix) else "(empty)".

---

## 3. UI-FREE POSE APPLICATION (the enabling refactor)
Extract the body of `FSFloaterPoser::loadPoseFromXml` into a **reusable, UI-free** function so the engine
never depends on the floater. **Recommended:** add a method on the business layer:
```
// fsposeranimator.h/.cpp
bool FSPoserAnimator::loadPoseFileOntoAvatar(LLVOAvatar* avatar,
        const std::string& poseFileBaseName, E_LoadPoseMethods loadMethod);
```
- Move the parse/apply logic from `loadPoseFromXml` (lines ~1291-1436) verbatim, replacing
  `mPoserAnimator.` with `this->`. Keep the `isPosingAvatar(avatar)` precondition, the version handling,
  the Black-Dragon swap, and the try/catch.
- `E_LoadPoseMethods` currently lives in `fsfloaterposer.h`. Move it to `fsposeranimator.h` (or a shared
  header) and update includes; OR keep it where it is and have `fsposeranimator.h` include it — pick the
  option that avoids a circular include and **compile-verify**.
- Rewrite `FSFloaterPoser::loadPoseFromXml` to a thin wrapper:
  `return mPoserAnimator.loadPoseFileOntoAvatar(avatar, poseFileName, loadMethod);`
  **The floater's load behavior must stay byte-identical** (same joints, same result) — this is a pure
  extraction, no logic change.

---

## 4. ENGINE — ALDirectorAnimSwitcher applies poses (aldirectoranimswitcher.h/.cpp)
- Add member `FSPoserAnimator mPoseAnimator;` and `std::set<LLUUID> mPosedAvatars;` (avatars this board has
  put into posing, so `stopAll`/target-change can release them). `#include "fsposeranimator.h"` — engine may
  depend on the business layer, **not** on `fsfloaterposer.h`/any UI header.
- In `applySlotToAvatar(LLVOAvatar* av, const Slot& next, const Slot& prev)` branch on `next.mKind`:
  - `KIND_ANIM` → existing code UNCHANGED.
  - `KIND_POSE`:
    1. Permission gate (§6). Not allowed ⇒ return (no-op).
    2. Ghost handling (§5) — set drive state so the pose is not overwritten.
    3. `if (!mPoseAnimator.isPosingAvatar(av)) mPoseAnimator.tryPosingAvatar(av);`
    4. `mPoseAnimator.loadPoseFileOntoAvatar(av, next.mPoseName, (E_LoadPoseMethods)next.mPoseLoadMethod);`
    5. `mPosedAvatars.insert(av->getID());`
  - `mPriority/mSpeed/mLoop/mSnapOnCut` are **ignored** for KIND_POSE.
- **Cut/switch release rule:** in `applySlot` (or applySlotToAvatar) when the previously-active program was a
  pose and the incoming program on that avatar is NOT a pose (KIND_ANIM, or the slot cleared), call
  `mPoseAnimator.stopPosingAvatar(av)` and drop it from `mPosedAvatars` **before** starting the anim, so the
  animation is visible (a held pose would otherwise fight it). Pose→pose just re-applies (poser replaces).
- `stopAll()`: for every id in `mPosedAvatars`, resolve to `LLVOAvatar*` and `stopPosingAvatar`; clear the
  set. Then existing anim stop logic. (A ghost put into a posing/frozen state must be restored — §5.)
- `tick()`/auto-director: pose slots participate exactly like anim slots (each auto-cut applies a pose).

---

## 5. GHOSTS — the one hard problem (must be handled, not ignored)
Ghosts (`LLGhostAvatar : LLControlAvatar : LLVOAvatar`) are re-driven **every frame** by their drive mode
(`DRIVE_MIRROR` copies the source; `DRIVE_DIRECTED` plays an anim; `DRIVE_FROZEN` holds). A per-frame drive
loop will **overwrite** a posing-motion pose. So a KIND_POSE slot targeting a ghost must first put the ghost
into a state where the posing motion wins.
- **Required approach (verify the API in `llghostavatar.h`/`ALGhostStudio`):** before applying a pose to a
  ghost, `ghost->setEntityDriveMode(ALGhostStudio::DRIVE_FROZEN, LLUUID::null)` (stop the drive loop from
  re-posing it), then `tryPosingAvatar` + `loadPoseFileOntoAvatar`. On release (cut to non-pose, or
  `stopAll`), restore `DRIVE_MIRROR` (or the prior mode if you can capture it).
- If, after verifying, FROZEN still re-writes joints each frame (i.e. the drive loop reasserts bind pose),
  add a minimal ghost "posed" hold that suppresses the per-frame joint reassert while a switchboard pose is
  active — but prefer reusing FROZEN.
- **Fail-safe:** if ghost posing cannot be made to hold in this pass, a KIND_POSE slot targeting Ghosts must
  be a **logged no-op** (never a visibly fighting half-state). Self and owned non-ghost control avatars must
  still work. Document the limitation in code comments (no `.md`).

---

## 6. PERMISSIONS (poses are local-only)
The engine must respect the same gates the poser uses. `couldAnimateAvatar` /
`havePermissionToAnimateAvatar` / `havePermissionToAnimateOtherAvatar` are **FSFloaterPoser** methods —
move the underlying logic to a UI-free location (free functions, or statics on `FSPoserAnimator`) so the
engine can call it, OR replicate the minimal check. Rule: **Self and owned control avatars (incl. ghosts)
always pass; other residents are permission-gated** (same as the poser's local posing). Never broadcast.

---

## 7. PANEL UI (alpaneldirectoranimswitcher.h/.cpp + panel_director_anim_switcher.xml)
- Add a per-slot **Kind** selector (combo `Anim` | `Pose`) bound to `mKind`.
- KIND_POSE editor: a **pose picker** combo listing saved poses (enumerate
  `LL_PATH_USER_SETTINGS/poses/*.xml` the way `refreshPoseScroll` does — strip the `.xml`), plus a
  **load-method** combo mapping to `E_LoadPoseMethods` (at least: Rotations only / Rotations+Positions /
  Rot+Pos+Scales / Face only / Hands). For KIND_POSE, the anim-only controls (UUID line editor, From
  Explorer, priority, speed, loop, snap) go disabled/hidden; Target stays.
- KIND_ANIM editor: unchanged.
- `draw()` bank-diffing and `banksEqual` must include the new fields (`mKind`,`mPoseName`,`mPoseLoadMethod`).
- Program/slot button labels reflect pose vs anim. Keep everything inside the Director Console (superset rule).
- `handleDragAndDrop(DAD_ANIMATION)` stays anim-only (dropping an anim implies KIND_ANIM; set `mKind=KIND_ANIM`).

## 8. POSER BRIDGE — send a pose to a slot (fsfloaterposer.h/.cpp + floater_fs_poser.xml)
Mirror the existing Anims-tab combo+Send, but from the **Poses** list (right-side `poses_scroll` / the Load
area). Add a slot-chooser combo + "Send pose" button whose handler writes into the chosen slot via
`ALDirectorAnimSwitcher::loadBank()`/`saveBank()`:
`bank[slot] = {mKind=KIND_POSE, mPoseName=<selected pose basename>, mPoseLoadMethod=<current load method>,
mTarget=<existing>, mLabel keep-or-blank}`. Reuse `refreshSwitchboardSlotCombo()` occupancy logic. Keep the
Anims-tab Send (writes KIND_ANIM + mAnimID) as-is.

## 9. IMPLEMENTATION ORDER (one commit group per step)
1. **§3 refactor** — `loadPoseFileOntoAvatar` on FSPoserAnimator + thin `loadPoseFromXml` wrapper + move
   `E_LoadPoseMethods`. (Build-verifiable in isolation; behavior-identical.)
2. **§2 model+serialization** — Slot fields, loadBank/saveBank/sanitizeSlot, displayLabel/labels.
3. **§4 engine apply** + **§6 permissions** + **§5 ghosts** — the pose punch path, release rule, stopAll.
4. **§7 panel UI** — kind selector, pose+method pickers, bank-diff fields.
5. **§8 poser bridge** — send-pose-to-slot from the Poses list.

## 10. VERIFY BEFORE RETURN (reviewer re-runs)
- **Backward compat:** an existing `DirectorAnimSwitcherBank` (v1, anim-only, no kind keys) loads with every
  slot `KIND_ANIM` and behaves exactly as before. A board with no bank is unchanged.
- **Refactor fidelity:** `FSFloaterPoser::loadPoseFromXml` applies a pose byte-identically to before (same
  joints, same Black-Dragon path, same version handling).
- **No UI in the engine:** `aldirectoranimswitcher.cpp` includes `fsposeranimator.h` but **no**
  `fsfloaterposer.h` / floater / XUI headers. (`git grep -n 'include' aldirectoranimswitcher.cpp`.)
- **Ghost path:** a KIND_POSE slot targeting Ghosts either visibly holds the pose (drive handled) or is a
  logged no-op — never a per-frame fight.
- **Symbols:** every FSPoserAnimator/ghost/cast symbol you call resolves in-tree (`git grep`). No invented
  signatures. No `.md` authored by you. No weasel placeholders/TODOs on added lines.
- Claude builds Release afterward; 0 `error C` / `error LNK` required before acceptance.

## 11. FILES TO TOUCH (expected)
- `indra/newview/fsposeranimator.h`, `fsposeranimator.cpp` — `loadPoseFileOntoAvatar`; maybe host `E_LoadPoseMethods`.
- `indra/newview/fsfloaterposer.h`, `fsfloaterposer.cpp` — thin `loadPoseFromXml`; poser→slot pose bridge; UI-free permission helpers (or expose them).
- `indra/newview/aldirectoranimswitcher.h`, `aldirectoranimswitcher.cpp` — Slot unification, serialization, pose apply, ghost handling, stopAll release, `FSPoserAnimator mPoseAnimator`.
- `indra/newview/alpaneldirectoranimswitcher.h`, `alpaneldirectoranimswitcher.cpp` — kind selector, pose/method pickers, bank-diff.
- `indra/newview/skins/default/xui/en/panel_director_anim_switcher.xml` — new controls (fits the 340-wide card; grow the card in `floater_director.xml` only if unavoidable).
- `indra/newview/skins/default/xui/en/floater_fs_poser.xml` — Poses-tab send-to-slot controls (respect the ~180px joint-tab content width if placed there; the Poses list is in the right column with more room).
- `indra/newview/llghostavatar.h`/`.cpp` — only if a "posed hold" beyond DRIVE_FROZEN is needed.
- No new global settings required (reuse `DirectorAnimSwitcherBank` + `DirectorAnimSwitcher*`). Optional:
  `DirectorAnimSwitcherPoseLoadMethod` default 7.

---

## APPENDIX — key facts already verified (do not re-derive; still confirm signatures)
- Pose apply needs `isPosingAvatar(av)==true` first (loadPoseFromXml:1295) → caller `tryPosingAvatar`.
- Posing state is per-avatar in `FSPosingMotion` via `getPosingMotion(av)` → a separate `FSPoserAnimator`
  instance is safe for the engine.
- Bank LLSD = `{version:1, slots:[{enabled,anim,label,target,priority,speed,loop,snap}]}`; add
  kind/pose/poseload as optional keys, no version bump.
- `getTargetAvatars` already resolves control avatars (ghosts) via `LLDirectorCast::resolve()`.
- Anim board already drives ghosts with `setEntityDriveMode(DRIVE_DIRECTED, animID)` — reuse the same
  `ALGhostStudio` mode enum for the FROZEN approach.

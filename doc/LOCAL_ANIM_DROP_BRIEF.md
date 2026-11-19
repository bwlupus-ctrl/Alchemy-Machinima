# Drop-to-play: drag an animation onto any avatar to play it locally — design brief

**Date:** 2026-11-19 · **rev 3**
**Status:** DESIGN ONLY. Nothing implemented, nothing built.

- **rev 1** was adversarially reviewed by an Opus sub-agent: 6 must-fix, 8 should-fix and 9 nits.
- **rev 2** applies all of them; §8 is the change log.
- **rev 3** applies the re-review of rev 2: 4 must-fix, 4 should-fix and 2 nits (see §8).

**Process (per `CLAUDE.md`):**
1. Codex implements (`--write`, `--prompt-file`).
2. Opus reviews the code; loop until 0 must-fix.
3. Claude builds once.
4. The user tests in-world.

Codex never builds.

> **BASE.** The `file:line` references are from branch `ccr-221def9f-h17t1m` (which contains
> `origin/fix/animesh-clone-pose-polish`). Re-anchor against any unpushed local work. There is **no
> existing in-world "drop onto clone" path** on this branch. The only animation-drop precedent is the
> Switchboard slot (`alpaneldirectoranimswitcher.cpp:574-600`).

---

## 1. The ask

> "Like the drag and drop animations for clones, would we be able to drag and drop animations onto
> normal avatars? With a way to cancel the animations I drag and drop on to them."

Drag an animation from inventory onto an avatar in the world — yourself, a resident, or a Ghost clone.
It plays on that avatar **locally**: only on your screen. There is a clear way to stop each animation,
or all of them.

**Local only:** the other person and everyone else see their normal motion. This is the same model as
the Director's Animation Switchboard and the poser. **Nothing is sent to the region** (§3.2 says how
that is guaranteed on self).

**Not targets:** animesh and HUD attachments. They resolve to `DT_OBJECT`
(`lltooldraganddrop.cpp:849`, `:2007`), where the stock behaviour (put into the object's contents) stays
unchanged.

---

## 2. What the code PROVES today

| Fact | Where |
|---|---|
| **Drop table.** In-world drops go through a table indexed by cargo type × target (`DT_NONE / DT_SELF / DT_AVATAR / DT_OBJECT / DT_LAND`). `DAD_ANIMATION`: self → `dad3dNULL`, avatar → `dad3dGiveInventory`. | `lltooldraganddrop.cpp:264-283` |
| **Target resolution.** Attachments resolve to their avatar. Self → `DT_SELF`; any other avatar, **every ghost included**, → `DT_AVATAR`. | `lltooldraganddrop.cpp:844-880` |
| **Each handler runs twice on a drop.** During a drop, each handler is called with `drop=false` and then `drop=true` in the same `pick()`. The `drop=false` branch must have **no side effects**. | `lltooldraganddrop.cpp:886-911` |
| **Shift is reliable at drop.** `pick_info.mKeyMask` is live at drop (`gKeyboard->currentMask(true)`). On hover it does not refresh if Shift changes while the mouse is still. | `llviewerwindow.cpp:4908` |
| **Give path.** `dad3dGiveInventory`: agent-inventory source, finished item, RLVa `@share`, then `LLGiveInventory::doGiveInventoryItem`. | `lltooldraganddrop.cpp:3240-3266` |
| **Drag tooltips** come from `mToolTipMsg` / `handleToolTip`. | `lltooldraganddrop.cpp:504-600` |
| **Local playback on real avatars (Switchboard):** `stopMotion(prev)`, `startMotion(anim, 0.f)`, then `findMotion(anim)->setPriorityOverride(p)`. | `aldirectoranimswitcher.cpp:410-425` |
| **The animation preview plays any owned animation by `getAssetUUID()`**, with no permission check, and re-applies the override in `draw()` once the motion goes active. | `llpreviewanim.cpp:116-141`, `:158-180` |
| **The override is cleared on deactivate.** `LLMotion::deactivate()` resets the override to −1. `setPriorityOverride` returns early when the value is unchanged. | `llcharacter/llmotion.cpp:268-278`, `:133` |
| **`flushAllMotions()` drops overrides.** It deletes every motion instance and restarts it without the override; "Reset Skeleton & Animations" triggers it. A stop and restart in under 1 s (blend-out) also produces a **new instance**. | `llmotioncontroller.cpp:1143-1169`, `:421-431`; `llvoavatar.cpp:2418` → `7264` |
| **Instances vanish.** Motion instances are purged (`purgeExcessMotions`), and deleted when the asset fails to load, so **`findMotion()` can return NULL**. A rebuilt avatar (after leaving draw distance) is a new `LLVOAvatar` with no motions. | `llmotioncontroller.cpp:225-235`, `:839-851` |
| **`getLoop()` is only valid once loaded.** `LLKeyframeMotion::getLoop()` returns `false` until the asset has loaded. | `llcharacter/llkeyframemotion.h:91-94` |
| **Load and stop state** can be read with `isMotionLoading(motion)` and `LLMotion::isStopped()`. | `llmotioncontroller.h:182`; `llmotion.h:176` |
| **Sim stop.** A sim echo stops an animation only if it's in `mPlayingAnimations`, which is public. A local motion is stopped by the sim only if the sim signals and then stops **that same id**. | `llvoavatar.cpp:7085-7126`; `llvoavatar.h:1078` |
| **Jellydoll / visual mute** stop only `mPlayingAnimations`, and `processAnimationStateChanges` skips non-`AOA_NORMAL` avatars. A local motion would **keep running on a jellydoll**. | `llvoavatar.cpp:13014-13024`, `:7105-7111` |
| **On self, `LLVOAvatar::startMotion`/`stopMotion` go through `AOEngine::override()`.** If the AO remaps the id, they **call `gAgent.sendAnimationRequest`** (a region message). They also do the AFK and gender remap. `LLCharacter::startMotion`/`stopMotion` are the plain virtual base versions. | `llvoavatar.cpp:7355-7376`, `:7405-7420`; `llcharacter/llcharacter.h:149-152` |
| **"Stop Animating Me"** stops only `mPlayingAnimations`. | `llagent.cpp:4794-4814` |
| **Ghosts.** `LLGhostAvatar::isGhostId()` / `isGhostAvatar()` identify **every** ghost. `ALGhostStudio::findInstanceByRuntime(object id)` matches only **entity clones** (harness ghosts and others return null). `setInstanceDriveMode(instance_id, mode, anim)` takes the **Studio instance id** (`Instance::mId`), not the avatar id; the runtime entity id changes when a clone is recovered. | `llghostavatar.h:179`; `alghoststudio.cpp:484-490`, `:2526`; `alghoststudio.h:363`, `:626-628` |
| **Frozen and the Switchboard.** Freezing goes through `setInstancePaused`, which keeps `mResumeDriveMode`. The Switchboard drives ghosts with `ghost->setEntityDriveMode` directly, bypassing `Instance::mDriveMode`. The ghost's own drive state is private. | `alghoststudio.cpp:1339-1369`; `aldirectoranimswitcher.cpp:391-407`; `llghostavatar.h:241-246` |
| **Menus and registration all exist:** | |
| · avatar menus (Director submenu) | `menu_avatar_other.xml:114-127`; `menu_avatar_self.xml:305` |
| · attachment menus (Director submenu) | `menu_attachment_self.xml`, `menu_attachment_other.xml` |
| · Me menu (holds Director Console) | `menu_viewer.xml:121` |
| · floater registration | `llviewerfloaterreg.cpp:773-783` |
| **Idle hook:** `ALDirectorAnimSwitcher::instance().tick(...)` runs per frame. | `llappviewer.cpp:5555` |
| **Chat commands:** `/ghostanim` precedent, which logs to `LL_INFOS` (not user-visible). | `alchatcommand.cpp:777-840`, `:815` |

---

## 3. Behaviour

### 3.1 The drop

**Setting `AlchemyDropAnimPlaysLocallyOnResidents`** (Boolean, default **false**; see D1). It applies to
**residents only**:

| Target | Plain drop | Shift + drop |
|---|---|---|
| **Resident**, setting **off** (default) | stock **give** | **play locally** |
| **Resident**, setting **on** | play locally | give |
| **Self** | play locally (today a no-op) | same, no give |
| **Any ghost** (`isGhostAvatar()`) | play on the clone (below) | same; **never a give** |

There is **never a give to a ghost**, on any path, setting or modifier.

**Tooltip** (set in both hover and drop calls, side-effect free):
- resident: "Give to <name> (Shift: play locally)" or "Play locally on <name> (Shift: give)";
- self: "Play locally on yourself";
- entity clone: "Play on clone (Directed)";
- a ghost that isn't an entity clone: "Play locally on clone".

**Names:** respect RLVa `@shownames` the same way other avatar-name UI does; Codex finds the helper used
by the hover tip. This is INFERENCE: the RLVa API was not checked.

**Ghost targets:**
- **Entity clone** (`findInstanceByRuntime(hit_avatar_id)` non-null): `setInstanceDriveMode(inst->mId,
  DRIVE_DIRECTED, anim)`. Store the instance id, plus the previous `mDriveMode` and directed anim (§3.3).
  If the clone is **already** `DRIVE_DIRECTED` with this same animation and has no entry, there is
  nothing to record. Treat the drop as the toggle: return the clone to `DRIVE_MIRROR`.
  If the clone is in a **locked group**, apply it to **every member**, as the panel does
  (`alpanelghoststudio.cpp:2255`), with one entry per member. A re-drop (toggle) or Stop on any member
  applies to **all** members of that group, matching the drop.
- **Other ghosts** (harness ghosts, ghosts outside the Studio): play locally, like a resident.

**Acceptance (both calls, no side effects in `drop=false`):**
- Item source is agent inventory, or the **Library** (`SOURCE_LIBRARY`; library animations are fine to
  play locally).
- The item is finished (`isFinished()`).
- `getAssetUUID()` is **non-null**. Otherwise return `ACCEPT_NO` with the tooltip "Animation asset not
  available". INFERENCE: on SL the asset id isn't withheld, since the preview plays any owned animation,
  but OpenSim or a partial fetch could give null.
- Reject built-in `gAnimLibrary` ids (the `ANIM_AGENT_*` system animations): they'd trigger AFK, sit and
  similar special handling.
- **No copy/transfer permission is needed** for local play. `isInventoryGiveAcceptable` and RLVa `@share`
  apply **only** when the result is a give.

**Multiple items:**
- Local play accepts `ACCEPT_YES_MULTI`. Each animation is processed once per drop: **deduplicate asset
  ids** inside one drop, so two copies don't toggle each other off.
- The handler runs once per item (`mCurItemIndex`) and never learns where a drop starts. Keep a
  per-drop `std::set<LLUUID>` that is **cleared when `drop && mCurItemIndex == 0`**.
- **Mixed drops:** a multi-item drop whose result for some item is a give gets the stock "must drop one
  at a time" rejection. `dad3dGiveInventory` returns `ACCEPT_YES_SINGLE` (`:3266`), and `pick()` rejects
  multi-item cargo below `ACCEPT_YES_COPY_MULTI` (`:897-916`). This is accepted: the give path is
  off-limits.

### 3.2 Playback

**Start / stop helpers** (in `ALLocalAnimDrop`), using **`LLCharacter` directly**:

```cpp
av->LLCharacter::startMotion(anim, 0.f);   // NOT av->startMotion(): bypasses the self-only
av->LLCharacter::stopMotion(anim, false);  // AOEngine remap + gAgent.sendAnimationRequest
```

This is what guarantees nothing reaches the region, even on self with an AO.

- **It is callable (PROVEN):** the inheritance is public (`LLVOAvatar` → `LLAvatarAppearance` →
  `LLCharacter`), and `aoengine.cpp:154-165` already calls `gAgentAvatarp->LLCharacter::stopMotion`.
- **It is used for every target, for uniformity.** What we skip is `remapMotionID` (built-in walk, run
  and sit ids) and, on self, `setAFK`/`onAnimStop`. All of these touch **only built-in ids**, which §3.1
  rejects; that rejection is what makes the bypass safe.
- **Reject built-ins** with `gAnimLibrary.animStateToString(id) != nullptr`, the same test
  `createMotion` uses (`llmotioncontroller.cpp:379`).

**Priority:**
- Setting **`AlchemyDropAnimPriority`** (S32, default **5**, range −1..6; −1 = the animation's baked
  priority).
- 5 rather than 4, because an override **tied** with an AO's priority blends rather than wins.
  INFERENCE: AOs commonly use 3–4.
- The override is (re)applied by the tick (§3.3), not just at start.

**Toggle and stacking:**
- Dropping the same animation onto the same avatar again stops it.
- Several dropped animations can play on one avatar; they blend by priority.

### 3.3 Registry, tick and cancel

**Registry:** `ALLocalAnimDrop` (`LLSimpleton` or `LLSingleton`), `newview/allocalanimdrop.{h,cpp}`.
The pure state machine lives in `newview/allocalanimdropmodel.h` for TUT tests.

**Entry:**

```
struct Entry {
    enum Kind { SELF, RESIDENT, GHOST_LOCAL, CLONE } kind;
    LLUUID   avatar_id;        // resident/self/ghost: avatar id; CLONE: runtime id at drop (display only)
    LLUUID   instance_id;      // CLONE only: Studio Instance::mId (stable across recovery)
    LLUUID   anim_id;          // asset id
    std::string avatar_name, anim_name;
    S32      priority;         // desired override (-1 = baked)
    enum Phase { PENDING, ACTIVE } phase;
    bool     loop_known = false, loops = false;
    bool     retried = false;  // PENDING: one restart allowed
    bool     suspended = false, failed = false;
    S32      fail_count = 0;   // consecutive restarts that never reached playing
    F64      started_at;       // frame time, for the PENDING timeout (never reset on retry)
    F64      next_retry_at;    // backoff for restarts
    // CLONE only: what to restore on Stop
    S32      prev_drive_mode;  LLUUID prev_directed_anim;
};
```

**Tick:** called from `llappviewer.cpp:5555`, beside `ALDirectorAnimSwitcher::tick`, throttled to
**1 Hz on frame (wall) time**, not presentation time. Never cache `LLMotion*`; call `findMotion()` again
on every tick.

**Non-clone entries:**
1. Avatar not found (out of draw distance, teleported away): keep the entry and skip it.
2. Avatar found but `getOverallAppearance() != AOA_NORMAL` (jellydoll or visually muted): if the motion
   is active, stop it, then set `suspended = true` (keep the entry, don't restart it). When
   `getOverallAppearance()` is `AOA_NORMAL` again, clear `suspended`; the next step then restarts the
   motion.
3. `m = av->findMotion(anim)`. "Playing" means `m && getMotionController().isMotionActive(m) &&
   !m->isStopped()`. A motion that is stopped but still active is **easing out**: leave it alone, and
   neither restart nor remove it until it is inactive.
   - **`m` non-null and `isMotionLoading(m)`**: still loading, so wait.
   - **`m` non-null and loaded:**
     - Record `loop_known = true`, `loops = m->getLoop()`.
     - Playing: phase = `ACTIVE`, `fail_count = 0`; if `m->getPriorityOverride() != priority`, call
       `setPriorityOverride(priority)`. This covers flush, blend-out and deactivate wiping the override.
     - Active but stopped (easing out): wait.
     - Inactive:
       - `loops`: restart with the start helper, then apply the override;
       - else: **finished**, so remove the entry. This also catches a short one-shot that started and
         finished between ticks.
   - **`m` null:**
     - `loop_known && loops`: restart (the avatar was rebuilt or the motion purged), with
       `++fail_count`. Each restart creates a new instance and a new asset request: `markBad` is
       effectively a no-op, since a NULL constructor falls back to `LLKeyframeMotion::create`
       (`llmotioncontroller.cpp:95-117`). So **back off**: retry after 1, 2, 4, … s, capped at 30 s.
       After 5 consecutive failures, set `failed = true` (a flag, not a `Phase` value). A failed entry
       is shown in the floater, never retried, and removable. `fail_count` resets when the motion is seen playing.
     - `loop_known && !loops`: remove.
     - `!loop_known` and phase `PENDING`: retry **once** (`retried` flag, `started_at` **not** reset).
       If `now - started_at > 30 s`, remove, logging "asset never loaded".

**Clone entries:**
- If the instance no longer exists, remove the entry.
- Otherwise do nothing per tick: the clone's drive owns playback.

**Cancel routes:**
- **Re-drop** the same animation on the same avatar (toggle).
- **Right-click → Director → "Stop local animations"**, enabled only when that avatar has entries:
  - in `menu_avatar_other.xml`, `menu_avatar_self.xml`, `menu_attachment_other.xml` and
    `menu_attachment_self.xml`;
  - the avatar is resolved with the same `find_avatar_from_object` the Director items use;
  - for a clone, the clicked runtime id is mapped through `findInstanceByRuntime`.
- **"Local Animations" floater** (`floater_local_anims.xml` + `alfloaterlocalanims.{h,cpp}`):
  - opened from the Me menu next to Director Console, and from the Director Console;
  - registered in `llviewerfloaterreg.cpp`;
  - rows show avatar, animation, priority and state (playing / loading / out of range / suspended /
    failed);
  - buttons: **Stop** per row, **Stop all on avatar**, **Stop all**;
  - rows refresh on a registry-changed signal.
- **Chat:** `/localanim list` and `/localanim stop <name-substring|all>`. Output goes to **local chat
  as a system line** (or a notification), not `LL_INFOS`.
- **Hooks.** For each non-clone entry on that avatar, a hook applies the sim-playing guard below: if
  `av->mPlayingAnimations` contains the id, it only removes the entry. Otherwise it **stops** the motion
  with `stop_immediate = true` (through the `LLCharacter::` helper), **then** removes the entry.
  - **CLONE entries are skipped** by the hooks: the clone's drive owns playback, and a flush restarts
    the directed animation anyway. Clones are stopped through the normal clone Stop. Removing entries alone is not
  enough: a non-self reset calls `flushAllMotions()`, which restarts every motion still in
  `mActiveMotions`, including ones easing out, without the override, leaving it untracked.
  - **"Stop Animating Me":** one line in `LLToolsStopAllAnimations::handleEvent`
    (`llviewermenu.cpp:6207-6213`), before `gAgent.stopCurrentAnimations()`, clearing **self** entries.
    - Not in `LLAgent::stopCurrentAnimations` itself: that is also called by AO disable/reload
      (`aoengine.cpp:1612`), the pose stand (`fsfloaterposestand.cpp:81`, `:108`) and self reset.
    - An AO reload must not kill drops.
  - **"Reset Skeleton & Animations":** one line at the top of the `if (reset_animations)` block in
    `LLVOAvatar::resetSkeleton` (`llvoavatar.cpp:2406-2419`), before either branch. It clears that
    avatar's entries (self or not).
    - This is the single point that all four menu callers reach (`llviewermenu.cpp:7078`, `:7125`,
      `:7155`; `alviewermenu.cpp:135`).

**Stopping an entry:**
- **Self, resident, ghost-local:**
  - If `av->mPlayingAnimations` contains `anim_id` (the sim is also playing it for real), **do not call
    stop**. Just remove the entry, so we never kill their real animation locally.
  - Otherwise call the stop helper (eased out) and remove the entry.
- **Clone:**
  - Restore only if the instance is still `DRIVE_DIRECTED` with `mDirectedAnim == anim_id`. Otherwise the
    user has since changed it: remove the entry and leave the clone alone.
  - Restore with `setInstanceDriveMode(inst, prev_drive_mode, prev_directed_anim)`, **including when
    the previous mode was `DRIVE_FROZEN`**.
    - `setInstanceDriveMode` leaves `mResumeDriveMode`/`mResumeDirectedAnim` alone (`:2526-2546`), so
      un-pausing later resumes the clone's original mode.
    - **Never** use `setInstancePaused(inst, true)` here. The clone is DIRECTED at that moment, so it
      would overwrite `mResume*` with the dropped animation (`:1346-1352`).
    - INFERENCE: a clone restored to Frozen holds the pose where the dropped animation left it, not its
      original frozen pose. Documented.

**Session only:** logout clears everything, so nothing persists into a later session.

---

## 4. Interactions

- **Same animation signalled by the sim:**
  - If the target's own viewer plays and then stops the same id, the sim's stop ends the local copy.
  - A loaded looping entry is restarted by the next tick (within 1 s). A one-shot isn't.
  - Stated, not fixed.
- **Director Switchboard on residents:**
  - It starts and stops its slot animations with `stopMotion`/`startMotion`.
  - If both systems use the same id on the same avatar, the Switchboard's stop ends ours, and the tick
    restarts it if looping.
  - The Switchboard's priority override can also be replaced by ours on the next tick, so the two
    fight. Documented, not fixed.
- **Director Switchboard on clones:**
  - It calls `ghost->setEntityDriveMode` directly, bypassing `Instance::mDriveMode`, and the ghost's drive
    state is private.
  - So **a Switchboard punch silently replaces a clone drop, and the registry cannot detect it**. The
    entry stays until the user stops it.
  - Stop then finds `Instance` still DIRECTED with our anim and restores, which overrides the
    Switchboard's punch. Documented as a limit.
- **Self and the AO:** your AO keeps running underneath. Priority decides what shows, which is why the
  default is 5.
- **True Mirror / Reflect clones:**
  - A drop switches the clone to Directed, which pauses the mirroring.
  - Stop restores the exact previous mode (D4).
- **Temporal Capture:** local motions follow the global animation time factor like all motions.
- **Clones and give:** never a give to any ghost (§3.1). A related give-path bug for **other** item types
  dropped on ghosts is tracked as a separate task.

---

## 5. Implementation sketch

1. **`lltooldraganddrop.cpp`:**
   - Add `dad3dAnimationOnSelf` and `dad3dAnimationOnAvatar` in place of the `DAD_ANIMATION` row's
     `DT_SELF` / `DT_AVATAR` cells.
   - The avatar handler decides by the §3.1 table. For a give it calls the unchanged
     `dad3dGiveInventory`, and only when the target is **not** a ghost.
   - Both handlers set `mToolTipMsg`.
   - Side effects only when `drop == true`.
2. **`ALLocalAnimDrop` + `allocalanimdropmodel.h`:**
   - registry, start/stop helpers (`LLCharacter::` qualified), the tick, clone restore logic, and a
     changed signal.
3. **Hooks (explicitly allowed one-line edits):**
   - the tick call at `llappviewer.cpp:5555`;
   - `LLToolsStopAllAnimations::handleEvent` in `llviewermenu.cpp`;
   - the top of the `reset_animations` block in `LLVOAvatar::resetSkeleton`.
4. **Menus:** "Stop local animations" in the four menus, with an enable callback.
5. **Floater:** the XUI and class, registered in `llviewerfloaterreg.cpp`, with a Me-menu item.
6. **Chat:** `/localanim` in `alchatcommand.cpp`.
7. **Settings** (`settings_alchemy.xml`): `AlchemyDropAnimPlaysLocallyOnResidents` (false) and
   `AlchemyDropAnimPriority` (5).

**OFF-LIMITS:**
- the give path's own code (`dad3dGiveInventory`, `LLGiveInventory`);
- `LLVOAvatar::startMotion`/`stopMotion` and `processAnimationStateChanges`, plus everything else in
  `llvoavatar.cpp` except the one `resetSkeleton` hook line;
- `LLAgent::stopCurrentAnimations`;
- `LLMotion` and `LLMotionController`;
- the Switchboard;
- AOEngine;
- anything that sends a region message.

---

## 6. Tests

**TUT** (`newview/tests/allocalanimdropmodel_test.cpp`, registered like `alghostmotionmath_test`):
- **Decision table:** every combination of target kind × setting × Shift. Assert **ghost → never give**.
- **Toggle:** a re-drop removes the entry. Two copies of one asset id in a single drop give one entry.
- **Tick transitions, with a fake motion state (null / loading / loaded-active / loaded-stopped,
  loop yes/no):**
  - PENDING → ACTIVE;
  - a one-shot finished between ticks is removed;
  - looping + null motion restarts;
  - a not-loaded entry times out at 30 s;
  - the override is re-applied when it's wrong;
  - stopped-but-active (easing out) is neither restarted nor removed;
  - looping + repeated null motion backs off and goes FAILED after 5;
  - PENDING retries only once;
  - a suspended (jellydoll) entry is never restarted.
- **Clone Stop:** restores only when still DIRECTED with our anim. FROZEN restores through
  `setInstanceDriveMode` and **leaves `mResume*` unchanged**.
- **Per-drop dedup:** the set is cleared at `mCurItemIndex == 0`.
- **Stop with the id also sim-signalled:** no stop call is made.

**In-world (outcomes stated in advance):**
1. **Plain drop on a resident:** SL's normal give confirmation appears (default unchanged).
2. **Shift-drop a dance on a resident:** the tooltip reads "…(Shift: play locally)" and the dance plays
   on your screen only. A second viewer, or the person, sees no change.
3. **Right-click them → Director → Stop local animations:** the dance eases out.
4. **Draw distance:** Shift-drop a looping dance, fly out of draw distance and come back. **PASS** if it's
   playing again. Shift-drop a bow: **PASS** if it plays once and its row disappears.
5. **Self with an AO:** drop on yourself. It plays over the AO at priority 5, and a second viewer sees
   only your AO. **PASS** also requires no `AgentAnimation` message in the log.
6. **Clones:**
   - Drop on a clone in True Mirror. It goes Directed; Stop returns it to **True Mirror**.
   - Pause a Mirror clone (Frozen), drop on it, then Stop: it returns to Frozen. **Then un-pause it:
     PASS if it resumes Mirror.** FAIL if it resumes Directed with the dropped animation.
7. **Reset and jellydoll:** "Reset Skeleton & Animations" on the resident removes their entries, and
   the dance is **gone**, not restarted at baked priority. "Stop Animating Me" clears your self entries.
   An AO off/on does **not** clear them.
   Jellydoll them: the dance stops and doesn't restart until un-jellydolled.
8. **Chat:** `/localanim list` prints to chat, and `/localanim stop all` clears everything.

---

## 7. Open decisions (defaults chosen)

- **D1 — Residents' plain drop.** Default: **give**, with Shift for local play.
  - Reason (review): dropping onto a resident is a primary stock way to give, so silently changing it
    risks "I thought I gave it".
  - The setting flips it.
  - Self and clones always play locally, since giving there was never meaningful.
- **D2 — Default priority:** 5.
- **D3 — Persistence:** session only.
- **D4 — Clone Stop:** restores the exact previous drive mode, but only if the clone is still on our
  animation.

---

## 8. Change log

**rev 2 must-fixes:**
1. Loop status is cached once loaded. A null `findMotion` is handled.
2. The override is re-applied every tick.
3. Every ghost is excluded from give.
4. Clone entries are keyed on the instance id; the full entry schema is given.
5. The Mirror/previous contradiction is resolved; FROZEN restores through pause, and a guarded restore
   is added.
6. One-shot detection uses loaded + stopped; jellydoll and visual mute suspend entries.

**rev 2 should-fixes:**
- null asset id is rejected; Library sources allowed;
- `LLCharacter::` start/stop on self, so the AO can't send anything to the region;
- "Stop Animating Me" / Reset hooks;
- D1 flipped;
- priority 5;
- `@shownames`;
- no local stop of a sim-playing id;
- the idle hook is named.

**rev 3 (re-review of rev 2) must-fixes:**
1. Frozen restore uses `setInstanceDriveMode`; `setInstancePaused` would corrupt `mResume*`.
2. The hooks stop motions immediately before removing entries.
3. Hooks are placed at `LLToolsStopAllAnimations` and `resetSkeleton` and explicitly allowed. They are
   not in `stopCurrentAnimations`, which the AO also calls.
4. The mixed-drop claim is withdrawn; a mixed drop gets the stock single-drop rejection.

**rev 3 should-fixes:**
- restart backoff and a FAILED state; PENDING retries once;
- easing-out (stopped but active) defined;
- the per-drop dedup set is specified;
- locked-group toggle and Stop apply to all members.

**rev 3 nits:**
- un-pause added to test 6;
- the built-in rejection is cross-referenced as the reason the `LLCharacter::` bypass is safe.

**rev 3 follow-ups** (third review returned 0 must-fix; these are its should-fixes and nits):
- the hooks apply the sim-playing guard and skip CLONE entries;
- `failed` is a flag and is shown in the floater;
- `suspended` is cleared on return to `AOA_NORMAL`;
- a drop on a clone already Directed with the same animation is treated as the toggle.

**rev 2 nits:**
- the twice-called handler stays side-effect free on hover;
- animesh and HUD are excluded;
- multi-cargo dedup and mixed drops;
- locked groups;
- no in-world clone-drop precedent;
- `/localanim` output goes to chat.

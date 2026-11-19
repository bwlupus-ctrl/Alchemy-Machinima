# Drop-to-play: drag an animation onto any avatar to play it locally — design brief

**Date:** 2026-11-19
**Status:** DESIGN ONLY. Nothing implemented, nothing built, **not yet adversarially reviewed**.
**Process (per `CLAUDE.md`):** an Opus adversarial review of this brief; then Codex implements
(`--write`, `--prompt-file`), Opus reviews the code, loop to 0 must-fix, Claude builds once, the user
tests in-world. Codex never builds.

> **BASE.** `file:line` references are from branch `ccr-221def9f-h17t1m` (which contains
> `origin/fix/animesh-clone-pose-polish`). Re-anchor against any unpushed local work.

---

## 1. The ask

> "Like the drag and drop animations for clones, would we be able to drag and drop animations onto
> normal avatars? With a way to cancel the animations I drag and drop on to them."

Drag an animation from inventory onto **any avatar in the world** — yourself, a resident, or a Ghost
Studio clone — and it plays on that avatar **locally** (only on your screen). There is a clear way to
stop each one, or all of them.

**Local only:** the other person and everyone else see their normal motion. This is the same model as
the Director's Animation Switchboard driving cast avatars today, and as the poser.

---

## 2. What the code PROVES today

| Fact | Where |
|---|---|
| In-world drops are dispatched through a table indexed by cargo type × drop target `DT_NONE / DT_SELF / DT_AVATAR / DT_OBJECT / DT_LAND`. For `DAD_ANIMATION`: self → `dad3dNULL` (nothing), avatar → **`dad3dGiveInventory`** (offers to give the item). | `lltooldraganddrop.cpp:264-283`; `lltooldraganddrop.h:103-107` |
| The target is resolved from the pick: attachments map to their avatar, then self → `DT_SELF`, any other avatar → `DT_AVATAR`. **Ghost clones are avatars, so they also land on `DT_AVATAR`.** Hover calls each handler with `drop=false` (acceptance and cursor), and the drop calls it with `drop=true`; `pick_info.mKeyMask` carries the modifiers. | `lltooldraganddrop.cpp:844-912` |
| `dad3dGiveInventory` requires an agent-inventory source and a finished item, checks RLVa `@share`, then calls `LLGiveInventory::doGiveInventoryItem`. | `lltooldraganddrop.cpp:3240-3266` |
| Drag tooltips come from `mToolTipMsg`, shown by `handleToolTip`. | `lltooldraganddrop.cpp:504-600` |
| **Local playback on real avatars already exists:** the Switchboard does `av->stopMotion(prev)`, then `av->startMotion(anim, 0.f)`, then `findMotion(anim)->setPriorityOverride(priority)`. | `aldirectoranimswitcher.cpp:410-425` |
| A per-playback client-side priority override (never mutating shared asset data) exists on `LLMotion`, with the same precedent in the animation preview. | `llcharacter/llmotion.h:105-106`; `llpreviewanim.cpp:116-141` |
| A sim echo stops an animation **only** if it's in that avatar's `mPlayingAnimations` (signalled by the sim). A locally started motion isn't, so the avatar's own signals don't stop it — **unless** the sim signals and then stops that same animation id. | `llvoavatar.cpp:7085-7126` |
| Entity clones: the drive is set through `ALGhostStudio::setInstanceDriveMode(id, DRIVE_DIRECTED, anim)`; a picked clone object maps back to its Studio instance with `findInstanceByRuntime(object id)`, and the reverse is `resolveEntityClone`. | `alghoststudio.h:626-628`; `alghoststudio.cpp:2526`, `:1407` |
| The avatar right-click menu has a Director submenu (`Avatar.AddToCast`, …), registered in `alviewermenu.cpp`. | `skins/default/xui/en/menu_avatar_other.xml:114-127`; `alviewermenu.cpp:1274-1275` |
| Chat commands are handled in `alchatcommand.cpp` (precedent `/ghostanim`). | `alchatcommand.cpp:777-840` |

---

## 3. Behaviour

### 3.1 The drop

- **Setting `AlchemyDropAnimPlaysLocally`** (Boolean, default **true**).
  - **On:** dropping an animation onto an avatar plays it locally. **Holding Shift while dropping
    restores the stock behaviour** (give the item, with SL's usual confirmation).
  - **Off:** stock behaviour, and Shift+drop plays locally instead.
  - D1 records this default.
- **The drag tooltip** names what will happen: "Play locally on <name> (Shift: give)" or
  "Give to <name> (Shift: play locally)". On self: "Play locally on yourself". On a clone: "Play on clone
  (Directed)".
- **Targets:**
  - **Self (`DT_SELF`):** today a no-op. Now it plays locally on your own avatar. It is **not** sent to
    the region; that's what "Play in world" in the preview is for.
  - **A resident (`DT_AVATAR`, not a clone):** local play, or give with the modifier.
  - **An entity clone:** `setInstanceDriveMode(instance, DRIVE_DIRECTED, anim)`. That's the clone's own
    drive, the same thing the Ghost Studio panel does, so it's cancelled the same way. It's still
    tracked in the list (§3.3), so "Stop" returns the clone to Mirror.
- **Acceptance:**
  - The item must be in agent inventory and finished, as for give.
  - **No copy/transfer permission is required to play locally.** Playing only needs the asset, which the
    viewer fetches by id, as it does for every avatar's animations. The give path's
    `isInventoryGiveAcceptable` check applies only to the give modifier.
  - RLVa: `@share` blocks the **give** only; local play gives nothing, so it isn't blocked.
- **Multiple items:** dropping several animations at once plays each on the target (ACCEPT_YES_MULTI for
  the local path).

### 3.2 Playback

- `av->startMotion(asset_id, 0.f)`, then the priority override from the setting
  **`AlchemyDropAnimPriority`** (S32, default **4**, range −1..6 where −1 means the animation's baked
  priority). 4 sits above typical AO stands (inference: AOs commonly use 3–4), so the dropped animation
  shows. Apply the override when the motion first exists and re-apply once it's active (the asset may
  still be loading at drop time; the preview's `draw()` re-apply is the precedent).
- **Dropping the same animation onto the same avatar again stops it** (a toggle).
- One avatar can have several dropped animations at once (they blend by priority, like any SL
  animations).

### 3.3 Tracking, re-apply and cancel

**Registry** (`ALLocalAnimDrop`, singleton, `newview/allocalanimdrop.{h,cpp}`): entries of
{avatar id, animation asset id, display name, inventory name, priority, phase}.

- **Phases:** `PENDING` (started, asset loading) → `ACTIVE` (seen active at least once) → removed.

**1 Hz tick, per entry:**

- Avatar not found (out of draw distance, teleported away): keep the entry; nothing to do.
- Avatar found, motion **active**: mark `ACTIVE`.
- Avatar found, motion **inactive**:
  - **looping** (`LLMotion::getLoop()` once loaded) → **restart** it and re-apply the override. This
    covers the avatar object being rebuilt after leaving and re-entering draw distance, after a
    teleport, or after an appearance rebuild.
  - **non-looping and `ACTIVE`** → it finished naturally: **remove** the entry (a one-shot played once).
  - **still `PENDING`** after 30 s → remove, with a log line (the asset never loaded).
- A clone entry whose clone was deleted is removed.

**Cancel, every route:**

- **Right-click any avatar → Director → "Stop local animations"** (enabled only when that avatar has
  entries). Add it to `menu_avatar_other.xml`, the self menu, and the attachment menus, next to the
  existing Director items.
- **A "Local Animations" floater** (Me menu, and the Director Console): one row per entry with avatar
  name, animation name and priority; **Stop** per row, **Stop all on avatar**, **Stop all**. Rows update
  as entries come and go.
- **Chat:** `/localanim list`, `/localanim stop <name-substring|all>`.
- **Drop the same animation again** (toggle, §3.2).

**Stopping:**

- **Resident or self:** `av->stopMotion(anim)` (eased out) and remove the entry.
- **Clone:** `setInstanceDriveMode(instance, DRIVE_MIRROR)` and remove the entry.

**Session:** entries live for the session only. Logout clears them; nothing persists, so a stale
animation never restarts unexpectedly in a later session.

---

## 4. Interactions

- **The same animation signalled by the sim:** if the target's own viewer plays and then stops the exact
  same animation id, the sim's stop also stops the local copy (§2). Looping entries are restarted by the
  next tick, so they recover within 1 s; one-shots don't. Stated, not fixed.
- **The Director Switchboard:** it starts and stops its own slot animations on cast avatars. If both
  systems drive the same id on the same avatar, either one stopping ends both. The registry restarts its
  own looping entries; document this.
- **Self:** a locally dropped animation on yourself is overridden by nothing the region sends unless the
  region signals that same id. Your AO keeps running underneath, so priority decides what shows.
- **True Mirror / Reflect clones:** their controller is paused, so a clone drop switches the clone to
  Directed. That ends the mirroring until Stop returns it to **Mirror**. (If the clone was in True
  Mirror, Stop returns it to Mirror, not True Mirror. Store the previous drive in the entry and restore
  it.)
- **Temporal Capture:** local motions follow the global animation time factor like all motions.
- **Ghost clones dropped with Shift:** never offer a give (a clone isn't a resident). A related
  give-path bug for other item types is tracked separately.

---

## 5. Implementation sketch

1. **Drag and drop** (`lltooldraganddrop.cpp`):
   - change the `DAD_ANIMATION` row's `DT_SELF` and `DT_AVATAR` handlers to a new
     `dad3dAnimationOnAvatar`;
   - it decides local vs give from the setting XOR `MASK_SHIFT` in `pick_info.mKeyMask`;
   - for give, it calls through to the existing `dad3dGiveInventory` unchanged (never for clones);
   - it sets `mToolTipMsg` during hover;
   - the self handler only ever does local play.
2. **`ALLocalAnimDrop`:** registry, start/stop, tick (called from the same idle place as other Alchemy
   per-frame managers, throttled to 1 Hz), signals for the floater.
3. **Menu:** the "Stop local animations" item, with an enable callback checking the registry.
4. **Floater** `floater_local_anims.xml` plus `alfloaterlocalanims.{h,cpp}`: a list, buttons, refresh on
   registry change.
5. **Chat:** `/localanim` in `alchatcommand.cpp`.
6. **Settings:** `AlchemyDropAnimPlaysLocally` (true), `AlchemyDropAnimPriority` (4).

**Off-limits:** the stock give path's behaviour (unchanged when give is chosen); `LLVOAvatar` animation
state processing (no edits); the Switchboard; the region protocol (nothing is ever sent for a local play).

---

## 6. Tests

**TUT (pure helper `allocalanimdropmodel.h`):** the registry state machine.

- Toggle on a second drop of the same id.
- PENDING → ACTIVE → removed for a one-shot.
- A looping entry restarts when inactive.
- The PENDING timeout.
- Stop-all per avatar and globally.
- The local-vs-give decision for every combination of setting and Shift, and that clones never give.

**In-world, outcomes stated in advance:**

1. Drop a dance onto a nearby resident: it plays on them on your screen. A second viewer, or the person,
   sees no change.
2. The tooltip reads "Play locally on <name> (Shift: give)". **Shift+drop** shows SL's normal give
   confirmation.
3. Right-click them → Director → Stop local animations: the dance eases out.
4. Drop a looping dance, fly away out of draw distance and come back: it's playing again. Drop a
   one-shot (a bow): it plays once and its row disappears from the list.
5. Drop onto yourself: plays locally. Drop onto a clone: the clone switches to Directed; Stop returns it
   to its previous drive.
6. `/localanim list` lists the entries, and `/localanim stop all` clears them all.
7. With an AO running on the target, the dropped animation shows at priority 4; at "baked" a low-priority
   animation may not show (expected).

---

## 7. Open decisions (defaults chosen)

- **D1 — Drop default:** play locally; Shift = give.
- **D2 — Default priority:** 4.
- **D3 — Persistence:** session only.
- **D4 — Clone Stop:** restores the clone's previous drive mode.

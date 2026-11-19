# Ghost AutoAnimate (learned performance) — design brief v3.1 (R3 GO + fixes)

**Date:** 2026-09-28 · **Base:** `fix/animesh-clone-pose-polish` HEAD `96aa1197555` (every `file:line` below was re-read at this HEAD).
**Status:** DESIGN ONLY. Codex IMPLEMENTS from this brief (`--write`), an Opus sub-agent reviews adversarially,
loop to 0 must-fix, THEN Claude builds once. **Codex: do not build, do not commit, do not touch OFF-LIMITS (§9).**
**Ship whole:** model + runtime glue + settings + Ghost Studio UI (also hosted in the Director) + Director cast
menu + chat verbs + reset wiring + instrumentation + unit tests, in one delivery.
**Supersedes** v1 (`doc/GHOST_AUTOANIMATE_BRIEF.md`) and v2 (this file, revised in place). Answers
`doc/GHOST_AUTOANIMATE_REVIEW_R1.md`, `_R2.md` and `_R3.md` item by item.
**Precedence:** §0c > §0b > later sections where they disagree (later sections were edited to match).

## 0c. R3 resolution table (v3.1 delta; R3 verdict GO after M1/M2)

| R3 item | v3.1 resolution | § |
|---|---|---|
| M1 switch with next == cur deletes the only stand | If no OTHER group member is resident, or the draw yields next == cur: redraw cur's dwell and do nothing else — never add-next/remove-cur with next == cur. | 3.5 |
| M2 uncached anims never become resident | On every scheduler bind (handoff, Load, Refresh rebind, COLD), for each pool id whose `LLKeyframeDataCache::getKeyframeData(id)` is NULL: `LLCharacter::createMotion(id)` (public, `llcharacter.h:142`) on the ghost (body) or the linkset's control avatar (animesh). A new motion is created stopped (`llmotion.cpp:45`), and `updateLoadingMotions` activates only non-stopped motions (`llmotioncontroller.cpp:834-837`), so it fetches without playing. Once per id per channel bind; `AUTOANIM-FETCH id=… ch=…`. Later purging of the idle instance (`MAX_MOTION_INSTANCES`) does not evict the keyframe cache, so residency persists. §9 widened to allow `createMotion`. | 3.5, 9 |
| P2 COLD during a cross-fade | A pending cross-fade removal is cancelled on a mode change; COLD re-issues only the post-switch set (the incoming stand), never the outgoing one. | 3.1, 3.5 |
| P2 presence gating after seal | `autoAnimGated(source)` is independent of the recorder and runs whenever `mAutoAnim` exists (learning or sealed), so a ground-sit without AO never false-hands-off. | 3.2 |
| P2 buffer semantics | A folded stop whose start was discarded (or never observed) yields no dwell sample and no replacement edge. | 3.3 |
| P2 remote AO hover | Speed gate uses **3D** `getVelocity().length() > 0.3 m/s` (take-off/landing gate; buffered stand/hover events are discarded), so a hover becomes an edgeless singleton; cold picks never choose an edgeless singleton unless its layer has no other state. Residual limit: a hover plays only if it was playing at handoff. | 3.3, 3.5 |
| P2 documentation items | §3.9a: dance-HUD-after-stand-stop joins the stand group; offscreen clones' clocks may stall (INFERENCE); freeze strips of a MANUAL clone snapshot the live source (HEAD strip semantics). | 3.9a |

## 0b. R2 resolution table (v3 delta)

| R2 item | v3 resolution | § |
|---|---|---|
| P1-1 animesh schedulers freeze in TRUE_MIRROR (body controller paused `llghostavatar.cpp:659`; paused time frozen `llmotioncontroller.cpp:881-884`); switchboard sets time factor without restore (`aldirectoranimswitcher.cpp:398`) | **Per-channel clock.** Body = ghost controller `getAnimTime()`. Each animesh channel = its control avatar's controller `getAnimTime()` + a per-channel offset, rebased for continuity when the control-avatar id changes (pattern `:2316-2324`); no control avatar ⇒ that channel's clock does not advance. Switchboard speed changes simply scale the clocks (pre-existing, not restored by us; documented). | 3.3, 3.5 |
| P1-2 sit poses learned as stands; remote `isSitting()` may lag | GATE (not just exclusion) on signaled `ANIM_AGENT_SIT, SIT_FEMALE, SIT_GENERIC, SIT_GROUND, SIT_GROUND_CONSTRAINED, SIT_TO_STAND, STANDUP` (`llanimationstates.h:146-158`). **1.0 s event buffer:** events are held 1.0 s (real time) before being folded; if any gate engages while they are buffered, the whole buffer is discarded. | 3.3 |
| P1-3 body loss inside `if (source && !source->isDead())` (`:2472`) never counts logout/TP; `simloss` bypass | Absence is evaluated **before** that check, from the resolved pointer. `simloss` sets the resolved `source` pointer to `nullptr` before the HEAD check (only when `mAutoAnim` is non-null), so the real null-source branch is exercised. | 3.2 |
| P1-4 copy ordering; freeze strips copy MANUAL | Share/drop rule placed **between `:779` and `applyEntityRuntimeState` at `:796-799`**. Freeze strips: in `updateFreezeStrips`, right after `duplicateInstanceInPlace` (`alghoststudio.cpp:3382`) and before the pause at `:3396-3397`, clear `mAutoAnimManual`, reset `mAutoAnimPool`, and call `setInstanceAutoAnimate(snap_id,false)` (no `idleUpdate` has run, so no picks were issued). | 3.6 |
| P2 snap on stand switch | Body switch removal no longer goes through `sync` (immediate stop, `:552`): the scheduler calls `LLCharacter::stopMotion(prev, false)` (ease-out) and erases `prev` from `mClonePlayingAnimations` and its own desired map directly. Animesh removals already ease out (stock stop path `llvoavatar.cpp:7149` + ~63 lines calls `stopMotion(anim_id)` without immediate). The "immediate swap" branch is deleted: always add next, then ease-out prev after `max(ease_in(next),0.2 s)`. | 3.5 |
| P2 gap-style AOs | Replacement edge = X stops and the **next same-layer start** Y comes within `REPLACE_GAP = 2.0 s` with no other same-layer start between; or Y starts up to `OVERLAP_TOL = 0.5 s` before X stops. | 3.4 |
| P2 `mInAir` heuristic can stall learning | `mInAir` removed from the gate. In-air = signaled `FLY, FLYSLOW, HOVER, HOVER_UP, HOVER_DOWN, PRE_JUMP, JUMP, LAND, MEDIUM_LAND, FALLDOWN` (`llanimationstates.h:97-130`), plus `gAgent.getFlying()` for a self source. Honest limit: an AO-overridden hover on a REMOTE source (no built-in id, ~0 velocity) can be learned. | 3.3 |
| P2 ground-sit without AO looks like loss | A gated source is **present** (not absent) even with an empty filtered set. | 3.2 |
| P2 early Go autonomous unreachable | Enabled when the live recorder has ≥1 entry OR a pool exists. The runtime takes its own snapshot inside `idleUpdate` and binds the schedulers to it; the instance receives it via publish one frame later. | 3.3, 4 |
| P2 `LLGhostAvatar::` types in Studio API | `Action`, `Config`, `Status` live in namespace `ALGhostAutoAnim` (model header). `alghoststudio.h` includes only `alghostautoanimmodel.h` (llcommon-only) and keeps forward-declaring `LLGhostAvatar` (`:66`). | 5.1-5.3 |
| P2 `Pool` sealed flag | `bool mSealed` is an explicit `Pool` member, serialized; loaded pools are sealed. | 3.4 |
| P2 hook ordering | `const S32 old_mode = mEntityDriveMode;` before `:613`; `autoAnimOnDriveModeChanged(old_mode, mode)` after the existing body of `setEntityDriveMode` (end of function). | 3.1 |
| P2 window default | `Instance::mAutoAnimWindow = 0` means "use `GhostAutoAnimateWindow`"; the spinner shows the effective value and writes an explicit value. | 3.3 |
| P2 Load on an autonomous clone | Loaded (sealed) pool replaces the runtime pool; any running learning stops; each autonomous channel re-adopts its CURRENT set against the new pool (adoption rule §3.5, nothing stopped, `HANDOFF kind=load`); non-autonomous channels unaffected. | 3.8 |
| P2 null attachment item id | Key = (point, item, prims, ordinal) always; when `item` is null, matching on Load/Refresh uses (point, prims, ordinal) and logs `keyweak`. | 3.6 |
| P2 teardown mid-learn | Recorder publishes an unsealed checkpoint every 30 s of observed time (plus at every seal/handoff/Refresh). Teardown loses at most the last 30 s; documented. `markDead` is not edited. | 3.6 |
| P2 unit tests need `BUILD_TESTING` | Codex writes them; Claude configures a `-DBUILD_TESTING=ON` build and runs `ctest` for this target at the single build after review converges. | 10 |
| P2 OFF-LIMITS gaps | Extended (§9). | 9 |

## 0. R1 resolution table

| R1 item | v2 resolution | § |
|---|---|---|
| A1 enum: `DRIVE_TRUE_MIRROR`=3 (`alghoststudio.h:107-110`, combo value 3 `panel_ghost_studio.xml:223`) | **No enum change at all.** AutoAnimate is a per-channel latch inside MIRROR. | 3.1 |
| A2-A4, A10 stale anchors | All anchors re-verified at HEAD (§2). Animesh loop runs in MIRROR+TRUE_MIRROR (`llghostavatar.cpp:2534-2535`). | 2 |
| A5 no grace timer exists | New per-channel real-time grace timer (`GhostAutoAnimateLossGrace`, 1.5 s) + 0.5 s recouple debounce. | 3.2 |
| A6 MIRROR exit also releases pauses / `resetHeldAnimeshRepeat()` | No new MIRROR-exit branch; existing exits untouched. Latch survives; re-entry = COLD restart, logged. | 3.1 |
| A7 clamp to TRUE_MIRROR (`:581-582`) | Clamp untouched (no new value exists). | 3.1 |
| A8 pause anchors | `setInstancePaused` `alghoststudio.cpp:1148-1179` round-trips MIRROR; latch lives on ghost + instance. | 3.1 |
| A9 cache claim + `purgeExcessMotions` (MAX 32, `llmotioncontroller.cpp:42`) | Residency = `LLKeyframeDataCache::getKeyframeData(id)!=NULL` (`llkeyframemotion.h:600`); never `findMotion`. | 3.3, 3.5 |
| A11 "Follow live" is overlay un-freeze (`btn_live`) | New action is named **Follow source**; `btn_live` untouched. | 5.4 |
| A12 no re-source path | v1 retarget rule deleted. After a Refresh (new outfit/AO) the pool is kept; user decides Relearn. | 3.6 |
| A13 (new) Ghost settings live in `app_settings/settings.xml` (next to `:7107`), not `settings_alchemy.xml` | Settings go in `settings.xml`. | 5.7 |
| P1-1 enum collision | Moot (no mode). | 3.1 |
| P1-2 state model / switchboard / desync | Per-channel `Autonomy` latch (body flag + `ClonedLinkset::mAutonomy`); MIRROR body block, MIRROR/TRUE_MIRROR animesh loop and `repeatHeldAnimesh()` skip autonomous channels. Switchboard/chat/pause interactions fully tabled. | 3.1 |
| P1-3 recouple defeats Detach | Recouple only after AUTOMATIC loss, on a debounced absent→present edge. MANUAL ("Go autonomous") is only cleared by **Follow source** / toggle-off / Relearn. | 3.1, 3.2 |
| P1-4 hold OFF breaks handoff | Loss = source empty OR missing; an AutoAnimate clone forces hold semantics on all its channels regardless of `GhostMirrorHoldOnSourceChange`. | 3.2 |
| P1-5 TRUE_MIRROR | Learning works (records source maps). Animesh channels hand off seamlessly (ledger path). Body: no automatic handoff (keeps TRUE_MIRROR's hold-last-stamp contract); manual Go autonomous switches the instance to MIRROR and logs verdict `COLD` (documented non-seamless). | 3.7 |
| P1-6 gates / exclusions | Gate on sit / in-air / horizontal speed / walk+fly built-ins; exclude every `gAnimLibrary` id except `AGENT_STAND_ANIMS`; learn only cached keyframe assets. | 3.3 |
| P1-7 grouping wrong; record from source | Recorder reads SOURCE `mSignaledAnimations` / source prim entries. Layers by joint coverage; groups = union-find on replacement edges with overlap tolerance, blocked by coexistence evidence. AO seq-id re-asserts ignored for loops. | 3.3, 3.4 |
| P1-8 pool lifetime / keying / teardown | Published pool = `shared_ptr<const Pool>` on `Instance`; recorder on runtime; Refresh transfers progress; entity copies share only SEALED pools; animesh keyed by (attach point, source item id, prim count, ordinal); region-teardown limit stated. | 3.6 |
| C1 one-shots ≥3 obs | `MIN_ONESHOT_OBS = 3`; sounds never replayed (only animation ids exist here). | 3.4 |
| C2 censor open dwell; THIN vs switches | Open intervals at seal/gate are censored (not samples). THIN tied to observed switch count per layer. | 3.4, 6 |
| C3 own sequence ids | Scheduler mints strictly decreasing negative seq ids per channel. | 3.5 |
| C4 equal priority / snap on switch | Scheduler switches cross-fade (start next, ease-out stop previous after next's ease-in) — v3 applies this for every priority pair (§0b). | 3.5 |
| C5 "repeatable take" false | RNG = splitmix64(instance id, seed salt, channel key) + decision counter. Wording: *deterministic choice sequence, not frame-exact repeatable*. | 3.5 |
| C6 animesh recouple after re-rez never matches | Stated: stays autonomous until **Refresh**; Refresh rebinds by key and re-applies MANUAL. | 3.6 |
| C7 POP only our stops; EMPTY-ADOPT | Verdicts `ok / POP / EMPTY-ADOPT / COLD / NOPOOL`; POP counts only ids OUR code stopped in the handoff frame + 1. | 6 |
| C8 "Detach" clash | Renamed **Go autonomous**. | 4 |
| C9 `/ghostanim` parse | Single-token verbs matched before the UUID fallback. | 5.5 |
| C10 Director needs runtime→instance lookup | `ALGhostStudio::findInstanceByRuntime()`; cast menu items with enable callbacks. The Studio panel itself is already hosted in the Director (`floater_director.xml:552-573`). | 5.6 |
| C11 overlay convert lifecycle | No conversion path. Overlays show the control **disabled** with an explicit tooltip (never a silent no-op). | 4 |
| C12 untrusted pool files | Versioned LLSD, 1 MB cap, count caps, finite/range checks, masks sanitized, groups recomputed. | 3.8 |
| C13 cost | Budget kept; measured by `AUTOANIM-COST`. Per-frame recorder cost is one map compare per channel (stated honestly, not "zero"). | 3.9 |

## 1. The ask (authoritative, unchanged from v1 §1)
Per-clone **toggle**. While mirroring, the clone **learns** the source's performance. When the source goes away
(or the user triggers it) the clone keeps performing from what it learned — **no scripts**. **Not** an AO
locomotion state machine (no walk/run/turn mapping, no velocity-driven playback, no Actor Mover integration).
Must handle the **body/bento skeleton** and **every cloned animesh linkset independently**. Low overhead.
**One recorder per entity** (user decision 2026-09-28). Time-capped learn window with **Relearn / Stop now**.
Save/load. Director reachability. Chat command.

## 2. What HEAD proves (PROVEN = read at `96aa1197555`; INFERENCE labelled)

| Fact | Where |
|---|---|
| `EDriveMode {MIRROR=0, DIRECTED, FROZEN, TRUE_MIRROR}`; per-instance `mDriveMode/mResumeDriveMode` | `alghoststudio.h:107-110`, `:365-372` |
| Body mirror: copy source `mSignaledAnimations` minus ground-sit; sync only if (hold off OR non-empty) AND different | `llghostavatar.cpp:2467-2494` |
| `synchronizeCloneAnimations` = pure ledger diff; stop = `stopMotion(id,true)`; restart on seq change | `llghostavatar.cpp:533-577` |
| `setEntityDriveMode`: clamp `:581-582`; `resetHeldAnimeshRepeat()` `:591`; release pauses leaving FROZEN/TRUE_MIRROR `:596-601`; leaving MIRROR = `sync(empty)` `:607-611`; entering DIRECTED also `clearClonedObjectAnimations()` `:616-620`; TRUE_MIRROR pauses wearer controller `:640-662` | `llghostavatar.cpp:579-663` |
| Animesh loop in MIRROR+TRUE_MIRROR; source-gone latch (hold-gated) `:2551-2572`; detach-hold latch in `mirror_one` `:2577-2617`; publishes `mAnimeshHeld` `:2642-2654`; `updateControlAvatar()` on change `:2656-2659` | `llghostavatar.cpp:2527-2661` |
| `repeatHeldAnimesh()` per-linkset loop | `llghostavatar.cpp:2283-2436` (loop `:2301`) |
| `ClonedLinkset` record; built in `cloneAttachmentsFrom` | `llghostavatar.h:253-285`; `llghostavatar.cpp:1985-2010` (attach state read `:1779`) |
| Region teardown: `markDead()` → `releaseClonedAttachments()` | `llghostavatar.cpp:2240-2247`, `:2036-2075` |
| `restartEntityAnimation` re-syncs `mCloneDesiredAnimations` | `llghostavatar.cpp:890-920` |
| Control avatar merges all prims' entries, **larger seq id wins** | `llcontrolavatar.cpp:599-657` (`:631-640`) |
| Seq-id change on a signaled id ⇒ `startMotion` | `llvoavatar.cpp:7041` (`processAnimationStateChanges`, compare `~:7110`) |
| Controller anim time does not advance while paused | `llmotioncontroller.cpp:881-884` |
| `JointMotionList` is public: `mLoop, mDuration, mEaseInDuration, mBasePriority, mJointMotionArray[i]->mJointName/mUsage` | `llkeyframemotion.h:507-551`; cache API `:589-607`; getter returns NULL on miss `llkeyframemotion.cpp:3232-3240` |
| Cache evictions only: local anims `lllocalanim.cpp:72,132,281,442`, BVH preview `llfloaterbvhpreview.cpp:1011`, shutdown `llappviewer.cpp:1850` | — |
| Built-in motions list; walk/stand arrays | `llvoavatar.cpp:1269-1305`; `llanimationstates.cpp:176,188`; `animStateToString` NULL for non-library ids `:352-364` |
| `mInAir` public `llvoavatar.h:1109`; `isSitting()` `:1171`; `mSignaledAnimations` `:1077` | — |
| Switchboard writes ghost drive mode directly, bypassing the instance | `aldirectoranimswitcher.cpp:313-314, 344-352, 373-374, 400-405, 559` |
| Instance copied by value; ALL entity copies go through `duplicateInstanceSnapshotInPlace` (`*copy = proto` `:779`) | `alghoststudio.cpp:754-802` (callers `:751, :840, :4381`; strips `:3381`) |
| Refresh builds a new runtime; `onEntityRuntimeReplaced` → `applyEntityRuntimeState` | `alghoststudio.cpp:640-722`, `:406-445`, `:372-404` |
| Runtime creation needs a live source | `createEntityRuntime` `alghoststudio.cpp:342-370` |
| `seeded_unit` is a stateless FNV hash of the instance id | `alghoststudio.cpp:159-170` |

INFERENCE (cannot be proven in-repo): the joint-blend result during a body COLD start (pose eases from whatever
the joints hold; may visibly settle) — hence COLD is logged as a distinct, non-seamless verdict.

## 3. Design

### 3.1 State model — no new drive mode
Each **channel** (the body; each `ClonedLinkset`) carries `enum Autonomy : U8 { AUTONOMY_NONE=0, AUTONOMY_AUTO_LOST, AUTONOMY_MANUAL }`.
- `NONE`: HEAD behaviour (live mirror + HEAD holds). Recorder may be learning.
- `AUTO_LOST`: source lost for ≥ grace; channel plays learned performance; recouples automatically (§3.2).
- `MANUAL`: user pressed **Go autonomous**; never undone by recouple.
- The body block (`:2467-2494`) runs only for `NONE`; for an autonomous body the scheduler owns the ledger.
  The animesh loop (`:2537`) and `repeatHeldAnimesh()` (`:2301`) `continue` for autonomous linksets (the
  scheduler step for that linkset runs instead, §5.2).
- Scheduler ticks only when `mEntityDriveMode == MIRROR` (body) / `MIRROR || TRUE_MIRROR` (animesh) — exactly
  where HEAD's mirror would have run. In DIRECTED/FROZEN latches are **dormant**, not cleared.

| Event | Body channel | Animesh channel |
|---|---|---|
| Grace expires while `NONE` (§3.2) | → `AUTO_LOST`, adopt (MIRROR only) | → `AUTO_LOST`, adopt |
| **Go autonomous** | → `MANUAL`, adopt; seals learning | → `MANUAL` (with pool entry: adopt; without: hold-only, verdict `NOPOOL`) |
| Debounced present edge, `AUTO_LOST`, `GhostAutoAnimateRecouple` | → `NONE`; mirror diff same frame; `RECOUPLE` | same |
| Present edge while `MANUAL` | no change | no change |
| **Follow source** / toggle OFF / Relearn | → `NONE` | → `NONE` |
| Ghost mode leaves MIRROR (Pause, Studio combo, `/ghostanim`, switchboard FROZEN/DIRECTED) | latch kept; HEAD `sync(empty)` runs (`:607-611`) → mark `mColdPending` | FROZEN: entries untouched, nothing pending; DIRECTED: HEAD clears entries (`:619`) → `mColdPending` |
| Ghost mode returns to MIRROR (Resume, switchboard release `:313/:405/:559`) | if autonomous & `mColdPending`: re-issue current picks with fresh seq ids, verdict `COLD` | same rule |
| Runtime replaced (Refresh) | instance `mAutoAnimManual` re-applied → all channels `MANUAL`, `COLD` | keyed rebind (§3.6) |

Drive-mode edge detection: `const S32 old_mode = mEntityDriveMode;` inserted immediately before
`mEntityDriveMode = mode;` (`:613`), and one call `autoAnimOnDriveModeChanged(old_mode, mode)` at the end of
`setEntityDriveMode` (after `:662`); it returns at once when `mAutoAnim` is null and otherwise only sets
`mColdPending` flags. Nothing else in
`setEntityDriveMode` changes. Pause/Resume therefore round-trips as MIRROR with the latch intact; the switchboard
needs **no edits** (its direct writes are handled by the edge hook). Known pre-existing quirk kept: `/ghostanim freeze`
does not save `mResumeDriveMode` — harmless here because the latch is not a drive mode.

### 3.2 Loss, grace, forced hold
- **Effective hold:** `hold_eff = hold_on_source_change || mAutoAnim != nullptr`. Replace the two uses of the static
  in the body condition (`:2488`) and animesh (`:2558`, lambda `:2599`) with `hold_eff` (captured by value into
  `mirror_one`). With AutoAnimate off this is byte-identical to HEAD.
- **Placement (body):** in the MIRROR branch, after `source` is resolved (`:2469-2470`) and **before**
  `if (source && !source->isDead())` (`:2471`): `if (mAutoAnim && mAutoAnim->mSimLoss) source = nullptr;` then
  `autoAnimBodyPresence(source)` computes absence. Absence bookkeeping must never live inside the `:2471` block.
- **Body absent** := `source` null (incl. sim-loss) OR dead OR (filtered mirrored set empty AND source not gated).
  A gated source (sitting, sit/fly ids, §3.3) is **present** even with an empty filtered set (ground-sit without AO).
  `autoAnimGated(source)` is a runtime helper independent of the recorder; it runs whenever `mAutoAnim` exists
  (learning or sealed).
  **Animesh absent** := source root missing/dead (sim-loss forces `source_gone = true` at `:2557-2563`) OR no source
  prim of the linkset has a non-empty entry (track `any_source_nonempty` inside `mirror_one`), unless the owning
  source avatar is gated. (Children with no entry are normal; do not use `mAnimeshHeld`.)
- Per channel: `F64 mAbsentSince=-1, mPresentSince=-1` on `LLFrameTimer::getTotalSeconds()` (real time).
  Handoff when absent continuously ≥ `GhostAutoAnimateLossGrace` (default 1.5 s, clamp 0.25–10). During grace the
  forced hold keeps the last set (no pop). Recouple after present continuously ≥ `RECOUPLE_DEBOUNCE` = 0.5 s.
- A source whose script deliberately stops all animesh anims for > grace also hands off, and recouples on its next
  non-empty signal — consistent with "keeps performing".
- No pool entries for the channel at handoff: stay HELD (HEAD behaviour), log verdict `NOPOOL` once.

### 3.3 Recorder — per entity, per channel, from the SOURCE maps
- Lives on the runtime (`LLGhostAvatar::mAutoAnim->mRecorder`), exists only while learning. Runs in MIRROR **and**
  TRUE_MIRROR at the top of `idleUpdate` (after the `hold_on_source_change` static, before `:2467`), reading
  `source->mSignaledAnimations` (body, minus both ground-sits) and `object_anims[source prim id]` (animesh). Immune to
  clone-side restarts/pauses/switchboard by construction.
- **Change detection:** body keeps `mLastSeen` (copy of the filtered map); per frame one `operator==`; on inequality
  diff → events. Animesh keeps `std::vector<signaled_animation_map_t> mLastSeenPerPrim` parallel to (root, children)
  plus a per-linkset refcount map; only prims whose entry differs are diffed (no per-frame allocation when unchanged).
- **Events:** id appears → `onStart`; disappears → `onStop`; seq change on a still-present id → `onRetrigger`, which
  the model ignores for entries whose facts say loop/state (AO re-asserts, `:540-546`) and counts as a new arrival
  for one-shots; unknown facts → ignored.
- **Exclusions:** ground-sits; any id with `gAnimLibrary.animStateToString(id) != NULL` unless in `AGENT_STAND_ANIMS`
  (so walk/run/fly/type/away/busy/aim/LLEmote express_* are never learned; express_* are procedural LLEmote and
  never in the keyframe cache anyway).
- **Facts:** `facts_for(id)` reads `LLKeyframeDataCache::getKeyframeData(id)` once per start; copies loop,
  duration, ease-in, base priority and a layer (§3.4); **never retains the pointer** (local anims can evict). Miss ⇒
  id goes to a pending list (cap 16) retried on the next event and at seal; still missing at seal ⇒ dropped,
  `AUTOANIM-SKIP <id> uncached`.
- **Gates** (whole entity, both channels; evaluated on the UNFILTERED source set): source `isSitting()` OR
  3D `getVelocity().length()` > 0.3 m/s (catches take-off/landing of an AO hover) OR (self source) `gAgent.getFlying()` OR signaled contains any
  `AGENT_WALK_ANIMS` (`llanimationstates.cpp:176`), any sit id `SIT, SIT_FEMALE, SIT_GENERIC, SIT_GROUND,
  SIT_GROUND_CONSTRAINED, SIT_TO_STAND, STANDUP` (`llanimationstates.h:146-158`), or any air id `FLY, FLYSLOW, HOVER,
  HOVER_UP, HOVER_DOWN, PRE_JUMP, JUMP, LAND, MEDIUM_LAND, FALLDOWN` (`:97-130`). `mInAir` is **not** used
  (foot-to-ground heuristic, can go stale). Limit: an AO-overridden hover on a remote source can be learned.
- **1.0 s fold buffer:** diffs produce events into a small ring (cap 32, reserved) stamped with real time; an event
  is folded into the pool only once it is ≥ 1.0 s old. If a gate engages while events are buffered, the entire
  buffer is discarded (covers a remote `isSitting()` that lands after the furniture anim — INFERENCE). Ring overflow
  ⇒ oldest folded early (logged once). A seal folds only events ≥ 1 s old and discards the rest. A folded stop
  whose start was discarded or never observed yields **no** dwell sample and **no** replacement edge.
  Gate entry: open intervals become censored. Gate exit + 1.0 s settle: re-baseline `mLastSeen` with **no** start
  events (their start times are unknown). Log `AUTOANIM-GATE on|off reason=sit|speed|loco|air|flying`.
- **Window:** `observed += dt` only while source present AND ungated. Seal when `observed ≥ window` (instance
  `mAutoAnimWindow`, where 0 = use `GhostAutoAnimateWindow` (default 120 s); explicit values clamp 15–600), on
  **Stop now**, or on **Go autonomous**. Automatic loss before seal: snapshot (unsealed) for playback, recorder
  pauses, resumes after recouple. **Checkpoint:** an unsealed snapshot is also published every 30 s of observed time.
- **Snapshot binding:** every handoff (auto, manual, load) is executed inside `idleUpdate`; the runtime takes its
  own snapshot there and binds the schedulers to it immediately. The instance receives the same `shared_ptr` via
  publish/pull a frame later — never the source of truth for the scheduler.
- **Clocks.** Recording: real seconds. Playback is **per channel**: body = ghost `getMotionController().getAnimTime()`;
  each animesh channel = its linkset's control avatar `getMotionController().getAnimTime()` + `mClockOffset`. When the
  control-avatar id differs from `mClockCavId` (first sight / lazy re-creation, same test as `:2316-2324`),
  `mClockOffset = lastChannelTime − newCavTime` so the channel clock is continuous; no control avatar ⇒ the channel
  does not tick. Consequences: TRUE_MIRROR (body paused `:659`, control avatars not paused) keeps animesh schedulers
  running; FROZEN pauses both (`:626-638`); Studio / switchboard speed (`aldirectoranimswitcher.cpp:398`, never restored
  by the switchboard — pre-existing) scales each clock.

### 3.4 Pool model (pure, unit-tested: `alghostautoanimmodel.*`)
- `Entry {LLUUID id; bool loop_asset; F32 dur, ease_in; S32 prio; U8 layer; U32 starts; Reservoir dwell, inter;
  U64 co_mask; U16 fires_without_state; F32 max_censored_dwell; bool state /*derived*/}`; `Reservoir` = 16 floats +
  count + seen (deterministic replacement index from a hash of `seen`). Cap entries `GhostAutoAnimateMaxAnims`
  (default 64, clamp 8–64; masks are U64). Edges `{U8 a,b; U16 replace, coexist}` cap 256 per channel.
- **Layer** (from joints with `mUsage != 0`): count per region — CORE (`mPelvis, mTorso, mChest, mSpine*, mHip*,
  mKnee*, mAnkle*, mFoot*, mToe*`), UPPER (`mNeck, mHead, mSkull, mCollar*, mShoulder*, mElbow*, mWrist*`), HANDS
  (`mHand*`), FACE (`mFace*, mEye*`), EXTRA (anything else: tail, wings, hind limbs, collision volumes). CORE>0 ⇒
  `LAYER_BODY`; else argmax with tie order FACE > HANDS > UPPER > EXTRA. Priority is **not** part of the layer key
  (deviation from R1, reasoned: real AO stand sets mix P3/P4 and would fragment; priority is used for the cross-fade
  rule instead). A face or hand loop can therefore never become an alternative to a body stand.
- **Classification:** `state = loop_asset || median(dwell) > dur + 1.0 s` (held beyond natural end ⇒ AO/HUD-owned);
  otherwise one-shot. One-shots with `starts < 3` are never scheduled.
- **Replacement edges** (same channel, same layer): (a) gap — X stops and the **next** same-layer start Y occurs
  within `REPLACE_GAP = 2.0 s` with no other same-layer start in between; or (b) overlap — Y starts ≤ `OVERLAP_TOL =
  0.5 s` before X stops. Either ⇒ `replace++`. X and Y co-active continuously > 0.5 s ⇒ `coexist++` (an overlap
  ≤ 0.5 s is a transition, not coexistence). `switches` = number of replacement events in the channel.
- **`Pool`** = `{U32 mVersion=1; bool mSealed=false; F32 mObservedSeconds; ChannelPool mBody;
  std::vector<std::pair<LinksetKey, ChannelPool>> mAnimesh;}` — `mSealed` explicit and serialized.
- **Groups** (at snapshot/load, deterministic): per layer, sort edges by `replace` desc then (a,b); union X,Y unless
  any member pair across the two components has `coexist > 0`. State entries with no edge are singleton groups
  (held, never switched).
- **Co-occurrence:** at each one-shot start OR bit of active state entries into `co_mask`; if none active,
  `fires_without_state++`.
- **Censoring:** dwell samples only from observed start→stop pairs; intervals open at seal/gate/pause record only
  `max_censored_dwell`.
- **THIN** (per channel): any layer with ≥1 state entry and 0 replacement edges while the channel has > 1 state in
  that layer, OR `switches < 2`. Message says the AO timer is probably longer than the window.

### 3.5 Scheduler / playback
- **Adoption** (handoff): current set = body `mCloneDesiredAnimations`; animesh = union of clone prim entries.
  Kept byte-for-byte (same ids, same seq ids): nothing is started or stopped on the handoff frame. For each layer,
  the adopted state entry (pool member, or unknown id whose cached facts put it in a layer that has pool states) is
  "owned" and gets first dwell = max(`MIN_FIRST_DWELL` 2 s, draw). Ids with no facts, or in layers without pool
  states, are **foreign** and never touched. Empty adopted set ⇒ immediate cold pick per layer, verdict `EMPTY-ADOPT`.
- **Dwell draw:** from the entry's dwell reservoir × U[0.85,1.15]; empty reservoir ⇒ max(`max_censored_dwell`×1.25,
  `FALLBACK_DWELL` 30 s).
- **Switch** at dwell end: next = other member of the same group, weighted by `starts`, never the same id if another
  exists and is resident. **If there is no other resident member, or next == cur: redraw cur's dwell and change
  nothing else (M1).** Otherwise: add next now, remove cur at now + max(ease_in(next), 0.2 s) (cross-fade; equal
  priority ⇒ newest wins, `llpose.cpp:213-217`). **Removal eases out:** body — `LLCharacter::stopMotion(cur, false)`
  and erase `cur` from `mClonePlayingAnimations` and the scheduler's desired map directly, then assign
  `mCloneDesiredAnimations = desired` (so `synchronizeCloneAnimations`'s immediate stop `:552` never sees it);
  animesh — erase from the prim entries; the stock stop path (`llvoavatar.cpp` `processSingleAnimationStateChange`,
  `:7149` + ~63) calls `stopMotion(anim_id)` non-immediate. A higher-priority outgoing stand therefore fades over its
  ease-out instead of snapping. Singleton group ⇒ keep.
- **One-shots** (≥3 starts): next fire = now + inter-arrival draw × U[0.8,1.2]; fire only if
  `fires_without_state>0 || (active_state_mask & co_mask)`; otherwise re-check in 2 s.
- **COLD during a cross-fade:** `markCold` cancels a pending removal; the re-issue set is the post-switch set only.
- **Cold picks** choose among state entries that have >= 1 replacement edge; an edgeless singleton is picked only
  if its layer has no other state (it may still continue if it was the adopted id).
- **Sequence ids:** per channel `S32 mNextSeq = -1`, decrement per issue (never 0/positive; wraps are unreachable).
- **Residency:** before issuing id: `LLKeyframeDataCache::getKeyframeData(id) != NULL`; else skip this pick (try
  another member; retry in 5 s), `AUTOANIM-SKIP <id> unloaded` rate-limited once per id per 60 s.
- **Prefetch (M2):** at every bind (handoff, Load, Refresh rebind, COLD) each uncached pool id gets one
  `createMotion(id)` on the channel's character (ghost / control avatar; no control avatar yet => done when the
  channel clock first sees one). Created stopped => fetched, never played. Logs `AUTOANIM-FETCH`.
- **Writes** — body: scheduler keeps its full desired map; additions / re-fires go through
  `synchronizeCloneAnimations(desired)` (called only when it changed); removals as above. Animesh: `applyAnimeshDesired(linkset, desired)`: ids removed ⇒ erase from **every** clone prim entry;
  ids added/re-fired ⇒ erase from child entries first (the control avatar keeps the larger seq, `llcontrolavatar.cpp:635`),
  then write on the root entry; `root->updateControlAvatar()` once if anything changed. Foreign ids untouched.
  Finished one-shots stay in the desired map (harmless; re-fire = new seq id).
- **RNG:** `splitmix64(hash(instance id) ^ hash(channel key) ^ seed_salt)`, advanced per decision. Reseed = salt++.
  Deterministic choice sequence for a given handoff state; not frame-exact repeatable (timing/load dependent).
- Per frame per channel: one compare `now >= mNextEventTime`; work only when due.

### 3.6 Lifetime, keying, copies, limits
- `Instance` gains: `bool mAutoAnimate=false; F32 mAutoAnimWindow=0 /*0 = setting*/; bool mAutoAnimManual=false;
  U32 mAutoAnimSeedSalt=0; std::shared_ptr<const ALGhostAutoAnim::Pool> mAutoAnimPool;`.
- Runtime publishes snapshots (seal, handoff, 30 s checkpoint, Refresh export); `ALGhostStudio::updateAutoAnimate()` (called
  from `updatePerFrame()` right after `finishPendingRuntimeFreezes()` `:1728`) pulls them via
  `ghost->takeAutoAnimPublishedPool()` into `inst.mAutoAnimPool`.
- **Refresh:** at the top of `onEntityRuntimeReplaced` (`:414` area, before `applyEntityRuntimeState`), if the old
  runtime is alive and learning, call `old->exportAutoAnimProgress()` (unsealed snapshot) into the instance; the new
  runtime seeds its recorder from it and continues with `window − observed`. Channel keys rebind by `LinksetKey`.
- **Copies** (`duplicateInstanceSnapshotInPlace`): the rule runs **after `*copy = proto` (`:779`) and after the
  field fix-ups (`:780-796`), strictly before `applyEntityRuntimeState(*copy, ghost)` (`:797-800`)**: a SEALED pool is
  shared (crowds get one learned performance, each with its own seed); an UNSEALED pool is reset and, if
  `mAutoAnimate`, the copy's runtime opens its own fresh window (one recorder per entity). `mAutoAnimManual` is copied.
- **Freeze strips** (`updateFreezeStrips`): immediately after `Instance* snap = duplicateInstanceInPlace(...)`
  (`alghoststudio.cpp:3382`, null-checked `:3383`) and before `setInstancePaused(snap_id, true)` (`:3396-3397`):
  `snap->mAutoAnimManual = false; snap->mAutoAnimPool.reset(); setInstanceAutoAnimate(snap_id, false);` (re-acquire
  `snap` by id afterwards, as the surrounding code does). No `idleUpdate` has run on the new runtime, so no pick was
  issued and the strip freezes exactly as at HEAD.
- **LinksetKey** `{S32 point (ATTACHMENT_ID_FROM_STATE of src_root, :1779); LLUUID item (src_root->getAttachmentItemID());
  U32 prims (1+children); U32 ordinal (index among equal (point,item,prims) in clone order)}` — two new
  `ClonedLinkset` fields set at `:1985-1997`. Null `item` (non-self sources / temp attachments; INFERENCE): the key is
  still stored as-is; matching on Load/Refresh then uses (point, prims, ordinal) and logs `AUTOANIM-LOAD keyweak`.
- **Teardown mid-learn:** progress since the last 30 s checkpoint is lost (runtime dies in `markDead`, which is not
  edited). Documented, not engineered around.
- **Limit (honest):** an autonomous clone lives only as long as its runtime. Region teardown / teleport away kills
  it (`markDead` → `releaseClonedAttachments`); the instance becomes RECOVERABLE and recreation needs the source
  present (`:346-350`). Pool + MANUAL survive on the instance; if the source is gone for good the clone cannot be
  recreated. A re-rezzed source animesh (fresh UUIDs, stale `mSourceRoot` `llghostavatar.h:260`) never recouples;
  **Refresh** is the way back.

### 3.7 TRUE_MIRROR
Recorder runs (source maps). Animesh channels: identical to MIRROR (loop runs there; their clocks are their own
control avatars', which TRUE_MIRROR does not pause — §3.3). Body: no automatic handoff —
TRUE_MIRROR keeps its "hold last stamped pose" contract (`:704-707`). **Go autonomous** on a TRUE_MIRROR clone:
Studio first calls `setInstanceDriveMode(id, DRIVE_MIRROR)`, then latches MANUAL; body verdict `COLD` (the
controller was paused with an empty ledger, `:640-662`); tooltip and log say it is not seamless.

### 3.8 Persistence
LLSD XML via `LLFilePickerReplyThread` (precedent `alpanelflycamrecorder.cpp:174-180`, IO `llflycamrecorder.cpp:713-737`).
Schema: `{kind:"GhostAutoAnimPool", version:1, observed_s, body:Channel, animesh:[{key:{point,item,prims,ordinal},
channel:Channel}]}`; `Channel {observed_s, switches, entries:[{id,loop,dur,ease_in,prio,layer,starts,dwell[],inter[],
co_mask(16-hex string),no_state_fires,max_censored}], edges:[{a,b,replace,coexist}]}`. Load = untrusted: file
≤ 1 MB; kind/version exact; entries ≤ 64, animesh channels ≤ 32, edges ≤ 256, reservoirs ≤ 16; every float finite
and in [0, 3600]; ids non-null and unique; layer < 5; edge indices < entry count; masks `&=` valid bits; `state`
and groups recomputed. Any failure ⇒ reject whole file, `AUTOANIM-LOAD reject reason=…`. Loaded pool is SEALED;
loading turns AutoAnimate on; animesh channels apply by exact key (null-item fallback §3.6), else skipped with
`AUTOANIM-LOAD skip key=…`. **Load onto a running clone:** any learning stops (recorder destroyed); schedulers rebind
to the loaded pool inside the next `idleUpdate`; every autonomous channel re-adopts its CURRENT set under the §3.5
adoption rule (nothing stopped; `HANDOFF kind=load`); non-autonomous channels keep mirroring.

### 3.9 Overhead budget (unchanged intent, honest wording)
Off: one `mAutoAnim` null check per clone per frame + `hold_eff` boolean. Learning: one map compare per channel per
frame + event work on change (a few per second). Sealed: recorder destroyed (zero). Playback: one time compare per
channel per frame. No per-frame allocation. Memory ≤ ~13 KB per channel pool, shared between copies.
`AUTOANIM-COST rec=<µs/s> sched=<µs/s> learners=N players=M` once per 60 s while any is active
(`GhostAutoAnimateCostLog`, default on), measured with `std::chrono::steady_clock` accumulators.

### 3.9a Documented behaviours (not bugs)
- A body anim that starts within 2 s after a stand stops (e.g. a dance HUD after the AO drops its stand) forms a
  gap edge and joins the stand group: it is part of the learned performance. Relearn without the HUD to avoid it.
- INFERENCE: a clone / control avatar that is not motion-updated while offscreen may stall its channel clock; the
  schedule resumes when it is updated again.
- Freeze strips of a MANUAL-autonomous clone snapshot the live source (HEAD strip semantics), not the clone's pose.

## 4. UI (Ghost Studio Pose tab — hosted in both the floater and the Director `ghosts_tab`)
`panel_ghost_studio.xml`: insert a block at `top=252` in `pose_section` and shift **exactly these 16 elements**
by +90 (`lens_gaze_label, lens_gaze_check, btn_lens_gaze_selection, lens_gaze_torso, reset_lens_gaze_torso,
lens_gaze_target_mode, lens_gaze_cast_target, lens_gaze_head_eye, reset_lens_gaze_head_eye, lens_gaze_intensity,
reset_lens_gaze_intensity, lens_gaze_smoothing, reset_lens_gaze_smoothing, lens_gaze_status, rig_diagnostics_label,
rig_diagnostics`; `:262-290`); `pose_scroll_body` (`:204`) and `pose_section` (`:207`) height 700→790. Verify by
counting (CLAUDE rule 6).
- `top=252` text `autoanim_label` bold "AutoAnimate (learned performance)".
- `top=272`: check `autoanim_check` "AutoAnimate" (left 8, w 100); spinner `autoanim_window_spinner` "Learn s"
  (left 112, w 110, 15–600, inc 15, initial 120 — shows the effective value, setting when the instance holds 0); `reset_autoanim_window` ↺ (left 224, w 18); buttons
  `btn_autoanim_stop` "Stop now" (248, w 70), `btn_autoanim_relearn` "Relearn" (322, w 66), `btn_autoanim_reseed`
  "Reseed" (392, w 60).
- `top=298`: `btn_autoanim_go` "Go autonomous" (8, w 110), `btn_autoanim_follow` "Follow source" (122, w 100),
  `btn_autoanim_save` "Save..." (226, w 60), `btn_autoanim_load` "Load..." (290, w 60).
- `top=324`: text `autoanim_status` (h 16, w 468): `Learning 1:24 / 2:00` · `Paused: source sitting` ·
  `Learned: body 3 stands / 9 one-shots · 2 animesh` · `Autonomous (manual) · body+2 animesh` · `(dormant: Directed)`;
  per-channel detail + THIN reason in its tooltip.
- Enables: all entity-only. Overlay: `autoanim_check` disabled, tooltip "AutoAnimate needs an entity clone (overlays
  redraw the live source)". Stop now ⇔ learning. Go autonomous ⇔ on AND (live recorder entry count ≥ 1 OR pool has
  ≥ 1 entry) AND mode MIRROR or TRUE_MIRROR (tooltip notes TRUE_MIRROR COLD); `Status::mLiveEntries` carries the count. Follow source ⇔ any channel autonomous. Save ⇔ pool present.
- Toggle ON: sealed pool present ⇒ no learning ("Learned"); else opens a window. Toggle OFF: destroys the runtime
  state (latches → NONE, recorder dropped), clears `mAutoAnimManual`, **keeps** the pool. Relearn: Follow source +
  drop pool + new window. Window spinner edits the selected instances' `mAutoAnimWindow` (applies to a running window;
  `observed ≥ new` seals next tick). Add `{ "reset_autoanim_window", "autoanim_window_spinner" }` to the reset table
  (`alpanelghoststudio.cpp:596-619`).
- All handlers iterate `selectedInstances()` (group-expanding, as `onDriveModeCommit` `:2094-2116`).

## 5. Implementation map

### 5.1 New files (add to `indra/newview/CMakeLists.txt` sources near `:305-306`, headers near `:1164-1165`)
- `alghostautoanimmodel.h/.cpp` — namespace `ALGhostAutoAnim`: constants (§3.4/3.5), `AnimFacts`, `ELayer`,
  `classifyLayer(const std::vector<std::string>&)`, `Reservoir`, `Entry`, `Edge`, `ChannelPool` (+`finalize()` →
  state/groups/thin), `LinksetKey`, `Pool` (+`findAnimesh`), `ChannelRecorder` (`reset, seedFrom, onStart, onStop,
  onRetrigger, onGateEnter, onGateExit, addObserved, snapshot`, 1 s fold ring), `ChannelClock` (`sample(sourceTime,
  sourceId)` → continuous channel time, rebases on id change), `ChannelScheduler` (`bind, adopt, markCold, tick,
  activeStateMask`), `toLLSD/fromLLSD(…, std::string& reason)`, and the API types shared with the Studio:
  `enum class Action : U8 {GoAutonomous, FollowSource, StopLearning, Relearn, Reseed, SimLossToggle}`,
  `struct Config {bool mEnabled; LLUUID mInstanceId; U32 mSeedSalt; F32 mWindowSeconds /*resolved, never 0*/;
  std::shared_ptr<const Pool> mPool; bool mManual;}`, `struct Status {bool mOn, mLearning, mGated; F32 mObserved,
  mWindow; U32 mLiveEntries, mBodyStates, mBodyOneShots, mAnimeshChannels, mAnimeshAutonomous; bool mBodyAutonomous,
  mManual, mThin; std::string mDetail;}`. Depends on llcommon/LLSD only (facts and residency are injected by the
  caller) so it is unit-testable. `alghoststudio.h` includes this header and keeps forward-declaring `LLGhostAvatar`
  (`alghoststudio.h:66`); neither Studio nor panel headers include `llghostavatar.h`.
- `tests/alghostautoanimmodel_test.cpp` (TUT); add `alghostautoanimmodel.cpp` to `viewer_TEST_SOURCE_FILES`
  (`CMakeLists.txt:2607-2626`).

### 5.2 `llghostavatar.h/.cpp`
- Header public: `setEntityAutoAnimate(const ALGhostAutoAnim::Config&)`;
  `requestAutoAnimAction(ALGhostAutoAnim::Action)` (queued, applied inside next `idleUpdate` so all latch transitions
  and snapshot bindings happen there); `bool takeAutoAnimPublishedPool(std::shared_ptr<const ALGhostAutoAnim::Pool>&)`;
  `std::shared_ptr<const ALGhostAutoAnim::Pool> exportAutoAnimProgress()`; `void getAutoAnimStatus(ALGhostAutoAnim::Status&) const`.
- Private: `struct AutoAnimRuntime; std::unique_ptr<AutoAnimRuntime> mAutoAnim;` (defined in .cpp before
  `~LLGhostAvatar`); `ClonedLinkset` += `S32 mAttachPoint=0; LLUUID mSourceItemId; U8 mAutonomy=0;`
  (per-channel clock state `mClockCavId, mClockOffset, mLastChannelTime` lives in the runtime's per-linkset slot).
  Helpers: `facts_for`, `autoAnimExcluded`, `autoAnimGated`, `autoAnimRecordTick`, `autoAnimBodyPresence`,
  `autoAnimBodyStep`, `autoAnimLinksetStep`, `autoAnimChannelClock`, `applyAnimeshDesired`,
  `autoAnimOnDriveModeChanged`, `autoAnimRebindLinksets`.
- Edits: `:1985-1997` set the two key fields; `:2465-2466` add `hold_eff`; before `:2467` `if (mAutoAnim)
  autoAnimRecordTick(...)`; MIRROR branch: after `:2470` the sim-loss null-out + `autoAnimBodyPresence(source)`
  (§3.2, outside the `:2471` block); then if body autonomous ⇒ `autoAnimBodyStep` instead of the `:2471-2493` mirror,
  else HEAD code with `hold_eff`; animesh loop `:2537`: autonomous ⇒ `autoAnimLinksetStep`, `continue`; sim-loss
  forces `source_gone` (`:2557-2563`); `mirror_one` gets `hold_eff` and `any_source_nonempty` by capture; after the
  loop, loss bookkeeping for `NONE` linksets; `repeatHeldAnimesh` `:2301` loop: the single skip line
  `if (linkset.mAutonomy != 0) continue;`; `setEntityDriveMode`: `old_mode` capture before `:613` + hook call at end.
  `releaseClonedAttachments`/`markDead`: `mAutoAnim` schedulers for linksets are dropped with the records (no other
  change).

### 5.3 `alghoststudio.h/.cpp`
- `Instance` fields (§3.6). Public API: `setInstanceAutoAnimate(id,bool)`, `setInstanceAutoAnimWindow(id,F32)`,
  `autoAnimateAction(id, ALGhostAutoAnim::Action)` (Go autonomous also sets `mAutoAnimManual`, handles
  TRUE_MIRROR per §3.7; Follow/Relearn clear it), `saveAutoAnimate(id, path)`, `loadAutoAnimate(id, path)`,
  `getAutoAnimateStatus(id, Status&) const`, `const Instance* findInstanceByRuntime(const LLUUID&) const`.
- `applyEntityRuntimeState` (`:372-404`): after the drive-mode block, `ghost->setEntityAutoAnimate(cfg_from(inst))`.
- `onEntityRuntimeReplaced` (`:406`): progress export before `applyEntityRuntimeState` (§3.6).
- `duplicateInstanceSnapshotInPlace`: sealed-share / unsealed-drop rule between `:796` and `:797` (before
  `applyEntityRuntimeState`). `updateFreezeStrips`: strip clear between `:3383` and `:3396` (§3.6).
- `updatePerFrame` (`:1728`): `updateAutoAnimate()` (pull pools, emit COST).

### 5.4 Panel `alpanelghoststudio.h/.cpp`
Members/getChild beside `:283-285, :332-333`; callbacks beside `:437-439`; enables in `refreshDetail` beside
`:1219-1231`; status text set-if-changed beside `:1539-1542`; handlers after `:2116`; Save/Load via
`LLFilePickerReplyThread::startPicker(&cb, FFSAVE_XML, "ghost_performance.xml")` / `FFLOAD_XML`; callbacks are static,
capture the stable instance ids, re-resolve on reply (panel may be closed).

### 5.5 Chat `alchatcommand.cpp` `/ghostanim` (`:777-865`)
Before the drive-mode mapping (`:790`), match single-token verbs `autoon | autooff | autonomous | follow | relearn |
stoplearn | reseed | simloss` with the same `[all|selected]` scope/fallback rules, calling the §5.3 API; they never
reach `setInstanceDriveMode` or the UUID parse. Update both usage strings. `simloss` toggles the per-runtime
test flag (§6) and is documented as a test instrument.

### 5.6 Director cast menu
`menu_director_cast.xml`: after `clear_loco_anim` add separator + items `autoanim_toggle` "AutoAnimate (ghost)",
`autoanim_go` "Go autonomous", `autoanim_follow` "Follow source", each with `on_click` `Director.AutoAnim*` and
`on_enable` `Director.CastHasGhostEntity`. `llfloaterdirector.cpp:356-383`: register clicks in the existing
registrar and the enable in an `LLUICtrl::EnableCallbackRegistry::ScopedRegistrar`; handlers map
`selectedCastIds()` → `findInstanceByRuntime` → §5.3 API; toggle sets ON if any selected entity is OFF.

### 5.7 Settings (`app_settings/settings.xml`, after `GhostMirrorRepeatHeldAnimesh` `:7107-7118`)
`GhostAutoAnimateWindow` F32 120 · `GhostAutoAnimateLossGrace` F32 1.5 · `GhostAutoAnimateRecouple` Boolean 1 ·
`GhostAutoAnimateMaxAnims` S32 64 · `GhostAutoAnimateCostLog` Boolean 1. All Persist 1, `[GhostStudio]` comments;
code clamps every read.

## 6. Instrumentation (tag `GhostStudio`; the log states a verdict)
- `AUTOANIM-LEARN start|resume|seal inst=<id> rt=<rt> window=<s> observed=<s> reason=window|stopnow|manual|refresh`
- `AUTOANIM-GATE on|off reason=sit|air|flying|speed|loco discarded=<buffered events dropped>`
- `AUTOANIM-SEAL ch=<body|ap:<pt>/<item8>/<n>#<o>> states=S oneshots=O groups=G switches=K observed=<s>`
- `AUTOANIM-HANDOFF ch=… kind=auto|manual|load verdict=ok|POP|EMPTY-ADOPT|COLD|NOPOOL carried=N ours_stopped=M clock=body|cav:<id8> absent=null|dead|empty|root|signal`
  — POP ⇔ `ours_stopped>0`, counting only ids in the adoption snapshot that OUR code removed during the handoff frame
  and the next. Natural one-shot expiry is not ours. `EMPTY-ADOPT` ⇔ snapshot empty. `COLD` ⇔ re-entry after a mode
  change / runtime replacement (expected, not seamless). **`POP` on a `kind=manual` or `kind=auto` handoff = feature broken.**
- `AUTOANIM-RECOUPLE ch=… absent=<s>` · `AUTOANIM-POOL THIN ch=… reason=…` · `AUTOANIM-NODATA ch=…` (recorder never
  observed the source for that channel — an untrustworthy-measurement outcome, not a pool verdict).
- `AUTOANIM-SKIP <id> uncached|unloaded` (rate-limited) · `AUTOANIM-LOAD ok|reject|skip|keyweak …` ·
  `AUTOANIM-SIMLOSS on|off rt=…` · `AUTOANIM-CLOCK rebase ch=… cav=<id8>` (control-avatar re-creation) ·
  `AUTOANIM-CHECKPOINT observed=<s>` · `AUTOANIM-FETCH id=… ch=…`
- `AUTOANIM-COST …` (§3.9).

## 7. Off path must be inert (CLAUDE rule 1)
With `mAutoAnimate=false` (default): `mAutoAnim == nullptr`; `hold_eff == hold_on_source_change`; every new branch is
guarded by `mAutoAnim` or `mAutonomy != 0` (always 0); `setEntityDriveMode` hook returns immediately; no allocation;
drive-mode enum/clamp/combo/chat mapping unchanged; no GL/render state touched anywhere. Reviewer must prove this per
edited function.

## 8. What stays as-is (explicit)
Locomotion (Actor Mover drives a walking ghost exactly as for MIRROR today); sounds; gestures; `btn_live`/overlay freeze.

## 9. OFF-LIMITS (do not edit)
`indra/llcharacter/**` (read-only API use only), `llvoavatar.*`, `llcontrolavatar.*`, `llactormover.*`,
`aldirectoranimswitcher.*`, `lllocalanim.*`, all shaders/`pipeline.cpp`/render code, the TRUE_MIRROR stamp functions
(`llghostavatar.cpp:665-857`), `cloneAttachmentsFrom` except the two field assignments, `synchronizeCloneAnimations`,
`clearClonedObjectAnimations`, `restartEntityAnimation`, `EDriveMode`, the clamp, `drive_mode_combo` items, other
`/ghost*` commands, any existing setting's default, `settings_alchemy.xml`, the enve tree, `llviewermessage.cpp`,
`aoengine.*`, `alghostspawnengine.*`, the Director cast model (`lldirectorcast.*`), `floater_director.xml`,
`repeatHeldAnimesh`/`resetHeldAnimeshRepeat` beyond the single skip line, `setEntityAnimTimeFactor`, `markDead`,
`releaseClonedAttachments`, every existing panel / Director handler (add new ones only), and `panel_ghost_studio.xml`
beyond the 16 shifted elements, the two height changes and the new block. No simulator messages: only
`LLCharacter::startMotion/stopMotion/createMotion` on the ghost / its control avatars and the clone-prim entries of
`LLObjectSignaledAnimationMap`. No builds, no commits.

## 10. Tests
**Unit** (`tests/alghostautoanimmodel_test.cpp`): (1) A→B→C stands with 0.3 s overlaps ⇒ one BODY group; (2) face
loop only during A ⇒ FACE layer, not grouped with stands; (3) BODY loop co-active >0.5 s with stands ⇒ not grouped;
(4) one-shot ×2 not scheduled, ×3 scheduled; (5) seq-only change: ignored for loop, arrival for one-shot; (6) gated
events yield no samples, straddling dwell censored; (7) open dwell at seal censored; `switches<2` ⇒ THIN;
(8) same seed+events ⇒ same choices; other instance id ⇒ different; (9) adoption leaves the map identical after the
first tick; first switch ≥ 2 s; foreign ids never removed; (10) seq ids strictly decreasing negative; (11) LLSD round
trip; reject bad version / NaN / 65 entries / 17 samples / null id / edge out of range; mask sanitized;
(12) `classifyLayer` table; (13) cross-fade emits add-next then (ease-out) remove-prev after ease-in, also when
next has lower priority; (14) gap AO: X stop, 1.5 s gap, Y start ⇒ edge; X stop, Z start (other layer) then Y at
1.5 s ⇒ edge; X stop, W start (same layer) then Y ⇒ edge X→W only; 2.5 s gap ⇒ no edge; (15) fold buffer: events
< 1 s old not folded; a gate inside the buffer window discards them (sit-after-furniture-anim case); (16)
`ChannelClock` (model helper): continuous across a control-avatar id change, frozen while the source time is frozen,
no tick without a clock source; (17) LLSD round trip preserves `mSealed`; loaded pools are sealed; (18) null-item key
match falls back to (point, prims, ordinal); (19) group {A,B} with B non-resident: at dwell end A is kept, dwell
redrawn, no add/remove emitted (M1); (20) buffered start discarded => its later stop yields no sample/edge;
(21) `markCold` mid cross-fade re-issues only the incoming id.
**Who runs them:** Codex only writes them. After the review loop reaches 0 must-fix, Claude configures a
`-DBUILD_TESTING=ON` build, builds once, and runs `ctest --test-dir <build> -R alghostautoanimmodel --output-on-failure`
alongside the single viewer build; a failing test starts a new fix→review round.

**In-world** (one pass; outcomes stated in advance; self-clone + an AO with ≥3 stands on a ≤60 s timer, a face
HUD, one animated animesh attachment):
1. Spawn entity clone, Mirror, AutoAnimate on (Learn 180 s). Stand still. *Expect* countdown → `AUTOANIM-SEAL`
   body states ≥3, `groups=1` for BODY, one-shots > 0, animesh 1 channel; `AUTOANIM-COST rec=0` after seal.
2. Walk around during a second Relearn. *Expect* `AUTOANIM-GATE on reason=speed|loco` and no walk UUIDs in the pool
   (status counts unchanged by walking).
3. **Go autonomous.** *Expect* `HANDOFF kind=manual verdict=ok ours_stopped=0` for body and animesh; no pop; clone
   keeps the current stand then cycles to other stands on their observed dwell (not in sync with you); face keeps
   firing. You change stand: the clone does NOT follow (MANUAL survives presence).
4. **Follow source.** *Expect* clone matches you within a frame; no RECOUPLE line (manual clear).
5. `/ghostanim simloss selected`. *Expect* after ~1.5 s `HANDOFF kind=auto verdict=ok` (body+animesh). `simloss`
   again ⇒ after 0.5 s `RECOUPLE` and live mirroring.
6. Detach the animesh attachment from yourself. *Expect* only that channel logs `HANDOFF kind=auto verdict=ok` and
   keeps animating; body stays live.
7. Pause → Resume while autonomous. *Expect* `verdict=COLD`, same pool, autonomy kept; Studio combo shows Mirror.
8. Director switchboard anim cut on the clone, then release. *Expect* directed anim plays; on release `COLD`,
   autonomy resumes.
9. Save; restart the viewer; spawn a clone, Load, Go autonomous. *Expect* `AUTOANIM-LOAD ok`, `AUTOANIM-FETCH`
   lines for uncached ids, then stand switches as in 3 once fetched (no `SKIP` storm).
10. Duplicate the clone ×5 (crowd). *Expect* all share the pool, choices differ per clone, `COST` scales ~linearly.
11. Relearn, sit on a chair (and separately ground-sit) for 20 s, stand, finish the window. *Expect*
    `AUTOANIM-GATE on reason=sit` (possibly `discarded>0`); no sit UUID in the pool; with the source ground-sitting,
    no body HANDOFF is logged (a gated source is present).
12. Switch the clone to True mirror, Go autonomous is not pressed; `/ghostanim simloss selected`. *Expect* animesh
    `HANDOFF kind=auto verdict=ok clock=cav:…` and the animesh keeps switching/firing (its clock runs); body logs no
    handoff and holds the stamped pose.
13. Freeze strip (3 captures) from an autonomous clone. *Expect* 3 frozen snapshots exactly as before this feature;
    no `HANDOFF`/`COLD` lines for the snapshot runtimes.
14. Relearn, and press Go autonomous after ~20 s (before seal). *Expect* button enabled, `AUTOANIM-LEARN seal
    reason=manual`, `HANDOFF kind=manual verdict=ok`.
Any `POP` in 3/5/6/12/14 ⇒ broken regardless of looks. Absent `NODATA`/`SEAL` lines in 1 ⇒ the instrument, not the
feature, failed. Step 5 must show the body channel reporting absence via the null-source path (`reason=null` in the
HANDOFF detail), proving the logout/TP branch is exercised.

## 11. R3 reviewer — attack these first
1. Per-channel clocks: rebase continuity, a control avatar that exists but is jellydolled/paused, and first-frame
   behaviour before any control avatar exists.
2. Body removal outside `synchronizeCloneAnimations` (ease-out stop + direct ledger erase): can the ledger and the
   controller disagree afterwards (restart path `:890-920`, MIRROR exit `sync(empty)`, recouple)?
3. Fold buffer: discard semantics vs `observed` time accounting and vs a seal landing while events are buffered
   (rule: seal folds only events ≥ 1 s old and discards the rest).
4. Placement of body absence before `:2471` and the sim-loss null-out: prove HEAD behaviour is unchanged when
   `mAutoAnim` is null.
5. Copy/strip ordering at `:796-800` and `:3382-3397`, and under `duplicateGroup`/crowd commit (`:840, :4381`).
6. Gap-edge rule on random-order AOs and on AOs whose stand change passes through a brief empty set.
7. Is anything here still wrong in approach? Say so.

## 12. Decisions that genuinely need the user (author defaults stand; R2 did not contradict them)
- **TRUE_MIRROR body:** v3 = no automatic body handoff (keeps its hold-last-stamp contract); manual Go autonomous
  switches to MIRROR with a non-seamless `COLD` start. Alternative: auto-switch to MIRROR on loss (also COLD).
- **Duplicates/crowds inherit a SEALED pool** (each with its own seed). Alternative: copies start unlearned.
- **Toggle OFF keeps the pool** (Relearn/Load replace it). Alternative: OFF discards.

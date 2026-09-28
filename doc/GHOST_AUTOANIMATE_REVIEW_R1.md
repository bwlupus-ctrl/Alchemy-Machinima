# Ghost AutoAnimate — adversarial design review R1

Reviewed: `doc/GHOST_AUTOANIMATE_BRIEF.md` against HEAD `96aa1197555` (fix/animesh-clone-pose-polish). Read-only.
Labels: **PROVEN** = read in code at HEAD; **INFERENCE** = not provable in-repo (SL sim behaviour etc.).

## (A) Stale-base corrections (brief §2 vs HEAD)

| Brief claim | HEAD reality |
|---|---|
| Drive modes MIRROR/DIRECTED/FROZEN, `alghoststudio.h:100` | 4 modes; `DRIVE_TRUE_MIRROR` = 3 at `alghoststudio.h:107-110`. XUI `drive_mode_combo` item value 3 = "True mirror" (`panel_ghost_studio.xml:223`). **`DRIVE_AUTO = 3` collides.** |
| Mirror copies source set every frame, `:1987-2013` | `llghostavatar.cpp:2467-2494`; syncs only when non-empty (hold on) and different. |
| `synchronizeCloneAnimations` `:492-526` | `:533-577` |
| Animesh mirror `:2050-2125`, hold `:2060-2077` | `:2527-2661`; source-gone latch `:2551-2569`; NEW detach-hold latch for an empty/missing source set `:2575-2611`; runs in MIRROR **and TRUE_MIRROR** `:2534`. |
| Hold-on-source-change `:1985-2012` | `:2462-2493`. There is **no grace timer** — hold is instantaneous; §3.4.3 "reuses the same grace idea" is false. |
| Leaving MIRROR stops all, `:548-552` / DIRECTED clears animesh `:557-561` | `:607-611` / `:616-620`. Also leaving FROZEN/TRUE_MIRROR releases pause requests `:596-601`; every mode change calls `resetHeldAnimeshRepeat()` `:591`. |
| Clamp `[MIRROR,FROZEN]` `:531-532` | `:581-582`, clamps to TRUE_MIRROR. A value 4 is silently clamped to 3 = TRUE_MIRROR. |
| Pause/Resume `alghoststudio.cpp:392, :1157-1166` | `:392-401` is `applyEntityRuntimeState` (runtime replacement), pause is `setInstancePaused` `:1148-1179`. |
| Keyframe cache `llkeyframemotion.cpp:2355-2358` | `flushKeyframeCache` `:2987-2994` (only clears locomotion-phase cache). Evictions: `lllocalanim.cpp:72,132,281,442`, `llfloaterbvhpreview.cpp:1011`, `LLKeyframeDataCache::clear()` at shutdown `llappviewer.cpp:1850`. Claim holds. |
| Anim-speed re-assert `:2128-2150` | `:2664-2685`. |
| "Follow live must mean MIRROR" | `btn_live` → `unfreezeInstance` (`alghoststudio.cpp:2988`) = overlay palette un-freeze; unrelated to drive mode. |
| "Retargeting (re-sourced to a different avatar)" | No re-source path exists (`inst.mSource` set only at creation `:532/:560`). |

**Cache claim VERIFIED (PROVEN):** `LLMotionController::createMotion` (`llmotioncontroller.cpp:363-405`) → `LLKeyframeMotion::onInitialize` checks `LLKeyframeDataCache` first (`llkeyframemotion.cpp:585`) → synchronous `STATUS_SUCCESS`; a miss goes to the local file cache and then a fetch (`STATUS_HOLD` → `mLoadingMotions`); a failure `markBad`s and returns NULL, so no crash. Caveat: `purgeExcessMotions` (MAX 32 instances, `:42`) deletes inactive instances. "Resident" must therefore mean "present in `LLKeyframeDataCache::getKeyframeData(id)`", not "`findMotion` non-null". Otherwise purged anims read as unloaded and are skipped forever.

## (B) Must-fix (P1)

1. **Enum collision.** `DRIVE_AUTO=3` compiles silently alongside the implicit 3 (there is no `switch` to catch it) and AUTO becomes TRUE_MIRROR. At minimum use 4, widen the clamp, and add combo value 4. See P1-2 for the preferred fix.
2. **The state model is wrong. Don't add a drive mode.** §3.5 wants per-linkset independent handoff, but drive mode is per entity. Entering AUTO stops the animesh mirror loop for every linkset (`:2534` gate), including ones whose source is still live. Other problems with the new mode:
   - `aldirectoranimswitcher.cpp:313,344-352,373,400-405,559` calls `ghost->setEntityDriveMode(MIRROR|FROZEN|DIRECTED)` directly and bypasses `Instance::mDriveMode`. Any switchboard cut or release knocks an AUTO clone into MIRROR (or DIRECTED, which runs `clearClonedObjectAnimations`) and never restores AUTO, while the panel still shows Auto.
   - An automatic handoff inside `LLGhostAvatar` desyncs `Instance::mDriveMode` in the same way.
   - `/ghostanim freeze` sets FROZEN without saving `mResumeDriveMode`.

   **Fix:** make AutoAnimate a per-channel *autonomous latch inside MIRROR*: one flag for the body and `ClonedLinkset::mAutonomous` for each linkset. HEAD's hold already is "source contributes nothing → keep last". Autonomy replaces "keep last" with "play learned". The MIRROR animesh loop and `repeatHeldAnimesh()` must skip autonomous linksets, and the body mirror block skips while the body is autonomous. This change removes the enum widening, the clamp, the combo, chat and switchboard hazards, and the §3.4.1 pop path entirely, and Pause/Resume round-trips as MIRROR.
3. **Recouple defeats Detach.** "AUTO + source present + Recouple → MIRROR" fires on the next frame after a manual Detach while the source is present. For a self-clone the source is always present, which is the whole §8 test. So test step 3 fails by design. **Fix:** recouple only on an absent→present edge after an *automatic* loss, and never after a manual Detach.
4. **Hold OFF breaks the handoff.** HEAD comments (`:2580-2586`) prove a source detach empties the source prim's set *before* the prim dies. With `GhostMirrorHoldOnSourceChange` off, `mirror_one` erases the clone entry first, the "root missing" detector fires later, and adoption finds nothing, which is a guaranteed POP. **Fix:** loss = "empty OR missing" and AutoAnimate forces hold semantics on its channels regardless of the setting.
5. **TRUE_MIRROR.** In TRUE_MIRROR the body ledger is emptied and the controller paused (`:640-662`), so the body channel learns nothing. A handoff releases the pause onto an empty controller, which pops to the default pose. **Fix:** Detach and auto-handoff are MIRROR-only (disabled with a tooltip in TRUE_MIRROR), or record from the source map (P1-7) and state that a TRUE_MIRROR handoff cannot be seamless.
6. **Learning gates / exclusions are missing.** Walk, run, turn, fly and crouch (`AGENT_WALK_ANIMS`, `llanimationstates.cpp:176`), prim-sit/AO-sit, typing, away/busy, gun-aim and every `gAnimLibrary` built-in (`llvoavatar.cpp:1269-1310`, including procedural `LLEmote`/`LLNullMotion` with duration 0) would all be learned. The AUTO clone would then moonwalk, sit in mid-air or type. **Fix:** suspend learning while the source `isSitting()`, is in the air, or any walk anim is signaled. Exclude every id with `gAnimLibrary.animStateToString(id) != NULL` except `AGENT_STAND_ANIMS` (express_* stays opt-in). Learn only assets present in `LLKeyframeDataCache`.
7. **Exclusion-group inference is wrong.** "Never observed together ⇒ alternatives" is non-transitive and conflates *exclusive* with *unobserved*. Two failure cases:
   - A face or hand-pose loop seen only during stand A becomes an "alternative" to stand C, so the scheduler swaps the body stand for a face loop.
   - Overlapping AO transitions (new stand started before the old one stops) mark A and B as co-occurring, so they are *not* alternatives.

   **Fix:** union-find on *replacement edges* (X stops and Y starts within ~0.5 s, and an overlap shorter than the tolerance counts as a transition), partitioned by layer (joint-coverage signature from `JointMotionList` joint names plus base priority). Record the pool from the **source** `mSignaledAnimations` diff (answer to §7.7): it works in TRUE_MIRROR, and it is immune to clone-side mutations like `restartEntityAnimation` (`:903-919`), the pause-path `synchronize(empty)` and the switchboard. Ignore sequence-id bumps on loops (AO re-asserts, `:540-546`). Treat them as retriggers only for non-loops.
8. **Pool lifetime and keying.**
   - `refreshEntityClone` and any runtime recreation build a new `LLGhostAvatar` and re-clone the linksets (`alghoststudio.cpp:640-722`). A pool held on the ghost is lost.
   - Recreation needs the source present (`createEntityRuntime :346-350`). An autonomous clone dies with its runtime on region teardown (`markDead → releaseClonedAttachments :2240-2246`) and cannot be recovered. State this limit.
   - The linkset *index* is not stable across Refresh/Load, and "count matches" can map a tail's anims onto a pet.

   **Fix:** keep the sealed pool on `Instance` as `shared_ptr<const Pool>`, because `Instance` is copied by value in duplicate, crowd and freeze-strip (`:731,759,3380`). Keep the recorder on the runtime. Key animesh pools by (attach point, source root attachment item id, prim count).

## (C) P2

- One-shots seen fewer than 3 times need a minimum sample count before they are scheduled. Otherwise a single `/laugh` or dance gesture repeats forever. Gesture sounds are never replayed.
- Censor the open dwell at seal. The 120 s default is shorter than common AO timers (60-300 s), so tie POOL THIN to the observed switch count.
- The scheduler must mint its own sequence ids to re-fire a finished one-shot. Body: a new sequence id in `synchronize` → `startMotion` replays. Animesh: bump the map entry, then `updateControlAvatar`. Use negative ids so they never collide with the sim.
- Equal priority means the newest-started motion wins (`llpose.cpp:213-217`, `push_front :1019`). Mirror already starts anims in UUID order within one diff. Adoption avoids a handoff change, but stand switches use `stopMotion(id, true)` (immediate, no ease-out) and snap. This is pre-existing and visible in AUTO.
- "Repeatable take" is false. `seeded_unit` (`:159`) is a stateless hash, so it needs a decision counter, and outcomes still depend on handoff time and load timing.
- Animesh recouple after a re-rez never matches (stale `mSourceRoot`, `llghostavatar.h:266-270`). Refresh is the only way back.
- Instrument (rule 4): POP = our code stopped a previously playing id in the handoff frame. Natural one-shot expiry is not a POP. Add an `EMPTY-ADOPT` verdict for the case where a pause (`synchronize(empty)`) preceded the handoff.
- Naming: "Detach" collides with HEAD's detach-hold. Use "Go autonomous".
- `/ghostanim` parses exactly `argument scope` (`alchatcommand.cpp:780-786`), so `auto on selected` fails. Use distinct verbs (`autoon`, `autooff`, `autonomous`) matched before the UUID fallback.
- The Director cast menu has no ghost items today and there is no runtime→instance lookup, so add one.
- Overlay "Convert to entity" is a new lifecycle path: the source must be present, and group/crowd members can't be converted singly. Consider showing a disabled control instead.
- Loaded pool files are untrusted: add a version field, cap sizes, reject non-finite values.
- Cost: 50 learners is fine (a source-map compare of about 20 entries per clone, and the existing mirror already copies the map every frame). The memory estimate holds if sealed pools are shared, not copied.

## (D) Verdict

**NO-GO on v1 as written. Redesign the state model (§3.1/3.4/3.5) as a per-channel autonomous latch inside MIRROR. Recorder and scheduler are GO after P1-6/7/8.** Revise the brief, then run R2.

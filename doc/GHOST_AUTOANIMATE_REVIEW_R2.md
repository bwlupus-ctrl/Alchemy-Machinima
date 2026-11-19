# Ghost AutoAnimate — adversarial review R2 (brief v2)

Read-only against HEAD `96aa1197555`. **PROVEN** = read at HEAD; **INFERENCE** = SL sim behaviour, not provable in-repo.

## What v2 fixes (verified)

The per-channel latch inside MIRROR resolves R1 P1-1, P1-2 and P1-3, and the enum/clamp/combo/chat hazards go away. Recouple-only-after-auto-loss fixes the Detach bug. `hold_eff` is byte-identical when AutoAnimate is off, and a captured local is the correct lambda change.

Spot-checked anchors, all accurate:
- the 16 shifted XUI elements (counted: 252…480; `anim_metadata_text` ends at 250) and heights 700 at `:204/:207`
- `settings.xml:7107`
- CMake `:305/:1164/:2607`
- the picker precedent
- the registrar `llfloaterdirector.cpp:356-383` and `selectedCastIds :1582`
- the copy paths `:779/:840/:4381`
- paused anim time `llmotioncontroller.cpp:881-884`
- the control-avatar max-seq merge

**Hook/COLD path: no pop or double-start found.**
- MIRROR→FROZEN runs `sync(empty)` (`:607-611`), which empties `mClonePlayingAnimations`. The COLD re-issue therefore starts each id exactly once.
- A `mRestartOnResume` restart (`alghoststudio.cpp:1175`) runs before the next `idleUpdate` with an empty desired map, so it is a no-op.
- DIRECTED entry clears the clone entries (`:619`). A COLD rewrite of the root entries (with negative seq ids) restarts through `updateControlAvatar`.
- On recouple, overlapping ids differ only by seq, so `startMotion` on the active motion does not rewind.

## P1 must-fix

1. **The playback clock freezes the animesh schedulers in TRUE_MIRROR.** §3.3 uses the *body* controller's `getAnimTime()`. TRUE_MIRROR pauses the body controller (`llghostavatar.cpp:659`), and paused time does not advance (`llmotioncontroller.cpp:881-884`). Autonomous animesh channels in TRUE_MIRROR, which v2 §3.7 calls "identical to MIRROR", would therefore never switch or fire. Separately, the switchboard sets `setEntityAnimTimeFactor(slot.mSpeed)` (`aldirectoranimswitcher.cpp:398`) and never restores it.
   **Fix:** a per-channel clock. Body keeps its controller. Each animesh channel uses its own control avatar's controller time, rebased when the control-avatar id changes, following the `mRepeatControlAvatarId` pattern (`:2316-2324`).
2. **The gate is incomplete, so sit poses get learned as stands.**
   - Ground-sit ids are only *excluded from events*. The gate list (§3.3) omits `SIT_GROUND`, `SIT_GROUND_CONSTRAINED`, `SIT_FEMALE`, `SIT_GENERIC`, `SIT_TO_STAND` and `STANDUP` (`llanimationstates.h:146-158`). An AO's custom ground-sit that starts as the stand stops therefore creates a BODY replacement edge and joins the stand group.
   - `isSitting()` on a remote avatar follows the ObjectUpdate parent change, which can land after the AvatarAnimation carrying the furniture anim (INFERENCE). The edge is committed before the gate engages, and "pending candidates dropped" does not remove an edge that was already counted.
   - Result: the clone sits mid-air.
   - **Fix:** add those ids to the gate. Buffer events for 1.0 s before folding them in, and discard the buffer if a gate engages inside it.
3. **Body loss detection is underspecified, and the test never exercises it.**
   - HEAD's body block is inside `if (source && !source->isDead())` (`:2472`). "HEAD code + absence bookkeeping" invites putting the timer inside that block, where the *null-source* case (logout or TP, the main real loss) never counts.
   - The only body-loss test is `simloss` on a self-clone, and a flag can bypass that branch.
   - **Fix:** name the placement: absence is evaluated before the source check. `simloss` must behave as if `source == nullptr`, so the real branch is what gets exercised.
4. **Copy rule ordering and freeze strips.**
   - `duplicateInstanceSnapshotInPlace` calls `applyEntityRuntimeState` at `:796`. The share/drop rule must run *between* `:779` and `:796`, or the new runtime is configured with the proto's unsealed pool and MANUAL flag.
   - Freeze strips (`updateFreezeStrips :3381` → `setInstancePaused`) copy MANUAL. The fresh runtime gets EMPTY-ADOPT cold picks and is frozen at once, which regresses an existing feature.
   - **Fix:** strips clear `mAutoAnimate` and `mAutoAnimManual` on the copy.

## P2

- The cross-fade still ends in `stopMotion(id,true)` (`sync :548`), so joints that only the outgoing stand covers snap. A lower-priority next stand is an immediate swap on every switch in mixed P3/P4 sets. Consider removing via `LLCharacter::stopMotion(id,false)` and erasing the ledger entry in the scheduler.
- `REPLACE_TOL` 0.5 s breaks AOs that stop, gap and then start: they become all singletons, so THIN and one frozen stand. Use "next same-layer start within about 2 s, with no intervening same-layer start".
- The `mInAir` gate is heuristic. It is foot-to-ground at `llvoavatar.cpp:4977-4987`, uses the hover param, and goes stale when the avatar isn't updated, so learning can pause permanently. Gate on the signaled fly/hover/fall ids only.
- A ground-sit with no AO leaves an empty filtered set. That hands the body off to autonomous while the source is present, and with Recouple off it stays autonomous.
- The Go autonomous enable condition requires a *published* pool, which only exists after seal, auto-loss or Refresh. Early manual handoff (§3.3 "seal on Go autonomous") is therefore unreachable. Enable on the live recorder count. The runtime scheduler must bind to its own snapshot, not the instance pool, which is pulled a frame later.
- Implementability:
  - The `LLGhostAvatar::EAutoAnimAction` and `AutoAnimConfig` types used in the `ALGhostStudio` API force including `llghostavatar.h`. Today `alghoststudio.h:66` only forward-declares the class. Move both into the model header's namespace.
  - `Pool` needs an explicit `bool mSealed`.
  - The hook must capture the old mode before `:613`.
  - Say whether the `mAutoAnimWindow` default comes from the setting or the hard-coded 120.
  - Specify Load onto an already-autonomous clone (rebind and adopt).
- `getAttachmentItemID()` may be null on non-self sources (INFERENCE). Define the key fallback.
- Region teardown mid-learn loses unsealed progress, because export only happens on Refresh. State it or export on `markDead`.
- The unit tests only build with `-DBUILD_TESTING=ON`. Say who configures and runs them before the single build.
- OFF-LIMITS is missing:
  - `llviewermessage.cpp`, `aoengine.*`, `alghostspawnengine.*` and the Director cast model
  - `floater_director.xml`
  - `repeatHeldAnimesh`/`resetHeldAnimeshRepeat` beyond the one skip line
  - `setEntityAnimTimeFactor`
  - existing panel/Director handlers
  - panel XUI other than the 16 shifted elements plus the new block

## Verdict

**Another round (R3), targeted, not a redesign.** The architecture is right. Fix P1-1…4, fold in the P2 implementability items so Codex has no forks, then re-review.

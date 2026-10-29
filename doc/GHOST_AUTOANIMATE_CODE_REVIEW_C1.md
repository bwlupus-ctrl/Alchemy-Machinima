# Ghost AutoAnimate — adversarial code review C1

Scope: `git diff 58f7cef958e` (13 files) plus the new `alghostautoanimmodel.h/.cpp` and `tests/alghostautoanimmodel_test.cpp`. Read-only review; nothing was built.
Labels: **PROVEN** = read in code. **INFERENCE** = not settled without the compiler or an in-world run.

## P0 — compile (/WX)

1. **`setToolTip` is called with `const char*` at `alpanelghoststudio.cpp:2184` and `:2200`.** PROVEN.
   - `LLView::setToolTip(const LLStringExplicit&)` is at `llui/llview.h:223`.
   - `LLStringExplicit(const char*)` is **explicit** (`llcommon/llstring.h:405`), and `const char*` → `std::string` → `LLStringExplicit` would be two user-defined conversions. Both calls fail with C2664.
   - `:2184` is a ternary of two string literals, which is still `const char*`.
   - The repo precedent wraps the literal (`alpanelpatheditor.cpp:1496`: `setToolTip(std::string("..."))`). This line is the only bare-literal `setToolTip("` in newview.
   - **Fix:** wrap both calls in `std::string(...)`.

Checked and OK:
- `JointMotionList` and its members are public (`llkeyframemotion.h:403` section).
- `LLCharacter::createMotion` is public.
- `ATTACHMENT_ID_FROM_STATE` (`indra_constants.h:364`) and `attach_state` are in scope at `:2755`.
- The LLSD integral/float convenience constructors cover `U8/U16/F32` (`llsd.h:209-218`), and `llsd::inArray` exists.
- `EnableCallbackRegistry` lambda signature, the `startPicker` overloads (`llviewermenufile.h:134-135`), and aggregate initializers (constants fit U8) are all fine.
- The `using namespace ALGhostAutoAnim` inside the unnamed namespace reaches global scope. I found no colliding `Entry/Status/Action/Config/Pool` names in the `LLVOAvatar` base chain.
- INFERENCE: `const bool hold_eff = mAutoAnim || hold_on_source_change;` (`llghostavatar.cpp:3237`) relies on `unique_ptr`'s explicit `operator bool` inside `||`. Compilers accept this, but `mAutoAnim != nullptr` removes all doubt.
- CMake: the source and header are registered and the test goes into `viewer_TEST_SOURCE_FILES`, inside `if (BUILD_TESTING)`, so a normal build is unaffected. The test follows the existing `al*model_test.cpp` TUT pattern and the model depends on llcommon only.

## Off path (AutoAnimate off) — proven inert

- **`idleUpdate`:**
  - `hold_eff == setting`, and the recorder and presence calls are guarded by `mAutoAnim`.
  - The body `else if` is the original condition.
  - The animesh early `continue` is guarded.
  - `mirror_one` captures `hold_eff` by value, and the value equals the setting.
- **`setEntityDriveMode`:** one local plus a hook that returns immediately on null `mAutoAnim`.
- **`repeatHeldAnimesh`:** `mAutonomy` is always 0 while off, and toggling off resets it (`llghostavatar.cpp:422`).
- **`cloneAttachmentsFrom`:** two side-effect-free field reads.
- **Studio:** `applyEntityRuntimeState` passes a disabled config and `setEntityAutoAnimate` returns at once. The copy rule is null-guarded. The strip's `setInstanceAutoAnimate(false)` is a no-op on a runtime that is already off.
- **Chat:** the new verbs match before the drive-mode and UUID parse.
- `synchronizeCloneAnimations`, `clearClonedObjectAnimations`, `restartEntityAnimation`, `markDead` and `releaseClonedAttachments` are unchanged.

## Brief mechanisms — implemented as specified

- **Per-channel latch.** Body autonomy bypasses the HEAD block; autonomous linksets `continue`; `repeatHeldAnimesh` skips them.
- **Loss detection.** Loss is evaluated before the source check, and `simloss` nulls the source there (`llghostavatar.cpp:3243-3249`). The grace timer uses real time. Recouple happens only from `AUTO_LOST` after a 0.5 s debounce.
- **M1.** Implemented at `alghostautoanimmodel.cpp:470-471`.
- **M2.** Prefetch uses a process-wide dedup map with a 60 s retry and runs on bind, on control-avatar rebase and every 60 s.
- **Gates.** The sit, air, speed and loco gates are all present. The 1 s ring discards on gate entry, and gate state is computed even after seal.
- **Grouping.** Gap and overlap edges are implemented, and coexistence blocks a union.
- **Copies.** The share/drop rule runs before `applyEntityRuntimeState`, and strips clear AutoAnimate before the pause.
- **Pool ownership.** Pools use aliasing `shared_ptr`s. Every raw `channel.pool` is re-pointed whenever `runtime.pool` changes (snapshot, incoming, Relearn).
- **Load.** Validation matches §3.8, with a size check before the parse.
- **Ease-out removal.** `stopMotion(id,false)` plus erasing both ledger maps happens before `sync`. There is no orphan motion and no double stop (restart goes through deprecation, `llmotioncontroller.cpp:421-431`).
- **Clock rebase.** Rebasing is continuous (test 16).
- **OFF-LIMITS.** Respected. Every touched file is in the brief's allowed set, and `repeatHeldAnimesh` got only the skip line.
- **XUI.**
  - All 16 elements moved by exactly +90 (counted), and both heights are 790.
  - The new block runs from 252 to 340, which is ≤ 342.
  - `rig_diagnostics` ends at 780 of 790, and the right-most button ends at 452 of 484.
  - All 13 named controls exist.

## P1

1. **COLD replays every one-shot that ever fired.**
   - `markCold` (`model.cpp:369`) collects every `mOneDue` id still in `mDesired`. Finished one-shots are never removed from `mDesired`, so that means all of them.
   - The COLD tick then issues them all at once (`:422-427`).
   - Every Pause→Resume or switchboard release therefore fires a burst of face and fidget expressions. That is in-world test steps 7 and 8, and it contradicts "re-issue only the current picks".
   - **Fix:** `markCold` only reschedules one-shots (`due = now + inter`) and never re-issues them. Adjust test 21's second half.
2. **Instrument verdicts are misleading (CLAUDE rule 4).**
   - `snapshot()` logs `AUTOANIM-SEAL`, `NODATA` and `THIN` for **every** snapshot: the 30 s checkpoints, each loss, Refresh export and Save (`llghostavatar.cpp:320-325`).
   - A static animesh at learn start hits NOPOOL through `presence()` → `snapshot("loss")`, which prints `AUTOANIM-NODATA ch=body observed=0`. The brief's own rule then reads that as a failed instrument.
   - **Fix:** emit SEAL/NODATA/THIN only when `seal == true`. Log checkpoints as `AUTOANIM-CHECKPOINT`.

## P2

- **Learn start has no baseline** (`llghostavatar.cpp:239`: `baseline = pool != nullptr`). Anims already playing get synthetic starts at learn start, which yields truncated first dwell samples. Baseline a fresh recorder the same way as after a gate exit.
- **`AGENT_WALK_ANIMS` includes turn-left/right.** Every in-place turn gates, censors open stands and re-baselines, which silently discards in-progress dwell samples. Consider treating turns as buffer-discard only.
- **Stale `body.cold`.** `llghostavatar.cpp:793` sets `body.cold` on any TRUE_MIRROR→MIRROR switch, even while the body is NONE. A later automatic handoff is then logged COLD and `markCold` runs, which drops adopted alternates and so immediately stops a co-adopted stand (a pop the audit doesn't see). Set it only when a GoAutonomous is pending.
- **The POP audit can never fire.** It only inspects the scheduler's own removals, and none happen within 2 s. Check `isMotionActive` for the adoption ids at handoff +1 frame instead.
- **Pending facts block the ring head** until the next signal change (`retryPending` only runs on a diff). Retry every frame while `pending` is non-empty.
- **Foreign ids are dropped at COLD** (the filter at `llghostavatar.cpp:678-685`). Held HUD loops that were not learned vanish after a pause. This is by the letter of the spec; document it.
- **Per-frame UI cost.** `refreshAutoAnimate` does 10 `getChild` lookups plus an `ostringstream` status per refresh. `updateAutoAnimate` re-sends the config every frame when the window is 0. Cache the pointers and only send on change.
- **Linux (INFERENCE):** Xlib's `#define Status` would break `ALGhostAutoAnim::Status` in any TU that includes Xlib. This is not an issue on Windows.

## Verdict

**Must-fix: P0-1, P1-1, P1-2** — each a few lines. After that it is GO for build, per the loop: fix → re-review → build once. Then run ctest with `-DBUILD_TESTING=ON` for `alghostautoanimmodel`.

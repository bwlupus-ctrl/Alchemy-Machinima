# Ghost AutoAnimate — adversarial code review C2

Scope: current `git diff 58f7cef958e` plus the new model and test files. This review re-checks the C1 fixes and attacks the new code. Read-only; nothing built.

## C1 fixes — verified

- **P0 tooltips:** `alpanelghoststudio.cpp:296` (now cached once in `postBuild`) and the `mAutoAnimCheck` tooltip are both wrapped in `std::string`. The sweep found no other bare-literal `setToolTip`/`setText`/`setLabel` in the new code.
- **P1-1 (COLD one-shots):** `markCold` (`alghostautoanimmodel.cpp:376-384`) erases fired and adopted one-shots and reschedules them. Finished shots retire from the desired set (`:498-507`). Tests 21 and 25 cover this.
- **P1-2 (seal-only verdicts):** SEAL/NODATA/THIN are now seal-only, and checkpoints log `AUTOANIM-CHECKPOINT` (`llghostavatar.cpp:342-356`).
- **P2 items, all verified:**
  - baseline events (`kind 3`, `startKnown=false`)
  - turn anims excluded from the loco gate
  - `body.cold` set only by a pending manual TRUE_MIRROR Go (`requestAutoAnimAction(action, cold)`)
  - pending facts time out after 1 s (no head-of-line blocking)
  - cached control pointers, with `LearnStatus` compared before refresh
  - change-only default-window config
  - `Status` renamed to `LearnStatus`
  - `mAutoAnim != nullptr`
  - COLD-drop comment.

## Off-path and behaviour of the hooked HEAD paths — proven inert

- `autoAnimSyncBody` audits under `if (mAutoAnim && body.auditHandoff)` and then calls `synchronizeCloneAnimations` unchanged.
- `autoAnimAuditBodyStop` and `autoAnimAuditLinksetSync` return immediately on a null `mAutoAnim`.
- Every call is inserted *before* the original stop or sync, and the call sites in `setEntityCloneVisible`, `setEntityEyeMotionEnabled`, `setEntityPhysicsEnabled`, `setEntityDriveMode`, the DIRECTED play-once stop and the animesh loop are untouched otherwise.
- `restartEntityAnimationAudited` only wraps `restartEntityAnimation`.
- Audits only read the maps and ledgers (`control->mPlayingAnimations` is public, `llvoavatar.h:1075-1078`) and write audit fields.
- **Behaviour of those functions is unchanged with AutoAnimate on or off.**

## Compile sweep (/W3 /WX)

- `const` methods only write `mutable` cache fields (`statusDetail`, `detailAutonomy` and the rest), reached through `const auto&` from `*mAutoAnim`.
- Default arguments are on the declarations only.
- `LearnStatus` is complete in the panel header through `alghoststudio.h` → the model header.
- `isMotionActive` is public and non-const on `LLCharacter*`.
- There are no size_t→U32 narrowings without a cast.
- **No new compile issues found.** The C1 checks (explicit `LLStringExplicit`, LLSD constructors, attach scope) still hold.

## P1

1. **POP audit false positives, which break the pass/fail instrument (CLAUDE rule 4).** The audit is now armed for 2 s and hooked into *every* stop path, but it never disarms on user actions or natural retirements.
   - **Mode changes:** a Pause, combo change or switchboard cut within 2 s of a handoff logs `verdict=POP`. The cause is `autoAnimSyncBody(empty)` in `setEntityDriveMode`, and the linkset audit on DIRECTED.
   - **Follow and recouple:** `act(FollowSource)` deliberately keeps the audit alive (`llghostavatar.cpp:609`). A recouple (0.5 s debounce) or a Follow within 2 s makes the HEAD mirror sync count legitimate stops as POP, on both the body and the animesh loop (`llghostavatar.cpp:3555` hook).
   - **One-shot retirement:** an adopted *active* one-shot is retired at anim-time `now + max(2 s, dur)` (`alghostautoanimmodel.cpp:350-351`). Anim time runs faster than real time with Studio speed > 1 or Chaos (±15 %). A retirement at, say, real 1.74 s is inside the 2 s real-time window and counts as POP, even though `stopMotion` on a naturally finished shot is a no-op.
   - **Fix:**
     - (a) `autoAnimOnDriveModeChanged`, Follow, Relearn, recouple and toggle-off set `auditHandoff = false`.
     - (b) one-shot retirements are not reported to `auditStop` (keep a separate retire list, or tag them).
     - (c) `auditStop` counts only when `isMotionActive(id)` holds at the moment of the stop.

## P2

- **Uncached starts are dropped after 1 s.** An uncached anim's start is folded away if its facts don't arrive within 1 s (`alghostautoanimmodel.cpp:269-281`), so that occurrence is never learned. It is usually cached by the mirroring clone; document it.
- **Retirement uses the adopted duration.** For adopted one-shots it assumes `dur` from handoff even though their age is unknown. That's harmless, because the stop is a no-op after natural end.
- **Recreation through the window check.** `updateAutoAnimate` compares `getAutoAnimWindow()` (0 when the runtime is absent) and will *create* a runtime for an enabled instance whose runtime lacks one. That is benign, arguably self-healing, but undocumented.

## Verdict

**Must-fix: P1-1** — about 10 lines, confined to the audit. Everything else is GO. After the fix: re-review, then build once, then ctest `-DBUILD_TESTING=ON`.

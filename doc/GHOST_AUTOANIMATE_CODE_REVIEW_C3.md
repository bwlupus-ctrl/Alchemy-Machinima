# Ghost AutoAnimate — adversarial code review C3 (POP audit fix)

Read-only review of the current `git diff 58f7cef958e`. Nothing built.

## Fix verified

- **Disarm points:**
  - `setEntityDriveMode` disarms after the no-op early return and before any stop (`llghostavatar.cpp:1479`).
  - Follow/Relearn disarm at `:618`, recouple at `:386`, and toggle-off at `:464` (before `mAutoAnim.reset()`).
- **Retirements:** they are cleared per tick (`alghostautoanimmodel.cpp:433`), pushed only for natural one-shot ends (`:508`), and un-marked on a same-tick re-fire (`:523`). The body and animesh audits drop them from `adoption` before any explicit or implicit stop (`:718`, `:790`).
- **`auditStop`:** it now requires `isMotionActive` on the relevant character (`:444`). The ghost is passed as `*this` and the control avatar as `*control` (`:861`). The audit call always runs *before* the stop or sync, so the motion is still active when it is checked.
- **Tests 25–27** cover retirement classification, output clearing, accelerated anim time and a long-frame re-fire.

## Can a real pop now be hidden? No.

- **Disarm is never on a handoff path.** An automatic handoff never changes drive mode. The only handoff that does is the manual True-mirror Go autonomous, and its `setInstanceDriveMode(MIRROR)` runs before `bind()` arms the audit.
- **Recouple, Follow, Relearn and toggle-off** are user or source transitions that end autonomy. Their stops are not part of the handoff.
- **`isMotionActive` stays true through ease-out.** It is true until deactivation, so a stop during ease-in or ease-out is still counted. It is false only after the motion has naturally ended, and a stop then is a no-op, so nothing visible is hidden.
- **A retired one-shot cannot be a real pop.** It retires at `now + max(2 s, dur)`, which is at or after its natural end.

## Off path and compile

- **Off path stays inert.** Every new call is guarded by `if (mAutoAnim)`, and the disabled `setEntityAutoAnimate` returns before `disarmAudit`.
- **Compile (/W3 /WX), no issues.**
  - The `tick` default argument is on the declaration only.
  - `LLGhostAvatar`/`LLControlAvatar` → `LLCharacter&` is an implicit upcast.
  - `isMotionActive(const LLUUID&)` is non-const and called on non-const refs.
  - `std::remove` is on a `std::vector<LLUUID>`.
  - `retired{d}` in the test is list-initialization.

## P2 (non-blocking)

- **Restart counts as POP.** A Studio **Restart** within 2 s of a handoff goes through `restartEntityAnimationAudited`, which does not disarm, so a user action is counted as POP. Disarm there too.
- **Already-fading adopted anims.** Adoption snapshots motions that are already easing out at handoff (they still report active). A stop of one within 2 s would be counted even though it is already fading. This is a rare false positive.

## Verdict

**GO.** Proceed to the single build, then ctest with `-DBUILD_TESTING=ON`.

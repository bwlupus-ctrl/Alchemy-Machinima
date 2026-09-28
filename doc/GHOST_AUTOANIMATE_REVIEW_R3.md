# Ghost AutoAnimate — adversarial review R3 (brief v3)

Read-only against HEAD `96aa1197555`.

## R2 fixes — verified

- **P1-1 (animesh clocks):** fixed. Control avatars are not paused in TRUE_MIRROR (only the wearer is, `:659`). The rebase `offset = last − new` on a control-avatar id change keeps the channel clock continuous.
- **P1-2 (sit poses learned):** fixed. The sit ids are now gates, and the 1 s buffer is discarded when a gate engages.
- **P1-3 (body loss placement):** fixed. The null-out and presence check sit between `:2470` and `:2471`, and `simloss` exercises the real null-source branch.
- **P1-4 (copy ordering, strips):** fixed. The copy rule runs before `applyEntityRuntimeState` at `:797-800`. The strip clear sits between `:3382` and `:3395-3396`; picks only issue inside `idleUpdate`, so the strip freezes as at HEAD.
- OFF-LIMITS additions verified; `lldirectorcast.*` exists.

**Ease-out stop plus direct ledger erase: sound.**
- `stopMotionInstance` sets a stop time and lets the motion fade (`llmotioncontroller.cpp:476`). While the controller is paused it forces an immediate stop instead (`:462`).
- A later re-add of the same id during the fade goes through `startMotion`'s deprecation path (`:421-431`): the fading instance is retired and a fresh one is created. No orphan and no double start.
- After the erase, neither `sync(empty)` nor `restartEntityAnimation` sees the id, so there is no double stop.
- On the animesh side, the stop branch calls `stopMotion(anim_id)` with the default non-immediate stop (`llvoavatar.cpp:7223`).

## P1

1. **A switch where next == cur deletes the only stand.**
   - §3.5 says "never the same id if another exists **and is resident**", then "Always: add next, remove cur after ease-in".
   - If the other group members are not resident, next = cur. Adding a no-op and then removing cur stops the stand, and the body falls to the default pose.
   - **Fix:** if next == cur, or no other member is resident, redraw the dwell and do nothing else.
2. **"Not resident" never becomes resident.**
   - Residency means "present in `LLKeyframeDataCache`", but the scheduler skips non-resident ids forever. Nothing ever triggers a fetch.
   - A pool **Loaded** in a fresh session is therefore stuck on the adopted anims, and test 9 would only pass if the anims happen to be cached.
   - **Fix:** on bind or Load, prefetch every uncached pool id with `LLCharacter::createMotion(id)` on the ghost, or on the control avatar for animesh. A new motion is created stopped (`llmotion.cpp:46`), so `updateLoadingMotions` fetches it without activating it (`:834`).
   - Allow `createMotion` in §9's "only startMotion/stopMotion" clause, and log `AUTOANIM-FETCH`.

## P2

- **The COLD re-issue set is ambiguous during a cross-fade.** If a mode change lands while a removal is pending, re-issue only the post-switch set. The pending removal is cancelled, not replayed.
- **Presence gating after seal.** The recorder is destroyed at seal, but body presence still needs the gate (a ground-sit without an AO). State that `autoAnimGated()` runs independently of the recorder whenever `mAutoAnim` exists.
- **Buffer semantics.** Say that a folded stop whose start was discarded contributes nothing. It must not produce a dwell sample or an edge.
- **AO hover on a remote source.** Use **3D** speed > 0.3 m/s instead of horizontal speed. Take-off and landing then gate, and the buffer discards the stand↔hover edge. The hover becomes an edgeless singleton that is never scheduled unless adopted. That shrinks the documented limit to "adopted while hovering".
- **Gap edges pull in any follow-up body anim within 2 s.** For example, a dance HUD that starts after the AO drops its stand joins the stand group. This is arguably "learned performance". Document it.
- **Offscreen clones.** INFERENCE: if a hidden or offscreen clone or control avatar is not motion-updated, its clock stalls. That is acceptable, but document it.
- **Freeze strips of a MANUAL clone** now snapshot the live source, not the clone's autonomous pose. That matches HEAD strip semantics; document it.

## Implementability

Codex can implement this unambiguously once P1-1/2 and the first three P2 wording items are added. Functions, data structures, anchors, settings location and OFF-LIMITS are complete.

## Verdict

**GO after the P1-1 and P1-2 edits.** Both are local, one-paragraph brief changes. A full R4 is not needed. A quick diff check of those two paragraphs is sufficient.

## R3 follow-up (v3.1)

- **M1 (§3.5 :256-258, §0c):** correct. With no other resident member, or next == cur, the dwell is redrawn and nothing is added or removed. This is consistent with the cold-pick and singleton rules.
- **M2 (§3.5 :273-275, §0c, §9 :485, test 9):** correct.
  - `LLCharacter::createMotion` is public (`llcharacter.h:142`), and motions are created stopped (`llmotion.cpp:46`).
  - `updateLoadingMotions` activates a motion only if it is not stopped (`llmotioncontroller.cpp:834`).
  - Bind, Load, Refresh and COLD are all covered. The control-avatar case defers until one exists.
- **Fetch storm (non-blocking):** a crowd bind costs up to channels × 64 `createMotion` calls per copy; for 50 copies that is about 3.2k transient instances. The keyframe cache is process-global, so one request per id is enough. Recommended edit: keep a process-wide `static std::set<LLUUID>` of requested ids and skip ids already in it. Erase an id once it is cached, or 60 s after it was requested, so a failed fetch retries.

**Verdict: GO** (the dedupe is advisory).

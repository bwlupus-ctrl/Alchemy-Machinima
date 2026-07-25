# Review batch 2 — strip diagnostics, replacement transfer, camera reset, region recovery

Acts on the independent ChatGPT deep review (`GHOST_STUDIO_CODE_REVIEW.md`). The two P0 blockers
are handled in a separate task. This batch is P1-4, P1-2, P2-9, plus a deliberately SCOPED-DOWN
version of P1-1 chosen by the user.

---

## 1. P1-4 — Remove ALL `GhostSit` diagnostics (highest priority here)
Temporary instrumentation is currently part of release behavior, and it leaked into **non-ghost**
render paths.

Remove completely — do not merely gate it behind a setting; **restore the hot paths to their prior
code**:
- the sit-transition probe called from every ghost idle (`llghostavatar.cpp:1114`, body
  `~1216-1417`), including the attachment/child UUID gathering, object resolution, face and
  vertex-buffer walks, and the log record;
- all probe state/counters on `LLGhostAvatar` (`llghostavatar.h` counters);
- `sit_probe` instrumentation in `LLDrawPoolAvatar` (`lldrawpoolavatar.cpp:847-997`);
- rigged batch / matrix-palette hooks in `LLDrawPool` (`lldrawpool.cpp:1019-1027`, `1080-1088`,
  `1112-1117`, `1147-1162`, `1193-1208`);
- **any added `isGhostAvatar()` branches in core render code that ordinary avatars now execute.**

The ground-sit bug it was diagnosing is fixed; the probe has served its purpose.

## 2. P1-2 — Complete runtime-replacement / duplication state transfer
`refreshEntityClone()` restores drive mode, speed and look but **omits `setEntityLoopMode()`**. It
transfers Director Subject A/B to the replacement runtime UUID but the separate **static cinematic
follow UUID** (`llcinematiccamera.cpp:130-176`) is neither transferred nor cleared — and removal has
the same omission. This is the SAME defect class as the already-fixed Subject-A regression.

Required:
- Centralize replacement into a single transaction, e.g.
  `onEntityRuntimeReplaced(stable_id, old_runtime, new_runtime)`, with **registered consumers**
  rather than ad hoc assignment lists, so a future runtime-UUID consumer cannot be forgotten.
- Transfer: loop mode, drive mode, animation speed, physics enable, look/style, procedural motion
  state, pause/frozen state, and **all** runtime-UUID references (Director Subject A/B, panel
  selection, cinematic follow).
- Apply the same discipline on **removal** (clear, don't leave dangling references).
- Pose continuity (exact skeletal transfer) is the ideal but is larger; if you cannot transfer the
  evaluated pose, say so explicitly and ensure at minimum that pause/frozen is applied only AFTER
  the replacement has a valid pose, not before.

## 3. P2-9 — Reset one-shot cinematic modes on mode/target change
Body Helix, Descent, Parallax Slide, Detail Sweep and Cable Cam use non-looping `cc_progress()`,
which clamps to the end pose after their duration. `updateCamera()` only resets `mPhase` after >3
inactive frames and stores no previous mode or target. So switching INTO one of these modes while
the camera is already active starts at its **final held pose** — it looks like the mode is broken.

Required: store the last mode and last resolved target UUID; on a change of either, reset phase,
tripod capture, smoothing seed and operator state. Allow an explicit exception only for modes
deliberately designed to preserve phase continuity (state which, if any).

## 4. P1-1 (SCOPED DOWN by user decision) — clones must RECOVER after region loss
The user explicitly does NOT want region-crossing migration. Their acceptance bar is:
> "so long as if I return to the region it's functional again for that session"

Today that likely does NOT hold: region teardown calls `gObjectList.killObjects()`, which marks the
client-only avatar AND its cloned attachment graph dead. The `ALGhostStudio::Instance` records
survive (model state, not region-owned), but they then resolve to nothing.

Required (small, NOT full migration):
- Detect that an instance's runtime clone has died (region teardown or any other object-list path)
  and mark that instance clearly recoverable rather than silently broken.
- Provide recovery from the STORED instance record — which already holds source id, foot/rotation,
  scale, drive mode, directed animation, speed, loop, look, physics, chaos — either automatically
  when a valid host region is available again, or via the existing Refresh affordance. State which
  you chose.
- Recovery must reuse the normal spawn path and re-run the full state application (the same set
  listed in item 2), so a recovered clone is indistinguishable from the original.
- Do NOT attempt live region-crossing migration or clone-graph rehosting.

## 5. P2-8 (small, closely related) — lifecycle refresh must not depend on the UI drawing
`refreshLifecycleStates()` is currently called only from `ALPanelGhostStudio::draw()`
(`alpanelghoststudio.cpp:298-302`). With both panel hosts closed, a clone killed by region teardown
leaves stale records indefinitely. Move lifecycle maintenance into the model's per-frame update
(`ALGhostStudio::updatePerFrame()`) or an object-death subscription. Item 4 depends on this being
correct.

---

## Constraints
- Client-only: nothing to the simulator; never mutate the SOURCE avatar or agent.
- Defaults unchanged for existing clones.
- Do NOT rename or remove any XUI control `name=` (72 `getChild` lookups must keep resolving).
- Do NOT reintroduce local-avatar-scale code (reverted, commit `ec82e1a49f8`).
- Note a separate task is concurrently fixing the two P0 blockers (animation-metadata parser and the
  clone animation ledger) — do not duplicate or conflict with that work.
- Self-review to 0 must-fix. Report: confirmation that ALL GhostSit code is gone (including
  non-ghost render branches), the replacement-transaction design and its consumer list, the camera
  reset conditions, the recovery mechanism chosen, and where lifecycle maintenance now runs.

# Reflect implementation report

2026-10-20. Contract: `doc/GHOST_REFLECT_DRIVE_BRIEF.md`, revision 4.
Implementation stays on `feature/director-kit-p0`, based on
`b3b5711f660178c080d6b9bf22623ff76f00d242`. Nothing committed, staged, pushed,
stashed, or switched. The viewer was neither built nor launched. Existing
untracked work and the already-dirty `vcpkg` submodule were left alone.

## Built, by brief section

### §§2, 4, 9: shared rules and tests

- Added pure `indra/newview/alreflectmath.h`: name pairing, quaternion and vector
  reflection, world root composition in LL multiplication order, foot reflection,
  Fixed capture/fallbacks, Live hysteresis, blink/physics/hand mappings, constraint
  offset rules, and the playback join/stop/echo/completion decisions.
- Added `tests/alreflectmath_test.cpp`, registered with `LL_ADD_INTEGRATION_TEST`
  beside `algazemath` in `newview/CMakeLists.txt`. Its 11 TUT cases cover T1–T11:
  literal 133-bone and 26-volume inventories, discriminator T2, 64 deterministic
  full-3D local samples, involution/composition, pitched/rolled world root tests
  at all five specified plane azimuths, facing, all capture fallbacks/hysteresis/
  NaN rejection, face params, hand poses, constraint frames, and playback races.
- T5 explicitly rejects wrong composition order, omission of S, and yaw-only
  reflection. Expectations use literal geometry/values, independently of the
  production helpers.

### §§4.4–5.5: clone drive and placement

- Appended `DRIVE_REFLECT = 4` and widened the ghost's real clamp to 4.
- Added `isPoseStampDriven()` while keeping `isTrueMirrorDriven()` unchanged.
  Actor Mover skips its pose/gaze writers for both stamp modes. The later Avatar
  Staging facing owner also excludes Reflect.
- Reflect pauses the clone controller and eye motion; AutoAnimate learning, Go,
  and autonomous linkset playback remain dormant. Animesh retains the ordinary
  ObjectAnimation ledger and held-repeat behavior, without reflecting its files.
- The partner table resolves names through `getJoint()` once at build/entry and
  stores source/destination **bone indices**. Per-frame writes use the table;
  collision volumes and scales are never stamped. Missing partners log their names.
- Reads source root rotation and foot placement in world coordinates, including
  a seated source's seat transform. Root uses `q_y(pi) * q_source * q_normal(pi)`;
  bones use swapped partners, `(-x,y,-z,w)` rotations and `(x,-y,z)` positions.
  Blink 58/59 swaps; physics 1206/1207 negates. All writes guard finite inputs.
- Bone positions are captured before the first Reflect stamp, and restored only
  on entry to Mirror/Directed: clear this ghost's attachment overrides, restore
  its saved bones, then reapply its local attachment overrides. Frozen/True Mirror
  retain the snapshot. Deferred first-build capture avoids saving an empty skeleton.
  A Reflect-to-Frozen transition also retains the stamped root facing; leaving the
  Reflect snapshot restores the current authored Studio facing.
- Session-only per-instance Fixed/Live plane, last valid plane, degeneracy state,
  and Mirror position. Capture is lazy, Live holds in the 0.1–0.3 m band, and close
  initial captures use source forward, last plane, then world +X. Copies clear
  Mirror position and all plane validity. Leaving Reflect except Frozen invalidates
  the plane, including runtime-only Director switcher re-drives.
- The post-pass gates on the **ghost's own drive**, and orders plane → optional
  foot push → stamp. Missing sources hold all three. Mirror position checks actual
  motion, group, Chaos, source, plane and drag ownership every frame. It writes the
  foot directly without a revision bump, clears the Chaos base, and calls only
  `setGhostFootPosition()`.
- `applyEntityTransform()` now suppresses rotation for stamp-owned facing. This is
  the sole deliberate change to True Mirror behavior. All single/group gizmo,
  transform, commit/cancel and revision-push callers pass this choke point.
  Proxy refreshes happen only outside a drag, including per-tick refresh while
  Reflect owns placement. Drag ownership includes whole groups and mouse capture
  before the proxy's later tick. Commit invalidates member planes; cancel keeps them.
- Face pair changes only the selected unlocked entity clone, uses the source's
  world forward and world foot, sets authored facing, captures Fixed, and selects
  Reflect. Distance is clamped to 0.5–10 m, default 2 m.

### §§5.6–5.8, 6.1: reachable controls

- Shared panel adds Reflect to the real drive combo, plus plane, Mirror position,
  Face pair, distance/reset, recapture and status controls. Existing Pitch/Roll and
  their wheel/reset paths were not changed.
- Added the same six rows to the Director v2 Clones data, in Motion → Reflect;
  the existing Pose & drive combo contains Reflect. Generated XUI, index, page
  source documentation and functional manifest were regenerated. The generator
  row pin is 2139, with a comment accounting for the six new rows.
- Added `/ghostanim reflect [all|selected]` and updated both usage strings. Existing
  multi-select/group drive fan-out remains in `onDriveModeCommit()`.
- Added persisted `GhostReflectFacePairDistance`, `GhostReflectVerify` (true), and
  `AlchemyAnimReflectInventory` (false). Both `GhostTrueMirrorCopy*` comments now
  include Reflect. Inventory preview has a globally bound Reflect checkbox.
- Audited hard-coded lists by searching drive enum names, drive widget names,
  True Mirror labels, and C++ combo additions. `alvcammonitorstrip.cpp` contains
  camera-add actions, not clone drive modes; it needs no Reflect item. No other
  independent hard-coded clone drive combo was found.

### Part B: self inventory playback (intent rewrite)

- `alreflectanim.{h,cpp}` retains the original-to-derived registry and
  PENDING/READY/FAILED asset builds. Playback is now one record per original id:
  the user's last intent (PLAY or STOP), its timestamp, LOCAL/INWORLD route, and
  flags for deferred natural-end cleanup and whether the derived motion has
  started for this PLAY. There are no echo sequence numbers,
  retired sequences, replay queues, or ten-second playback timeouts.
- Preview Play with Reflect enabled sets PLAY, replacing STOP immediately,
  including immediate replay and replay after 30 seconds. Duplicate previews
  share the same motion without restarting it. A later stock Play also supersedes
  old STOP intent. User Play can take over a currently signaled original; region
  echoes without an intent record retain stock behavior. Disabling Reflect leaves
  an existing reflected play reflected until it is stopped.
- H2 START for PLAY ensures the derived motion is running and consumes the stock
  original start. During a build neither motion plays. READY checks the current
  intent; it can start only for PLAY (and, for Inworld, a current region signal).
  Play, START, build completion, and idle all reconcile the same desired state.
  Thus a coalesced START callback cannot strand a replay: the current region
  signal suffices, without a remembered sequence or timeout.
- Each PLAY tracks three separate facts: `requested` with `requestedAt` when
  `startMotion()` is called after READY, `started` only when running activation
  is observed, and `completed`. The activation signal is
  `isMotionActive(derived)` (delegates to `LLMotion::isActive()`) together with
  `!isStopped()`. `LLMotion::activate()` sets active; the controller's loading
  path only clears stopped and returns success, so success is not activation.
  Pending activation waits without restarting or recording STOP. If activation
  is still unobserved two seconds after the READY request, completion takes the
  deferred natural-end path once, covering instant-stop short animations. Once
  running activation is observed, inactivity/stopped completes immediately;
  active looping/long motions have no completion timeout. Natural completion
  records STOP and defers cleanup and Inworld STOP(original) until idle. Direct
  H3 natural-stop signals also mark completed. Completed derived motions never
  restart; a new user Play resets all three facts and the request time.
- H2 START for STOP younger than 60 seconds consumes the echo and stops both
  local ids. START at or after 60 seconds expires STOP and uses stock behavior in
  that same call. Echoes never refresh the timestamp. STOP records also expire at
  idle; PLAY has no timeout. Region STOP stops both ids and clears either intent.
- User Stop/close replaces PLAY with timestamped STOP, stops both ids locally,
  and sends normal STOP(original) for Inworld. H3 natural end changes intent to
  STOP immediately, then defers local cleanup, `onAnimStop(original)`, and the
  Inworld STOP(original) until idle. A new Play supersedes deferred cleanup too.
  Local-only play, Stop, close, and natural end send no animation requests.
- A failed Local build records STOP and stops locally, marking the asset finished
  so preview controls reset instead of leaving PLAY pending. A failed Inworld
  build starts ORIGINAL locally only while intent is PLAY and the region
  currently signals it. It never resends START. The existing notification
  appears once per failed asset while PLAY; cancelled builds cannot start either
  motion or show a late fallback notice. Failed-original natural end uses H3 too.
- Both `sendAnimationRequest` and `sendAnimationRequests` retain their derived-id
  wire guards. All network requests use ORIGINAL ids. Preview priority follows
  the actual playback id, and Stop/region-stop state resets the preview controls.
- Original bytes still come from LLFileSystem or an original-id asset request.
  Scratch decoding, joint/constraint/hand-pose mirroring, round-trip validation,
  registration, and RAII cleanup are unchanged by this rewrite. Async callbacks
  retain avatar/session generation checks.

Part B tests call the production pure `decide(intent, age, event)`,
`readyDecision(playback, active, now)`, and `failedDecision(intent, route, signaled)` functions:

| TUT | Coverage |
|---|---|
| 11 | Late START after Stop at 0, 0.1, 9.9, 10, 11, 30, 59 and 59.999 seconds; no timestamp renewal. |
| 12 | START at/after 60 seconds uses stock immediately; idle expiry; no-intent START/STOP remain stock. |
| 13 | Replay immediately and after 11/30/59/61 seconds replaces STOP; first START and READY activate it. |
| 14 | START callback coalesced away: idle/current signal and READY reconcile PLAY, with no deadline or remembered echo. |
| 15 | Natural end records STOP, defers cleanup, rejects late START/READY; repeated end queues nothing; replay supersedes it. |
| 16 | User Stop during build; READY and failure after Stop never start either id, before or after expiry. |
| 17 | Build failure with/without region signal, later signal, and no-intent fallback rejection. |
| 18 | Region STOP stops both ids and clears PLAY/STOP; later READY stays stopped and unrelated START is stock; PLAY never times out. |
| 19 | Instant-stop short motion waits until exactly two seconds after request, then completes once without restarting. |
| 20 | Replay resets requested/started/completed; loading waits, actual activation starts, and inactivity completes once. |
| 21 | Local build failure resolves PLAY to STOP regardless of region signal; cancelled failure stays cancelled; Inworld fallback still requires the signal. |

| 22 | Loading then active then complete: no premature STOP; activation at the deadline wins over timeout. |
| 23 | Long looping animation stays PLAY while active through 86400 seconds. |

Tests 1-10 (reflection/Part A math) are unchanged.

R5 activation fix verification (2026-10-20): both touched translation units
pass separate `zs_check.ps1 -Files <one file>` calls, with `-IncludeTests` for
`tests/alreflectmath_test.cpp`. Standalone compile/link/run passes **23/23**.
Evidence: `.tmp/reflect_r5_zs_alreflectanim.log`, `.tmp/reflect_r5_zs_tests.log`,
`.tmp/reflect_r5_test_compile.log`, `.tmp/reflect_r5_test_link.log`, and
`.tmp/reflect_r5_tests.log`. The existing compile/link environment emits a
non-fatal missing `vswhere.exe` message.

`prebuild_guard.ps1 -ChecksOnly` passes all 11 gates using the existing wrapper
and required `GIT_CONFIG_COUNT=1`, `GIT_CONFIG_KEY_0=core.safecrlf`, and
`GIT_CONFIG_VALUE_0=false`; `.tmp/reflect_r5_prebuild.log`. `git diff --check`
and touched untracked-file whitespace checks pass. This fix changes only
`alreflectanim.cpp`, `alreflectmath.h`, tests 19/20 plus new tests 22/23, and this
report. No viewer build, commit, push, or stash; in-world acceptance is unrun.

R4 small-fix verification (2026-10-20): both touched translation units
(`alreflectanim.cpp` and `tests/alreflectmath_test.cpp`, the latter with
`-IncludeTests`) pass separate `zs_check.ps1 -Files <one file>` calls.
Standalone tests pass **21/21**, including the three new cases above. The
existing standalone compile/link commands succeeded; their environment setup
printed a non-fatal missing `vswhere.exe` message. Evidence:
`.tmp/reflect_r4_zs_alreflectanim.log`, `.tmp/reflect_r4_zs_tests.log`,
`.tmp/reflect_r4_test_compile.log`, `.tmp/reflect_r4_test_link.log`, and
`.tmp/reflect_r4_tests.log`.

`prebuild_guard.ps1 -ChecksOnly` passes all 11 gates (exit 0) through the
existing wrapper described below, with `GIT_CONFIG_COUNT=1`,
`GIT_CONFIG_KEY_0=core.safecrlf`, and `GIT_CONFIG_VALUE_0=false`;
`.tmp/reflect_r4_prebuild.log`. `git diff --check` and an additional whitespace
check of the four touched untracked files pass. This fix changes only
`alreflectanim.cpp`, `alreflectmath.h`, tests 19-21 (plus their Part B comment),
and this report. No viewer build, commit, push, or stash; runtime acceptance
remains unrun.

### §§7–8: instrumentation and documentation

- `[GhostReflect]` entry counts and build PASS/FAIL verdicts always log. Periodic
  foreign-writer (bones, excluding root), root-frame symmetry and world forward/up
  checks distinguish PASS, FAIL and INCONCLUSIVE. Missing source, skipped stamp,
  non-finite input, shape mismatch and disabled facing are explicit outcomes.
- Confirmed `LLGhostAvatar::updateEntityOuterTransform()` and light rig
  `scaledPoint()` apply uniform render scale outside skeleton world matrices.
  Symmetry compares both root-frame vectors at clone render scale, against a
  `0.01 m * scale` tolerance; it never divides already-unscaled joint positions.
- Inventory verification independently decodes original and mirrored bytes using
  fresh scratch IDs. Plain expected keys use the pure rules; actual data is
  decoded independently. It checks names, ordered key counts/times, xyz rotation
  codes (not w), position codes and constraint partners with the specified
  tolerances. It logs PASS/FAIL and prevents playback on a failed check.
- Start/stop traces include the original/derived IDs, route/reason and stop phase;
  natural end logs `STOP(original) sent`. Wire violations log FAIL and are blocked.
- Added clone/self Reflect instructions and all material §7 limitations to
  `doc/MACHINIMA_USER_GUIDE.md`, including shared playback and clone-of-self behavior.

## Re-anchoring from 057e558 to current code

| Brief anchor | Current implementation anchor / finding |
|---|---|
| `alghoststudio.h:97–111` enum | Enum is now at `alghoststudio.h:110`; append-only value 4. |
| `llghostavatar.cpp:1468–1559` clamp and suppression | `setEntityDriveMode`, now around 1475; preserve the existing True Mirror branch and add Reflect-only snapshot/lifecycle work. |
| `llghostavatar.cpp:1568–1759` stamp and face | New Reflect stamp is separate; original `applyTrueMirrorStamp` and all three original stamp/param helpers compare byte-identical to HEAD. |
| `alghoststudio.cpp:1425–1438` placement | `applyEntityTransform:1492`; position still pushes, rotation now checks stamp ownership. |
| `alghoststudio.cpp:2016–2053` facing/post-pass | `stampOwnsFacing:2213`, `updatePostObjectList:2222`; original True Mirror eligibility is retained, Reflect uses runtime mode. |
| `alghoststudio.cpp:414`, `:1345` freeze/resume | `applyEntityRuntimeState` and `setInstancePaused`; existing stored resume mode already round-trips appended enum values. Reflect-only root preservation supplements the position snapshot. |
| `alghoststudio.cpp:965` copy | Current funnel is `*copy = proto` at about 1009; fresh entity runtime IDs remain intact and plane/position options reset. |
| Formation/Chaos and later transform writers | Current `updateFormationMotion`, Chaos, `applyGroupTransforms`, `transformUnit` and proxy callers inspected; runtime placement predicates exclude procedural owners, and all later transform pushes share the rotation choke point. |
| `alghostmanipproxy.cpp:741–756` revision push | Current non-drag tick branch; also current grouped `endDrag` paths. Group-aware drag predicate samples actual mouse capture before tick. |
| `llactormover.cpp:4406,5793` | Current hooks around 4640 and 6039 use `isPoseStampDriven`. |
| Later Avatar Staging writer | `alavatarstaging.cpp:ghostStudioOwns` also needs Reflect; absent from the old brief's anchor table. |
| AutoAnimate old 633/892/3448 etc. | Current learning presence still allows only Mirror/True Mirror; dormant transitions/text and ledger/repeat gates updated, autonomous linkset step skips Reflect. |
| `alpanelghoststudio.cpp:1427,1525,2208` | Shared controller now also hosts the generated kit page. New binding at 364+, gated detail refresh at 1608+, existing group fan-out retained. |
| `panel_ghost_studio.xml:218–224` | Real combo is around 224; nested pose documents were both 790 and are now 874. v2 source data and generated `panel_dpage_cast_ghosts.xml` also updated. |
| Old preview H1 | `LLPreviewAnim::play`, `draw`, `onClose`, and `refreshFromItem`; both inventory context actions still enter `play`. |
| Old `llvoavatar.cpp:7158+` H2 | Current function begins near 7209; substitutions only at start/stop hooks near 7265/7290. |
| `llagent.cpp:2760`, `:3381` | H3 remains near 2761; guards in plural/singular senders near 3409/3433. |
| Old keyframe serialization / local anim precedent | Current `LLKeyframeMotion::deserialize/serialize`, public cached list, protected constraint type accessed through `auto`, and `LLLocalAnimMgr` scratch removal order rechecked. No llcharacter edits. |
| Old light-rig scaled-joint warning | Re-located to `alcinelightrig.cpp:scaledPoint` and ghost outer-transform code. |
| Scene persistence | Current clones are session-only. No scene serializer or saved-scene clone state was introduced. |

The five unchanged True Mirror functions are mechanically compared against HEAD
in `.tmp/reflect_true_mirror_parity.json`. The re-grep of all True Mirror/drive
sites is recorded in `.tmp/reflect_gate_regrep.txt`. No pipeline, shader, GL,
render-target, poser, skeleton XML or llcharacter file was edited. The LLVOAvatar
and LLAgent diffs are limited to their named hooks plus the necessary include.

## Exact click paths

All clone paths first require selecting an **Entity clone** in the roster.

| Control | Standalone / classic shared hosts | Director v2 |
|---|---|---|
| Reflect drive | Machinima → Clone Studio… → Pose → drive combo → Reflect; or Machinima → Director Console… → Cast → Clones → Pose → drive combo → Reflect | Machinima → Director Console… (F2) → Cast → Clones → Motion → Pose & drive → Drive → Reflect |
| Plane Fixed/Live | Same host paths → Pose → Plane combo | Cast → Clones → Motion → Reflect → Plane |
| Mirror position | Same host paths → Pose → Mirror position | Cast → Clones → Motion → Reflect → Mirror position |
| Face pair | Same host paths → Pose → Face pair | Cast → Clones → Motion → Reflect → Face pair |
| Distance / reset | Same host paths → Pose → Distance (m) / adjacent ↺ | Cast → Clones → Motion → Reflect → Distance (m) / row reset glyph |
| Recapture plane | Same host paths → Pose → Recapture plane | Cast → Clones → Motion → Reflect → Recapture plane |
| Status | Same host paths → Pose → line below Face pair | Cast → Clones → Motion → Reflect → Status |
| Self Reflect checkbox | Inventory → double-click an animation → Reflect beside Play priority → Play Locally or Play Inworld | Same inventory preview, independent of Director host |
| Inventory context actions | Inventory → right-click animation → Play Locally / Play Inworld; inherits the global Reflect checkbox state | Same |
| Global setting | Advanced → Show Debug Settings → select/type `AlchemyAnimReflectInventory` → Value; the preview checkbox changes this same persisted setting | Same |
| Distance / diagnostic settings | Advanced → Show Debug Settings → `GhostReflectFacePairDistance` / `GhostReflectVerify` → Value | Same |
| Existing face/facing settings | Advanced → Show Debug Settings → `GhostTrueMirrorCopyFace` / `GhostTrueMirrorCopyFacing` → Value | Same |
| Chat | Nearby Chat input → `/ghostanim reflect selected` or `/ghostanim reflect all` → Enter | Same |

If Advanced is hidden, use World → Show → Advanced Menu (the `UseDebugMenus` toggle). Plane and Mirror position require
Reflect; Mirror position forces Fixed and disables Live. Face pair can enter
Reflect directly and is disabled for a locked group, with a status explanation.
These paths are verified against XUI, controller callbacks and kit section data;
actual mouse interaction remains part of the in-world acceptance below.

### Rect arithmetic / fit proof

Both shared-panel pose documents increased together, 790 → 874 (+84). New widgets
occupy y=132..154, 160..182, and status y=188..206; the former y>=132 controls all
move down exactly 84, starting at y=216. The last diagnostics editor is now at
710..866, leaving 8 px inside both 874 px documents. New horizontal intervals are
8..148, 156..288, 296..446; second row 8..108, 116..276, 284..306. Status ends at
x=476 inside the 484 px document. No new rectangles overlap or overflow.

Standalone min width 520 gives the shared panel 504 px and its pose document
484 px. Classic Director's apparent 354/374 px embed literals are not its runtime
size: LLTabContainer reshapes leaf panels and `ALScrollFit::fillPage` fills the
Clones page. The 713 px Cast content at default width 1020 loses 170 px at the
850 px minimum (543 px before small borders), still wider than the 484 px document.
Both hosts install the existing named pose scroll controller. Vertical scrolling
keeps the expanded document reachable. The v2 card/row/scroller layout is generated,
with setting-bound reset metadata, and passes kit/layout checks. The preview was
widened to 420 px; priority occupies x=124..274 and Reflect x=286..386. Existing
preview label widths were corrected and its multiline advanced readout marked as
wrapping to satisfy layout checks.

## Gate results

The original implementation's UI/layout, generated-kit, math, and source gates
passed before the rewrite. The Part B rewrite passes all required checks below.
Only the five Part B translation units, their two headers, and this report are
edited in this round; Part A is unchanged.

| Gate | Result / evidence |
|---|---|
| Per-file `zs_check.ps1 -Files <one file>` | **PASS, 5/5** separate invocations for `alreflectanim.cpp`, `llpreviewanim.cpp`, `llagent.cpp`, `llvoavatar.cpp`, and `tests/alreflectmath_test.cpp` (`-IncludeTests`); `.tmp/reflect_rewrite_zs_*.log`. |
| Standalone `alreflectmath_test` compile/link/run | **PASS, 18/18**: original Part A tests 1-10 plus rewritten Part B tests 11-18; `.tmp/reflect_rewrite_tests.log`. No viewer/dependency build. |
| `prebuild_guard.ps1 -ChecksOnly` | **PASS, exit 0, all 11 gates**, with required `GIT_CONFIG_COUNT=1`, `GIT_CONFIG_KEY_0=core.safecrlf`, `GIT_CONFIG_VALUE_0=false`; `.tmp/reflect_rewrite_prebuild.log`. |
| `git diff --check` and untracked touched-source whitespace | **PASS**, including all new/untracked Part B source files and this report; `.tmp/reflect_rewrite_whitespace.log`. |
| Part A preservation | **PASS**: byte comparison with the pre-rewrite working tree for the Part A implementation/fixes; reflection math, tests 1-10, and asset-mirroring function are unchanged. Wire sender guards are also unchanged. `.tmp/reflect_rewrite_audit.log`. |

The unchanged installed guard runs through the existing sandbox wrapper,
`.tmp/reflect_prebuild_checks.ps1`. It uses a temporary Git global config in
`.tmp` (safe directory and empty excludes file) and forwards the compiler helper's
supported `-CachePath` to `.tmp/reflect_zs_flags.json`. No installed script or user
configuration is edited. All guard checks execute, with no object purge.

## Required in-world acceptance (§10)

These are UNRUN; no runtime PASS is claimed by static checks or TUT.

1. Face pair at 2 m, play right-arm-forward: clone left arm reaches toward source.
   Require entry 50 pairs / 33 centre / 0 unresolved PASS, symmetry PASS, facing PASS.
   Backward left arm is FAIL-A; wrong right arm is FAIL-B; Reflect combo behaving
   like True Mirror is FAIL-C. Repeat with a rotated seat, pitched/rolled source,
   and clones at 0.5×, 1× and 2× scale. Seated facing must include the seat angle.
2. Turn left 45°: clone turns right 45°. Check Fixed recapture, source-forward /
   steep-forward fallbacks, Live 0.1/0.3 m hold/recovery, and close-pass whirl note.
   Enable Mirror position and move toward the clone: it approaches at source foot
   height. Exercise path motion, Chaos, locked groups and missing source; status
   must name the owner and Reflect must stop writing placement.
3. Ghost Edit single and whole-group translate/rotate/scale, commit and cancel:
   facing must not flicker, foot pushes must not fight a drag, and the proxy must
   follow mirrored movement without snapping on the next drag. Verify Pitch/Roll
   wheel editing/reset and existing clone probes. True Mirror must behave as before
   except its allowed drag-facing fix. Freeze Reflect, resume, then enter Mirror /
   Directed: saved bone positions and attachment overrides must recover.
4. Verify both shared hosts and the v2 Motion page by the click paths above,
   including group drive fan-out, overlay disabled state, global distance/reset,
   AutoAnimate dormant controls/text, and source/runtime replacement. Save/load a
   scene and confirm clones remain session-only.
5. Self: right-arm animation locally with Reflect gives left-arm motion with no
   original flash. Inworld gives left-arm locally and original right-arm on a
   second viewer. Non-looping natural end must stop both, log STOP(original), and
   never log a derived-wire FAIL. Repeat looping, stop, close and region crossing.
6. Untick during playback: existing member remains reflected; a later join is
   stock. Open two inventory items sharing an asset: they share one play without
   restarting it. Explicit Reflect Play takes over an original already signaled by
   another source. Echoes for ids without intent must remain stock.
7. Stop before START echo; cancel an uncached local play before build success and
   before build failure. Nothing cancelled may start later or resend START. Exercise
   late STARTs at 1/11/30/59 seconds, START at/after 60 seconds, region STOP,
   immediate/30-second replay, and coalesced START callbacks; require no
   unmirrored flash or stuck playback. Confirm priority and preview button resets,
   including region stops while pending. Require build and round-trip PASS, and
   test the one-time stock-fallback notice for an actual failed build.
8. Check asymmetric body/props/one-sided expression limitations, unmirrored animesh,
   uneven-ground foot plants, self procedural layers, and Mirror / True Mirror /
   Reflect clones of a reflected self. These limitations are documented, not fixed.

The separate independent Opus review described by the brief's process is not
claimed here; no Opus review tool is available in this session. This report is
an implementation/test handoff for that review and subsequent viewer build and
in-world acceptance.


## Review rewrite scope (2026-10-20)

The Part B sequence/timeout machinery described in the first three reviews is
replaced by the intent rule above. Its prior review-fix descriptions are
superseded. This round preserves the pre-existing Part A implementation and
fixes (Frozen facing, one rotation push, and True Mirror skip) byte-for-byte.

No viewer build, commit, push, staging, or stash is performed. In-world acceptance
remains unrun; passing pure tests and syntax gates does not claim runtime acceptance.

## Floor reflection

2026-10-20, on `feature/director-kit-p0`, based on `249a9f05b96`.

Each entity clone now has a session-only **Orientation: Wall / Floor** choice
beside the Reflect plane controls. Wall is the default and retains its existing
behavior. Copies start with Wall. Changing orientation clears the old plane and
its fallback so Wall cannot inherit a Floor normal. Fixed captures lazily on the
next post-object-list pass when Floor is chosen, Fixed is set, or Recapture plane
is clicked, matching Wall's capture timing. Live updates height every frame.
Floor uses `getBaseRootPosition()` minus finite, nonnegative pelvis-to-foot,
exactly the `source_foot_agent` convention; it has no bisector hysteresis.

For a horizontal plane `z=h`, point/root reflection is `(x,y,z) -> (x,y,2h-z)`.
Let `H=diag(1,1,-1)` and retain the existing local bone stamp
`S=diag(1,-1,1)`. The proper clone root rotation is `H R_source S`, or in LL
multiplication order `q_y(pi) * q_source_world * q_z(pi)`. This is the existing
Wall mirror followed by a whole-body half turn about a horizontal axis in the
plane. Consequently `S(v) * q_clone = H(v * q_source)`: the source's right-hand
motion stays directly below that hand in world XY, the soles coincide at the
plane for a standing source, and the head points down. The formulas are next to
the Wall formulas in `alreflectmath.h`.

Floor reflects the source's actual world root position, including seated-source
transforms. It bypasses the ordinary upright foot API, which would add a positive
pelvis-to-foot offset. The clone's scale pivot remains at its reflected foot,
including while Frozen. Floor always owns position and root rotation, including
when the Wall copy-facing preference is off; later Studio/proxy transform pushes
cannot overwrite it. Wall's optional placement-owner guards are unchanged.
Returning to Wall or leaving Reflect/Frozen for another drive restores the
ordinary upright foot lock. Existing bone snapshot and facing restoration paths
remain in place; the reflected bone/face stamp and all five True Mirror functions
are unchanged.

Face pair and Mirror position stay visible and are disabled for Floor, both with
the tooltip **“Floor reflection always sits under the source”**. The stored Wall
Mirror position preference survives; returning to Wall with it enabled selects
Fixed as before. `postBuild()` populates the actual orientation combo in both
shared-panel and generated-kit hosts. No new setting or scene serialization.

Mirror light rig already consumes `mReflectPlanePoint` and `mReflectPlaneNormal`
generically after the pose pass. Floor supplies the same captured/live horizontal
plane, so projector, bounce and catchlight positions/axes reflect below it. No
light-rig or renderer changes were needed: copies remain clone-only, cast no
shadows, and asymmetric cookies remain unflipped (the accepted limitation).

Exact click paths (select an entity clone first):

1. **Machinima → Clone Studio… → Pose → Drive: Reflect → Wall/Floor combo →
   Floor**. The combo sits immediately above Plane: Fixed/Live. The second
   shared-panel host is **Machinima → Director Console… → Cast → Clones →
   Pose → Drive: Reflect → Wall/Floor combo → Floor**.
2. **Machinima → Director Console… (F2) → Cast → Clones → Motion → Pose & drive
   → Drive: Reflect → Reflect card → Orientation → Floor**. Fixed/Live and
   Mirror light rig remain in that card.

Shared-panel fit: both pose documents grow 874 → 902 px. Orientation occupies
y=132..154; all prior controls at y>=132 move down 28 px. The plane row is now
160..182, Face pair/light rig 188..210, status 216..234, and diagnostics ends at
894, retaining the 8 px bottom margin. Existing host/scroller wiring is retained.
The kit row count pin is 2142, with a comment for the new orientation row.
Sibling generated XUI diffs are source-hash updates required by the generator.

Validation:

| Gate | Result / evidence |
|---|---|
| Separate `zs_check.ps1 -Files <one .cpp>` calls | **PASS** for `alghoststudio.cpp`, `llghostavatar.cpp`, `alpanelghoststudio.cpp`, and `tests/alreflectmath_test.cpp` (with `-IncludeTests`). Logs: `.tmp/floor_zs_*.log`. Uses the existing supported workspace `-CachePath`. |
| Standalone `alreflectmath` tests | **32/32 PASS**, using the earlier `.tmp/reflect_math_compile.cmd` and `.tmp/reflect_unit_link.cmd`, then `.tmp/reflect_unit_bin/INTEGRATION_TEST_alreflectmath.exe`. Tests 29–32 cover z=h point/root reflection, unchanged world XY for the hand, full-3D root orientation, coincident soles, head down, horizontal plane capture/live updates, invalid input, and agreement with the generic light plane. Logs: `.tmp/floor_test_compile.log`, `.tmp/floor_test_link.log`, `.tmp/floor_tests.log`. The existing non-fatal `vswhere.exe` setup message remains. |
| Generated kit | Regenerated after final C++ edits; `director_kit_gen.py --check`, `director_page_sources.py --check`, and `director_functional_manifest.py --check` **PASS**. Logs: `.tmp/floor_{kit,sources,manifest}_check.log`. Manifest retains its five existing warnings. |
| `prebuild_guard.ps1 -ChecksOnly` | **PASS, all 11 gates, exit 0**, through the existing `.tmp/reflect_prebuild_checks.ps1` wrapper with `GIT_CONFIG_COUNT=1`, `GIT_CONFIG_KEY_0=core.safecrlf`, `GIT_CONFIG_VALUE_0=false`. It only redirects the supported compiler cache and uses the existing workspace Git config. Log: `.tmp/floor_prebuild.log`. |
| `git diff --check` | **PASS**. |
| `git ls-files --eol` | All three touched JSON files are **i/lf w/lf attr/text eol=lf**: mockup data, page sources, functional manifest. |

In-world checks remain **UNRUN**:

1. Exercise both shared hosts and Director v2 via the paths above. Confirm Wall
   default, Floor selection, enabled Fixed/Live, disabled Face pair/Mirror
   position with the exact tooltip, and independent per-clone/session state.
2. Use see-through ground and a standing source at ordinary clone scale. Raise
   the right hand: the reflected hand must be directly below it, soles together,
   head down. Move, turn, pitch/roll and animate the source; repeat seated and at
   different clone scales. Live follows foot height; Fixed holds the captured
   height during vertical source motion; selecting Fixed/Recapture captures anew.
3. With an asymmetric Self light rig, enable Mirror light rig and verify every
   copied light follows the same horizontal plane below the floor. Check clone
   illumination only, no copy shadows, and the accepted unflipped-cookie limit.
4. Switch Floor → Wall and back, including a previously enabled Wall Mirror
   position option. Freeze/resume, leave Reflect for Mirror/Directed/True Mirror,
   lose/recover the source, replace the runtime, and delete/copy the clone. Check
   ordinary facing/bones/placement recovery and light-copy cleanup. Floor must
   continue to own placement during path/Chaos/group/proxy activity.

The clone is only visible where the ground is see-through or reflective; opaque
ground can occlude this below-floor geometry. No ground material or render path
was changed. No viewer build/launch, commit, push, staging, or stash operation.

### P0 review fix: retain the Floor root while stamping stops (2026-10-20)

Addressed `.tmp/review_codex_floor.md`. Pausing the motion controller does not
pause `LLVOAvatar::updateOrientation()`: its world-up interpolation was rotating
the inverted root during Frozen and missing-source holds.

Each ghost now owns an `ALReflect::FloorRootHold` snapshot of the last finite
Floor-stamped root quaternion and global root position. The snapshot is captured
when the Floor root rotation is stamped, after reflected placement. A single
level-triggered enforcement block at the end of `LLGhostAvatar::idleUpdate()`
restores both components on every frame, after the base avatar's orientation
update and the mover/foot writers, before the outer transform is updated. This
runs while that runtime's instance has Floor selected, Floor placement remains
active, and its actual drive is Reflect or Frozen. It does not depend on a pause
edge or source availability. Live stamping refreshes the snapshot; Frozen and
missing/out-of-range sources retain it. Global storage survives agent-origin
changes, and the existing reflected-foot scale pivot remains in use.

The existing Studio placement/facing ownership guards now also retain a valid
Floor hold during Frozen, so subsequent proxy/Studio transform pushes cannot
discard it. Ordinary root/foot placement invalidates the snapshot; changing to
Wall or leaving Reflect/Frozen releases it through the existing placement and
facing paths. A fresh runtime starts without a snapshot. Wall, True Mirror,
other clones and real avatars acquire no Floor hold; `llvoavatar.cpp` and its
orientation interpolation are unchanged.

Added pure-helper lifecycle test **33**: entry before any stamp, 120 repeated
simulated upright/position overwrites for each of Frozen, source missing and
source out of range, exact root restoration and soles at the plane, independent
clone state, source recovery replacing the snapshot, invalid-position rejection,
and release/reentry without stale reuse. Release cases name Wall, Mirror,
Directed, True Mirror and leaving Reflect. The test exercises the production
snapshot/restore helper; it supplies the active/inactive level itself and does
not instantiate the viewer or execute avatar scheduling/Studio UI callbacks.

Validation:

| Gate | Result / evidence |
|---|---|
| Separate `zs_check.ps1 -Files <one .cpp>` calls | **PASS** for `llghostavatar.cpp`, `alghoststudio.cpp`, `tests/alreflectmath_test.cpp` with `-IncludeTests`, and the existing Floor `alpanelghoststudio.cpp`. Logs: `.tmp/floor_hold_zs_{ghost,studio,tests,panel}.log`. Each call uses the supported workspace cache. |
| Standalone reflection tests | **33/33 PASS** after compile/link with the existing `.tmp/reflect_math_compile.cmd` and `.tmp/reflect_unit_link.cmd`. Logs: `.tmp/floor_hold_test_compile.log`, `.tmp/floor_hold_test_link.log`, `.tmp/floor_hold_tests.log`. The existing non-fatal missing `vswhere.exe` setup message remains. |
| Generated kit | Regenerated kit, page sources and functional manifest after final C++ changes. All three `--check` invocations **PASS**; `.tmp/floor_hold_{kit,sources,manifest}_check.log`. The manifest retains its five existing warnings. |
| `prebuild_guard.ps1 -ChecksOnly` | **PASS**, all 11 gates, exit 0, using the existing `.tmp/reflect_prebuild_checks.ps1` wrapper with `GIT_CONFIG_COUNT=1`, `GIT_CONFIG_KEY_0=core.safecrlf`, `GIT_CONFIG_VALUE_0=false`. Log: `.tmp/floor_hold_prebuild.log`. No object purge or viewer build. |
| `git diff --check` | **PASS** after the report append. |

In-world acceptance remains **UNRUN**. Specifically verify Clone Studio → Pose →
Reflect → Floor → Pause (Frozen) over many frames; repeat with Live Floor while
the source disappears/goes out of range, then returns. The root must remain at
the last reflected position and orientation with soles on the plane until a new
stamp. Exercise Studio/proxy pushes during Frozen, resume, Floor → Wall, and
Reflect → Mirror/Directed/True Mirror; confirm existing facing restoration and
unaffected Wall/True Mirror, neighboring clones and real avatars. No viewer
build/launch, commit, push, staging, or stash operation was performed.


## In-world fixes 10/20

Fixed the Part B inventory round-trip validator and live preview toggle on
`feature/director-kit-p0`. This supersedes the earlier join-time-only behavior
and manual acceptance step 6.

Codec investigation: reflection preserves incoming w; inventory decode reconstructs
nonnegative w, and `packToVector3()` already normalizes and canonicalizes signs.
An unexplained negative w from reflection was not established as the cause, so
no extra canonicalization was added. Validation now compares normalized rotations
using absolute dot product, with a U16-derived bound including reconstruction
error near pi and a tighter angular bound away from w=0. Position validation uses
the actual symmetric [-5,5] range, allowing three U16 steps plus float noise for
the serializer's two floor passes and the decoder's zero snapping. Negating a
decoded position does not assume an exactly symmetric output code.

The exhaustive time-code regression reproduces the old decoded-float re-encoding
drift and adjacent-key collapse. Mirrored scratch keys now serialize from inside
their original U16 bins. Input duplicate times have already been coalesced by
stock decode; that decoded inventory remains the comparison baseline. Joint array
order is preserved by the codec. The validator still requires exact mirrored
partners at each array index and exact rotation/position key counts. First key
failure logs WARN with asset, joint, index, expected/actual time and value;
structural and constraint failures also log their first reason at WARN.

Ticking Reflect during playback keeps the original running during an asynchronous
build, then stops it locally and starts the READY derived copy. A fresh Reflect
Play retains its no-flash behavior. Unticking stops the derived locally and starts
the original locally, including during a pending build. Preview IDs and priority
observation follow mode swaps without treating them as completion. Active
LOCAL/INWORLD route and PLAY intent are retained. STOP, natural completion and
late READY cannot promote playback back to PLAY. Cached completion state is reset
only on a valid live mode change; another shared-asset Play respects the checkbox.

The toggle uses local motion-controller calls, without region START/STOP. The
Inworld original stays signaled through a toggle, and existing outbound guards
continue to prohibit derived IDs. Ordinary user Stop and natural-end behavior
retain the original-ID stop path.

Validation:

- Separate `zs_check.ps1 -Files <one .cpp>` calls: PASS for `alreflectanim.cpp`,
  `llpreviewanim.cpp`, and `tests/alreflectmath_test.cpp` (with `-IncludeTests`).
  Logs: `.tmp/reflect_inworld_zs_{anim,preview,tests}.log`.
- Standalone alreflectmath compile/link/run: **36/36 PASS**. Logs:
  `.tmp/reflect_inworld_test_{compile,link}.log`, `.tmp/reflect_inworld_tests.log`.
  Existing nonfatal vcvars setup message about missing `vswhere.exe` remains.
- Test 34 runs 20,000 real-style original-decode/mirror/serialize/decode codec
  cases, including near-pi rotations, negative-w serialization, double cover and
  quantized positions. Production helpers explicitly reject wrong rotation axis,
  wrong position axis, wrong partner and missing rotation/position keys.
- Test 35 checks all U16 codes 2..65535 at five durations, reproduces old time-code
  drift/collapse, and verifies preserved output codes. Codes near zero already
  subject to stock zero snapping are not claimed distinct.
- Test 36 exercises tick/untick, pending-build mode ownership, retained route,
  Stop and late READY for LOCAL and INWORLD through production state helpers.
  These are pure codec/state tests, not a live viewer or packet capture.
- `git diff --check`: PASS. `prebuild_guard` deliberately skipped while the
  concurrent Relight job edits shader/generated inputs.

In-world retesting remains UNRUN, including the two reported assets
`58e63007-c4f9-18a8-343c-9c5f526fece7` and
`58c57348-7488-9150-f419-f03f88deead5`; they were not fetched in this session.
Require Verify round-trip PASS and visible mirrored playback. Exercise Local and
Inworld tick/untick, uncached builds, original-until-READY, unchanged region
playback, no extra wire requests, Stop/close before completion, natural end and
shared-asset duplicate previews. The reproduced time-code defect is established
by tests; attribution of each reported real asset awaits that runtime check.

Only the five requested Reflect source/test files and this report were edited.
No shaders, pipeline, llrender or Relight files were touched. No viewer build,
prebuild guard, commit, push, staging or stash operation was performed.

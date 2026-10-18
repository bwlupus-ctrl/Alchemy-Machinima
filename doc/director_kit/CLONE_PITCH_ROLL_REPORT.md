# Clone Pitch / Roll report

Date: 2026-10-18. Worktree: `I:/am-clone-roll`, branch `wip/clone-roll`.

## Status

Pitch and Roll controls and quaternion editing are implemented. The generator, kit/XUI gates and standalone production-math tests pass. Viewer runtime behavior is not tested: no viewer build or launch was performed. Per the user's scope decision, the pre-existing scene-persistence gap is documented below. Clone scene serialization and its round-trip acceptance item are excluded from this task. The requested syntax checker was invoked for every touched `.cpp`, but skipped them because this worktree has no generated project files.

No commit or push. No access to `I:/alchemy-machinima` was used. No baseline, exemption, or gate threshold was relaxed.

## Mockup first

Edited `.tmp/clone_mockup/project/ClonesPage.dc.html` before implementing the controls:

- Added Pitch (deg) and Roll (deg) immediately after Turn in single/member, group and multi-selection detail arrays.
- Help: `Nose up/down` and `Lean sideways`; each row retains the existing reset-dot presentation.
- Multi-selection shows `mixed`; the place hint now distinguishes wheel adjustment over values from Place-tool rotation.
- The multi note says pitch/roll changes apply to all and locked groups rotate around their group centre. Free multi-selection edits preserve each unit's foot position.

This existing `.tmp` mockup directory remains untracked; it was not staged or moved.

## Implementation

`alghostrotation.h` contains dependency-free quaternion compose/decompose math, shared by production code and the executable standalone tests. Coordinates are +X forward, +Y side and +Z up. Composition is yaw about world Z, pitch about the yawed side axis, then roll about the pitched forward axis. Positive pitch dives. In LLQuaternion multiplication order this is `rollX * pitchY * yawZ`.

Read-back starts on the canonical Euler branch. Later reads choose the equivalent branch closest to that unit's previous angles, allowing pitch to pass through +/-90 degrees without flipping yaw/roll by 180 degrees. Exact singularities retain reference yaw and solve the coupled roll. An authored-angle reference is retained when the only quaternion difference is F32 quantization. Successful commits retain their requested branch. References now survive selection changes and are pruned only for deleted units (`indra/newview/alpanelghoststudio.cpp:1678`); facing actions invalidate only affected members, preserving unrelated clones and an unchanged group frame. Newly selected non-anchor units start from the anchor's branch so equal orientations can share equal readouts.

`ALPanelGhostStudio` now resolves Pitch/Roll alongside Yaw and installs normal commit callbacks and reset-table entries. Each angle edit reads the full rotation, changes only its component, and applies a full quaternion through the existing transform APIs:

- Single clone: existing `transformUnit` / `setTransform` path.
- Member: values are offsets from `mAuthoredLocal.mRotation * group.mWorld.mRotation`; edits compose back through that frame and use the existing pinned-member path. Authored slots are not changed.
- Group header: existing `transformGroup`, retaining the group's shared pivot and applying its frame to all members. Locked member rows normalize to the group ID, so duplicate selected rows cannot apply the group rotation twice (`indra/newview/alpanelghoststudio.cpp:925`, `:959`, `:1000`).
- Multiple units: the chosen component applies to every selected unit, preserving each unit's other components. Different components show literal `mixed`.

The small opt-in `LLSpinCtrl::setMixedValue` extension displays `mixed` while retaining the backing number. Wheel, arrow, focus and reset/commit restore the numeric editor before existing parsing/stepping. Other spinners retain their previous behavior.

Align feet still aligns the selected units' Z plane and now levels their world rotation while keeping yaw. Upright also preserves the displayed yaw branch. Both reseed their read-back cache after success; Face now invalidates it. For a member under a tilted authored slot/group, the member readout remains slot-relative after physical world leveling, so those offset angles need not be zero. Drop to ground still changes only foot Z and preserves the full quaternion.

## Wheel wiring and reset verification

The generated Yaw, Pitch and Roll controls are all `kit_row kind="number"` hosting `LLSpinCtrl`, with range -180..180, increment 1, two decimal places, default 0 and widget history disabled consistently with the existing per-clone Yaw row. The standalone panel has matching spinners and reset buttons. The existing generator produces the kit reset glyph from each default.

The wheel path is unchanged (`indra/newview/alkitrow.cpp:1282`, `indra/llui/llspinctrl.cpp:515`, `:188`, `:233`; Pitch/Roll callbacks at `indra/newview/alpanelghoststudio.cpp:555`):

`ALKitRow::handleScrollWheel -> LLPanel::handleScrollWheel -> child LLSpinCtrl::handleScrollWheel -> onUpBtn/onDownBtn -> existing commit callback`.

Native spinner modifier priority and factors remain Shift x0.01, Ctrl x0.1, Alt x10, otherwise x1. X/Y/Z retain their own 0.1 base increments; Pitch/Roll use Yaw's 1-degree base increment and exactly the same modifier implementation. No per-panel wheel override or alternate arithmetic was introduced.

Generator selftest pins all three rows' native type, exact range/precision/increment/default and history declaration. Functional manifest adds:

- `pitch_spinner` and `roll_spinner`: in-world disposable-clone fixture seeded with combined yaw/pitch/roll. The oracle checks the expected full quaternion after changing only the requested axis, as well as unchanged foot position and scale (`indra/newview/aluiperfselftest.cpp:3206`, `:7660`).
- `clone_rotation_wheel`: isolated native ALKitRow + LLSpinCtrl fixture (`indra/newview/aluiperfselftest.cpp:3104`). It tests X/Y/Z/Yaw/Pitch/Roll through row dispatch, both directions, three notches, clamps at both limits, commits, and clearing mixed values. A temporary keyboard fixture tests no modifier, Shift, Ctrl, Alt and combined-modifier priority without sending viewer key events; an RAII guard restores the original keyboard. Native position precision rounds its Shift step as before. The fixture does not touch live clones.

The runtime probes were added, not executed: they require the viewer. Generated manifest comparison against HEAD: **0 removed entry keys, 0 changed prior actions, 3 added entries**. Keywords include pitch, dive, tilt, roll, bank and lean.

## Scene persistence finding

This is a **pre-existing gap**, not a regression introduced by Pitch/Roll: clone instances and their rotations are not serialized by the current scene path. No clone scene persistence was added in this task.

- `indra/newview/alghoststudio.h:32` explicitly states: `Session-only; scene serialization is future work (documented).`
- `indra/newview/llfloaterdirector.cpp:4036` (`LLFloaterDirector::captureSceneData()`, through its return at `:4081`) writes cast, version/name, prism, light rig and settings. It does not serialize ALGhostStudio instances or rotations.
- `indra/newview/lldirectorcast.cpp:2232` (`LLDirectorCast::sceneData()`) writes cast membership, marks, locomotion, look/style, gaze and actor paths. It has no clone foot/rotation/scale/kind/group record.
- ALGhostStudio has no scene serializer: its session-only contract is explicit at `indra/newview/alghoststudio.h:32`; its transform/access APIs at `:648` through `:674` and session-owned groups/instances at `:858` and `:862` operate on in-memory data. Its AutoAnimate asset-file persistence is unrelated.

The user explicitly decided to document this gap and exclude clone persistence. **Scene save/load round-trip acceptance is removed from this task.** Old yaw-only quaternion read-back and F32 quaternion round trips remain pure-math checks, not scene-file tests. In-session selection, group and member operations remain in scope.

## Regeneration and validation

All three generators were run: `director_kit_gen.py`, `director_page_sources.py`, `director_functional_manifest.py`. The pinned source-row count changes **2124 -> 2126, exact delta +2**, with an explanatory comment. Other generated page edits are shared input fingerprint stamps; page-source evidence also changes as C++ line references move.

| Check | Result |
| --- | --- |
| Standalone production-math test, MSVC C++17 `/W4 /WX` | PASS: independent axis/order checks, yaw-only compatibility, 450 boundary/gimbal cases in double/F32, 10,000 random round trips, 4,002 scrolling steps through both vertical orientations; additional singular coupled-roll, scaled/sign-negated quaternion, branch-retention and independent-axis cases |
| `director_kit_gen.py --check` | PASS |
| `director_kit_gen.py --selftest` | PASS, including new native-spinner wheel/reset contract |
| `director_page_sources.py --check` | PASS |
| `director_functional_manifest.py --check` | PASS |
| `director_functional_manifest.py --self-test` | PASS: 12 rule fixtures plus generation/prerequisite tests |
| `kit_lint.py --all` | 0 errors, 14 existing warnings |
| `xui_layout_check.py --changed HEAD` | 41 files, 0 findings |
| `xui_layout_check.py --selftest` | PASS |
| Mockup component state validation | PASS: single, group, member and multi arrays contain Turn / Pitch / Roll in order; multi values are mixed |
| `git diff --check` | PASS |
| `zs_check.ps1` for all three touched `.cpp` files | NOT CHECKED: no generated vcxproj mapping in this worktree |
| Viewer functional probes / in-world single/member/group/multi interactions | NOT RUN: no viewer build/launch |

The XUI gate initially lacked the ignored runtime font. Downloaded the exact font from the repository's pinned `AlchemyViewer/alchemy-fonts` 2.5.0 package into this worktree's ignored `indra/newview/fonts/IBMPlexSansVar-Roman.ttf`; then reran both XUI checks successfully. The Place document remains fitted by its existing ALScrollFit controller; its hand-typed document height was removed and the containing placement section was extended for the added controls.

Requested syntax invocation was run separately for `indra/newview/alpanelghoststudio.cpp`, `indra/newview/aluiperfselftest.cpp`, and `indra/llui/llspinctrl.cpp`, with:

```text
-Repo I:/am-clone-roll
-BuildDir I:/am-clone-roll/build-Windows-vs2026-os
-CachePath C:/Users/xianw/AppData/Local/Temp/claude/C--Program-Files-AlchemyBeta/1dc5f917-00a4-4ca1-8703-3a4b1dd24eec/scratchpad/cache.json
```

Actual results:

```text
SKIP (no vcxproj mapping): I:\am-clone-roll\indra\newview\alpanelghoststudio.cpp
Nothing to compile.
SKIP (no vcxproj mapping): I:\am-clone-roll\indra\newview\aluiperfselftest.cpp
Nothing to compile.
SKIP (no vcxproj mapping): I:\am-clone-roll\indra\llui\llspinctrl.cpp
Nothing to compile.
```

No parent-worktree build directory was used. The standalone math executable compiled only the pure helper and test harness, not the viewer.

Additional check: `ui_draw_lint.py --changed HEAD` reports 3 errors, **0 new**, 3 warnings and 0 baseline additions. Neither the tool nor its baseline was changed.

Evidence logs include `.tmp/clone_math_test.log`, `.tmp/clone_mockup_test.log`, `.tmp/clone_draw_lint.log`, `.tmp/clone_zs_panel.log`, `.tmp/clone_zs_selftest.log`, `.tmp/clone_zs_spinner.log`, `.tmp/clone_kit_selftest.log`, `.tmp/clone_kit_lint.log`, `.tmp/clone_xui.log`, `.tmp/clone_xui_selftest.log`, `.tmp/clone_pages_check.log` and `.tmp/clone_manifest_check.log`.

## In-session acceptance and remaining runtime validation

- Single clone: enter and wheel Pitch/Roll through -180..180; positive pitch dives, roll banks about its own forward axis; resetting one axis preserves the others.
- Gimbal read-back: retain entered yaw/roll while wheeling pitch through +/-90 degrees. Select another clone and return, including a clone at +/-120 degrees pitch.
- Group member: edit the slot-relative Pitch/Roll offset; the member is pinned and other members/authored slots stay fixed. Reset member offset uses the existing group path.
- Locked group: rotate about the same shared centre; member spacing and local offsets survive. Selection helpers canonicalize a locked member to its group unit.
- Multi-selection: differing components show mixed; editing one component updates every selected unit and retains each unit's other components.
- Align feet: match the first selected unit's Z plane and level world pitch/roll while preserving yaw. A selected group levels its group frame while retaining authored/local member offsets.
- In-session copy/paste, duplicate, drop-to-ground and grouping retain the existing full-quaternion model paths. Native runtime verification is pending; no viewer was built or launched.
- Undo: no clone-transform undo journal exists on these panel/model paths. The existing Yaw and position rows already disable subject-unsafe widget history; Pitch/Roll follow that declaration (`scripts/code_tools/director_kit/kit_pages.json:422` through `:429`). No widget-only undo or new clone undo mechanism was added. Existing model transformations continue to preserve the full quaternion.

The continuation was audited for quaternion convention, member-frame composition, locked-unit deduplication, retained Euler branches and native wheel dispatch. Viewer runtime probes are implemented but **not executed**. The missing local Visual Studio project mappings remain a syntax-validation limitation, not a passing gate or a relaxed requirement.

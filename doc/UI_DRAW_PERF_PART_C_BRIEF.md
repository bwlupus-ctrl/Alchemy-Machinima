# Part C: draw() refresh conversion for the other Alchemy panels (v3)

v3 incorporates Codex plan review round 2 (NOT APPROVED):
- **Helper compare:** the helper's F32, combo and button-label compares must match exactly what each widget's own setValue/setLabel stores (fixes assigned to the Part B helper owner):
  - LLSpinCtrl compares raw;
  - fine-grid sliders use the snapToFineGrid path or are excluded;
  - combos keep their selected/unselected semantics;
  - buttons compare both labels.
- **Switcher Tier 2 dropped:** `resetToDefault(false)` and group reset do not fire the signal.
- **Lightbox mirror:** it must pass `force=true` when the selected projector changes.
- **Safety net:** every revision gate also re-diffs on a 1 Hz fallback tick, so a missed bump self-heals within 1 s.

v2 incorporated Codex plan review round 1 (NOT APPROVED). Its findings are summarised at the end. The user approved Tier 1 + Tier 2 in this build, with Codex reviewing this plan adversarially until it is approved, before any code is written.

Sources:
- `doc/GHOST_PERF_AUDIT_FABLE.md` sections 4, 6.C and 7.
- `CLAUDE.md` "UI PANELS MUST NOT DO WORK IN draw()".
- `indra/newview/aluirefresh.h`.

## Helper contract, a prerequisite owned by Part B's aluirefresh.h
No Part C code may be written until the helper has all of the following:
- **Focus guard.** Covers the control and ANY descendant (inner line editors), plus mouse capture. It has a `force` parameter that bypasses the guard, matching the existing guard in `alpanelcinelightrig.cpp:~1146`.
- **Widget-aware compare in `setValueIfChanged`:**
  - F32 sliders, spinners and slider ctrls compare after the control's own clamp and increment rounding (`llslider.cpp:~113`);
  - combos compare like `LLComboBox::setValue` (`llcombobox.cpp:~382`);
  - `setTextIfChanged` compares after CRLF removal (`lltextbase.cpp:~2418`).
- **Range and label helpers.** `setMaxValueIfChanged` (the max must be set before the value when the range shrinks) and `setLabelIfChanged`.
- **No setting writes.** Programmatic setValue never calls setControlValue (`lluictrl.cpp:~510`), and the helpers add no commit.
- **Global force switch.** `UIRefreshForceAlways` (Boolean, default 0) is read inside `ALRevisionGate` via LLCachedControl.

## Tier 1: own-state diffing and cached lookups, with no logic changes (Sonnet)
1. **Director console** `llfloaterdirector.cpp`. Swap the read-only status/header text writes to `setTextIfChanged`:
   - refreshAnimateTab header (~2625);
   - refreshCameraTab texts (~2941-2947);
   - the refreshStatusStrip write (the actual write is at ~3169).

   The cast list, columns and combos stay on their current per-frame logic, untouched.
2. **Cine Light Cues** `alfloatercinelightcues.cpp`: refreshStatus text (~203-225) to `setTextIfChanged`, and the five button enables (~379-383) to `setEnabledIfChanged`.
3. **Path editor** `alpanelpatheditor.cpp`: the tooltips (~617, 1368-1372, 1488) to `setToolTipIfChanged`. Enables use `setEnabledIfChanged`.
4. **Flycam recorder** `alpanelflycamrecorder.cpp`:
   - the time and status text (~110-112) and the labels (89-107, 116-127) use `setTextIfChanged`/`setLabelIfChanged`;
   - enables use `setEnabledIfChanged`;
   - **scrub slider:** convert only with `setMaxValueIfChanged` followed by the increment-aware `setValueIfChanged`, keeping max-before-value order. If the helper can't do increment-aware comparison, leave the scrub untouched.
5. **Lightbox** `alfloaterlightbox.cpp`:
   - cache the 1 s-tick `getChild`/`findChild` pointers (~630-662) per instance in postBuild. This is safe after `lbWrapTabPanel`, because reparenting uses addChild and does not destroy children. Keep the required-`getChild` vs optional-`findChild` null semantics;
   - diff-guard the per-frame Selected Light mirror (from ~686) with the widget-aware helpers. Pass `force=true` on every mirrored control (checkboxes, buttons, values) in the frame where the selected projector or light identity changes, so keyboard focus can never leave a newly selected light showing stale state. Other frames use the focus guard.
6. **Cine Light Rig** `alpanelcinelightrig.cpp`:
   - replace per-frame `gSavedSettings.getXxx(prefix + "Name")` lookups with validated 4-role x field arrays of `LLControlVariable*`, resolved once after settings init. Index them by `mFixtureRoleIndex`, which changes only in `onFixtureRoleCommit` (~1175). A null control falls back safely (default value, no write) and never keeps a pointer from a previous role;
   - keep the panel's own descendant-focus guard and `force` semantics. Only the value compare moves to the widget-aware helper. Object/group/shadow status texts are already model-guarded (~1403, ~2445), so leave them.
7. **Tracy zones.** Add an `LL_PROFILE_ZONE_SCOPED_CATEGORY_UI` zone on every converted panel's draw().

## Tier 2: narrow, signal- or own-state-gated only (Sonnet, after helpers exist)
1. **Cine Light Cues status.** Gate `refreshStatus` on `{rig slot, mCueRevision, active cue index, playback/transition active, timecode mode}`. All of these come from the model's own fields:
   - `setCueList`, load/apply scene and save-name bump `mCueRevision`;
   - `startCueTransition`, Go/Back/Goto/Release, timecode and follow advancement mutate the active/runtime state, which is part of the key.

   `cueListNames()` stays on its current guard. Filesystem changes have no counter.
2. ~~Director camera and animation switchers.~~ **DROPPED in v3.** `LLControlVariable::resetToDefault` fires its signal only when passed `true` (`llcontrol.cpp:300-317`). The generic external reset passes false (`llviewercontrollistener.cpp:136-149`), and group reset suppresses signals (`llcontrol.cpp:1178-1187`). So a signal-driven dirty flag can miss bank replacements. The switchers keep their current per-frame normalized compare, untouched.
3. **Fallback tick on the gate.** The cue-status gate (item 1) also re-diffs on the 1 Hz fallback tick provided by `ALRevisionGate`.

## Dropped from Part C (Codex round 1)
- Camera/animation switcher bank *revisions*, replaced by the setting-signal dirty flag above.
- Prop Mover roster and per-path revisions. Paths are mutable through `LLActorMover::editPath()` and many external paths.
- All `LLDirectorCast`-revision conversions (Director cast list and status strip, Lens Gaze and Gaze Cues cast combos). The cast has public mutable members and async name resolution, and the Director rows show subject, group, mark, look-at and liveness, none of which are covered.
- Cue-list-name revision gating.
- Weather `updateControlGroup` (not a draw path; already lazily cached) and Atmospheric pointer caching (no such function).
- **Lens Gaze `refreshControls()`**, deferred. It is a large value mirror, but every control is user-editable and gaze state changes from many sources (cues, Director, hotkeys). It needs its own audit and should not ride a mechanical batch.

## Constraints for every implementer
- Write code only. Do not build or compile single files into build-Windows-vs2026-os, and do not commit or git add.
- Do not touch vcpkg or dullahan, `indra/llui` or `indra/llcommon` (Part A), or the Part B files, except to read `aluirefresh.h`.
- The Bash tool is broken in this environment; use PowerShell, Read, Grep and Edit.
- Tag new code `[UIPerfC1]` (Tier 1) or `[UIPerfC2]` (Tier 2).
- Behaviour must be identical except for removed redundant writes.

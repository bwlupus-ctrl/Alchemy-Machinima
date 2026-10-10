# Adversarial review R5: rev5 (d58358f97f2)

Reviewer: Opus (independent context), 2026-10-10.
- Scope: `git diff HEAD~1` (the P0 spec, the rollout spec, `page_sources`, the Rig/Shot/Gaze Cues/Diopter contracts, `director_mockup_data.py`/JSON and `director_page_sources.py`).
- I ran `python -I scripts/code_tools/director_page_sources.py --check`: **exit 0**. It reports 35 pages, 1,839 rows, 0 unresolved rows and 22 multi-page settings.
- The `vcpkg` entry in `git diff HEAD~1` is only the dirty working-tree submodule. `git diff HEAD~1 HEAD -- vcpkg` is empty, so nothing was committed.

**VERDICT: GO** (0 BLOCKER, 0 MAJOR). The 5 minors below are bounded. Fold them into the WP1 brief (1-2) and the WP8 brief (3-5).

---

## R4 findings: verification

| R4 | Status | Evidence |
|---|---|---|
| M1 selector values | FIXED | Data: Shot `move_settings` has 50 options with `value` 1…39, 41…51. Labels and values were spot-checked at the gap: Detail Sweep = 39, Cable Cam = 41 (matching `panel_cinecam_params.xml:30` and `CinematicCamMode`). Rig/Rim selectors carry 0-3. P0 §3.4 reads and writes by `value`, never by index. Lint rule 10 and `--check` round-trip every pair against the v1 combo |
| M2 Gaze Play/Stop | FIXED (justified) | See below |
| M3 one-home vs shortcuts | FIXED | 8 declared shortcuts, each carrying `shortcut_of` in the data. Easy `cine_easy_cone_feather`/`cine_easy_shaft_length` keep their v1 callbacks. The shortcut and home ranges match exactly: feather 0-0.5 step 0.01 and shaft 0-64 step 0.1, in both `panel_cine_light_rig.xml:152-155` and `panel_projector_shafts.xml`. The Shot mode-block shortcuts are generated with the home ranges, a documented normalisation from 0.5 s to 0.1 s minimum. Lint rule 9 rejects any undeclared second writer or range drift |
| Minor 1 WP0 equality | FIXED | Scoped to unchanged pages; strips the additive `ew`/`es`/`value`/`shortcut_of` keys; decision pages are explicit transforms |
| Minor 2 retired launcher names | FIXED | 8 narrow `parity_allow` entries (`card_snapshot_*`, `card_diopter_*`, `card_gaze_cues_*`, `card_light_cues_*`) |
| Minor 3 scanner residuals | FIXED | Shaft is now attributed to `ALCineLightRigManager`; `ALPanelCineCamParams::modeTable()` is extracted explicitly |
| Minor 4 Diopter | FIXED | In-world label confirmation plus a preset→Custom history/Custom-bank test in the contract |

### Was the preview API justified over ACTION/CUT?

Yes.
- The transport cannot preview a single cue. `LLDirectorCast` exposes only `action()`/`cut()` (`lldirectorcast.h:419-420`), and `action()` "fires everything armed by the DirectorArm* settings": moves, camera and capture.
- `LLPresentationTime` has no setter or seek (`llpresentationtime.h`).
- Recorder `seek` affects only flycam playback.
- So ACTION would play the whole scene from zero, and could start a capture, just to preview one gaze cue.

A scoped two-method API consulted inside `LLActorMover::applyDirectorLookAt()` (`llactormover.cpp:6528`, cue evaluation at `:6649-6668`) is the smaller and safer change. Its contract is sound: false or no-op on an invalid actor or index; no cue-list or clock writes; cleared on Stop, removal or logout; a global barrier around Play/Stop. Three edge cases are unspecified (minors 3-5).

---

## MINOR (fold into WP briefs; not GO-blocking)

1. **Shortcut undo watch set (P0 §3.8 vs §3.11).** "Search and undo treat the home as canonical" can be read as "only the home page watches the setting". But §3.11 records a setting change only if the setting is watched for the **gesture's** page (`DIRECTOR_KIT_P0_SPEC.md:726`). State it explicitly: a shortcut's setting is in the **shortcut page's** watch set, and "home-canonical" applies only to search ranking, help text and the step label. Otherwise an edit made on Shot's Dolly Zoom block is never undoable from Shot.
2. **The Rig Easy feather is code-bound.** `cine_easy_cone_feather` has no `control_name` and writes through `onEasyConeFeatherCommit` only while Easy is active (`alpanelcinelightrig.cpp`, `onEasyConeFeatherCommit`). Its history therefore comes from the row's `recordWidget` hook, not the setting watch. Say so in the Rig contract so the shortcut test exercises the right path.
3. **Rebase the preview time explicitly (WP8).** `evaluateGazeCues` returns no override while `presentation_time < cue.mStartSec` (`lldirectorcast.cpp:1476-1485`). "A one-cue copy on preview-local elapsed time" must set the copy's `mStartSec = 0`, or evaluate at `cue.mStartSec + elapsed`. Otherwise Play does nothing for any cue that does not start at 0. Also state that the one-cue copy has no "from" cue: the transition starts from the live target, and influence keys still follow global time.
4. **Preview versus transport and capture (WP8).** The preview is consulted *before* track evaluation, so a forgotten preview would override a running take or recording. `LLDirectorCast::action()` (and capture start) must clear all gaze previews, or `setGazeCuePreview` must refuse while `isRunning()`.
5. **Preview lifetime and identity (WP8).**
   - Clear previews when the Director page hides or the floater closes; the floater is destroyed on close, and a preview is engine state that would otherwise persist invisibly.
   - Define `actor_id` as the same id `LLDirectorCast::setGazeCues`/`getGazeCues` use (runtime id for ghosts, avatar id otherwise), not "an avatar UUID".

---

## Regression / contradiction scan

- Rollout §2 now names Focus, Bloom & Glow and Shadows & Probes as pure-setting, which matches the `--check` table. No stale "only two" statement remains.
- P0 §4.2 and the selector table, rollout §1/§5/§12 and the Shot contract agree on values (1…39, 41…51) and the 8 shortcuts. The shortcut/home tables agree with the data's `shortcut_of`.
- The data has `CinematicCamUseSelected` as an ordinary non-home Shot occurrence, a link or read-only row. That is consistent with the shortcut-location rule, since it is not in a mode block.
- No new code anchors are wrong. Verified: `lldirectorcast.h:415-421`, `llactormover.cpp:6528/6649-6668`, `llpresentationtime.h` (no setter), the projector-shaft and Easy slider ranges.

## Final status across R1-R5

Every R1-R4 BLOCKER and MAJOR is resolved in the specs. The P0 spec, the VCam design (VC1-VC7), the rollout spec and the 35 generated contracts are consistent with each other and with the code. Codex may start **WP1**. Carry minors 1-2 into the WP1 brief and minors 3-5 into the WP8 brief, under the usual adversarial review of each WP diff.

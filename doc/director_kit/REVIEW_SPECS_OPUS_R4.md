# Adversarial review R4: rev3 (6e40678d8a7) + rev4 (7a6c5cc473b)

Reviewer: Opus (independent context), 2026-10-10. Scope: `git diff c7f913491e6..7a6c5cc473b`, which covers:
- the P0 spec, the rollout spec and the VCam design;
- `page_sources.json`/`.md` and the 35 page contracts;
- `director_page_sources.py`, `director_mockup_data.py` and the data JSON;
- `USER_DECISIONS_20261010.md`.

**VERDICT: NO-GO.** By the agreed rule, GO requires 0 BLOCKER and 0 MAJOR; this round has 0 blockers but 3 majors.
- Every R3 finding is genuinely fixed. The evidence generator is now trustworthy on the samples I checked (24 rows across 11 pages).
- The user decisions are applied faithfully for Diopter, Light Cues, Capture and Mirror.
- Two problems the rebuilt evidence now exposes, plus one decision binding, block GO:
  - the Shot mode selector maps option index to `CinematicCamMode`, but the real mode values are 1-based with a gap at 40;
  - Gaze Cues "Play/Stop" is bound to a preview capability the code does not have;
  - the one-home rule contradicts the P0 Rig Easy card.

All three have small, concrete fixes.

Counts: **0 BLOCKER, 3 MAJOR, 4 MINOR.**

---

## MAJOR

### R4-M1. The Shot "Move" selector writes the wrong camera mode for every option.
- **Doc/data:** the data's `shot_tab/move_settings` has `sel_setting: "CinematicCamMode"` and 50 options with `v` = 0…49, contiguous. P0 §3.4 says "the active index comes from `sel_setting`". There is no value map anywhere in P0, the rollout, or the `shot_tab` contract.
- **Evidence:**
  - Real values come from the combo at `panel_cinecam_params.xml:30` and the setting comment "1=Bone Lock (GoPro) 2=Orbit…" (`settings.xml`, `CinematicCamMode`, default 2). They run `1=Bone Lock, 2=Orbit … 39=Detail Sweep, 41=Cable Cam … 51=Pedro Cam`.
  - Value **40 is unused**. `ALPanelCineCamParams::modeTable()` (`alpanelcinecamparams.cpp:127-170`) has the same gap.
- **Why it breaks:**
  - Index semantics write `CinematicCamMode = 0` for Bone Lock and `1` (Bone Lock) for Orbit, and so on. Everything past option 38 is off by two.
  - The page also shows the wrong option's parameter rows for the live mode.
  - Shot is the largest page, and choosing a camera move is its core action.
- **Fix:**
  - Add an explicit per-option `value` to the selector contract: `<kit_option value="…">`, generated from the v1 combo item values. Use it for read (setting → option) and write (option → setting).
  - The generator takes the values from the hosted v1 combo, not from the data's `v` index, and fails on any mismatch or missing value.
  - `DirectorV2SelectedLight` (Rig/Rim) keeps index semantics by declaring values 0–3.
  - TUT: round-trip setting → option → setting for all 50 options, including 39/41.

### R4-M2. Gaze Cues "Play/Stop" is bound to a capability that does not exist.
- **Doc:** `pages/gaze_cues_tab.md` (Play = "transient selected-cue preview evaluated by the real `LLDirectorCast::evaluateGazeCues()`"; Stop = `LLActorMover::clearDirectorLookAtRuntime()`); user decision 3.
- **Evidence:**
  - `evaluateGazeCues(cues, seed, presentation_time)` (`lldirectorcast.cpp:1467-1492`) is a pure static function.
  - Its only consumer is the per-frame actor-mover gaze path, evaluated at the global `LLPresentationTime` (`llactormover.cpp:6649-6668`).
  - No "preview one selected cue" state exists. `floater_gaze_cues.xml` has no play or preview control, and the widgets `gaze_cue_play`/`gaze_cue_stop` exist nowhere; they are kit-native names attached to a pure function.
  - The contract also forbids touching the presentation clock. Codex would therefore have to invent a forced-cue override inside `LLActorMover`'s per-frame gaze runtime.
  - Light Cues, by contrast, binds to real transport: `cueGoto`/`cueRelease` (`alcinelightrig.cpp:4051, 4059`; `alfloatercinelightcues.cpp:374-384`). That part is correct.
- **Fix:** pick one and specify it.
  - (a) Play/Stop drive the existing Director transport: ACTION from the selected cue's time, and CUT. Name the exact `LLDirectorCast` start/cut and seek API, or say that seek does not exist and Play starts the take from zero.
  - (b) Define a new, scoped preview API, `LLActorMover::setGazeCuePreview(actor, cue_index)` / `clearGazeCuePreview(actor)`, consulted at `llactormover.cpp:6662-6668` before the normal evaluation. Give it its own review item and a feature-off proof.
  
  (a) uses only existing code. Either way, remove the claim that `evaluateGazeCues` is a command.

### R4-M3. The one-home table contradicts the P0 Rig Easy card, so P0 generation fails or Easy breaks.
- **Doc:** rollout §5 makes Atmosphere the sole writable home for `BDMergeProjectorVolumetricsFeather`/`MaxDistance`, and requires the Rig occurrences to be `link`/read-only. P0 §3.8 says a second writable occurrence is a generator/lint error "P0's two-page data follows the same rule". P0 §5.1 and `pages/cine_light_rig_tab.md` keep `cine_easy_cone_feather` and `cine_easy_shaft_length` as writable sliders.
- **Evidence:**
  - Both are C++-owned Easy controls: `mEasyConeFeather = getChild<LLUICtrl>("cine_easy_cone_feather")` with its commit callback, and `cine_easy_shaft_length` in `easy_gated_names` (`alpanelcinelightrig.cpp:741, 795, 923`).
  - In P0, Atmosphere is a verbatim legacy page whose `ps_BDMerge…` sliders are also writable.
- **Why it breaks:** obeying the rule turns two core Easy controls into links, which breaks the Easy card and its gating C++. Ignoring the rule fails generation.
  - The same rule also turns Shot's per-mode parameter rows into links inside the mode selector: Bone Lock "Joint" goes to Frame & Orbit; Dolly Zoom and Arc durations go to Moves. That is a UX regression on the core page.
- **Fix:** define a third occurrence class, a **shortcut**: a writable duplicate that is allowed when it lives in an Easy card or a selector mode-parameter block and is bound to the same setting or owner callback. Amber, reset and undo flow through the shared setting watch, and the home page is named in help.
  - The homes table then marks these occurrences `shortcut` rather than `link`.
  - Lint errors only on an unlisted second writable occurrence.
  - P0 §3.8 and the Rig contract name the two Easy shortcuts explicitly.

---

## MINOR

1. **WP0 equality wording is stale.** P0 WP0 acceptance ("removing ew/es reproduces the old JSON exactly") no longer holds globally. I verified that rev4 changed only `diopter_tab`, `gaze_cues_tab` and `light_cues_tab` (cards/sum/sections), removed only `capture_settings_tab`, and left chrome and group metadata identical. Restate it as: equality holds for every page not named in `apply_user_decisions`/`PAGE_EXCLUSIONS`, and the generator verifies that.
2. **Parity allow entries for retired launcher names.** `card_snapshot_*`, `card_diopter_*`, `card_gaze_cues_*` and `card_light_cues_*` remain C++-referenced v1 names (`addLauncherCard`, `cpp_name_patterns.json` P0 §4.4), and v2 no longer creates them. Add narrow parity replacement entries. `addLauncherCard` is already `findChild`-tolerant (P0 §3.17).
3. **Scanner residuals (do not block).**
   - `cine_*_shaft` is labelled `ALCineLightRigModel::setShaftEnabled`. The call at `alpanelcinelightrig.cpp:955-959` is `ALCineLightRigManager::instance().selected().setShaftEnabled`; the method is right but the class is not.
   - `ALPanelCineCamParams::modeTable()` (`alpanelcinecamparams.cpp:127+`) is rejected as "not in owning class", because the method header is preceded by `//static` and has a templated return type. That table drives Shot "Reset mode" and per-mode visibility, so list it in the Shot contract by hand.
4. **Diopter details.**
   - "Focus split" maps to `CineDiopterPower`. That is plausible, and the help text says so, but it is a loose reading of the user's words; confirm in the in-world check.
   - `CineDiopterPower` is a preset-owned control. Editing it auto-materialises and flips the preset to Custom (`llviewerfloaterreg.cpp:597-626`). Add an `ALKitHistory` test: edit Power under a non-Custom preset, undo, and verify the preset and look return without corrupting the Custom bank (`alDiopterHandlePresetTransition`).

---

## R3 status

| R3 | Status | Evidence |
|---|---|---|
| B1 option rows skipped | RESOLVED | 1,839 rows (1,509 card + 330 option); Shot has 334 rows, with options carrying `option_label`/`option_index`; `independent_json_rows` self-check |
| M1 attribute callbacks / llformat | RESOLVED | `auto_frame_resolve` → `Machinima.AutoFrameResolve` → `LLCinematicCamera::requestAutoReframe` (`alpanelcinecamparams.cpp:105-111`); `anim_slot_01` → `ALPanelDirectorAnimSwitcher::onSlotButton` (`:56-59`); `switcher_slot_01` → `ALPanelDirectorSwitcher::onSlotButton` (`:65-68`) |
| M2 collision attribution | RESOLVED | `scope_radio` and `btn_stop` → `ALPanelActorMover`; `hint` → `ALPanelPathEditor` only; Shot `btn_reset_all` → `ALPanelCineCamParams::onClickResetAll` (no avatar-light panel); Operator setting-name hits rejected (`FlycamOperatorEnabled` has no C++ binding) |
| M3 `?::?` placeholders | RESOLVED | 0 occurrences across 35 contracts; rows carry "wired in X; behaviour Y" |
| M4 object-target Mirror | RESOLVED | P0 §3.14: `fwd = (1,0,0) * root->getRenderRotation()` at the object branch (`alcinelightrig.cpp:2511`), same rule as the avatar path (`:2637-2641`); sigma −1 for any mirrored live target; rotated-prop test |
| M5 homes incomplete | RESOLVED (count) | 22 multi-page settings, all with `writable_home` set; the policy conflict is R4-M3 |
| MINOR 1-4 | RESOLVED | SSAO AO-binding is covered by the VC2 site list; option-row contracts regenerated; data-hygiene rows resolved (0 unresolved / 0 ambiguous / 0 USER DECISION); 1,000-sample wrap test kept |

## User decisions: fidelity check

| Decision | Status | Evidence |
|---|---|---|
| Diopter: everyday rows + full-editor button | FAITHFUL | `CineDiopterEnabled` (Boolean, default 0), `CineDiopterPreset` (U32, default 0; combo items 0-20 match `floater_ultimate_diopter.xml:72-93` exactly), `CineDiopterBlend` (F32, default 1, range 0-1, step 0.01, `:105`), `CineDiopterPower` (F32, default 2, range −10…10, step 0.25, 2 decimals, `:287`). The button targets `ultimate_diopter` (registered `llviewerfloaterreg.cpp:769`). Preset selection is a global setting listener (`llviewerfloaterreg.cpp:639-651`), so a plain setting-bound combo materialises correctly |
| Capture dropped | FAITHFUL | `PAGE_EXCLUSIONS` removes exactly `capture_group/capture_settings_tab`; the page had 0 rows, so no setting or control is orphaned; 35 contracts; P0 and rollout counts say 35. Restoring `DirectorLastTabName="capture_settings_tab"` falls back to `move_tab` (`llfloaterdirector.cpp:746-753`) |
| Cue lists in Director | Light Cues FAITHFUL; Gaze Cues **R4-M2** | Light: `cue_rows`, `cue_capture`, `cue_delete`, `cue_goto`, `cue_release` all exist (`floater_cine_light_cues.xml:27-83`; `alfloatercinelightcues.cpp:60-101`); model `setCueList` (`alcinelightrig.h:302`), `cueRevision` (`:314`). Gaze: list/add/remove are real (`alfloatergazecues.cpp:110-113`; `LLDirectorCast::setGazeCues`, `lldirectorcast.h:305`); play/stop are not |
| Mirror around the object's front | FAITHFUL | See R3-M4 above |

## New claims verified OK in R4

- Light cue `cueGoto`/`cueRelease` at `alcinelightrig.cpp:4051/4059`; `onGoto` at `alfloatercinelightcues.cpp:374`; `onDeleteCue` at `:339`; `onCapture` at `:307`.
- `LLDirectorCast::getGazeCueRevision()` (`lldirectorcast.h:309`).
- The Diopter widget names `diopter_enabled`/`diopter_preset`/`diopter_blend`/`diopter_power` exist in the Diopter floater, so hosting resolves.
- The data JSON diff between rev3 and rev4 is limited to the four decision pages.

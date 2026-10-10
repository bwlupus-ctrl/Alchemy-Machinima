# Adversarial review R3: rev2 (c7f913491e6)

Reviewer: Opus (independent context), 2026-10-10. Scope: `git diff 839ac92a80a..c7f913491e6`, which covers:
- P0 spec +110;
- VCam design +46;
- the rewritten rollout spec;
- `scripts/code_tools/director_page_sources.py` and its outputs `page_sources.json`/`.md`;
- 36 page contracts under `doc/director_kit/pages/`.

Claims were re-verified against the tree and against the data JSON, which has not changed since `241029a65e2` (`git log` on the file).

**VERDICT: NO-GO overall.**
- `DIRECTOR_KIT_P0_SPEC.md` and `VCAM_MONITORS_DESIGN.md` are now **GO-WITH-FIXES**. Every R2 item against them is genuinely fixed and the maths checks out. One major item remains: object-target Mirror (R3-M4).
- The rollout is NO-GO because its new evidence generator is defective:
  - it silently drops all 330 selector-option rows, which is where Shot, Rig and Rim keep most of their controls;
  - it misses the repo's dominant callback and dynamic-name patterns;
  - it attributes owners by bare name collision.
  
  The "16 not 20 conflicts" and "Shot drifted to 78 rows" claims are artifacts of that bug. Most of the 48 "USER DECISION" rows are scanner false negatives.

Counts: **1 BLOCKER, 5 MAJOR, 4 MINOR.**

---

## BLOCKER

### R3-B1. `director_page_sources.py` ignores selector-option rows, so the evidence, contracts, homes table and parity basis are missing 330 rows.
- **Doc/code:** `director_page_sources.py:373-386` (`flatten_data` iterates only `card.get("rows", [])`, never `card["opts"][i]["rows"]`). Rollout §1 ("1,509 rows … R2 cited 20 conflicts from an earlier 334-row Shot snapshot. The current approved Shot page has 78 rows"), §3 matrix, and §5 ("16 current homes").
- **Evidence:**
  - The data has 1,509 top-level rows **plus 330 option rows**: Shot 256 (the `move_settings` selector, 12 options from Bone Lock to Slow Zoom), Rig 58 (Key/Fill/Back/BG), Rim 16.
  - The JSON is unchanged since WP0 (`git log -- scripts/code_tools/director_kit/director_mockup_data.json` shows only `241029a65e2`). There was no "earlier snapshot"; the drift explanation is false.
  - Recomputed with option rows: 20 settings have a declared writable home on two pages, and the generator's `control_name` resolution adds `ActorMoverEndMode` and `TemporalWorldScale`, giving **22**.
  - The 6 conflicts the table drops are all Shot option rows: `CinematicCamJoint`, `CinematicCamVertigoDirection`, `CinematicCamVertigoDuration`, `CinematicCamArcDuration`, `CinematicCamArcGeneratorAngle`, `CinematicCamArcHeightDrift`.
  - The rollout matrix lists Rig 116 / Rim 25 / Shot 78, while the P0 spec builds the Lights selector with all four options. The two documents now disagree on what the Rig page contains, and parity would never check the per-light yaw, pitch and EV rows.
- **Why it breaks:** the "machine authority" omits a quarter of the product's controls, including the largest page. Every gate built on it (`--check`, homes, parity, audit manifests, completeness) passes while missing those rows.
- **Fix:**
  - `flatten_data` walks `opts[*].rows`, recording the option label and index.
  - Regenerate the outputs.
  - The homes table gains the 6 Shot conflicts, for 22 total.
  - Delete the "drift" paragraph and the "16 not 20" statement.
  - Add a self-check that the row total equals `sum(len(card.rows)) + sum(len(opt.rows))` straight from the JSON.

---

## MAJOR

### R3-M1. The scanner misses the repo's attribute-form callbacks and `llformat` dynamic names, inflating unresolved and "USER DECISION" rows.
- **Evidence:**
  - `scan_xui` only reads `commit_callback` **child elements** (`director_page_sources.py:209-213`). The English XUI uses the **attribute** form `commit_callback.function="…"` 760 times; there are 913 element-form uses. Example: `auto_frame_resolve` is flagged "no C++ reference", yet it is `commit_callback.function="Machinima.AutoFrameResolve"` (`panel_cinecam_frame.xml:145`), registered at `alpanelcinecamparams.cpp:107`.
  - `scan_cpp` matches only whole string literals. It misses `llformat("anim_slot_%02d", …)` (`alpaneldirectoranimswitcher.cpp:56`) and `llformat("switcher_slot_%02d", …)` (`alpaneldirectorswitcher.cpp:65`). Those 24 slot buttons are the bulk of the 48 "USER DECISION" rows.
  - A further 14 "USER DECISION" rows are fully specified kit-native rows. The data has `k:"link"` with `to` (for example `exposure_link`, `tr_roto_link`), or a button with `target`/`target_tab` (for example `open_lightbox_image`, `open_lightbox_shadows`, `cue_open_stack`, `diopter_open_editor`). P0 §4.2 already maps these to `link_to` and `action="lightbox:<tab>"`/`"floater:<name>"`, so they need no widget.
  - `cue_timecode_check` is a data-name mismatch for `cue_timecode_mode` (`floater_cine_light_cues.xml:23`; `alfloatercinelightcues.cpp:59`).
  - Rows named literally `check_box` (Focus, Filters, Atmosphere) are data defects resolvable by `control_name`.
- **Fix:** parse attribute-form callbacks. Recognise `llformat`/printf patterns and `prefix + "suffix"` concatenation, and cross-check them against XUI names by regex. Treat `link` rows and `target`-bearing buttons as kit-native and resolved. Fix the data names. After that, only the page-level questions listed at the end should remain as user decisions.

### R3-M2. Owner attribution is by bare name collision, so the contracts bind rows to unrelated classes.
- **Evidence:**
  - `scan_xui` and `scan_cpp` match a widget name in **any** XML or `.cpp`. Contracts then emit those matches as required bindings:
    - Shot `btn_reset_all` → "`ALPanelAvatarLightResponse::postBuild()` (`alpanelavatarlightresponse.cpp:155`), `ALPanelCineCamParams::postBuild()`". The first is the avatar light-response panel's reset, not the camera's.
    - Move `scope_radio` → `ALFloaterSceneExplorerFilters`.
    - Path `hint` → `LLEmojiHelper::showHelper`, `LLFloaterEmojiPicker::onOpen`.
    - `btn_stop` → the flycam recorder and the prop mover.
  - Where a widget name equals a setting name, ordinary setting reads are counted as C++ widget references. Operator's "66 C++-referenced rows" via `LLCameraOperator` are setting-name strings (the `panel_cinecam_operator.xml` widgets are named after their settings), and the same happens for Shot's `CineFisheye*` rows. These inflate the code-driven classification and generate bogus "preserve/call" duties.
- **Fix:**
  - Resolve each row's owner along the real host chain. For a v1 Director leaf, follow its `filename=`/`class=` embed; for a launcher card, use the owning floater named in the per-page `owner_roots` allow-list. Keep only XML hits under that root and C++ hits in classes registered for that root (`LLPanelInjector`/`LLFloaterReg` mapping).
  - Exclude string hits that occur inside `gSavedSettings`/`LLCachedControl`/`getControl` expressions.
  - Contracts must not list out-of-root candidates as required bindings.

### R3-M3. The contracts do not deliver the promised row-by-row "exact existing functions".
- **Doc:** rollout §2 item 3 ("the exact existing functions listed row-by-row").
- **Evidence:** the contracts contain **257** `?::?()` placeholders, where method resolution failed for lambdas and registrar blocks:
  | Contract | Placeholders |
  |---|---|
  | Operator | 69 |
  | Shot | 19 |
  | Frame & Orbit | 17 |
  | Path | 11 |
  | Atmosphere | 10 |
  | Takes | 8 |
  | Diopter | 7 |
  | Focus | 4 |
  
  Many of the remaining "handlers" are `postBuild()`, which is where a callback is wired, not the function that does the work. Shot, for example, cites `LLFloaterDirector::postBuild()` for `btn_set_a`.
- **Fix:**
  - Resolve the lambda body or registered callback name to the function it calls (for example `[this]{ onFoo(); }` resolves to `onFoo`).
  - Report "wired in X, behaviour in Y". Fail generation on any `?::?`.
  - Hand-review a sample of 20 rows per large page (Ghosts, Gaze, Dance, Path, Shot) against the code before WP6 starts.

### R3-M4. Object-target Mirror is disabled on a wrong premise, and a correct facing source exists.
- **Doc:** P0 §3.14 ("Mirror is not valid for an object target … stale serialized facing … disables the Mirror row"); risk 6; WP1 file list.
- **Evidence:**
  - The facing is not "stale serialized". `tickShared` default-constructs `Transforms transforms;` every tick (`alcinelightrig.cpp:2453`), and `readSettings` sets only mirror, yaw and pitch (`:1501-1503`). For object targets `mFacingAzimuthDeg` is therefore always **0**, and today's engine mirrors about world east (`alcinelightrigmodel.cpp:1058-1060`).
  - A correct facing source is in hand at the same point: the object branch already resolves `root = object->getRootEdit()` (`:2511`).
  - The avatar path computes facing as `(1,0,0) * root_joint->getWorldRotation()` (`:2637-2641`). The identical expression on `root->getRenderRotation()` gives the object's +X facing, which is the SL forward convention for both avatars and objects.
- **Why it matters:** disabling Mirror removes a user-visible capability, and it is itself an engine behaviour change. Mirroring about the object's facing keeps the feature working, fixes today's world-east bug, and lets the Light map use the same live-offset path for avatars and objects.
- **Fix:** in the `object_targeted` branch, set `transforms.mFacingAzimuthDeg = atan2(fwd.y, fwd.x)` from the root's render rotation, with the same `isfinite` guard. Keep the Mirror row enabled and `sigma = −1` for mirrored object targets. Add an in-world check with a rotated prop. This is a 6-line change in an already-permitted file. Because it changes how existing scenes with object targets plus Mirror look (from world-east to object-front), confirm it with the user (decision 4 below).

### R3-M5. The homes table is incomplete, and the rollout states the wrong count as binding.
- **Doc:** rollout §5 ("Count: 16 current homes chosen … not reproducible").
- **Evidence:** see R3-B1. The six Shot/Moves/Frame `CinematicCam*` conflicts exist in the current data.
- **Fix:** add homes for all six. The natural home is the Moves page for `CinematicCamVertigo*` and `CinematicCamArc*`, and Frame & Orbit for `CinematicCamJoint`. Make `--check` fail when the computed conflict set differs from the table.

---

## MINOR

1. **Preview SSAO override.**
   - VCam §4.5 sets `LLPipeline::RenderDeferredSSAO = false` for preview scope (the static exists, `pipeline.h:2180`). The soften/deferred-light shaders compiled with SSAO on must then not sample an AO target this render never wrote.
   - The pinned pack's `deferredLight` is reused across previews, so it would hold the previous preview's AO.
   - Specify binding a neutral (white) AO texture, or choosing the non-SSAO permutation, under `sPrismPreviewRender`.
2. **The Rig contract versus the P0 spec.** The Rig contract (116 rows) and the P0 Rig spec (17 cards including the 4-option Lights selector) disagree until R3-B1 is fixed. The P0 generator reads the JSON directly, so it would include the options, but the rollout's Rig parity and audit manifest would not. Regenerate the contract before WP4 review.
3. **Mis-named data rows are not user decisions.** Data rows whose `w` is literally `check_box` (Focus 2, Filters 2, Atmosphere 2) and `set_*` synthetic names (Image 53, Bloom 13, Atmosphere, Shadows) need classifying. The `set_*` rows are kit-native setting rows. These are data hygiene for Codex (resolve by `control_name`/setting), not user decisions.
4. **Wide-drag wrap-around.** P0 §3.14's Top drag uses `wrap180(a−a0)`, so a drag past 180° of pointer travel flips by 360°. Yaw is periodic, so this is harmless, but note it in the TUT expectations (a 1,000-sample out-and-back with a full turn must still return within 0.01°).

---

## R2 status

| R2 | Status | Note |
|---|---|---|
| N1 light-map drag runaway | RESOLVED | Immutable `DragStart`, absolute formulas, uneased `mapTargetFrame()`. The 2.5D inverse rotation and the Side/Top inverses were checked algebraically against `projectLight`; shared `r0 = mLightRadius/2.2` (`alcinelightrigmodel.cpp:1703`) |
| N2 wrong sources/owners | **PARTIAL** | The 15 mis-sourced pages now name the right owners (verified: Tron/Roto/Image/Practicals → `ALFloaterLightBox`; Light Cues → `ALFloaterCineLightCues`; Gaze Cues → `ALFloaterGazeCues`; Diopter → `ALFloaterUltimateDiopter`; Animate/Actor FX → `LLFloaterDirector`). The generator behind them is defective (R3-B1, M1, M2, M3) |
| N3 invented custom kinds | RESOLVED | Kinds removed. Existing lists and buttons are used; only bounded embeds for Ghost preview and the cue editor remain |
| M-R1 setValue flips | RESOLVED | `setValue` override calls `noteState` |
| M-R2 Easy adapter scope/re-entrancy | RESOLVED | Page-scoped handle, `mSyncingEasyControls`, effective-mode publication |
| M-R3 Remove undo and Gate | RESOLVED | Full arm/index/role/subscription record plus `gateRearm`, matching `llprismlens.cpp:6031-6057` |
| M-R4 quality profile and pack pinning | RESOLVED | Scoped statics exist (`pipeline.h:2178-2270`); pinned target outside the LRU (MINOR 1 caveat) |
| M-R5 exposure timing | RESOLVED | Previous presented frame, shared permutation helper, bloom excluded |
| M-R6 writable homes | **PARTIAL** | Rule is right; table missing 6 (R3-M5) |
| M-R7 audit explosion | RESOLVED | 256 states / 2 min per page / 72 min suite, no Cartesian product |
| M-R8 scene permission regression | RESOLVED | Skip and report, structural failures stay atomic, Director load continues |
| M-R9 external barrier | RESOLVED | `globalBarrier` through `LLFloaterReg`; playback start/stop with a held suspend token |
| M-R10 per-page extraction detail | **PARTIAL** | 36 contracts exist, but they contain `?::?` placeholders, collision bindings and missing option rows (R3-M2, M3, B1) |
| M-R11 WP numbering | RESOLVED | VC1-VC7 |
| M-R12 object-target Mirror | **PARTIAL** | Anchor fixed, but the remedy (disable) rests on a wrong premise (R3-M4) |
| MINOR 1-6 | RESOLVED | Width table 132/320; `rec_indicator` kept inside `kit_rec_cluster`; tab migration tests; rebind; 48×48 review; owner tokens for both cue editors |

Tally (16 R2 items, counting MINOR 1-6 as one row): 12 RESOLVED, 4 PARTIAL.

---

## User decisions: real ones versus scanner noise

Of the 48 rows the contracts label **USER DECISION**:
- 24 are dynamic slot names (`llformat`);
- about 14 are link or "open window" rows the kit already supports;
- 2 are data-name mismatches;
- 1 is an attribute-form callback.

None of those need the user. Codex resolves them.

Genuine product questions, phrased for the creator:

1. **Diopter page.** The mockup's Diopter page is a 6-line summary with an "Open Diopter editor" button. The real Diopter editor is a large separate window (560×720, about 345 controls).
   - Options: (a) keep the summary and button; (b) bring the handful of everyday controls (on/off, preset, strength, focus split) into the Director with a button for the full editor; (c) move every Diopter control into the Director.
   - **Recommended: (b).**
2. **Capture page.** The approved mockup has an empty Capture page.
   - Options: (a) fill it with snapshot basics (size, format, quality, "Take snapshot", "Open Snapshot window"); (b) drop the page.
   - **Recommended: (a).**
3. **Light Cues and Gaze Cues pages.** Today these are summaries that open the cue windows.
   - Options: (a) keep summary + open button; (b) show the cue list in the Director with add/remove/play, and keep detailed keyframe editing in the cue window.
   - **Recommended: (b).**
4. **Mirror when the light rig is aimed at an object (not an avatar).** Today it mirrors around a fixed compass direction, which is meaningless.
   - Options: (a) mirror around the object's front; (b) switch Mirror off for objects.
   - **Recommended: (a).**

## Claims verified OK in R3

- `alcinelightrigmodel.cpp:1703` `mLightRadius = effective_radius * 2.2` (the shared radius derivation).
- `LLPipeline` statics `RenderShadowDetail`, `RenderDeferredSSAO`, `RenderFarClip`, `RenderVolumetricLighting` exist (`pipeline.h:2178-2270`).
- `floater_ultimate_diopter.xml` is 560×720 with a `tab_container` and about 345 value controls.
- `Machinima.AutoFrameResolve` is registered at `alpanelcinecamparams.cpp:107`.
- The slot names come from `llformat` at `alpaneldirectoranimswitcher.cpp:56` and `alpaneldirectorswitcher.cpp:65`.
- `cue_timecode_mode` exists (`floater_cine_light_cues.xml:23`; `alfloatercinelightcues.cpp:59`).
- The four rows the scanner skips as `target`/`link` carry `target`/`target_tab`/`to` in the data.
- The data file is unchanged since `241029a65e2`.

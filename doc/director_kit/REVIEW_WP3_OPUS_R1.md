# Director kit WP3 + gate tooling: adversarial review R1 (Opus)

Scope: commit 62a140703a3 (WP3 generator, generated XUI, Codex gate triage) on top of aa4e0b9df6d (Haiku gate tools). The spec is `DIRECTOR_KIT_P0_SPEC.md` (sections 3, 4, 6, WP3, and the ledgers), `DIRECTOR_KIT_ROLLOUT_SPEC.md`, and `USER_DECISIONS_20261010.md`. No source or tool file was edited.

**Verdict: FIX-NEEDED.** There are 2 BLOCKER, 9 MAJOR and 14 MINOR findings.

Reproduce the evidence with the review fixtures in `doc/director_kit/review_wp3_r1/` (all are read-only and work on temp copies of the real tree; run each with `python -I`):
- `fixtures.py`: 13 adversarial fixtures against `kit_lint` and `director_parity`. All 13 report **MISSED**: K1–K8 for lint, P1–P5 for parity.
- `probe_xui.py`: a vacuity probe for `xui_layout_check` on the legacy pages.
- `verbatim.py`: an independent re-parse of all 31 legacy pages against their v1 leaves.
- `params.py`: checks the generated attributes against the WP1 C++ `Params` blocks.

## What is verified correct

- **Determinism.** Running the generator twice into scratch directories gives byte-identical output (37 files) that equals the repo. `--check` exits 0 under `PYTHONHASHSEED` values 1, 777 and 99999. `--selftest` passes.
- **Classification (4.3).** Kit (P0) ×2, pure kit ×1 (Bloom), launcher ×1 (Tron) and legacy ×31 are reproduced, and the generator hard-fails on drift.
- **Verbatim copy.** Independently re-parsed from the bytes on disk, the children of all 31 legacy pages are identical to their v1 leaves. This holds both canonically and with exact leaf text: 0 mismatches. The v1 floater has no duplicate panel names.
- **Hosted fidelity on Rig and Rim.** All 176 + 43 hosted controls equal their v1 nodes, apart from spec-sanctioned changes: geometry and label stripped, sliders given `show_text`/`text_width=52`, `check_box`→`kit_switch`, and the upgrades. Card and option enable checkboxes are not emitted as rows.
- **Selector values.** In the data, Shot `CinematicCamMode` is exactly 1..39, 41..51 (50 options). Rig and Rim options emit 0..3. `director_page_sources.py --check` passes (35 pages, 1,839 rows, 0 unresolved).
- **Shortcuts, pages and report.**
  - The 8 shortcuts match `EXPECTED_SHORTCUTS` in both the generator and page_sources.
  - Capture is dropped (35 pages).
  - The v1 `*_tabs` names are kept.
  - The 10 upgrades and the KITNAMES lines are in `gen_report`.

## BLOCKER

### B1. The "page master as home" relaxation ships a dead, duplicate master switch on both P0 pages

**Evidence**
- The data gives `rig_status` the enable `cine_enabled`/CineLightRigEnabled and `rig_rim` the enable `cine_rig_rim_enable`/CineRigRimEnabled. Each equals its page master.
- `director_kit_gen.py:494` drops the card's `enable_setting` but keeps `enable_widget`. The result is in `panel_dpage_light_cine_light_rig.xml:87` and `panel_dpage_light_rim.xml:8`.
- `ALKitCard` searches only its own subtree (`alkitcard.cpp:228`). It therefore creates a second, **unbound** `ALKitSwitch` with the same name. The page master is created separately at `alkitpage.cpp:272-276`.
- That switch is always off, so the card body is permanently dimmed (`alkitcard.cpp:768`). Toggling it changes nothing. The v2 tree also contains two widgets named `cine_enabled` and two named `cine_rig_rim_enable`.
- Codex reached "0 errors" for this by de-binding the card switch and adding `PAGE_MASTER_HOMES` to `director_page_sources.py`.
- `kit_lint` rule 4 never sees the collision, because it counts only XML `name=` attributes. Fixture K1 confirms this.

**Fix**
- The generator omits the card's `enable_widget` and `enable_tooltip` when they equal `page.master.w`. The kit_page owns the master, as section 4.5 states.
- Revert `PAGE_MASTER_HOMES`. It is unnecessary once lint skips `kit_lazy_page`, and it moves page_sources away from the rollout's 22-home table.
- `kit_lint` rule 4 must add the names the kit creates: `enable_widget`, `master_widget`, `mode_widget`, `label_name` or `<w>_label`, `<w>_reset/_value/_unit/_state`, `kc_<id>`, or simply the KITNAMES.
- Add K1 as a selftest fixture.

### B2. `xui_layout_check` is vacuous on all 31 legacy pages, so "0 findings on 37 files" proves nothing

**Evidence**
- The generator wraps each legacy `kit_page` in a sizeless `<panel name="<page>_legacy_host" follows="all">` (`director_kit_gen.py:551`). This is not the file form of spec 4.3, where `kit_page` is the root.
- `check_file` returns early on a zero-size root (`xui_layout_check.py`, `if root.rect is None or width <= 0`). Only the kit-geometry findings survive.
- `probe_xui.py` injects a 5000×9000 button into `panel_dpage_cast_move.xml` and the checker still reports **0 findings**.
- With `kit_page` as the root, the same 31 pages report **32 findings** (overlap, clipped, text-width, no-scroller).
- Those 32 also differ from the v1 leaves, where `floater_director.xml` reports 17. The spec 6.3 rule "exactly the same findings as the v1 leaf" is therefore proven only on a fixture pair, never on the real tree.

**Fix**
- Emit `kit_page` as the file root for legacy pages, or give the wrapper the leaf size and lay it out.
- `xui_layout_check` must hard-error on a zero-size root that contains a legacy `kit_page`, rather than skipping it.
- Add a real-tree regression: for each legacy page, its findings must equal its v1 leaf's findings, keyed by widget name.
- This also exposes a runtime gap for WP2: `ALKitPage` has no parent-fill (`alkitpage.cpp` never follows its parent). A `kit_page` without geometry inside a follows-all wrapper stays 711×525 at the default origin. That applies to the P0 `panel`+`kit_page` form as well.

## MAJOR

### M1. Generated compass dials violate sections 3.3, 3.13 and 4.4

**Evidence**
- All 9 dials (`panel_dpage_light_cine_light_rig.xml:204,207,…,454`) lack `kit_geometry="true"` and `show_readout="false"`.
- Without `kit_geometry` they use the legacy geometry: radius `min(w,h)/2-12`, letters outside, a 0.25 dead zone, and no tab stop or keyboard (`alcompassdial.cpp:91,136,165,243`).
- `show_readout` defaults to true (`alcompassdial.cpp:49`), which duplicates `<w>_value`.
- The upgrade also copies the slider-only attributes `can_edit_text` and `show_text`, which are not ALCompassDial params (`params.py`). Spec 4.4 keeps only `control_name`, min, max, increment and decimals.
- No lint rule covers any of this (fixture K8). This is the user's U3 arc.

**Fix**
- Upgraded and native dials emit `mode`, `kit_geometry="true"` and `show_readout="false"`, and drop non-dial attributes.
- Add a lint rule for `compass_dial` inside `kit_row`.

### M2. The search index is built from raw data, not the config-applied page

**Evidence**
- `index_bytes()` walks `flatten_pages(data)`. So `director_kit_index.xml` has widget `CineGobo01` for the gobo row; the page has `cine_gobo_preview`, which means `findRow` or `navigateTo` will fail.
- The index flags `cine_shadow_fixit` as advanced (`a`), but the generated row is not (row_overrides `a:0`).
- All 10 `add_rows` (the clip readouts, `cine_group_status`, `cine_shadow_hint`, `night_mask_status`, `cine_rimq_status`, …) are missing from the index.

**Fix**
- Index the `apply_config` result.
- Add a `--check` assertion that every kit-page index entry resolves to a generated `kit_row` widget, and the reverse.

### M3. Parity check 3 is tree-global, not owner-scoped

**Evidence**
- `ALPanelCineLightRig::postBuild` calls `getChild<LLTextBox>("cine_rimq_status")` and `getChild<LLButton>("cine_rimq_enable_tag")` (`alpanelcinelightrig.cpp:767-768`).
- Those names exist only in `panel_dpage_light_rim.xml`, yet parity reports PASS (fixture P3).
- Spec 5.1 says "Every name in its getChild list must exist on **this page**". As it stands, the WP4 rim extraction (section 3.18) cannot be proven by the gate.

**Fix**
- Resolve each owner's requests against the page file whose root `class` matches: `panel_cine_light_rig` → the Rig page, `panel_rig_rim` → the Rim page.
- The gate is expected to go red until WP4 lands.

### M4. Codex extended `parity_allow` globs to check 3

**Evidence**
- `cine_*_label` and `cine_*_card` now excuse **C++-referenced** getChild targets.
- In fixture P1, removing `cine_radius_label` (`getChild<LLTextBox>`, `alpanelcinelightrig.cpp:785`) still passes. The glob's own reason says "the referenced ones must exist".

**Fix**
- In check 3, honour only exact-name entries from a separate `retired_cpp_targets` list (today: `cine_rig_rim_preset`). Never use globs there.

### M5. Parity check 2 accepts the shell's on-pip observer as a binding

**Evidence**
- `build_v2_index` adds every `master_setting`, including `kit_lazy_page.master_setting` in the floater.
- In fixture P2, deleting the Rig page's `master_setting` still passes, even though nothing on the page binds CineLightRigEnabled.
- This contradicts lint, which correctly says the lazy page is not a writer.

**Fix**
- Exclude `kit_lazy_page` and `kit_shell.rail_item` from `B.controls`.

### M6. `kit_lint` rule 9 has four holes

**Evidence**
1. Fixture K3: code-bound rows are not writers. A `bind="code"` row writing BDMergeProjectorVolumetricsFeather on Bloom passes. The real `cine_easy_cone_feather` shortcut is never validated at all, which is why only MaxDistance shows up as pending.
2. Fixture K4: "Easy card" is implemented as "listed in any `kit_page.mode`", so a shortcut in Advanced-only `rig_main` passes.
3. Fixture K5: `pending_homes` skips the range/step/unit drift comparison completely. `cine_easy_shaft_length` 0..99 against a 0..64 home passes, even though the data has the home row.
4. Fixture K2: the same-page exemption allows **any** duplicate writer with any range. The legitimate ones are the Easy↔Advanced pairs.

**Fix**
- Model writers as `setting` on any `kit_row`, whether bound by setting or by code.
- Treat as "Easy" only the cards in the `mode_on` mode list, or rows inside a `kit_option`.
- While the home is pending, compare the shortcut against the home row in the data/page_sources.
- Limit the same-page exemption to the Easy↔Advanced pairs.

### M7. `check_kit_pages.py` is red on the committed config

**Evidence**
- `python scripts/code_tools/director_kit/check_kit_pages.py` exits 1 on four unknown top-level keys: `pending_settings`, `pending_homes`, `shared_writers` and `help_audit_allow`.
- Spec 4.7 says Codex writes the config "exactly". The orchestrator's gate list never ran this validator.

**Fix**
- Move the gate-policy keys to their own file, for example `director_kit/gate_policy.json`, giving each entry a spec citation and an `expires: WPn`. Alternatively, amend the spec and the validator together.
- Add `check_kit_pages.py` to the gate list.

### M8. Transport chrome ignores the 4.6 icon and pill contract

**Evidence**
- These buttons carry text labels in 24–28 px slots and will clip: `btn_set_marks` "Set marks", `btn_reset_marks` "Reset to marks", `btn_arm` "Arm", `btn_take_play` "Play", `btn_take_stop` "Stop" (`floater_director_v2.xml:102-111`).
- No textures were chosen from `textures.xml` and none are listed in the report.
- `take_timecode` is a `<text label=…>`, and LLTextBox has no `label`.
- The recorder exposes a playhead (`llflycamrecorder.h:16-17`), but there is no report note about it.
- The ACTION/CUT `image_color` and `label_color` are not applied.
- `xui_layout_check` skips the inside of `kit_shell`, so no gate sees any of this.

**Fix**
- Use `image_overlay` icon buttons with empty labels, and list the textures in `gen_report`.
- Give the timecode its text content and a note.
- Apply the pill colours.

### M9. Rollout parity is disabled

**Evidence**
- `main()` always passes `p0_scope=True` and rejects any `--groups` value except `light_group`. The owner-based path, which covers Lightbox, cue floaters and shared panels (ROLLOUT §6), is unreachable.
- `--all` replaces every non-P0 page with a fixed "pending" finding. In fixture P5, renaming a v1 widget on Move leaves the output unchanged at 33 findings.
- `ROLLOUT_WP` uses the key `perf_group`, but the real group is `performance_group`. The Performance pages are therefore labelled "WP6+" instead of WP8.

**Fix**
- `--all` must still run v1-leaf parity on the legacy pages (which pass, because they are verbatim) and owner-based parity on converted pages.
- Report "pending" only for checks that need the conversion.
- Fix the map key.

## MINOR

1. **The in-generator verbatim check is a tautology.** It compares a deep copy with its source before serialisation. The selftest re-parses only `move_tab`. It should re-parse all 31 serialised pages; `verbatim.py` shows the result holds today.
2. **`gen_report` is incomplete.**
   - `warnings=0` is hard-coded.
   - It has no pure-kit predicate for the legacy pages (4.3).
   - It has no icon texture list (4.6).
   - The disagreement `cine_rimq_actors_alpha` (data has no `s`, v1 binds CineRigRimActorsOnlyAlpha) is not reported.
3. **`--out-dir` does not redirect `gen_report.txt`.** It is fixed to `REPORT`, so a scratch run writes the repo report.
4. **The Tron launcher is a `rows` card holding a button row**, not a `kind="launcher"` card with `target`, `target_tab` and `button_label` (sections 3.4 and 4.3).
5. **Native combo item names are generic.** They use `item_<i>`, so the second native combo will collide. Emit `<w>_item_<i>`.
6. **`shortcut_of` is not an `ALKitRow` param** (`params.py`). It parses with a warning and C++ cannot attribute search or undo to the home (3.8). The index also carries no home or shortcut field. This is cross-WP work for WP1/WP2.
7. **Shortcuts copy v1 bounds.** Spec 3.8 says they get the home row's kind, range, step, decimals and unit. The values are equal today, but Shot's 0.5 against 0.1 will regress when it is converted. Their help text does not name the home (ROLLOUT §5).
8. **`help_audit_allow` entries are too loose.**
   - `open_lightbox_glow` is silenced permanently and cites ROLLOUT §§4/6, which are irrelevant. The real citation is `pages/bloom_glow_tab.md:38`.
   - The entries are keyed by widget and silence any future text on that widget.
   - The `pending_*` entries have no expiry and no release mode in which `pending-wp` becomes an error.
   - `shared_writers` exempts every attribute. It should cover only `sel_setting` and `mirror_setting`.
9. **The code-bound row type exemption is broad** (K6: a toggle over F32 passes). It should be limited to rows whose v1 node is a known presentation mismatch, such as `cine_seed`.
10. **The `light_rig_map` `view_setting` and `side_from_setting` are never resolved** (K7). `DirectorV2LightMapView` is missing from settings and produces no finding; it should be `pending-wp` like SelectedLight.
11. **Parity check 4 skips a selector when the v1 combo has no `control_name`** (P4). Fall back to the combo name mapped through page_sources.
12. **The header sha omits the v1 sources and the generator.** As a result, `kit_lint` rule 7 cannot detect staleness after a v1 XML edit; only `gen --check` can.
13. **`preset_mode="snapshot"` is invalid.** The fallback at `director_kit_gen.py:448-449` emits a value outside `none|widgets|provider`. It is latent today.
14. **`PAGE_MASTER_HOMES` moves the page_sources homes to 24**, against the rollout's 22-home table. Remove it with the B1 fix.

## Gate re-run required after the fixes

Run, in order:
1. `gen --check` and `--selftest`, with the selftest checking all 31 pages and the dials.
2. `check_kit_pages.py`.
3. `kit_lint --all` and `--selftest`, with fixtures K1–K8 added.
4. `director_parity --groups light_group` (expected **red** until WP4 extracts the rim names) and `--selftest`, with P1–P5 added.
5. `xui_layout_check --selftest`, plus the real-tree legacy regression.
6. `review_wp3_r1/fixtures.py`: every row must read CAUGHT.

---

## R1 verification (Opus, on HEAD 3f9fdbeba09 "Director kit WP3 R1 fixes")

**Verdict: CLEAN.** There are 0 BLOCKER, 0 MAJOR and 8 new MINOR findings. All 2 blockers and 9 majors from R1 are fixed. None of the relaxations added in rounds 1–3 hides a real defect, though four of them are coarser than they need to be (N1–N4 below).

### Gate re-run (independent)

- **Generator.**
  - `gen --check` exits 0.
  - `--selftest` re-parses all 31 legacy pages and checks the dials.
  - Two runs into scratch directories are byte-identical (38 files; `gen_report.txt` now follows `--out-dir`).
  - The repo tree stays clean.
- **Config and sources.** `check_kit_pages.py` passes. `director_page_sources.py --check` passes (35 pages, 1,839 rows, 0 unresolved, 22 multi-page settings).
- **`kit_lint`.**
  - `--all` gives 0 errors and 4 `pending-wp` warnings; `--selftest` passes.
  - `--release-wp N` turns entries due at or before WPN into errors: WP10 gives 2 errors, WP11 gives 4, WP12 gives 15.
- **`director_parity`.**
  - `--selftest` passes 40/40.
  - `--groups light_group` passes, with 2 PENDING records and check-3 allowing only the exact name `cine_rig_rim_preset`.
- **`xui_layout_check`.** `--selftest` passes, and 31/31 real legacy pages match their v1 finding keys.
- **Original fixtures.** Rerunning the unmodified scratch copy of the R1 `fixtures.py`, K0 and P0 are clean and K1–K8 plus P1, P2, P4 and P5 are all CAUGHT.
  - The original P3 now passes legitimately. WP2 (3661c636e42) moved the rim lookups out of `ALPanelCineLightRig::postBuild` into `ALRigRimController::attach`, which uses the tolerant `findChild` (`alrigrimcontroller.cpp:108-110`), so its premise no longer exists.
  - Codex rewrote P3 inside the committed reviewer file; see N8. Q1 and Q2 below replace it.

### R1 findings: status

| ID | Status | Evidence |
|---|---|---|
| B1 | FIXED | No card carries an `enable_widget` equal to its page master. `PAGE_MASTER_HOMES` is gone (back to 22 homes). Lint rule 4 counts the kit-created names (K1 caught). |
| B2 | FIXED | Legacy files have the sized `kit_page` as root. A zero-size root reports `invalid-root` (X3). The R1 5000×9000 probe now fails through the CLI (X5). |
| M1 | FIXED | All 9 dials have `kit_geometry="true"` and `show_readout="false"`, with no slider-only params. Lint and the generator selftest enforce it (K8). |
| M2 | FIXED | The index is built from the config-applied pages. `CineGobo01` is gone; `cine_gobo_preview`, the clip readouts and `cine_rimq_status` are present; `cine_shadow_fixit` is no longer flagged advanced. |
| M3 | FIXED | Requests are scoped to their owner's page. Fresh fixtures Q1 (rim status moved to Rig) and Q2 (`cine_shadow_hint` moved to Rim) both FAIL as they should. |
| M4 | FIXED | Check 3 honours only the exact `retired_cpp_targets` (P1 caught). |
| M5 | FIXED | The lazy-page and rail indicators no longer count as bindings (P2 caught). |
| M6 | FIXED | K2–K5 are all caught: code rows are writers, "Easy" means `mode_on`, pending homes are checked against the data's home row, and the same-page policy is exact per setting and attribute. |
| M7 | FIXED | Policy moved to `gate_policy.json` with a citation and expiry per entry. `check_kit_pages` passes and validates it. |
| M8 | FIXED | The transport uses icon overlays with empty labels and a text-node timecode. ACTION/CUT have their colours. `ICON_TEXTURES` is in the report. |
| M9 | FIXED | `--all` runs real parity, and the Move widget removal now changes the result (P5). `performance_group` is spelled correctly. |
| m1–m14 | FIXED or acceptable | See the remainders listed in N5 and N6. |

### Fresh fixtures for the new relaxations

These are in `review_wp3_r1/fixtures_r1v.py`; a further probe is `review_wp3_r1/xui_transitive_probe.py`.

| ID | Planted case | Result |
|---|---|---|
| X1 | A new clipped child on a baselined page (Gaze) | CAUGHT, as `legacy-baseline-drift` |
| X2 | A baselined clipped subject (`director_gaze_panel`) grows to 9000 px | **MISSED** (N1) |
| X3 | Legacy `kit_page` root loses `width`/`height` | CAUGHT (`invalid-root`) |
| X4 | An unreferenced copy of a legacy page | CAUGHT (`scroller-wiring` plus drift), so the transitive clearance is not blanket |
| X5 | The R1 B2 5000×9000 probe | CAUGHT |
| Q1 / Q2 | A C++-owned name moved to the wrong page, in either direction | CAUGHT |
| Q3 | The PENDING WP4 name `cine_rimq_enable_tag` removed | CAUGHT; PENDING is a report-only note and masks nothing |
| L1 | A stale `pending_settings` entry | **MISSED** (N2) |

**The "20 transitive-wiring false positives" (actually 24).** `xui_transitive_probe.py` lists what the `page_file`→`floater_director_v2.xml`→`LLFloaterDirector` owner walk clears: 20 `scroller-wiring` and 4 `scroll-content-overflow` findings on legacy pages. In the runtime these really are handled. `ALKitLazyPage::ensureBuilt` → `ALScrollFit::Controller::adoptPage` (`alscrollfit.cpp:461-503`) wraps a legacy host that has no scroller, installs focus on every document container, calls `fitPage`, and enables the slider wheel. The v1 floater gets the same clearance today (17 static findings, 0 with the source index). The evidence the checker uses is indirect, though; see N3.

**The 12-entry v1 baseline.** It equals exactly the static findings of the 31 legacy pages. Those findings match the v1 leaves 31/31. The baseline has no globs and no duplicates; additions fail, and removals fail in the selftest. It is acceptable as intermediate scaffolding: the rollout requires absolute zero, and WP6+ must delete entries as the pages convert.

### New MINOR findings

- **N1. The baseline key is too coarse.** It is `file|kind|subject`, so any growth of an already-baselined clipped or overflowing subject is masked (X2). Fix: key on the normalized message including the rect, or store the v1 rect and fail on growth.
- **N2. `pending_settings` never goes stale.** WP2 has landed, and `DirectorV2SelectedLight`, `DirectorV2LightMapView` and `DirectorV2LightMapSideFrom` are now in `settings.xml`. Their WP2 entries stay in `gate_policy.json`, and neither `--release-wp 2` nor `check_kit_pages` flags them (L1). Fix: flag an entry whose setting now resolves, the same way parity already flags a stale `pending_cpp`; then delete the three entries.
- **N3. Transitive clearance relies on indirect evidence.** It accepts any `.installForTabs(`/`fitAll(` text in an ancestor owner (`llfloaterdirector.cpp:764`) as proof of wiring, but the real mechanism is `adoptPage`. If `adoptPage` regressed, nothing would notice. Fix: for `kit_lazy_page` children, require `adoptPage(` evidence in `alkitlazypage.cpp` and `alscrollfit.cpp`.
- **N4. `same_page_mode_writers` expire at WP12.** Their WP12 expiry makes `--release-wp 12` fail on a permanent, legitimate Easy/Advanced design, which is 8 of the 15 WP12 errors. Fix: before WP12, move these to a declared per-row `mode_pair` in the data/config instead of a policy that expires.
- **N5. The Tron launcher now draws two identical buttons.** The `kind="launcher"` card creates `kc_open_launch` (`alkitcard.cpp:348-362`), and the generator still emits the `open_tron` button row. Fix: drop the row, or keep the row and use `kind="rows"`. The page is temporary (WP10).
- **N6. The report's predicate field is a constant.** Every legacy page reports `predicate=verbatim v1 leaf`, not the spec 4.3 pure-kit planning predicate (true/false).
- **N7. Arm and Set marks share one icon.** `btn_arm` uses the same `Flag` texture as `btn_set_marks`, so the two buttons look alike. Pick a distinct texture.
- **N8. Codex edited the reviewer's fixture file.** It rewrote P3 in the committed `review_wp3_r1/fixtures.py`. The rewrite is technically justified (the premise changed after WP2), but implementers should add new fixtures rather than edit a reviewer's. Q1 and Q2 now cover the original intent.

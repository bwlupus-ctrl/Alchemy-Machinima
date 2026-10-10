# WP3 gate triage

Authority: `DIRECTOR_KIT_P0_SPEC.md` sections 4 and 6, its P0 page contracts, and `DIRECTOR_KIT_ROLLOUT_SPEC.md` sections 5–7. Categories are (a) generator/output, (b) gate tool, (c) data/config, and (d) explicitly pending rollout work.

| Finding class (original output) | Decision | Resolution |
|---|---|---|
| Lint: slider children reject/omit `text_width` | **a+b** | Section 3.3's slider row is the specific rule: generator emits `show_text="true" text_width="52"`; lint permits and requires exactly that exception. Fixture covers it. |
| Lint: `CineLightRigSeed` U32 rejected on a code-bound text row (1) | **b** | A code-bound row's `setting` is watch/reset/amber metadata, not a line-editor control binding (3.3). Check existence but do not apply the presentation widget's String type. Fixture adds a U32 code-bound text row. |
| Lint: Rig master, Live Probe, Night Mask, and Rim settings have 2 writers/no home (9; final rerun left Rig/Rim only) | **a+b (R1 supersedes)** | The page master is the sole emitted switch. The duplicate card-enable widget is removed; lazy-page master metadata remains observation-only, and page masters are not added to the rollout's 22-home table. |
| Lint: `BDMergeProjectorVolumetricsMaxDistance` home absent (2) | **d** | Atmosphere is the ledger home but remains a P0 legacy page; config explicitly reports the missing home as **pending WP11** (rollout §§5,7), while retaining the declared Rig Easy shortcut. |
| Lint: `DirectorV2SelectedLight` graph/home findings (4) | **d+c** | P0 §3.16 assigns the setting to WP2 and §§5.1/5.2 intentionally share it across Rig/Rim/map/fixture. Config explicitly reports **pending WP2** and declares it shared selection state, not a writable content home. |
| Lint: three `Lightbox` help warnings | **b+d** | These are actionable external-editor/temporary-launcher references, not leaked implementation prose: Bloom's external action is contractual and explicitly allowed; Tron/Rim now report as named, spec-cited **pending WP10** warnings instead of generic regex hits. |
| Parity check 1 (25 light; 293 all): missing v1 names | **b+d** | Tool used rollout `page_sources` owners (Lightbox/cue/shared panels) for the P0 gate, contrary to P0 §6.2's recursively expanded v1 Director leaves. P0 now uses those leaves; non-P0 conversions are reported once per page as pending WP7–WP11. |
| Parity check 2 (14 light; 263 all): missing control bindings | **b+d** | Tool indexed only `control_name`, omitting kit-created `master_setting`, `mode_setting`, `enable_setting`, and `sel_setting` bindings. Those now count. Remaining non-P0 pages are explicit rollout-pending entries. |
| Parity check 3 (124 light = 96 missing + 28 dynamic; 186 all) | **b+c+d** | Restrict P0 C++ extraction to the two P0 page owners; honor reasoned `parity_allow` for retired targets; expand literal C arrays and explicit bare expressions from `cpp_name_patterns.json`. Other page adapters/owners are pending WP7–WP11. Fixtures cover arrays, expressions, and allowed retired targets. |
| Parity check 4 (16 light; 16 all): Key/Fill/Back/BG absent from v1 combo labels | **b** | Check 4 now keys options to their selector setting and applies only when that selector replaces a v1 combo. `DirectorV2SelectedLight` is new, so its 0–3 role options are not compared to unrelated v1 combo labels; missing option values still fail. |
| `parity --all` total 758 = 293/263/186/16 | **d (with the tool defects above)** | P0 has only Rig/Rim converted. `--all` now validates those P0 pages and emits one spec-cited pending finding for every remaining page, assigned to WP7, WP8, WP9, WP10, or WP11, instead of 758 misleading row/name deltas. The release gate remains failing until rollout completion. |

## R1 fix ledger (REVIEW_WP3_OPUS_R1)

| ID | Resolution |
|---|---|
| B1 | Generator removes a card enable when it is the page master; lint now inventories kit-created master/mode/enable/label/reset/value/unit/state names. `PAGE_MASTER_HOMES` was removed. K1 is permanent. |
| B2 | All 31 legacy files serialize `kit_page` as the sized root. Layout check emits `invalid-root` for a zero-size legacy wrapper and selftests every real legacy page against its v1 leaf by finding kind/widget. |
| M1 | Hosted/native dials emit `mode`, `kit_geometry=true`, `show_readout=false`; slider-only params are removed and lint-enforced. K8 is permanent. |
| M2 | Index generation consumes config-applied pages, includes added/overridden rows and launcher output, and requires exact generated-row/index equality. |
| M3-M5 | getChild requests carry their C++ owner and resolve against one page; check 3 permits only exact retired targets; lazy-page/rail master indicators do not count as bindings. P1-P3 are permanent. |
| M6 | Code rows are writers; Easy means `mode_on` (or option); pending homes still compare metadata; same-page duplicates require one matching Easy/Advanced pair. K2-K5 are permanent. |
| M7 | Gate policy moved to strict `gate_policy.json`; `check_kit_pages.py` validates keys, citations and WP1-WP12 expiry. `--release-wp` turns expired pending policy into errors. |
| M8 | Transport uses Flag/Refresh/Play/Stop textures, empty icon labels, text-node timecode, ACTION/CUT colors, and report notes/texture inventory. |
| M9 | `--all` runs actual parity across all 35 generated pages; legacy removals are no longer replaced by blanket pending rows; `performance_group` is spelled correctly. P5 is permanent. |
| m1-m5 | Generator selftest reparses all 31 legacy outputs; report derives warnings/predicates and records icons/control mismatch; scratch report follows `--out-dir`; Tron is a launcher card; combo item names are widget-scoped. |
| m6-m10 | Shortcut metadata stays explicit for WP2 runtime attribution; canonical home presentation/help is generated; help exceptions are exact-text/cited/expiring; code-kind exception is exact `cine_seed`; both map settings resolve. |
| m11-m14 | Selector fallback covers a sole code-populated combo (P4); header hashes generator plus v1 sources; invalid snapshot preset fallback is gone; rollout home table is again exactly 22. |

## Round 2 gate fixes

| Gate | Resolution |
|---|---|
| Page sources | `light_rim_tab` now recognizes the WP2 `panel_rig_rim` root's `ALPanelRigRim` / `ALRigRimController` ownership chain, removing the single false unresolved row without weakening row evidence. |
| Kit lint | Feather joins Max Distance as the same explicit WP11 Atmosphere-home transition. Eight intentional Easy/Advanced same-page writers are exact setting policies with allowed writer attributes; unrelated same-page duplicates still fail K2. |
| Parity | Both bounded regex searches use compiled-pattern `finditer(text, 0, end)`; the module-wide audit found no remaining module-level `re.finditer` call with `pos`/`endpos`. |
| Layout | Lazy `page_file` ownership is followed transitively to `floater_director_v2.xml`, so its real `LLFloaterDirector` `ALScrollFit` wiring clears 20 false scroller findings. The 12 remaining geometry findings are byte-for-byte v1-inherited identities recorded in `gate_policy.json`; exact drift, additions, removals, duplicates, and globs fail. |
| Selftests | Real legacy fixtures resolve widget/include defaults from the actual XUI tree, report the true matched-page count, compare all 31 generated pages with their v1 leaves, validate the 12-key baseline, and rerun with transitive runtime ownership. |

Generated XUI is never hand-edited. WP6+ must remove the 12 exact legacy baseline entries as pages convert; the eventual rollout gate remains absolute zero.

## Round 3 gate fixes

| Gate | Resolution |
|---|---|
| Kit lint | Easy Drama now declares `cine_ratio` as its same-page canonical target. Exact cited mode policy permits that adapter presentation, while pending-home shortcuts are checked against source-data metadata so range drift still fails. |
| Page sources | `cine_rimq_enable_tag` has exact `ALRigRimController::attach` callback evidence; the generated index is back to zero unresolved rows. |
| Parity | The end-to-end fixture is in P0 scope again. WP4 Rim entries are visible `PENDING` records, do not fail the gate, name an exact C++ file/owner, and become stale-policy errors when that owner stops referencing them. Per-page checks no longer let `KITNAMES` mask a removed hosted child. |
| Layout | Real legacy comparison preserves XML policy comments and normalizes the intentional generated `*_content` root names; all 31 pages now match while the exact baseline and transitive runtime-wiring checks remain independent. |
| Acceptance | K0/P0 are clean; K1-K8 and P1-P5 are all caught. |

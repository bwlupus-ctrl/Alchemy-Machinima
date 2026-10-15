# SPEC: Batch A, every Director v2 feature is searchable (2026-10-15)

Implementation spec only. Nothing here has been built. It answers the user mandate "All the Director's features should be searchable" and the search findings G1-G11 in `doc/director_kit/COVERAGE_AUDIT_2026-10-15.md` (section 3, and the fix design in section 5). Scope is Director v2 only (`floater_director_v2.xml`).

Paths are relative to the repo root. XUI paths are under `indra/newview/skins/default/xui/en/`. Line numbers are from the working tree on 2026-10-15 (HEAD `a04435b1f55`). Other agents are editing nearby code, so Codex must re-locate each anchor by its symbol name before editing.

Batches B (new pages) and C (conversions) follow this one. The design therefore has one rule: **the index is derived from the generated and embedded XUI trees, never from a hand list.** Every row a later batch adds is indexed with no extra work, and the gates fail when something is reachable but unindexed.

---

## 0. Measured baseline (what the spec is sized against)

These figures come from scratch scripts run against the working tree. They are reproducible with the WP3 gate once it exists.

| Item | Count | Source |
|---|---:|---|
| Index entries today | 4,252 (2,059 Director + 2,197 external); 1.06 MB | `director_kit_index.xml` |
| Lazy pages | 36 + Quick (23 kit, 13 legacy) | `floater_director_v2.xml:36-108`, `director_kit_gen.py:1203` |
| `kit_row` nodes on kit pages | 1,488 | generated `panel_dpage_*.xml` |
| Named controls on kit pages outside any `kit_row` | 5 (`cine_setup_combo`/`_save`/`_delete`, `combo_preset`, `btn_apply_preset`) | the same files |
| `kit_card` / with `enable_setting` | 177 / 26 (27 `enable_widget`) | the same files |
| `kit_page master_widget` | 7 | the same files |
| Named interactive controls on the 13 legacy pages (embedded XUI, includes followed) | 639; 132 are reset buttons; 19 are statically hidden (`visible="false"` on the control or an ancestor) | walk of `page_file` + `filename=` |
| Legacy label sources (label / text sibling / tool tip / name only) | 414 / 84 / 136 / **5** | same walk |
| Inner `tab_container` tabs on legacy pages | Ghosts 4, Dance 5, Exposure 3 | same walk |
| Duplicate widget names inside one legacy page | 0 | same walk |
| Combo / radio / `kit_option` item labels in Director pages | 1,279 / 32 / 58 | same walk |
| External rows: reset / setting already on a Director row / name-only label / labelled | 476 / 669 / 419 / 633 | today's index |

The Python port of the current scorer reproduces the audit: "practicals" tops out on rig Flicker rows (score 95), "stepped particles" returns 0 hits, "kaleidoscope" returns two unrelated external rows, "drone" returns 1,048 hits, "camera shake" tops out on `dof res combo`, and "onion skin" tops out on Diopter reset buttons.

---

## 1. Index generation

### 1.1 Ownership and module layout

- New module `scripts/code_tools/director_kit/search_index.py` holds the reachability walker, the label resolver, the row builders, the floater registry and the dedupe step. `director_kit_gen.py` calls it from `index_bytes()` (`director_kit_gen.py:953-1015`) in place of the current mockup loop (`:975-989`) and `external_index_rows()` (`:918-950`).
- The walker reads the trees that `generate()` has already built in memory (`director_kit_gen.py:1209-1243`: the kit and legacy page roots, the floater tree and the `names` set per page) and follows `filename=` includes from disk. It does **not** read rows from `director_mockup_data.json`. The mockup is used only as **enrichment joined by widget name**: `kw`, `h`, and `l` when the real control has no label. The `chrome.cards` rows supply labels and help for the shell controls.
- The output stays `director_kit_index.xml`, generated and drift-checked by `director_kit_gen.py --check` (prebuild `C:/Users/xianw/cxs/prebuild_guard.ps1:74`).

### 1.2 Index format v4 (append-only)

Fields 0-21 keep their current meaning (`alkitsearch.cpp:125-153`, `director_kit_gen.py:982-988`). Two fields are appended:

| # | Name | Content |
|---|---|---|
| 22 | `context` | Space-joined human context: section (chip) label, inner tab or accordion title, and the page's feature aliases (1.6) |
| 23 | `parent` | For item rows, the owning control's label ("Tool mode"); empty otherwise |

- The header becomes `<director_kit_index version="4" fields="24">` (`alkitsearch.cpp:17,81`, `director_kit_gen.py:36,1013`). The C++ accepts only v4, so an old index fails closed with an empty search, which the functional gate catches.
- Because the fields are append-only, the existing readers keep working: `director_functional_manifest.py:508-511` reads `fields[1]`, and `director_wiring_table.py:530-545` reads `f[1]` and `f[8..11]`.
- **Field 19/20 on Director rows** (`external_tab` and `external_tab_label`, empty today) now carry the **host tab**: the inner `tab_container` panel name and label for legacy-embedded controls. External rows already use them for the floater tab.
- **Flags (field 13)** gain row-type letters. Existing: `a` advanced, `l` link, `x` legacy, `e` external. New:

| Flag | Row type |
|---|---|
| `g` | group row |
| `p` | page row |
| `t` | inner tab or accordion row |
| `c` | card row |
| `m` | master or card switch |
| `i` | combo, radio or option item |
| `s` | shell control |
| `f` | floater or floater-tab link row |
| `r` | label came from the cleaned-name fallback (report only; the runtime ignores it) |

### 1.3 What gets a row (every Director-reachable control)

The walker starts at `floater_director_v2.xml`. It follows every `kit_lazy_page@page_file`, every `filename=` include (recursively), every `kit_popover` and `kit_bar`, and the `kit_cast_roster` panel.

| Source | Rows emitted | Label | Navigation key |
|---|---|---|---|
| `kit_shell.rail_item` (`floater_director_v2.xml:5-13`) | 1 group row each (`g`) | `label` ("Camera") | page key = the group's first page |
| `kit_lazy_page` | 1 page row each (`p`); help = page `summary`; context = section labels | `label` ("Practicals") | page key |
| `kit_page@master_widget` (7) | 1 switch row (`m`), setting = `master_setting` | `master_label` + " on/off" | widget = `master_widget` |
| `kit_card` (177) | 1 card row (`c`); help = card `help` | `title` | `card_id` |
| `kit_card@enable_widget` (27) | 1 switch row (`m`), setting = `enable_setting`, keywords "enable on off" | card title + " on/off" | widget = `enable_widget`, plus `card_id` |
| `kit_row` (1,488) | 1 row, as today but read from the generated XML (`widget`, `label`, `kind`, `setting`, `help`, `keywords`, `advanced`) | `label` | widget |
| `combo_box.item`, `radio_item`, `kit_option`, `flyout_button.item` under a row or control | 1 item row (`i`) per item; `parent` = control label | item `label` | the owning control's widget |
| Named interactive node on a kit page outside any `kit_row` (5 today) | 1 row | label resolver (1.4) | widget |
| **Legacy-embedded panels** (all 13 legacy pages) | 1 row per named interactive node in the **real embedded XUI** (`panel_actor_mover.xml`, `panel_path_editor.xml`, `panel_prop_mover.xml`, `panel_ghost_studio.xml`, `panel_dance_ball.xml`, `panel_lens_gaze.xml`, `panel_pose_polish.xml`, `panel_director_anim_switcher.xml`, `panel_exposure_hub.xml`, plus the page-level nodes such as `dance_popout` and `group_run_combo`); host tab in fields 19/20; card = the joined mockup card title, else the accordion title, else the inner tab label | label resolver | widget + host tab |
| Inner `tab_container` panel or `accordion_tab` inside a legacy panel | 1 tab row (`t`) | tab label or accordion title | host tab |
| Shell controls (`kit_cast_roster`, `kit_transport`, `kit_arm_popover`, `kit_scene_popover`, `legend_panel`; `floater_director_v2.xml:14-194`) | 1 shell row (`s`) each; group key `shell`, page `transport` / `cast` / `arm` / `scene` / `legend` | label resolver (the mockup `chrome` labels fill icon-only buttons such as `btn_set_marks`) | widget |
| **LINK targets** (1.5) | 1 floater row (`f`, `e`) per target, 1 floater-tab row (`f`, `e`) per tab the Director opens, then the floater's named controls as external rows (`e`) | floater `title` / tab label / label resolver | floater + tab + widget |

"Named interactive node" uses one tag set shared with the gate: today's `EXTERNAL_CONTROL_TAGS` (`director_kit_gen.py:911-915`) plus `kit_switch`, `radar_list`, `multi_slider`, `text_editor`, `compass_dial` and `name_list`, plus any node carrying `control_name`.

**Not emitted.** Each skip below is a rule the gate enforces (3.1), not a silent drop:
- `kit_row` with `hidden="true"` or `show_if="never"` (manifest-only rows; today's `manifest_only_rows`, `director_kit_gen.py:970,992-993`).
- Reset buttons: tag `button`, and the name matches `^(reset|rst)_|_reset(_|$)`, and the label is empty or starts with "Reset"/"↺", or the tool tip starts with "Reset". They are reached through the control they reset.
- Statically hidden controls (19 today, listed in the exclusion file).
- Items that are empty, numeric-only, one character, equal to their parent label, placeholders (`^(choose|select|pick)\b.*(\.\.\.|…)$`, `^-+$`), or identifier-shaped (no space, and either an internal capital or an underscore, for example `mHipLeft`). A combo with more than 64 items emits none and is reported.
- `text`, `icon` and `view_border` nodes (not interactive).
- The Quick page (`quick_tab`). Its rows are runtime proxies of starred rows that are already indexed. It still gets its page row.

### 1.4 Label resolver (human-readable, never a raw widget name)

The resolver tries these sources in order and uses the first that gives a non-empty result after trimming:
1. A Director structure label: `kit_row@label`, card `title`, `master_label`, page or rail `label`.
2. The control's own `label` attribute. For `check_box`, the text body if there is no label.
3. The mockup or chrome join: the `l` of the row with the same widget on the same page.
4. An **adjacent text sibling**: a `text` node with the same parent whose `top` is within 6 px of the control's `top`, preferring the nearest one to the left.
5. The **tool tip's first clause**: cut at the first `. `, `: `, `; ` or ` (`, trim trailing punctuation, cap at 48 characters, and capitalize the first letter.
6. **Cleaned name, flagged `r`**: split on `_` and camelCase; drop UI-noise tokens (`btn chk check cb combo spin spinner slider sld ctrl edit editor txt lbl kc`) and a leading feature prefix of 3 characters or fewer when 2 or more tokens remain; join with spaces; sentence-case the result. Each use is listed under `REVIEW_LABELS` in `gen_report.txt`.

This applies to external rows too, which replaces the widget-name fallback at `director_kit_gen.py:926`.

Help text is the tool tip, plus the mockup `h` when it is different. Keywords are the mockup `kw`.

### 1.5 LINK target discovery (automatic) and the floater registry

**Registry.** The floater registry is derived from `llviewerfloaterreg.cpp`, using the regex `LLFloaterReg::add\("(\w+)",\s*"([\w.]+)".*?build<(\w+)>`. That gives name to XUI file to C++ class. For example `actor_gaze` maps to `floater_actor_gaze.xml` and `ALFloaterActorGaze` (`llviewerfloaterreg.cpp:783`), and `lightbox` maps to `floater_lightbox_settings.xml` (`:897`). The floater label is the floater XUI's `title`.

`external_search.json` becomes an optional **override** file (`version: 2`, `overrides: {name: {label?, tabs?}}`). It no longer decides *whether* a floater is indexed.

**Targets.** A floater is a LINK target when any of these reaches it:
- XUI inside the walked tree:
  - `kit_row@action="floater:X"` or `"lightbox:TAB"`
  - `kit_card kind="launcher" target=... target_tab=...`
  - `button.commit_callback function="Floater.Show*|Floater.Toggle*" parameter="X"` (today `env_adjust_snapshot` and `cine_light_rig` at `panel_exposure_hub.xml:53,61,66`)
- C++ in `llfloaterdirector.cpp`, `alkitcard.cpp`, `alkitrow.cpp` and the .cpp of every hosted class (a walked `class="panel_x"` maps to its file through the `LLPanelInjector<...> ...("panel_x")` registration, for example `alpanelghoststudio.cpp:56`). These calls count:
  - `LLFloaterReg::(show|toggle)(Typed)?Instance(<T>)?("X")`
  - `ALFloaterLightBox::showTab("TAB")`, which also records the tab
  - any `ClassName::show*(` where `ClassName` is a registry class, for example `ALFloaterActorGaze::showForSelection` (`llfloaterdirector.cpp:5717`), which maps to `actor_gaze`
  - and the trigger: `addLauncherCard("id", ...)` (`llfloaterdirector.cpp:1035-1095`) links button `card_<id>_open` to the first target inside its lambda.

  Today this finds the 13 indexed floaters plus `actor_gaze`, `env_adjust_snapshot` and the lightbox tabs `sellight_settings`, `actor_mask_settings`, `cinehaze_settings`, `shadow_settings` and `colortone_settings` (`alpanelexposurehub.cpp:133`). `toggleInstance("pose_polish")` (`alpanellensgaze.cpp:527`) and `showInstance("gaze_cues")` (`:2154`) are already covered.
- **Unmapped launcher heuristic.** A Director-reachable `button` whose resolved label matches `^Open\b` or ends in `...`, `…` or `›`, with no discovered target, is a gate failure (3.3). This catches a new popout whose C++ idiom the scanner does not know.

**External rows per target.** Each target gets:
- 1 floater row: label = title, help "Opens the <title> window".
- 1 tab row per tab the Director opens.
- 1 row per named interactive control in the floater, after includes, with the label resolver and tab context, minus the dedupes below.

**Dedupe** (keeps the external list small and puts Director rows first):
1. Drop an external control row whose `setting` is bound by any Director row. The Director row wins, and the control still counts as covered.
2. Drop an external control row whose (source XUI file, widget) is also embedded in the Director, for example `floater_ghost_studio.xml` including `panel_ghost_studio.xml`.
3. Drop reset buttons by the rule in 1.3.

This removes the 476 + 669 noisy rows, and most of `ghost_studio`, `dance_ball`, `exposure_hub`, `pose_polish` and `actor_gaze`.

The launcher regex at `director_kit_gen.py:994-1001` is deleted, and its job moves to G-links (3.3).

### 1.6 Synonyms strategy (`synonyms.json` v2)

`synonyms.json` is today a flat list of 22 bidirectional groups expanded into field 16 by `synonym_aliases()` (`director_kit_gen.py:898-908`). v2 has two parts:

1. **`groups`**: the same bidirectional vocabulary groups (weight SYNONYM = 5, lowest), extended for G10:
   - gamepad / controller / joystick / xbox / pad
   - height / hover / elevation / altitude
   - water level / sea level / water height
   - practical / practicals / selected light / in-world light
   - letterbox / aspect / aspect ratio / frame format
   - pose / poser / posing
   - shake / handheld / wobble / jitter
   - drone / flight model / flycam
   - countdown / delay / timer
   - onion skin / pose ghosts / pose preview
   - slow mo / slow motion / time scale / world clock
   - stop motion / stepped / on twos / on threes
   - outline / ink / roto
   - kaleidoscope / mirror tiles

   Groups match whole words (the current regex), so they cannot fan out by substring.
2. **`features`**: `[{"target": "camera_group/operator_tab", "aliases": ["camera shake", "handheld", "operator"]}, {"target": "camera_group/operator_tab#shake_look", ...}]`. These are curated aliases. They attach **only** to the page row (or card row with `#card_id`) and are written into **keywords** (weight 45) plus the `context` field, not to every row. A target that does not exist is a gate error (stale alias).

**Morphology.** Plurals and tenses (stepped/step, isolate/isolation, shadows/shadow, particles/particle) are handled by a stem rule in the scorer (section 4.1), not by synonyms.

**For Batches B and C.** A new page *should* add a `features` entry when its user vocabulary differs from its labels. The probe table (3.4) is what forces this: if "gamepad" must land on the new Flycam page, the probe fails until the page's labels, keywords or aliases carry the word.

### 1.7 Expected index size

| Part | Rows (estimate) |
|---|---:|
| Kit rows + outside-row controls | ~1,495 |
| Legacy real controls (639 − 132 resets − 19 hidden) | ~490 |
| Group, page, tab, card, switch and master rows (9 + 36 + ~20 + 177 + 27 + 7) | ~276 |
| Shell rows | ~28 |
| Director item rows (after placeholder and identifier filtering) | ~1,200 |
| External control rows after dedupe (incl. `env_adjust_snapshot`) | ~900 |
| External item rows for kept combos | ~500 |
| Floater and floater-tab rows | ~60 |
| **Total** | **~5,000 (cap 8,000)** |

That is about 1.3 MB against 1.06 MB today (cap 2.0 MB). The caps are gate G-budget (3.6).

---

## 2. Result navigation

Activation still goes through `ALKitShell::activateSearchResult(const Hit&)` (`alkitshell.cpp:1610-1624`). It now dispatches on the row type from the flags:

| Row type | Behaviour |
|---|---|
| Group `g` / page `p` | `navigateTo({page_key})`: selects the group and page (`selectPage`, `alkitshell.cpp:1083-1108`) and scrolls the page to the top. `navigateTo` now **returns true** when only a page was asked for (today it returns false at `:1080`). |
| Kit row / item `i` | Unchanged path: `findRow` → `revealRow` (`alkitpage.cpp:1404-1436`), which already switches to Advanced (`:1416-1428`) and to the right chip or section (`revealCard`, `:1438-1466`) and flashes the row. Item rows target the owning combo's row and **never change its value** (Q3). |
| Card `c` | `findCard` → `revealCard` (`alkitshell.cpp:1050-1051`). |
| Card or master switch `m` | `revealCard(card_id)` as today, **then** `findChild<LLView>(widget)` inside the card (or the page, for a master) and set `mHighlight` (new). Today the switch is never highlighted, because `navigateTo` returns right after `revealCard`. |
| Legacy control (`x`) | New order in the legacy branch (`alkitshell.cpp:1052-1079`): (1) if `host_tab` is set, find that tab panel and search for the widget **inside it** (this also disambiguates); (2) **for every `LLTabContainer` ancestor of the found view, select the child panel on the path** (generic, so nested tabs work, which fixes G11 / A8); (3) open accordions (existing `:1055-1057`); (4) center in the scroller (existing); (5) highlight (existing). |
| Inner tab or accordion `t` | Steps (1), (2) and (3) on the tab or accordion itself. |
| Shell `s` | New `ALKitShell::revealShellControl(widget)`: find the child under the shell, then **`show()` every `ALKitPopover` ancestor** (`kit_arm_popover`, `kit_scene_popover`, `legend_panel`; the cast popover, which hosts the roster at narrow widths, `alkitshell.cpp:482-492,694-712`), then highlight it. If the control is still not visible (it collapsed out of `kit_transport` at this width, `floater_director_v2.xml:110`), show "<label> is hidden at this window width" in `transport_toast` (`:137`) and return false. |
| Floater `f` / external `e` | `openExternalResult` (`alkitshell.cpp:1626-1655`). It already opens the floater, selects the tab and expands accordions. Floater and tab rows (empty widget and setting) open the floater or tab with **no pulse**. |

**Two existing bugs fixed in the same place:**
- `findExternalTarget` (`alkitshell.cpp:63-80`) compares `control->getControlVariable() == saved` even when `setting` is empty. With an empty setting, the first `LLUICtrl` with no control variable matches, which is the floater root itself. A dead external row therefore pulses the whole floater instead of failing. Fix: only compare when `!setting.empty()`.
- In-page filtering (`runSearch`, `alkitshell.cpp:1408-1418`) keeps only hits that map to a row. A card or switch hit on the current page should keep **all rows of that card** visible. Otherwise "camera shake" on the Operator page filters down to an empty card.

**Result display** (`updateSearchResults`, `alkitshell.cpp:1495-1515`):
- Item rows show `label` with the context "· <card> › <parent>".
- Switch rows show "· <card>".
- Shell rows use group "Director" and page "Transport" / "Cast" / "ACTION".

No other UI change.

---

## 3. Fail-closed gates

A new gate script, `scripts/code_tools/director_search_check.py --check`, is added to the prebuild guard's `$kitPy` list (`C:/Users/xianw/cxs/prebuild_guard.ps1:73-79`):

```
@{ Label = 'Director search gate'; File = 'scripts\code_tools\director_search_check.py'; PyArgs = @('--check') }
```

**`--check` runs its fixture self-test FIRST** (3.7). If any fixture gives the wrong verdict, it exits 1 with "GATE SELFTEST FAILED: <fixture> passed (gate loosened)" before looking at the real tree.

**Independence.** The coverage enumerator in the gate is its own deliberately simple walk, about 60 lines of `ElementTree` over `page_file` and `filename=`. It does **not** import `search_index.py`'s walker. Otherwise a walker bug would make the generator and the gate agree.

### 3.1 G-coverage: every reachable named interactive control is indexed or excluded

- **Enumeration.** The gate enumerates:
  - every named interactive node (the shared tag set in 1.3) in `floater_director_v2.xml` and every reachable page and include;
  - plus every `kit_card@enable_widget`, `kit_page@master_widget`, `kit_lazy_page@master_setting` and `rail_item@master_settings` token;
  - plus every combo, radio or option item;
  - plus every discovered LINK target's floater (one floater row each).
- **Matching.** Each must match an index row by (page, widget) or, for masters, by setting on a row in the same group. Items match by (page, widget, item label).
- **Rail masters** (`floater_director_v2.xml:7-12`) must each be bound by an indexed control in that group (today `polish_master_check`, `vc_enable`, `chk_enabled`, `cine_enabled`, `cine_rig_rim_enable`, `sa_enable`, `weather_enabled`). A master with no control anywhere is a FAIL. It means the user cannot switch it from the Director.
- **Legacy controls inside a `tab_container`** must carry a non-empty host tab (fields 19/20).
- **Exclusions** live in `scripts/code_tools/director_kit/search_exclusions.json`, which has two parts:
  - `entries`: `[{"page", "widget", "reason"}]`. Each entry needs a reason of at least 12 characters, has a **hard cap of 40 entries**, and is **stale-checked**: an entry whose widget no longer exists is an error (like kit_lint's `stale-policy`). Expected contents: the 19 statically hidden controls (or Q4's alternative), `scene_name_editor`, `status_strip`, `rec_indicator`, `take_timecode`.
  - `rules`: exactly two allowed kinds.
    - `reset_button`, with the predicate in 1.3 and a **pinned exact count per page**. A new reset, or a toggle or check box named `reset_*`, changes the count and fails.
    - `item_filter` (1.3), with the per-page pinned count of filtered items.

  Pattern or wildcard exclusions are rejected.

### 3.2 G-dead: every index row resolves to a real control

- **Director rows.** The widget (or, for masters, the setting) must resolve to **exactly one** node on its page, inside its recorded host tab when there is one. "On its page" means the generated page XML after includes, plus the generator's runtime `names` set for kit pages (`KITNAMES`, `director_kit_gen.py:1239-1241`), which covers `enable_widget`, `master_widget` and `kc_*_launch`. This removes the 26 dangling rows on `gaze_cues_tab` (5), `light_cues_tab` (5) and `practicals_tab` (16), and `vc_monitor_tile_manifest` on `vcam_tab`.
- **Item rows.** The item label must exist under that control.
- **Shell rows** resolve under `kit_shell`.
- **External rows.** The widget must resolve in the floater XUI (after includes) inside the recorded tab. Setting-only rows must have a node with that `control_name`.
- **Page and group rows** must match a `kit_lazy_page` or `rail_item`.
- **Feature aliases** (1.6) must point at an existing page or card.

### 3.3 G-links: every launcher target is indexed

- Every discovered LINK target (1.5) has a floater row, and the registry resolves it.
- Every Director-reachable button that looks like a launcher (`^Open\b`, or ending in `...`, `…` or `›`) has a discovered target.
- An unknown `ClassName::show*` call, where `ClassName` is not in the registry, inside the scanned .cpp files is reported with file:line, and it is a FAIL when its enclosing callback is wired to a Director-reachable widget.

### 3.4 G-probe: user-phrased feature queries land on the right page

**Files.**
- `scripts/code_tools/director_kit/search_probes.json`: `[{"q", "expect", "top_n", "max_hits"?, "pending_until"?, "batch"?}]`.
  - `expect` is `"<group>/<page>"`, `"<group>/<page>/<widget>"`, `"shell/<widget>"` or `"floater:<name>[:<tab>]"`.
  - The probe passes when **any of the top `top_n` hits** lands on the target, and, when `max_hits` is set, the total hit count is at most `max_hits` (the noise check).
- `scripts/code_tools/director_kit/search_port.py`: the Python port of the C++ scorer, tokenizer, query loop and sort (section 4).

**Rules the gate enforces on the table itself:**
- `top_n` ≤ 5.
- At least 80 probes in total.
- No silent skip: a probe whose `expect` names a target that is not in the index is a FAIL, **unless** it has `pending_until` (a setting name or `floater:<name>`). In that case it is SKIP-listed only while no Director row binds that setting. **Once the target exists, the probe is armed automatically.**
- The pending count is printed every run, and each pending probe names its Batch B id.

**Parity with the real C++ scorer:**
- `search_port.py --emit-parity` writes `indra/newview/tests/alkitsearch_parity.inc`. It contains:
  - (a) about 300 `(query, candidate, fuzzyTokenScore)` triples taken from the probe tokens against the index tokens, plus edge cases;
  - (b) `splitTokens` cases (camelCase, digits, UTF-8 separators);
  - (c) a **mini index** in the real v4 line format, built from the top 10 hits of every probe plus decoys (about 600 lines), with the expected ordered `(page_key, widget, score)` top 10 for every probe query.
- The header of that file records the SHA-256 of the C++ scorer source region between the markers `// [search-scorer-begin]` and `// [search-scorer-end]` in `aldirectorqol.cpp` and `alkitsearch.cpp`.
- **Prebuild side.** `director_search_check.py` re-emits the file and fails if it differs from the committed one (the port changed), or if the recorded SHA differs from the current C++ region (the C++ changed and the port was not re-run).
- **Build side.** The new tut test `indra/newview/tests/alkitsearch_test.cpp` (added to `viewer_TEST_SOURCE_FILES`, `indra/newview/CMakeLists.txt:2754-2780`, with `LL_TEST_ADDITIONAL_SOURCE_FILES aldirectorqol.cpp`) includes the `.inc` and asserts that the real `ALDirectorQol::fuzzyTokenScore`, the token splitter and `ALKitSearch::parse()` + `query()` reproduce every vector and every ranked list exactly.

Both implementations are tied to one committed truth, so neither can drift on its own.

**Probe table** (the starting set; WP3 may add probes, and may change targets only for a cited Batch B or C move):

| Query | Expect | N | Note |
|---|---|---:|---|
| walk speed | cast_group/move_tab | 3 | |
| custom walk anim | cast_group/move_tab | 3 | |
| start group | cast_group/move_tab | 3 | |
| onion skin | cast_group/path_tab | 3 | G1 |
| path camera | cast_group/path_tab | 3 | |
| sync to take | cast_group/path_tab | 3 | |
| waypoints | cast_group/path_tab | 3 | |
| prop spin | cast_group/props_tab | 3 | |
| prop hover | cast_group/props_tab | 3 | |
| shuttle | cast_group/props_tab | 3 | |
| true mirror | cast_group/ghosts_tab | 3 | |
| autoanimate | cast_group/ghosts_tab | 3 | today Ext only |
| crowd formation | cast_group/ghosts_tab | 3 | |
| ghost chaos | cast_group/ghosts_tab | 3 | |
| dance ball | cast_group/dance_tab | 3 | |
| blocking | cast_group/dance_tab | 3 | today the floater wins |
| auto advance | cast_group/dance_tab | 3 | |
| wave start | cast_group/dance_tab | 3 | |
| hover height | (B2 card) | 3 | pending_until `AvatarHoverOffsetZ` (B2) |
| local height | (B2 card) | 3 | pending_until B2 client-height key |
| play animation | performance_group/animate_tab | 3 | |
| animation explorer | performance_group/animate_tab | 3 | |
| switchboard | performance_group/switchboard_tab | 3 | |
| look at camera | performance_group/gaze_tab | 3 | |
| gaze persona | performance_group/gaze_tab | 3 | |
| gaze performance | performance_group/gaze_tab | 3 | `gaze_performance_combo` |
| pose polish | performance_group/polish_tab | 3 | |
| foot ik | performance_group/polish_tab | 3 | |
| inertialization | performance_group/polish_tab | 3 | |
| gaze cues | performance_group/gaze_cues_tab | 3 | |
| poser | floater:fs_poser | 3 | pending_until `floater:fs_poser` (B6) |
| cinematic camera | camera_group/shot_tab | 3 | |
| auto switcher | camera_group/shot_tab | 3 | `DirectorSwitcherArmed` |
| cut transition | camera_group/shot_tab | 3 | |
| world slow mo | camera_group/shot_tab | 3 | |
| subject a | camera_group/shot_tab | 3 | |
| dolly zoom | camera_group/moves_tab | 3 | |
| hero arc | camera_group/moves_tab | 3 | |
| over the shoulder | camera_group/moves_tab | 3 | |
| camera shake | camera_group/operator_tab | 3 | user example |
| handheld | camera_group/operator_tab | 3 | |
| breathing | camera_group/operator_tab | 3 | |
| auto reframe | camera_group/frame_orbit_tab | 3 | |
| flycam orbit | camera_group/frame_orbit_tab | 3 | |
| letterbox | camera_group/frame_orbit_tab | 3 | synonym |
| record take | camera_group/takes_tab | 3 | |
| drone | camera_group/takes_tab | 3 | user example; max_hits 20; B1 may retarget to the Flycam page |
| gamepad | (B1 page) | 3 | pending_until `JoystickXInputEnabled` (B1) |
| xbox controller | (B1 page) | 3 | pending_until `JoystickXInputEnabled`; max_hits 10 |
| virtual camera | camera_group/vcam_tab | 3 | |
| scanlines | camera_group/vcam_tab | 3 | |
| skeleton pov | camera_group/vcam_tab | 3 | |
| depth of field | lens_group/focus_tab | 3 | |
| composition guide | lens_group/focus_tab | 3 | |
| grad nd | lens_group/filters_tab | 3 | |
| polarizer | lens_group/filters_tab | 3 | |
| aerochrome | lens_group/filters_tab | 3 | |
| soft edge | lens_group/filters_tab | 3 | |
| diopter | lens_group/diopter_tab | 3 | |
| kaleidoscope | floater:ultimate_diopter | 3 | user example; item row; max_hits 10 |
| heart bokeh | floater:ultimate_diopter | 3 | item row |
| light rig | light_group/cine_light_rig_tab | 3 | |
| gobo | light_group/cine_light_rig_tab | 3 | |
| catchlight | light_group/cine_light_rig_tab | 3 | |
| live probe | light_group/cine_light_rig_tab | 3 | |
| saved setups | light_group/cine_light_rig_tab | 3 | `cine_setup_combo` |
| rim light | light_group/light_rim_tab | 3 | |
| light wrap | light_group/light_rim_tab | 3 | |
| exposure | light_group/exposure_tab | 3 | |
| night mask | light_group/exposure_tab | 5 | also on Rig; the Exposure toggle must be in the top 5 |
| practicals | light_group/practicals_tab | 3 | user example; page row |
| selected light | light_group/practicals_tab | 3 | |
| actor only lights | light_group/actors_tab | 3 | |
| silhouette | light_group/actors_tab | 3 | |
| light cues | light_group/light_cues_tab | 3 | |
| kelvin sun | (B4 row) | 3 | pending_until `AlchemyEnvSunKelvin` (B4) |
| tone mapper | look_group/image_tab | 3 | |
| film grain | look_group/image_tab | 3 | |
| lut | look_group/image_tab | 3 | |
| bloom | look_group/bloom_glow_tab | 3 | |
| roto ink | look_group/stylize_tab | 3 | |
| roto ink isolate | look_group/stylize_tab | 3 | user example; stem rule |
| tron trails | look_group/tron_tab | 3 | |
| on twos | look_group/stepped_tab | 3 | |
| stepped particles | look_group/stepped_tab/sa_particles | 3 | user example; context field + stem |
| shimmer | look_group/actor_style_tab | 3 | |
| rain | world_group/weather_tab | 3 | |
| lightning | world_group/weather_tab | 3 | |
| rain shelter | world_group/weather_tab | 3 | |
| water level | (B3 card) | 3 | user example; pending_until `RenderWaterLevelOverride` (B3) |
| fog volumes | world_group/atmosphere_tab | 3 | |
| projector cones | world_group/atmosphere_tab | 3 | |
| shadow distance | world_group/shadows_probes_tab | 3 | |
| probe on demand | (B7 card) | 3 | pending_until `RenderProbeOnDemand` (B7) |
| time scale | capture_group/temporal_tab | 3 | |
| snapshot | floater:snapshot | 5 | |
| auto capture mode | (B5 card) | 3 | pending_until `BDMergeCineModeAuto` (B5) |
| countdown | shell/delay_spinner | 3 | G5 |
| save scene | shell/btn_scene_save | 3 | G5 |
| set marks | shell/btn_set_marks | 3 | |
| show paths | shell/show_paths_global_check | 3 | |
| add animesh | shell/btn_add_animesh | 3 | |
| arm camera | shell/arm_camera_check | 3 | |

That is 103 probes: 94 active and 9 pending. Typos ("expsoure", "lightnign") and plurals ("shadows", "particles") go in a short second list with `top_n` 3, to pin the fuzzy behaviour.

### 3.5 G-labels: no raw widget-name labels

A row's label is **raw** if any of these is true:
- it equals its widget, or `widget.replace('_', ' ')`;
- it matches `^[a-z0-9]+(_[a-z0-9]+)+$`;
- it equals its setting name;
- it is a single CamelCase identifier with 2 or more humps and no space;
- it starts with `rst ` or `fx reset `.

Any raw label is a FAIL.

Cleaned-name labels (flag `r`) are allowed but counted. The count is pinned in `search_exclusions.json` as `review_label_baseline` and may only shrink. A new kit row always has a label, so Batches B and C cannot raise it.

### 3.6 G-budget

- At most 8,000 entries and 2.0 MB.
- Every external row has flag `e`, a floater, and no Director page key.
- Every v4 line has exactly 24 fields.

### 3.7 Planted adversarial fixtures (gate self-test)

The fixtures live in `scripts/code_tools/director_kit/search_fixtures/<name>/`. Each is a mini tree:
- `floater_director_v2.xml`
- one or two pages and one included panel
- a mini `llviewerfloaterreg.cpp` / `llfloaterdirector.cpp` excerpt when needed
- `director_kit_index.xml`
- `expect.json` = `{"gate": "...", "verdict": "fail", "finding": "<rule id>"}`

`clean/` must **pass** (so a gate that fails everything is also caught). The fixture name list is pinned inside `director_search_check.py`, so deleting a fixture fails the self-test.

| Fixture | Planted defect | Must fail |
|---|---|---|
| `cov_hosted_toggle` | `check_box fx_chaos_check` in an included panel, absent from the index | G-coverage `unindexed-control` |
| `cov_inner_tab` | a control inside a `tab_container` tab, indexed with an empty host tab | G-coverage `missing-host-tab` |
| `cov_card_switch` | `kit_card enable_setting` with no switch row | G-coverage `unindexed-switch` |
| `cov_page_master` | `kit_page master_widget` with no row | G-coverage `unindexed-master` |
| `cov_rail_master` | `rail_item master_settings="FooEnabled"` with no control in the group | G-coverage `unbound-master` |
| `cov_shell_popover` | a `kit_switch` in `kit_arm_popover`, unindexed | G-coverage `unindexed-control` |
| `cov_item` | a combo item "Kaleidoscope", unindexed | G-coverage `unindexed-item` |
| `excl_reason` | an exclusion with an empty reason | G-coverage `bad-exclusion` |
| `excl_stale` | an exclusion for a widget that does not exist | G-coverage `stale-exclusion` |
| `excl_wildcard` | an exclusion with `"widget": "reset_*"` | G-coverage `bad-exclusion` |
| `excl_reset_toggle` | `check_box reset_mode` hidden by the reset rule | G-coverage `reset-count` |
| `dead_row` | an index row for `sl_gobo` on a LINK page without it | G-dead `dead-row` |
| `dead_ambiguous` | a widget present in two tabs with no host tab recorded | G-dead `ambiguous-row` |
| `dead_external` | an external row whose widget is not in its floater tab | G-dead `dead-external` |
| `link_xui` | `Floater.ShowOrBringToFront parameter="foo_float"`, not indexed | G-links `unindexed-target` |
| `link_cpp_typed` | `showTypedInstance<ALFloaterFoo>("foo")` in a hosted .cpp | G-links `unindexed-target` |
| `link_unknown_popout` | a button "Open Foo window..." with no resolvable target | G-links `unmapped-launcher` |
| `link_action_row` | `kit_row action="floater:bar"` with `bar` not indexed | G-links `unindexed-target` |
| `probe_rank` | the expected page is at rank 4 with `top_n` 3 | G-probe `probe-miss` |
| `probe_missing_target` | the expected page is not in the index, and there is no `pending_until` | G-probe `probe-target-missing` |
| `probe_armed` | `pending_until` setting now bound, but the probe misses | G-probe `probe-miss` |
| `probe_table_shrunk` | 79 probes | G-probe `probe-table` |
| `probe_noise` | "drone" returns 50 hits with `max_hits` 20 | G-probe `probe-noise` |
| `page_label_only` | the page label exists only in `page_label`, with no page row ("practicals" probe) | G-probe `probe-miss` |
| `parity_weight` | the port with the LABEL weight changed 80 to 81 | G-probe `parity-drift` |
| `parity_stale_sha` | a C++ scorer region whose SHA differs from the `.inc` header | G-probe `parity-stale` |
| `label_raw` | rows labelled `cine_key_ev`, `CineLightRigKeyEV` and `fx reset BDMergeFroxelDensity` | G-labels `raw-label` |
| `label_baseline` | review-label count baseline + 1 | G-labels `review-baseline` |
| `budget` | an index of 8,001 lines | G-budget `index-budget` |

The Opus reviewer adds hidden fixtures on top of these, as the "Codex relaxes gates" rule requires.

---

## 4. C++ changes (minimal)

| File:line | Change |
|---|---|
| `aldirectorqol.h:19-28` | Append `SearchField::CONTEXT` after `SYNONYM`, so the existing enum values do not change. Declare `S32 searchExternalPenalty(size_t query_tokens)` and `void splitSearchTokens(const std::string&, std::vector<std::string>&, bool lowercase)` (moved here from `ALKitSearch`, so the test needs no LLDir). |
| `aldirectorqol.cpp:51-69` (`fuzzyTokenScore`) | Wrap the region in `// [search-scorer-begin/end]` markers. Then: (1) add a **stem rule**: when both tokens are at least 4 characters and `stem(q) == stem(c)`, return 85 (after exact, prefix and substring). The stem strips one of `ations ation ions ion ings ing ers er es ed e s` (not `s` after `s` or `u`), leaving at least 4 characters, then undoubles a final consonant. (2) **Subsequence** (`:64-65`) now also requires `candidate[0] == query[0]` and `candidate.size() <= 2 * query.size()`. (3) The **edit ceiling** (`:66`) becomes 0 below 4 characters, 1 for 4-7 characters and 2 for 8 or more. All 6 checks in `tests/aldirectorqol_test.cpp:14-24` still hold: `expsoure` has 8 characters, `shdw` is a first-letter subsequence of `shadow`, and `zzz` is rejected. In the port, "drone" falls from 1,048 hits to 6. |
| `aldirectorqol.cpp:75` | `WEIGHTS` becomes `{80, 55, 45, 25, 90, 15, 5, 20}` (CONTEXT = 20, a qualifier that AND-joins page, group and section words such as "stepped particles" and "camera shake"). Add `EXTERNAL_RANK_PENALTY = 30` per query token, which `searchExternalPenalty` returns. It is not applied to floater or tab rows (`f`). |
| `alkitsearch.cpp:17,81` | `INDEX_FIELDS` becomes 24; the header string becomes `version="4" fields="24"`. |
| `alkitsearch.cpp:67-116` | Split into `bool parse(const std::string& xml)` (no I/O) and `load()`. File reading and `gDirUtilp` stay in `load()`, which moves to `alkitshell.cpp:1691-1703` (`startSearchLoad`) and `aluiperfselftest.cpp:8060`. Both read the file and call `parse()`, so `alkitsearch.cpp` links into a tut test with only llcommon. The perf log at `alkitsearch.cpp:262-266` moves to the caller for the same reason. |
| `alkitsearch.cpp:118-155` (`tokenizeBatch`) | Tokenize fields 2, 3 and 22 (and, for external rows, 18 and 20) into `context_tokens`, and field 23 into `parent_tokens` (scored as CARD). Parse the new flags into `Entry::kind`. Copy fields 19/20 to `host_tab` / `host_tab_label` for Director rows. Build `path` for item rows as "<group> › <page> › <card> › <parent>: <label>". |
| `alkitsearch.cpp:178-251` (`query`) | **Perf:** for each query token, compute `fuzzyTokenScore` once per interned token into a `std::vector<S32>` indexed by token id. Each entry and field is then scored with a table lookup plus the weight. This is about 8k fuzzy calls per query token instead of entries × tokens (about 150k), so query cost stops scaling with the index. Score the CONTEXT and parent fields. Subtract the external penalty for `e` rows without `f`. Keep AND semantics, the rule that tokens of 2 characters or fewer score only label/card/keyword (`:209`), the full-label bonus (`:216-218`) and the sort (`:252-261`). |
| `alkitsearch.h:16-41,54-83` | `Hit` and `Entry` gain `kind`, `host_tab`, `host_tab_label` and `parent_label`; `Entry` gains `context_tokens` and `parent_tokens`. |
| `alkitshell.h:39-43` | `ALKitTarget` gains `host_tab` and `kind`. Declare `bool revealShellControl(const std::string& widget)`. |
| `alkitshell.cpp:1610-1624` | `activateSearchResult` dispatches on the kind (section 2): shell rows go to `revealShellControl`; external and floater rows go to `openExternalResult`; everything else goes to `navigateTo` with `host_tab`. |
| `alkitshell.cpp:1036-1080` | `navigateTo`: after `revealCard` (`:1050-1051`), highlight `target.widget` when it is found inside the card or page (switches and masters). In the legacy branch (`:1052-1079`), scope by `host_tab`, then select the tab on every `LLTabContainer` ancestor before the existing accordion, scroll and highlight steps. Return true for a page-only target. |
| `alkitshell.cpp:63-80` | `findExternalTarget`: compare the control variable only when `!setting.empty()` (bug fix). `openExternalResult` (`:1626-1655`): a floater or tab row with an empty widget and setting opens with no pulse and returns true. |
| `alkitshell.cpp:1408-1418` | A local card or switch hit keeps all rows of its card visible. |
| `alkitshell.cpp:1495-1515` | Display the context for item, switch and shell rows (section 2). |
| new `alkitshell.cpp` `revealShellControl` | `findChild` under the shell, `show()` each `ALKitPopover` ancestor, highlight, and fall back to the toast. |
| `aluiperfselftest.cpp:8069-8072` | Add "kaleidoscope", "camera shake" and "drone" to the timed queries. |
| `indra/newview/CMakeLists.txt:2754-2780` | Add `alkitsearch.cpp` to `viewer_TEST_SOURCE_FILES`, with `LL_TEST_ADDITIONAL_SOURCE_FILES aldirectorqol.cpp`. |
| new `indra/newview/tests/alkitsearch_test.cpp` | Include `alkitsearch_parity.inc`; assert the fuzzy vectors, the splitter vectors and the mini-index ranked lists (3.4). |

---

## 5. Work packages (Codex), size, and performance

**WP1: generator and index (M, about 1-1.5 days).**
- New `scripts/code_tools/director_kit/search_index.py` (sections 1.1-1.6).
- `director_kit_gen.py`: `index_bytes()` calls it. Delete the mockup Director-row loop (`:975-989`), the launcher regex (`:994-1001`), the legacy-skipping drift check (`:1006-1010`, now redundant because the index is derived from the same XML) and `external_index_rows()`. Update the selftest checks at `:1323-1335`.
- `synonyms.json` v2 and `external_search.json` v2 (overrides).
- Report sections `REVIEW_LABELS`, `LINK_TARGETS` and `INDEX_COUNTS` in `gen_report.txt`.
- Regenerate. Run `director_page_sources.py`, `director_wiring_table.py` and `director_functional_manifest.py` afterwards, per `SPEC_light_actors_kit.md:309`.
- Acceptance:
  - `director_kit_gen.py --check` and `--selftest` are green.
  - The index has about 5,000 entries and 2.0 MB or less.
  - Zero rows with a raw label.
  - The cleaned-name count is printed.

**WP2: C++ search and navigation (S/M, about 1 day).** Everything in section 4.
- Acceptance:
  - `zs_check` is clean.
  - The new tut test passes.
  - The existing `aldirectorqol_test.cpp` tests are unchanged and pass.
  - In-world click test (the functional gate): one hit of each row type. That means a kit row, an Advanced row, a card switch, a page master, a legacy control in a Ghosts inner tab, an Exposure tab control, a shell popover control, a combo item, a floater row and a Lightbox tab row. Each must show the highlighted control (screenshot evidence).

**WP3: gates, fixtures, probe table, parity (M, about 1 day).**
- `director_search_check.py` (G-coverage, G-dead, G-links, G-probe, G-labels, G-budget).
- `search_port.py`, `search_probes.json`, `search_exclusions.json` and `search_fixtures/` (the 29 fixtures plus `clean`).
- `alkitsearch_parity.inc` emission.
- The prebuild guard line (`prebuild_guard.ps1:73-79`).
- Acceptance:
  - The self-test proves every fixture's verdict.
  - `--check` is green on the real tree.
  - Every active probe passes.
  - The pending list matches the B ids.

**Order and review.** WP1 → WP3 (the gates are written against WP1's output; the port must exist before WP2 changes the scorer) → WP2 (C++ changes go in with the parity vectors already committed, so the tut test is red until the C++ matches the port). Opus adversarial review after each WP. One build at the end, following the checkpoint-commit-then-`fresh_build.ps1` rule.

**Performance.**
- **Load.** The index is read once on the first idle after the shell is built (`alkitshell.cpp:1691-1716`) and tokenized at 400 lines per idle (`:1705-1716`). At about 5,000 lines that is about 13 idles against 11 today; the file grows by about 25%.
- **Memory.** About +1 MB of strings, plus token ids.
- **Query.** Per-token memoization makes the cost about `tokens_in_dictionary × query_tokens` fuzzy calls plus `entries × fields` lookups. That should be faster than today despite the larger index.
- **Budget**, measured by `ui_perf_selftest` (`aluiperfselftest.cpp:8058-8080`, metrics `kit_index` and `kit_search`): worst `kit_search` ≤ 5 ms and `kit_index` ≤ 80 ms total CPU. WP2 reports both numbers. The 120 ms debounce (`alkitshell.cpp:1429`) is unchanged.

---

## 6. How Batches B and C plug in

- **New kit rows, cards, switches, masters, items and pages (B1-B9).** The generator emits them from the generated XML. There is nothing to register. A row without a label fails kit_lint (existing) and G-labels.
- **New LINK buttons** (for example `edit_hover_height`, `pref_joystick`, `env_adjust_water`, `env_intensity_presets`, `fs_poser`). They are discovered from the `kit_row action` or `kit_card target`, then indexed through the registry. An unusual C++ idiom is caught by G-links (`unmapped-launcher`).
- **Pending probes arm themselves.** When B2 binds `AvatarHoverOffsetZ`, B3 binds `RenderWaterLevelOverride`, B1 binds `JoystickXInputEnabled`, B4 binds `AlchemyEnvSunKelvin`, B5 binds `BDMergeCineModeAuto`, B7 binds `RenderProbeOnDemand` and B6 adds the `fs_poser` link, the matching probes become hard requirements in the same prebuild.
- **Conversions (C1-C6).** When a page flips from legacy to kit, its rows switch from the legacy walker to `kit_row` automatically. Exclusions and reset counts that pointed at the old legacy widgets go stale and fail, which forces cleanup in the same change. The probes for that page act as a regression guard (for example "onion skin" must still land on Path after C5). The classification pin at `director_kit_gen.py:1196-1204` is unaffected.
- **External dedupe follows the Director.** When C4 moves Diopter settings into kit sections, the matching `ultimate_diopter` rows drop out by setting dedupe automatically. The "kaleidoscope" probe is retargeted in that same change with a cited reason.

---

## 7. Open questions for the user (each has a recommended default)

1. **Lightbox scope.** Should the whole Lightbox stay indexed (all tabs), or only the tabs the Director opens? **Default: keep all tabs.** No regression; the dedupe already removes rows that duplicate Director settings.
2. **Combo and preset items.** Should each item be its own search result ("Kaleidoscope", shown under "Tool mode"), or only a hidden keyword on the combo row? **Default: its own result.** Clearer, at the cost of about 1,700 more rows.
3. **Should a search hit ever change a value**, for example selecting "Kaleidoscope" in the combo? **Default: no.** It only navigates and highlights.
4. **Controls hidden until some state is reached** (19 on Ghosts, Gaze, Path and Switchboard). **Default: exclude them with reasons** and revisit them in C5/C6. The alternative is to index them with a "shown when..." note.
5. **Scorer tightening** (subsequence must start on the same letter, edit ceilings 0/1/2, stem rule, external rows ranked 30 points per word lower). Fuzzy matches become slightly stricter. **Default: adopt.** It turns "drone" from 1,048 hits into 6, and typos such as "expsoure" still match.
6. **Pending probes for Batch B features** ("water level", "hover height", "gamepad", "kelvin", "poser", "probe on demand", "auto capture mode"). Should they stay listed as pending until their batch lands, or be red now? **Default: pending, with automatic arming.**

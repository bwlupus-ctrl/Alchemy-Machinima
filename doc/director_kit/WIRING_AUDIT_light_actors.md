# Wiring audit: Light > Actors (Avatar Light Response) kit conversion

Static, read-only audit of the legacy Actors page before it becomes a generated kit page (10/15). No build, no commit, no edits to any other file. Legacy sources: `indra/newview/skins/default/xui/en/panel_avatar_light_response.xml` and `indra/newview/alpanelavatarlightresponse.{h,cpp}`. Consumers: `alavatarlightresponse.{h,cpp}` (store) and `alavatarlightresponsemodel.{h,cpp}` (clamps). Mockup: `scripts/code_tools/director_kit/director_mockup_data.json`, which is generated from curated files outside the repo (see Section 3).

## Result

- The Actors page is still legacy. `kit_pages.json` `p0_pages` has no `light_group/actors_tab` entry, so `classify()` returns "legacy" (`scripts/code_tools/director_kit_gen.py:175-185`). The current wrapper `panel_dpage_light_actors.xml` is generated and in sync: its header hashes (`d365d577933be8a7`, `bd0338bef77202fd`) match the current `director_mockup_data.json` and `kit_pages.json`.
- Legacy panel: 28 named widgets plus one unnamed static label ("Fill"). 3 are setting-bound (`chk_enabled`, `chk_persist`, `chk_debug_tint`). 24 are driven in code (per-avatar store, panel-local state, or live readouts). 1 is a static header that the curated source drops.
- Every legacy range equals its consumer clamp. No range needs to change. Clamps: `alavatarlightresponsemodel.cpp:106-110`.
- Mockup gaps: `chk_persist`, the preset row (only a page-level list exists), and two conditional readouts (`txt_budget`, `txt_sil_note`) are missing. The Actor Mask card (6 rows) is mockup-only and replaces a v1 launcher stub.
- Mockup defects to fix before conversion: the `who` block and the `avatar_list` row's `who_picker` would hide the target list and leave an inert chip (Section 3, defects a-b). The Isolation test combo is a trigger, not a setting (defect e).
- Required code: a generator P0 entry, a Director binder (or a custom host), and updated hard-coded classification counts (Section 5).

## 1. Legacy controls

Consumer clamp = `alavatarlightresponsemodel.cpp` `sanitize()` (lines 103-119), which is applied on every store `set()` (`alavatarlightresponse.cpp:272`). Limits are the constants in `alavatarlightresponsemodel.h:26-36`.

Read-back for every per-avatar row: `refreshEditors()` (`alpanelavatarlightresponse.cpp:692-753`), which reads the first selected row (`keys.front()`, line 700). It runs on selection change, on store change (`postBuild` 160-164, `draw` 225-228), and at a 1 Hz cadence (`draw` 219-224, `idleUpdate` 259-289).

| # | widget | type | label | drives (setting, or handler + exact write) | range / items and consumer clamp | read-back |
|---|---|---|---|---|---|---|
| 1 | chk_enabled (`xml:3-13`) | check_box | Enable Avatar Light Response | setting `AvatarLightResponseEnabled` (Boolean, default 1, `settings.xml:30604-30610`) | none | control binding. Master commit resets impostors and marks the table dirty (`alavatarlightresponse.cpp:94-104`). Table read at `alavatarlightresponse.cpp:397` |
| 2 | chk_persist (`xml:14-24`) | check_box | Remember between sessions | setting `AvatarLightResponsePersist` (Boolean, default 0, `settings.xml:30611-30617`) | none | control binding. ON commit: `captureFileExtras()` then `saveToFile()` (`alavatarlightresponse.cpp:106-119`). Gates load (`:823`) and every save (`saveIfPersist` `:739-744`). **Not in the mockup.** |
| 3 | btn_refresh (`xml:25-34`) | button | Refresh | none; sets `mIdleRowsPending` and `mIdleStatePending` (`panel:120-126`) | n/a | next idle tick (`panel:238-289`) |
| 4 | target_list (`xml:35-59`) | scroll_list, multi_select, columns state / name / kind | Avatars (heading) | panel-owned rows from `collectRows()` (`panel:329-424`: LLDirectorCast choices 354-370, residents 372-384, animesh 386-394, clones 396-400, pinned 402-406, stored 407-423). Selection read by `selectedKeys()` (`panel:621-632`); commit calls `onSelectionChanged()` (`panel:113, 806-813`) | none | `refreshRows()` (`panel:427-527`) and `refreshStateCells()` (`panel:530-570`) |
| 5 | chk_only_adjusted (`xml:60-68`) | check_box | Only adjusted | panel-local filter, no setting. Commit calls `refreshRows(true)` (`panel:115-119`); filter at `panel:445-457` | none | not persisted; the list reflects it on commit |
| 6 | sld_diffuse (`xml:69-84`) | slider | Diffuse (%) | per selected row `p.mDiffuse = v / 100` (`panel:826`) via `onSliderCommit` (`panel:815-840`) | UI 0-100 (step 1, 0 dp). Consumer 0..1: `DIFFUSE_MIN/MAX` (`model.h:26-27`), `sanitize` line 106 | `refreshEditors` `panel:703` |
| 7 | sld_tame (`xml:85-100`) | slider | Tame reflections (%) | `p.mTame = v / 100` (`panel:827`) | UI 0-100 (0 dp). Consumer 0..1: `model.h:28-29`, `sanitize` line 107 | `panel:704` |
| 8 | sld_brightness (`xml:101-116`) | slider | Brightness | `p.mExposureEV = v` (`panel:828`) | UI -2..1, step 0.05, 2 dp. Consumer EV -2..1: `model.h:30-31`, `sanitize` line 108 | `panel:705` |
| 9 | txt_brightness_ev (`xml:117-126`) | readout | "+0.00 EV" | none (display) | n/a | `refreshBrightnessValue()` `panel:755-761`, format `%+.2f EV` at line 759 |
| 10 | sld_glow (`xml:127-142`) | slider | Glow (%) | `p.mGlow = v / 100` (`panel:829`) | UI 0-200 (0 dp), default 100. Consumer 0..2, default 1: `model.h:32-34`, `sanitize` line 109 | `panel:706` |
| 11 | chk_bypass (`xml:143-152`) | check_box | Bypass (A/B) | `p.mBypass` via `onBypass()` (`panel:928-941`), `applyParams(..., save_now=true)` at line 938 | Boolean. Bypass is an explicit identity (`alpanelavatarlightresponse.cpp:546` shows identity entries as hollow) | `panel:707` |
| 12 | txt_sil_header (`xml:153-162`) | static text "Silhouette" | none | none. Dropped in the curated source as obsolete (`light_group.json` actors_tab `dropped`) | n/a | n/a |
| 13 | sld_sil_amount (`xml:163-178`) | slider | Silhouette (%) | `p.mSilAmount = v / 100` (`panel:830`), then `refreshSilhouetteNote()` (`panel:838`) | UI 0-100 (0 dp). Consumer 0..1: `sanitize` line 110; `silQuant` clamps again (`model.cpp:137-143`) | `panel:708` |
| 14 | swatch_sil_fill (`xml:188-198`) | color_swatch, can_apply_immediately | (no label) | `onSilFill(save_now, custom)` (`panel:848-876`): commit `(false,true)` `panel:146`; select `(true,true)` `:147`; cancel `(true,false)` `:148`. Writes `p.mSilFillSrgb` for all selected rows | each channel clamped 0..1 (`sanitize` line 113) | `panel:709-711` |
| 15 | combo_sil_fill (`xml:199-213`) | combo, 6 items | Fill | `onSilFillPreset()` (`panel:878-903`) sets fill from `EFillPreset`. Value 5 "Custom" returns early (`panel:890-893`). Enum order matches the XML values (`model.h:105-114`) | Black 0, Charcoal 1, White 2, Chroma Green 3, Chroma Blue 4, Custom 5 | `matchFillPreset` `panel:712-713`; model `:466` |
| 16 | chk_sil_haze (`xml:214-223`) | check_box | Haze over silhouette | `p.mSilHaze` via `onSilCheckbox(true)` (`panel:905-926`) | Boolean (default true, `model.h:46`) | `panel:714` |
| 17 | chk_sil_keep_rim (`xml:224-233`) | check_box | Keep Rim | `p.mSilKeepRim` via `onSilCheckbox(false)` | Boolean (default false) | `panel:715`. Note text if Rig Rim is off (`refreshSilhouetteNote` `panel:790-801`, reads `CineRigRimEnabled` `:769-770`) |
| 18 | txt_sil_note (`xml:234-244`) | text, visible=false | none | none (display). Text: "Preparing silhouette buffer..." (when `RenderEnableEmissiveBuffer` and `AvatarSilhouetteForceEmissive` are both off, `panel:765-768`) or "Keep Rim: Rig Rim is off (Cine Light Rig)" (`panel:799-801`) | n/a | `refreshSilhouetteNote()` `panel:763-804` |
| 19 | combo_preset (`xml:245-289`) | combo, 9 items | Preset | selection read by `onApplyPreset()` (`panel:943-962`); `selectFirstItem()` at `panel:159` | Reset 0, Powdered 1, Matte skin 2, Tame gloss 3, Kill sheen 4, -1/2 stop 5, -1 stop 6, Glow off 7, Silhouette 8. Matches `EPreset` (`model.h:158-170`) and `applyPreset` (`model.cpp:612-652`) | none (the selection is not read back) |
| 20 | btn_apply_preset (`xml:290-298`) | button | Apply preset | `onApplyPreset()`: `applyPreset(currentParams(key), index)` per selected row (`panel:954-959`), then `saveIfPersist()` (`:960`) | n/a | `refreshEditors()` |
| 21 | btn_reset_selected (`xml:299-308`) | button | Reset | `onResetSelected()` (`panel:964-973`): `applyParams(key, Params())`. Explicit identity, keeps the entry | n/a | `refreshEditors()` |
| 22 | btn_inherit_selected (`xml:309-318`) | button | Inherit | `onInheritSelected()` (`panel:975-984`) to `store.inherit()` (`alavatarlightresponse.cpp:287-308`). Removes the entry | n/a | enabled only when every selected row is animesh or clone with an entry (`panel:736-752`) |
| 23 | btn_copy (`xml:319-327`) | button | Copy | `onCopy()` (`panel:1004-1016`) stores `mClip` (static, `header:131-133`) | n/a | enabled with a selection (`panel:733`) |
| 24 | btn_paste (`xml:328-336`) | button | Paste | `onPaste()` (`panel:1018-1030`) applies `mClip` to all selected rows | n/a | enabled with a selection and a clip (`panel:734`) |
| 25 | btn_reset_all (`xml:337-346`) | button | Reset all... | `onResetAll()` (`panel:986-1002`): confirm `AvatarLightResponseResetAll`, then `clearAll(true)` (`alavatarlightresponse.cpp:310-331`) | n/a | `mIdleRowsImmediate` (`panel:997-998`) |
| 26 | chk_debug_tint (`xml:347-357`) | check_box | Debug: magenta tint | setting `AvatarLightResponseDebugTint` (Boolean, default 0, not persisted, `settings.xml:30625-30631`). Consumer `lldrawpool.cpp:541` | Boolean | control binding |
| 27 | txt_status (`xml:358-368`) | readout | "N adjusted / uploaded / visible" | none (display) | n/a | `refreshStatus()` `panel:572-600` (1 Hz) |
| 28 | txt_budget (`xml:369-379`) | text, visible=false | none | none (display). Shown when `countLiveOverBudget() > 0` (`panel:602-618`), text includes `LLVOAvatar::sMaxNonImpostors` | n/a | `refreshStatus()` |

Checkbox and slider `initial_value` attributes in the XML are overwritten by `refreshEditors()`; they are not the effective defaults.

### Consumer clamp summary

| field | UI range (XML) | consumer range | clamp location | default |
|---|---|---|---|---|
| Diffuse | 0-100 % | 0..1 | `alavatarlightresponsemodel.h:26-27`, `.cpp:106` | 0 |
| Reflections (tame) | 0-100 % | 0..1 | `.h:28-29`, `.cpp:107` | 0 |
| Brightness (EV) | -2..1, step 0.05 | -2..1 | `.h:30-31`, `.cpp:108` | 0 |
| Glow | 0-200 % | 0..2 | `.h:32-34`, `.cpp:109` | 1 (100 %) |
| Silhouette amount | 0-100 % | 0..1 | `.cpp:110`, `silQuant` `:137-143` | 0 |
| Fill colour | 6 presets + swatch | each channel 0..1 | `.cpp:113` | black (zero vector) |
| Mask bias (Actor only) | -64..0, step 0.5 | -64..0 | `pipeline.cpp:25703` (also `:25538`) | -8 (`settings.xml:2218-2224`) |
| Mask debug (Actor only) | 0 Off / 1 Overlay / 2 Mask only | no clamp found | `alactormaskdiagnostics.cpp:1537` | 0 |
| Mask isolation test | 0 Idle / 1 Run / 2 Cancel | trigger; self-resets to 0 on idle | `alactormaskdiagnostics.cpp:428-429, 685-686, 700` | 0 |
| Stored entries | cap 512 | refused with notification | `alavatarlightresponse.cpp:39, 264-266`; `panel:680-681` | n/a |

## 2. Non-trivial UI parts (not a standard kit row)

| part | what it is | what it needs in the kit | why not a plain row |
|---|---|---|---|
| Avatar list (target_list, Only adjusted, Refresh) | 3-column multi-select list, 1 Hz refresh, rows from 6 sources, state dot colour per cell | custom host of the panel's own `scroll_list` (kind custom). `Only adjusted` and `Refresh` are code-bound buttons/toggles | rows come from the panel; state cells are coloured by store status (`panel:530-570`) |
| Per-avatar trim (Diffuse, Reflections, Brightness, Glow, Bypass) | values belong to each store key, not to a setting | code-bound rows (binder reads first selected, writes all selected) or custom host | see Section 5, risk 1 |
| Silhouette fill (swatch + combo) | swatch has select/cancel semantics (`panel:146-148`); combo "Custom" is a no-op | code-bound colour row and combo, or custom host | callbacks differ from a commit-only kit row |
| Preset combo + Apply | page-level preset strip in the curated source (`presets.json` `page_strip_rows`: `combo_preset` label "Preset", `btn_apply_preset` label "Apply"). The generator emits `presets` but no row (`director_mockup_data.py:383`, `director_kit_gen.py:690-691`) | a kit preset strip (`preset_mode widgets`, `alkitpage.cpp:359`) with `combo_preset` and `btn_apply_preset`; `kit_pages.json` needs a `preset` entry | without a `preset` entry the kit sets `preset_mode none` and renders nothing |
| Reset, Inherit, Copy, Paste, Reset all | act on selected rows, with enable rules (`panel:733-752`) and a confirm dialog (`panel:986-1002`) | code-bound button rows | enable rules and the dialog live in the panel |
| Open window (`light_response_popout`) | opens `avatar_light_response` for the first Director cast selection (`llfloaterdirector.cpp:1970-1974`, in `bindCameraPage`; also `:1011-1017`) | button row, `target` `floater:avatar_light_response:selected-actor` (already in `director_page_sources.py:134`) | target is a floater with a cast-id argument |
| Live readouts (txt_brightness_ev, txt_status) | text updated from the store and the slider | readout rows, code-refreshed | no setting to watch |
| Conditional notes (txt_budget, txt_sil_note) | visible only in some states | `show_if` rows (`director_kit_gen.py:625-626`) with a page adapter that sets visibility in `postBuild`. The adapter is named `ALActorLightKitAdapter` in `director_page_sources.py:297`, but no C++ class of that name exists in `indra/` (grep) | `show_if` needs runtime adapter code |
| Actor Mask card (6 rows) | `am_disable_check` (setting), `am_debug_combo` (setting), `am_bias_slider` (setting), `am_reset_CineLightRigActorMaskBias` (writes -8), `am_test_combo` (trigger), `open_lightbox_actormask` (button, target lightbox) | setting rows for the first three; a reset button; a trigger control for the test; a Lightbox button | the test combo writes 1 (arm) or 2 (cancel) and self-resets (`alactormaskdiagnostics.cpp:428-429`); it is not a persistent value |

## 3. What the mockup says today

Source chain. `director_mockup_data.json` is generated (`director_mockup_data.py:1008`) from the curated folder `C:\Users\xianw\cxs\content_curated` (`director_mockup_data.py:21`), which is outside this repo. Inputs for this page:
- `light_group.json` `actors_tab`: `preset_provider: none`; cards `actor_mask` (launcher), `avatar_list`, `light_response`, `silhouette`, `light_presets`, `actors_diagnostics`.
- `inline_pages.json` (~line 5991): `replace_page: actors_tab`, `replace_card: actor_mask`. It replaces the launcher with 6 rows.
- `presets.json` `page_strip_rows`: `combo_preset` / `btn_apply_preset` in card `light_presets`.

Edits belong in those curated files and then in a regeneration. Editing the repo JSON directly will be overwritten.

Current mockup page (JSON, `light_group` / `actors_tab`): sections `Avatars` (cards avatar_list, light_response, light_presets), `Mask & silhouette` (actor_mask, silhouette), `Advanced` (actors_diagnostics). Master `chk_enabled` / `AvatarLightResponseEnabled`. `who`: label "Adjusting", pickers `["target_list"]`. `presets`: the 9 names, identical to the legacy combo.

| card (JSON id) | mockup rows (widget, kind) | legacy coverage | verdict |
|---|---|---|---|
| actor_mask ("Actor only", `a`-flags on most rows) | am_disable_check (toggle), am_debug_combo (combo), am_bias_slider (slider), am_reset_CineLightRigActorMaskBias (button), am_test_combo (combo), open_lightbox_actormask (button -> lightbox) | not in the legacy panel. The v1 has a launcher stub `card_actor_mask` (`floater_director.xml:1421-1424`), replaced by this card in the curated source | new; see defect e |
| avatar_list (custom) | target_list (list, `who_picker: 1`), chk_only_adjusted, btn_refresh, light_response_popout | all present | defect a |
| light_response (`ew`/`es` = master) | sld_diffuse, sld_tame, sld_brightness, sld_glow, chk_bypass, txt_brightness_ev (readout, advanced) | present | defect b (EV readout advanced) |
| silhouette | sld_sil_amount, combo_sil_fill, swatch_sil_fill, chk_sil_haze, chk_sil_keep_rim | present | none |
| light_presets | btn_reset_selected, btn_inherit_selected, btn_copy, btn_paste, btn_reset_all (advanced) | present. combo_preset and btn_apply_preset are absent as rows (strip) | the strip has no `preset` config in `kit_pages.json`, so nothing renders |
| actors_diagnostics (advanced card) | chk_debug_tint (toggle), txt_status (readout) | present | none |

### Gaps between mockup and legacy

Legacy controls not in the mockup:
- `chk_persist` (Remember between sessions). Also missing from the curated sources; it appears only in the Director search index (`director_kit_index.xml:4182`, under the floater). Add it as a setting toggle (Section 4).
- `combo_preset` and `btn_apply_preset`. Present only as a strip intent (`presets.json`, `STRIP`), not as a generated strip.
- `txt_budget` (over-budget warning). Not in the mockup or curated sources.
- `txt_sil_note` (silhouette buffer / Rig Rim note). Not in the mockup or curated sources.
- `txt_sil_header` (static "Silhouette"). Intentionally dropped as obsolete. The card title covers it.

Mockup rows not in the legacy panel: the 6 Actor Mask rows (curated intent, see above).

### Mockup defects found

a. **`who` block and `avatar_list` row `who_picker: 1`.** The JSON sets `who_picker` on the `target_list` row (generator maps it at `director_kit_gen.py:615`). `ALKitRow::isEffectivelyVisible()` returns false for `mWhoPicker` rows (`alkitrow.cpp:640-645`), and the card layout uses that check (`alkitcard.cpp:717`). The target list would not be laid out. The page's `who` block then looks for an `LLComboBox` named `target_list` (`alkitpage.cpp:653`). A `scroll_list` is not a combo, so the chip falls to the "Actor: follow selection" branch (`alkitpage.cpp:687-691`). Its pin only works with `mActorTarget` set (`:690`), and the cast-selection callback returns early (`:712-713`). The chip is inert. Remove the `who` block and the row's `who_picker`. The list is the picker.
b. **Brightness EV readout is `a: 1` (advanced).** Legacy shows it at all times beside the slider (`xml:117-126`). Advanced rows are hidden unless "More" is open (`alkitrow.cpp:645`). Make it non-advanced.
c. **Presets.** The page has a `presets` list but no preset config in `kit_pages.json`. The generator therefore sets `preset_mode none` (`director_kit_gen.py:690-691`). Add a `preset` entry (mode `widgets` or a provider). Whether `widgets` works without save/delete buttons is **unknown**; verify at `alkitpage.cpp:359` before choosing.
d. **Help text is jargon-heavy.** "albedo", "2 to the power of the EV value", "roughness", "emissive", "tame". Plain replacements are in Section 4.
e. **Isolation test is a trigger.** `am_test_combo` is a plain combo in the mockup. The setting `CineLightRigActorMaskTest` is a trigger: 1 arm, 2 cancel, self-resets to 0 on idle (`settings.xml:2225-2231`; `alactormaskdiagnostics.cpp:428-429`). Model it as trigger buttons ("Run test", "Cancel"), not a persistent combo.

## 4. Proposed kit layout

Binding strategy (decision for the user). Two options:
- **Option 1 (recommended):** native code-bound rows, with a new Director binder `bindLightActorsPage` in `llfloaterdirector.cpp`. It calls public methods on `ALPanelAvatarLightResponse` that do not exist yet (for example selected keys, per-field commit, mouse-up save, refresh). It keeps the card layout. Risk: duplicates the panel logic (see Section 5).
- **Option 2:** custom host of the whole panel in one card, as the current wrapper does. Minimal change, no card split. Only the master, persist, debug tint and Actor Mask rows stay native.

The table below assumes Option 1. Under Option 2, every per-avatar row becomes `kind custom` inside the panel.

Page master: `chk_enabled`, toggle, setting `AvatarLightResponseEnabled`, label "Enable Avatar Light Response".

| section / card | row | kind | label | binding | range / items | dp | help (proposed; plain) |
|---|---|---|---|---|---|---|---|
| Avatars / avatar_list | target_list | custom (list host) | Avatars | panel-owned list | n/a | n/a | "Avatars and their state: green is uploaded and visible, amber adjusted but not visible, hollow bypassed, blank inherits." |
| | chk_only_adjusted | toggle | Only adjusted | code (panel filter) | Boolean | n/a | "Shows only the avatars you have adjusted." |
| | btn_refresh | button | Refresh | code | n/a | n/a | "Rebuilds the avatar list." |
| | light_response_popout | button | Open window | code (`llfloaterdirector.cpp:1970`), target `floater:avatar_light_response:selected-actor` | n/a | n/a | "Opens Avatar Light Response in its own window." |
| | chk_persist | toggle | Remember between sessions | setting `AvatarLightResponsePersist`, NOT advanced | Boolean | n/a | "Keeps each avatar's light response for this account after you log out. Off: changes last for this session only." |
| Avatar light response / light_response | sld_diffuse | slider | Diffuse | code | 0-100, step 1 | 0 | "Softens shine and highlights on the avatar. 0 leaves it unchanged." |
| | sld_tame | slider | Reflections | code | 0-100, step 1 | 0 | "Dims reflections from the sun, probes, lights and mirrors. Diffuse light is unchanged." |
| | sld_brightness | slider | Brightness | code | -2..1 EV, step 0.05 | 2 | "Makes the avatar brighter or darker, in stops. 0 is unchanged. Highlights and glow keep their level." |
| | txt_brightness_ev | readout (NOT advanced) | Brightness EV | code (live) | n/a | 2 | "The current brightness offset, in stops, for the selected row." |
| | sld_glow | slider | Glow | code | 0-200 %, step 1 | 0 | "Scales the avatar's own glow and emissive. Actor FX effects are not affected." |
| | chk_bypass | toggle | Bypass (A/B) | code | Boolean | n/a | "Shows the selected avatar untrimmed for comparison. It still blocks fallback to a wearer or source." |
| Silhouette / silhouette | sld_sil_amount | slider | Amount | code | 0-100 %, step 1 | 0 | "Blends from the normal look (0) to a flat, unlit fill (100). For exact key colours use Actor Chroma Key." |
| | combo_sil_fill | combo | Fill | code | Black 0, Charcoal 1, White 2, Chroma Green 3, Chroma Blue 4, Custom 5 | n/a | "The solid colour of the silhouette. Choose Custom to use the colour below." |
| | swatch_sil_fill | color (code, select/cancel semantics) | Custom colour | code | sRGB 0..1 per channel | n/a | "Colour used when Fill is Custom." |
| | chk_sil_haze | toggle | Haze | code | Boolean (default on) | n/a | "Adds haze over the silhouette." |
| | chk_sil_keep_rim | toggle | Keep rim | code | Boolean | n/a | "Keeps the rim light on the silhouette. Needs Rig Rim on in the Cine Light Rig." |
| | txt_sil_note | readout, show_if | (none) | code (adapter) | n/a | n/a | "Shown only while the silhouette buffer is preparing, or when Keep rim needs Rig Rim." |
| Avatar presets / light_presets | preset strip: combo_preset + btn_apply_preset | preset strip (`widgets`) or combo + button | Preset / Apply | code | 9 items, 0..8 | n/a | "Look to apply to the selected avatars. Apply replaces the trim on those avatars." |
| | btn_reset_selected | button | Reset | code | n/a | n/a | "Sets the selected avatars to no trim, but still blocks fallback." |
| | btn_inherit_selected | button | Inherit | code (enable rule) | n/a | n/a | "Removes the selected avatars' own entry so they follow their wearer or source again." |
| | btn_copy | button | Copy | code | n/a | n/a | "Copies the selected avatar's trim." |
| | btn_paste | button | Paste | code | n/a | n/a | "Pastes the copied trim onto the selected avatars." |
| | btn_reset_all | button, advanced | Reset all... | code (confirm) | n/a | n/a | "Removes every avatar's light response." |
| Mask & silhouette / actor_mask (Actor only) | am_disable_check | toggle | Disable | setting `CineLightRigActorMaskDisable` (Boolean 0) | Boolean | n/a | "Turns Actor only off everywhere without losing your settings." |
| | am_debug_combo | combo, advanced | Debug view | setting `CineLightRigActorMaskDebug` (U32 0-2, not persisted) | Off 0, Overlay 1, Mask only 2 | n/a | "Overlay tints where the actor mask is; Mask only shows just the mask. Not saved between sessions." |
| | am_bias_slider | slider, advanced | Mask bias | setting `CineLightRigActorMaskBias` (F32, default -8) | -64..0, step 0.5 | 1 | "Depth nudge for the actor mask. If the debug view shows holes, move it lower." |
| | am_reset_CineLightRigActorMaskBias | button, advanced | Reset bias | reset path for the setting (`director_kit_gen.py` `reset_setting`); verify | n/a | n/a | "Resets the mask bias to -8." |
| | am_test_run / am_test_cancel (replace am_test_combo) | trigger buttons, advanced | Run test / Cancel | code: write 1 / 2 to `CineLightRigActorMaskTest` (self-resets) | 1, 2 | n/a | "Checks that Actor only lights change nothing outside the actor. Results go to the log. Needs a still camera and subject." |
| | open_lightbox_actormask | button, advanced | Open in Lightbox | target `lightbox:actor_mask_settings` | n/a | n/a | "Opens the full Lightbox Actor Mask tab, with its limits notes." |
| Advanced / actors_diagnostics (advanced) | chk_debug_tint | toggle | Debug tint | setting `AvatarLightResponseDebugTint` (Boolean 0, not persisted) | Boolean | n/a | "Paints every adjusted avatar magenta, revealing surfaces the trim does not cover." |
| | txt_status | readout | Status | code (live) | n/a | n/a | "Counts of adjusted, uploaded and visible avatars." |
| | txt_budget | readout, show_if | Over budget | code (adapter) | n/a | n/a | "Adjusted avatars beyond the impostor limit render at full detail, which costs frame rate." |

Ranges: every range above equals the legacy XML range and the consumer clamp (Section 1). No range is widened or narrowed.

Decimals: as listed. Glow, Diffuse, Reflections, Amount are 0 dp. Brightness is 2 dp (step 0.05). Mask bias is 1 dp (step 0.5).

## 5. Risks

1. **Per-avatar values, not settings.** Every trim value is per store key. Reads take the first selected row (`panel:700`). Writes apply to all selected rows (`panel:821-834`, `856-861`, `895-899`, `911-923`, `934-939`, `954-959`). A setting-bound kit row cannot express this. `ALKitSettingWatch` (`alkitbinding.cpp:228`) watches settings only. Store changes reach the panel through `setChangeCallback` (`alavatarlightresponse.h:134-137`; `panel:160-164`) and the revision check (`panel:225-228`). A binder must subscribe to the store, not the settings.
2. **Who-chip is wrong for multi-select.** The per-target precedent is `actor_style_tab` (single target, `actor_target: true`, `bindActorStylePage` `llfloaterdirector.cpp:1720`, `actorStyleTargetId` `:4740`). The Actor page's list is multi-select, so the single-target chip model does not fit. The list must stay the picker. Remove the `who` block (Section 3, defect a).
3. **Slider drag and save.** Drags write with `save_now=false` (`panel:830-833`). Persistence happens on mouse-up (`panel:842-846`, `saveIfPersist` at `:845`). A generic kit slider does not save on mouse-up. With persistence ON, a binder that skips this loses drags on exit. The 1 Hz refresh must not reset a slider the user holds (`panel:134-143`, `:278-283`; `alIsBeingEdited` `aluirefresh.h:124`).
4. **Persistence toggle.** Turning it ON captures file-only entries and writes the file (`alavatarlightresponse.cpp:106-119`; `captureFileExtras` `:691-737`). Turning it OFF stops saves and leaves the file. It must be a setting-bound row so the commit hook runs. It is missing from the mockup (add it, Section 4).
5. **Capacity refusal.** 512 stored entries (`alavatarlightresponse.cpp:39`). A refused new entry shows a notification (`panel:680-681`). A kit row must not fail silently.
6. **One panel, three hosts.** The same class is hosted by the `avatar_light_response` floater (`floater_avatar_light_response.xml:14`; `alfloateravatarlightresponse.cpp:17-26`), the v1 Director tab (`floater_director.xml:1419`), and the kit wrapper (`panel_dpage_light_actors.xml:9`). Any public-surface change must keep all three working. The clipboard `mClip` is static and shared across instances (`header:131-133`).
7. **Actor Mask launcher removal breaks three references.** The launcher card is bound in `llfloaterdirector.cpp:2190-2196` (`addLauncherCard("actor_mask", ...)`, also `:1069`), named as the functional launcher in `aluiperfselftest.cpp:2421`, and listed in `director_functional_manifest.py:56`. Update all three with the conversion.
8. **Generator counts are hard-coded.** `director_kit_gen.py:1191-1192` requires kit (P0) 21, pure kit 1, legacy 14. Converting this page makes it kit 22, legacy 13. The `expected` dict (`~1183-1190`) and the comment history must change too, or the generator raises "classification table drift". A new `p0_pages` entry in `kit_pages.json` is required.
9. **`show_if` needs an adapter.** `txt_budget` and `txt_sil_note` need a page adapter that sets visibility in `postBuild` (`director_kit_gen.py:620-626`). `ALActorLightKitAdapter` is named in `director_page_sources.py:297` but is not defined in `indra/`. Either write it or drop these two rows into the status readout.
10. **Regeneration source is outside the repo.** Fixes go to `C:\Users\xianw\cxs\content_curated` (`light_group.json`, `inline_pages.json`, `presets.json`), then `director_mockup_data.py` regenerates the JSON. Edits to `director_mockup_data.json` in the repo are lost on the next run.
11. **No Lightbox reference.** Nothing in `alfloaterlightbox.cpp` / `alfloaterlightbox.h` edits the light response, so the legacy panel is the only reference (unlike the Look pages). Confirm the Lightbox "Actor only" tab stays the same.
12. **Help text drift.** Mockup help and XML tooltips already differ (Diffuse, Reflections, Brightness). The kit will show the mockup text. Pick one set and update the curated source.

Not run: the layout check (`xui_layout_check`), the generator, and any build. This audit is static.

## Open items (unknown, to verify before conversion)

- Whether the preset strip works in `widgets` mode without save/delete buttons (`alkitpage.cpp:359` onward).
- Whether the generator's `reset_setting()` path produces the "Reset bias" button for `CineLightRigActorMaskBias`.
- Whether `alkitrow` can host an existing panel-owned `scroll_list` as a custom row without the generator creating its own list (the `list` kind mapping is `director_kit_gen.py:53-66`).
- Whether `ALActorLightKitAdapter` exists under another name or in an unpushed branch.

# SPEC: Quick page as favourite FEATURES (Director v2)

Status: draft for review, 2026-10-15. This is a document only. No code, XUI or data was changed while writing it.

User decision (10/15): *"The favorite page should function as a favorite feature, not a favorite setting."* The chosen design: one **feature tile** per favourite. A tile shows the feature's name and icon, its on/off master switch, its preset picker if it has one, and an **Open** button that jumps to the full page. Nothing is duplicated, so a tile cannot go out of sync. There are no per-row proxies.

Scope: Director v2 only (`floater_director_v2.xml`). Paths are relative to the repo root (`I:/alchemy-machinima`). XUI is under `indra/newview/skins/default/xui/en/`. `doc/director_kit/SPEC_search_index.md` does not exist yet, so search references point to `COVERAGE_AUDIT_2026-10-15.md` §4-5 (batch A3: page and card rows).

---

## 0. What exists today (to be replaced)

| Piece | Where | Behaviour |
|---|---|---|
| Storage | `DirectorFavorites` LLSD array of `{page, widget}` (`indra/newview/app_settings/settings.xml:8823-8824`); load and clean at `alkitshell.cpp:344-348` | One entry per starred **row** |
| Model | `ALKitQol::Favorite`, `sourceFavorite`, `isValidFavorite`, `favoritesFromLLSD/ToLLSD` (`alkitqol.h:15-23, 68-104`); test `indra/newview/tests/alkitqol_test.cpp:41-55` | Rejects `quick_proxy_*` junk |
| Row star | `alkitrow.cpp:414-431` (create), `:603-604` (layout), `:918-928` (`refreshFavorite`), `:931-947` (hover); row context menu "Add to Quick" `:1017-1025, :1040-1045`; `alkitpage.cpp:838` | Star shows on hover or when set |
| Toggle | `ALKitShell::isFavorite/toggleFavorite` (`alkitshell.cpp:1783-1805`) | Writes the setting, refreshes stars, rebuilds Quick on idle |
| Proxies | `ALKitProxyBinding` (`alkitrow.cpp:68-84`), `ALKitRow::createProxy` (`:847-916`), `favoriteKey` (`:837-845`), `mSourceRow` (`alkitrow.h:91,126`) | Clones a switch, spinner or line editor bound to the source row. Map or array values become an "Open on its page" link row |
| Quick build | `ALKitShell::rebuildQuick` (`alkitshell.cpp:1807-1843`) | **Builds every favourited page** (`ALKitLazyPage::ensureBuilt`, `:1824-1830`) just to find the source rows |
| Quick host | `ALKitCard::setQuickRows` (`alkitcard.cpp:955-993`); empty text (`:272-281`); height (`:704-705`); body visibility (`:807`); `alkitcard.h:89,135,140`; dynamic rows (`alkitpage.cpp:1334-1345`) | Card `kc_quick`, kind `quick` |
| Quick search | `alkitshell.cpp:1382-1388` (local filter over proxy rows plus the global popup); changed-marker special cases `:1899-1906`; `showRowInSearch` unwraps proxies (`:1845-1858`) | |
| Generator | `quick_page()` (`scripts/code_tools/director_kit_gen.py:888-895`); inserted as group 0 with the **camera** icon (`:1247-1253`); comment `:222`; selftest "37 page files" (`:1326`) | `panel_dpage_quick.xml` holds one `kc_quick` card |
| Floater | `kit_shell.rail_item group="quick_group" icon="KitIcon_rail_camera_16"` (`floater_director_v2.xml:5`); lazy page (`:36-38`); `llfloaterdirector.cpp:1217` | Quick uses the same icon as Camera |
| Manifest | `director_functional_manifest.py:522` skips Quick ("shell-only") | No functional coverage |
| Mockup | No Quick group in `director_mockup_data.json` or `doc/director_kit/mockup/Director.dc.html` (rail at `:118-120`). Help text `doc/director_kit/mockup_src/Main.dc.html:182` says "add to Quick panel" | Quick has never been mocked up |

The building blocks that tiles reuse:

- **Page master.** `ALKitPage` creates `ALKitSwitch mMaster` with `setControlName(mMasterSetting)` (`alkitpage.cpp:499-525`). The "X is off" banner with Turn on is at `:569-597`. `updateMaster` is at `:1095-1104`. The generator gets the master from mockup `page.master.{w,l,s}` (`director_kit_gen.py:664-675`). The lazy page carries `master_setting` (`floater_director_v2.xml:55,66,78,79,82,92,98`), which drives the rail and tab "○" pips (`alkitshell.cpp:589-646`).
- **Card master.** `kit_card@enable_setting` becomes `mEnable->setControlName` (`alkitcard.cpp:284-293`) and is placed in the header (`:777`).
- **Presets.** `kit_page@preset_mode/preset_provider` comes from `kit_pages.json` `p0_pages.*.preset` (rig `:78` widgets, rim `:105`, weather `:131`, atmosphere `:136`, stylize `:171`, tron `:176`, stepped `:187`, actors `:212` widgets). `ALKitPresetStrip` (`alkitpresets.cpp:205-452`) gets its provider from `ALKitPresetRegistry` (`alkitpresets.h:28-37`). Providers are registered at `llfloaterdirector.cpp:610-624` (roto_ink, tron_look, stepped_anim, weather, projector_shafts). **rig_rim is registered lazily**, in the `ALPanelRigRim` constructor (`alpanelrigrim.cpp:68-83`).
- **The "(modified)" state is per strip instance.** `mAppliedId/mAppliedValues/mModified` are members (`alkitpresets.h:90-100`), set in `captureApplied` (`alkitpresets.cpp:409-420`) and checked in `isModified` (`:436-452`). A second strip on a tile would **not** know what the page applied. §3.3 fixes this.
- **Gate status shown on pages.** Hard-coded in `ALKitPage` (`alkitpage.cpp:602-633`, `updateGateStatus` `:1106-1129`): Rim when the Rig is off, Weather off, and light masking hiding projector volumetrics.
- **Navigation.** `navigateTo(ALKitTarget)` (`alkitshell.cpp:1036-1080`) and `revealCard` (`alkitpage.cpp:1438-1467`), which selects the mode and chip and scrolls, but **does not expand a collapsed card body**.
  - Side finding: the string overload `navigateTo("group/page/widget")` (`alkitshell.cpp:1023-1034`) splits at the **first** `/`, so `page_key` becomes `"look_group"`. `showRowInSearch` (`:1852`) passes exactly that form, so "Show in search" probably fails to find the page. The new code must always use the struct overload. Flag this to the search/QoL owner.

---

## 1. What a "feature" is, and the generated registry

### 1.1 Definition

A **feature** is something the user turns on, tunes and opens as a unit. There are two kinds:

1. **Page feature.** Every Director page is a feature by default: kit, legacy-embedded and launcher pages alike. Examples: Roto Ink (`look_group/stylize_tab`), Stepped, Light Response (`light_group/actors_tab`), VCam, Tron.
2. **Card feature.** On a multi-feature page, a card with its **own master** is also a feature. It must be listed in the curated allowlist. Examples: Lens > Filters > Polarizer, World > Weather > Rain, Camera > Shot > Auto switcher.

Candidate rule (generated, fail-closed): a card is a **candidate** when it has an enable setting (mockup `es`, or the generated `kit_card@enable_setting`) **and** that setting differs from its page master. Every candidate must be either in `card_features` or in `card_exclusions` with a reason. Cards with an enable widget but no setting (code-bound, for example Props `hover_on_check` or Ghosts `chaos_check`) are not candidates, because a tile cannot bind them without the page.

Master resolution for a feature, in order:
- (a) The page master `page.master.s` from the mockup or `vcam_page.json`.
- (b) For pages without a page master, a curated `master_from_row`, which must name a Boolean toggle row on that page.
- (c) A card feature's own enable setting.
- (d) **None.** A master widget with no setting is per-actor. Actor FX `actor_style_enabled` is bound in C++ to the selected actor's style (`llfloaterdirector.cpp:1751`, `:4785`, `:5046`), so its tile has **no switch**: a global tile cannot know which actor it would act on.

Preset resolution:
- `kit_pages.json` `preset.mode == "provider"` gives the tile picker, using the same provider key as the page.
- `mode == "widgets"` (Rig setups for each selected rig, `alpanelcinelightrig.cpp:1953-1963`; Light Response for each selected actor, `alpanelavatarlightresponse.cpp:974-993`) is **page-only**. The tile shows no picker, because these presets depend on the page's selection.
- Card features never have a tile preset.

### 1.2 Registry (generated; 58 features = 36 page + 22 card)

The `id` is the persisted key: the page name without `_tab`, plus `.card_id` for card features. Widget-safe slugs replace `.` with `_`. The icon is the rail icon of the owning group, `KitIcon_rail_<group>_16` (`director_kit_gen.py:67-70`). There are no per-page icons today (`doc/director_kit/icons/`). The context line on the tile is "Group › Page" for card features and "Group" for page features.

| # | id | Name | Rail group | Kind | Master (setting / handler) | Preset on tile | Open target |
|---|---|---|---|---|---|---|---|
| 1 | `move` | Move | Cast | page | none | none | page `cast_group/move_tab` |
| 2 | `path` | Path | Cast | page | none | none | page `cast_group/path_tab` |
| 3 | `path.pose_ghosts` | Onion skin | Cast | card | `PathShowOnionSkin` | none | legacy widget `onion_skin_check` on `cast_group/path_tab` |
| 4 | `path.node_camera` | Path camera | Cast | card | `PathCameraEnabled` | none | legacy widget `path_camera_enable_check` on `cast_group/path_tab` |
| 5 | `props` | Props | Cast | page | none | none | page `cast_group/props_tab` |
| 6 | `ghosts` | Ghosts | Cast | page | none | none | page `cast_group/ghosts_tab` |
| 7 | `dance` | Dance | Cast | page | none | none | page `cast_group/dance_tab` |
| 8 | `animate` | Animate | Performance | page | none | none | page `performance_group/animate_tab` |
| 9 | `switchboard` | Switchboard | Performance | page | none | none | page `performance_group/switchboard_tab` |
| 10 | `gaze` | Gaze | Performance | page | none | none | page `performance_group/gaze_tab` |
| 11 | `gaze.camera_lookat` | Look at camera | Performance | card | `DirectorLookAtCameraEnabled` | none | legacy widget `look_at_camera_enabled` on `performance_group/gaze_tab` |
| 12 | `polish` | Polish | Performance | page | `ALPolishEnabled` (page master `polish_master_check`) | none | page `performance_group/polish_tab` |
| 13 | `gaze_cues` | Gaze Cues | Performance | page | none | none | page `performance_group/gaze_cues_tab` |
| 14 | `shot` | Shot | Camera | page | none | none | page `camera_group/shot_tab` |
| 15 | `shot.cinematic_cam` | Cinematic camera | Camera | card | `CinematicCamEnabled` | none | card `cinematic_cam` on `camera_group/shot_tab` |
| 16 | `shot.camera_switcher` | Auto switcher | Camera | card | `DirectorSwitcherArmed` | none | card `camera_switcher` on `camera_group/shot_tab` |
| 17 | `moves` | Moves | Camera | page | none | none | page `camera_group/moves_tab` |
| 18 | `operator` | Operator | Camera | page | none | none | page `camera_group/operator_tab` |
| 19 | `operator.shake_look` | Operator style | Camera | card | `CameraShakeApplyActive` | none | card `shake_look` on `camera_group/operator_tab` |
| 20 | `frame_orbit` | Frame & Orbit | Camera | page | none | none | page `camera_group/frame_orbit_tab` |
| 21 | `frame_orbit.auto_reframe` | Auto-reframe | Camera | card | `CinematicAutoFrameEnabled` | none | card `auto_reframe` on `camera_group/frame_orbit_tab` |
| 22 | `frame_orbit.flycam_orbit` | Flycam orbit | Camera | card | `FlycamOrbitEnabled` | none | card `flycam_orbit` on `camera_group/frame_orbit_tab` |
| 23 | `takes` | Takes | Camera | page | none | none | page `camera_group/takes_tab` |
| 24 | `vcam` | VCam | Camera | page | `PrismLensEnabled` (page master `vc_enable`) | none | page `camera_group/vcam_tab` |
| 25 | `focus` | Focus | Lens | page | none | none | page `lens_group/focus_tab` |
| 26 | `focus.dof` | Depth of field | Lens | card | `RenderDepthOfField` | none | card `dof` on `lens_group/focus_tab` |
| 27 | `focus.comp_guide` | Composition guide | Lens | card | `RenderCompositionGuide` | none | card `comp_guide` on `lens_group/focus_tab` |
| 28 | `filters` | Filters | Lens | page | none | none | page `lens_group/filters_tab` |
| 29 | `filters.gradnd` | Graduated ND | Lens | card | `RenderGradNDEnabled` | none | card `gradnd` on `lens_group/filters_tab` |
| 30 | `filters.polarizer` | Polarizer | Lens | card | `RenderPolarizerEnabled` | none | card `polarizer` on `lens_group/filters_tab` |
| 31 | `filters.infrared` | Infrared film | Lens | card | `RenderInfraredEnabled` | none | card `infrared` on `lens_group/filters_tab` |
| 32 | `filters.softedge` | Soft edge | Lens | card | `RenderSoftEdgeEnabled` | none | card `softedge` on `lens_group/filters_tab` |
| 33 | `diopter` | Diopter | Lens | page | `CineDiopterEnabled` (row `diopter_enabled`) | none | page `lens_group/diopter_tab` |
| 34 | `cine_light_rig` | Rig | Light | page | `CineLightRigEnabled` (page master `cine_enabled`) | page only (widgets, per rig) | page `light_group/cine_light_rig_tab` |
| 35 | `cine_light_rig.rig_catchlight` | Eye catchlight | Light | card | `CineLightRigCatchlight`; parent `CineLightRigEnabled` | none | card `rig_catchlight` on `light_group/cine_light_rig_tab` |
| 36 | `cine_light_rig.live_probe` | Live probe | Light | card | `CineLightRigLiveProbeEnabled`; parent `CineLightRigEnabled` | none | card `live_probe` on `light_group/cine_light_rig_tab` |
| 37 | `cine_light_rig.night_mask` | Night mask | Light | card | `CineLightRigNightMaskEnabled`; parent `CineLightRigEnabled` | none | card `night_mask` on `light_group/cine_light_rig_tab` |
| 38 | `light_rim` | Rim | Light | page | `CineRigRimEnabled` (page master `cine_rig_rim_enable`) | provider `rig_rim` | page `light_group/light_rim_tab` |
| 39 | `exposure` | Exposure | Light | page | none | none | page `light_group/exposure_tab` |
| 40 | `practicals` | Practicals | Light | page | none | none | page `light_group/practicals_tab` |
| 41 | `actors` | Actors (Light Response) | Light | page | `AvatarLightResponseEnabled` (page master `chk_enabled`) | page only (widgets, per actor) | page `light_group/actors_tab` |
| 42 | `light_cues` | Light Cues | Light | page | none | none | page `light_group/light_cues_tab` |
| 43 | `image` | Image | Look | page | none | none | page `look_group/image_tab` |
| 44 | `bloom_glow` | Bloom & Glow | Look | page | none (`RenderBloomHDR` has no C++ reader: HIDDEN_GATES.md row 8) | none | page `look_group/bloom_glow_tab` |
| 45 | `stylize` | Roto Ink | Look | page | `CineOutlineEnabled` (row `ri_enable`) | provider `roto_ink` | page `look_group/stylize_tab` |
| 46 | `tron` | Tron | Look | page | `TronEnabled` (row `tr_enable`) | provider `tron_look` | page `look_group/tron_tab` |
| 47 | `stepped` | Stepped | Look | page | `StepAnimEnabled` (page master `sa_enable`) | provider `stepped_anim` | page `look_group/stepped_tab` |
| 48 | `actor_style` | Actor FX | Look | page | none on tile (`actor_style_enabled` is per actor; status "Per actor: open to edit") | none | page `look_group/actor_style_tab` |
| 49 | `weather` | Weather | World | page | `AlchemyWeatherEnabled` (page master `weather_enabled`) | provider `weather` | page `world_group/weather_tab` |
| 50 | `weather.rain` | Rain | World | card | `AlchemyWeatherRainEnabled`; parent `AlchemyWeatherEnabled` | none | card `rain` on `world_group/weather_tab` |
| 51 | `weather.lightning` | Lightning | World | card | `AlchemyWeatherLightningEnabled`; parent `AlchemyWeatherEnabled` | none | card `lightning` on `world_group/weather_tab` |
| 52 | `atmosphere` | Atmosphere | World | page | none | provider `projector_shafts` | page `world_group/atmosphere_tab` |
| 53 | `atmosphere.projector_cones` | Projector cones | World | card | `BDMergeProjectorVolumetrics` | none | card `projector_cones` on `world_group/atmosphere_tab` |
| 54 | `atmosphere.soft_shadows` | Soft projector shadows | World | card | `BDMergeSoftProjectorShadows` | none | card `soft_shadows` on `world_group/atmosphere_tab` |
| 55 | `atmosphere.local_fog_volumes` | Fog volumes | World | card | `BDMergeLocalFogVolumes` | none | card `local_fog_volumes` on `world_group/atmosphere_tab` |
| 56 | `shadows_probes` | Shadows | World | page | none | none | page `world_group/shadows_probes_tab` |
| 57 | `temporal` | Time | Capture | page | none | none | page `capture_group/temporal_tab` |
| 58 | `scene_options` | Scene | Capture | page | none | none | page `capture_group/scene_options_tab` |

Card candidates **excluded**. There are 8, each with a reason, and the gate requires every one to be listed:

| Candidate | Reason |
|---|---|
| `move.custom_anim` (`ActorMoverUseCustomAnim`) | An option of the walk, not a feature |
| `shot.cut_ease` (`DirectorSwitcherEaseCuts`) | Sub-option of the Auto switcher |
| `shot.slow_world` (`DirectorSwitcherEaseFreeze`) | Sub-option of the Auto switcher |
| `cine_light_rig.easy_probe` (`CineLightRigLiveProbeEnabled`) | Easy-mode shortcut that shares Live probe's setting. The duplicate-master gate would also reject it |
| `light_rim.rim_wrap` (`CineRigRimWrapEnabled`) | Rim quality sub-option |
| `weather.splashes` (`AlchemyWeatherSplashEnabled`) | Sub-option of Rain |
| `weather.rain_cover` (`AlchemyWeatherRainOcclusion`) | Sub-option of Rain |
| `weather.lightning_hq` (`AlchemyWeatherLightningQualityEnabled`) | Quality sub-option of Lightning |

Page exclusions: **none**. The Quick page itself is not a feature and is implicitly skipped.

### 1.3 Source of the curation and the generated output

**Curation** lives in `director_mockup_data.json` under a new top-level `quick` block (§5). The mockup is the source of truth, and the artifact must render the same tiles. Its sub-object `quick.registry` holds:

```json
"registry": {
  "master_from_row": {"look_group/stylize_tab": "ri_enable", "look_group/tron_tab": "tr_enable", "lens_group/diopter_tab": "diopter_enabled"},
  "page_only_presets": {"light_group/cine_light_rig_tab": "Setups apply to the selected rig", "light_group/actors_tab": "Presets apply to the selected actors"},
  "per_actor_master": {"look_group/actor_style_tab": "Per actor: open to edit"},
  "names": {"light_group/actors_tab": "Actors (Light Response)", "camera_group/shot_tab#cinematic_cam": "Cinematic camera", "cast_group/path_tab#pose_ghosts": "Onion skin", "cast_group/path_tab#node_camera": "Path camera", "world_group/atmosphere_tab#projector_cones": "Projector cones", "world_group/atmosphere_tab#soft_shadows": "Soft projector shadows"},
  "card_features": ["cast_group/path_tab#pose_ghosts", "cast_group/path_tab#node_camera", "performance_group/gaze_tab#camera_lookat", "camera_group/shot_tab#cinematic_cam", "camera_group/shot_tab#camera_switcher", "camera_group/operator_tab#shake_look", "camera_group/frame_orbit_tab#auto_reframe", "camera_group/frame_orbit_tab#flycam_orbit", "lens_group/focus_tab#dof", "lens_group/focus_tab#comp_guide", "lens_group/filters_tab#gradnd", "lens_group/filters_tab#polarizer", "lens_group/filters_tab#infrared", "lens_group/filters_tab#softedge", "light_group/cine_light_rig_tab#rig_catchlight", "light_group/cine_light_rig_tab#live_probe", "light_group/cine_light_rig_tab#night_mask", "world_group/weather_tab#rain", "world_group/weather_tab#lightning", "world_group/atmosphere_tab#projector_cones", "world_group/atmosphere_tab#soft_shadows", "world_group/atmosphere_tab#local_fog_volumes"],
  "card_exclusions": {"cast_group/move_tab#custom_anim": "walk option", "camera_group/shot_tab#cut_ease": "switcher sub-option", "camera_group/shot_tab#slow_world": "switcher sub-option", "light_group/cine_light_rig_tab#easy_probe": "Easy shortcut of Live probe", "light_group/light_rim_tab#rim_wrap": "rim quality sub-option", "world_group/weather_tab#splashes": "rain sub-option", "world_group/weather_tab#rain_cover": "rain sub-option", "world_group/weather_tab#lightning_hq": "lightning quality"},
  "page_exclusions": {}
}
```

Legacy card features (Path ×2, Gaze ×1) target their enable widget, because legacy pages have no kit card. That widget is the mockup card's `ew`.

**Generated output.** `director_kit_gen.py` emits one `kit_shell.feature` per feature into `floater_director_v2.xml`, next to the existing `kit_shell.rail_item` (`director_kit_gen.py:1032-1035`). This puts the registry in the shell at construction, so stars on any page can resolve it without building the Quick page.

```xml
<kit_shell.feature id="stylize" label="Roto Ink" context="Look" icon="KitIcon_rail_look_16"
    page="look_group/stylize_tab" card="" widget=""
    master_setting="CineOutlineEnabled" master_label="Enable ink" master_mode="setting"
    parent_setting="" parent_label=""
    preset_provider="roto_ink" preset_note=""
    summary="..." keywords="roto ink outline cel" migrate_widgets="" />
<kit_shell.feature id="weather.rain" label="Rain" context="World › Weather" icon="KitIcon_rail_world_16"
    page="world_group/weather_tab" card="rain" widget=""
    master_setting="AlchemyWeatherRainEnabled" master_label="Rain" master_mode="setting"
    parent_setting="AlchemyWeatherEnabled" parent_label="Weather"
    preset_provider="" migrate_widgets="weather_rain_enabled,..." />
```

- `master_mode` is one of `setting`, `none` or `per_actor`.
- `migrate_widgets` (card features only) lists every row widget of that card, option rows included. It is used only by the migration in §4.2.
- `summary` is the page `sum` or the card `help`. It fills the tile when the feature has no switch and no picker.
- `keywords` is the page and card labels plus their `synonyms.json` aliases, for filtering on the Quick page.

`gen_report.txt` gets a `FEATURES` section with one line per feature, plus the candidate and exclusion lists. It is reviewed like `CLASSIFICATION`.

---

## 2. How the user favourites and unfavourites a feature

**Chosen set (minimal and consistent).** There is one rule: **the star sits next to the feature's name, wherever the feature's own header is.** The Quick page also has its own **Add feature** picker.

| Entry point | Where | Widget name |
|---|---|---|
| **Page star** ☆/★ | First item of every page bar (kit, legacy and launcher pages all have `mBar`; `alkitpage.cpp:347` and `:401-411`) at `PAGE_PAD_LR`, 24×24. Everything after it shifts right by 30 px. Tooltip: "Add Roto Ink to Quick" or "Remove Roto Ink from Quick" | `<page prefix>_feature_star` |
| **Card star** ☆/★ | Only on the 22 card-feature cards (kit cards). Sits left of the card title in the header (`alkitcard.cpp:760-780`); the title shifts by 20 px. Legacy card features (Path ×2, Gaze ×1) have no kit card header, so the tab menu (below) and the Quick picker cover them | `kc_<card>_feature_star` |
| **Quick "Add feature…"** | A combo on the right of the Quick card header. It lists every registry feature that is not yet favourited, grouped by rail group, with card features indented under their page. It is also the empty-state action | `quick_add_feature` |
| **Tile menu → Remove from Quick** | On each tile (§3) | `qt_<slug>_menu` → `remove` |

**Not added**, with reasons:

- **Search results.** The results popup is keyboard-driven (`alkitshell.cpp:1441-1690`). Hits are rows, not features, so a star there would need a row-to-feature lookup and a second hit target inside each result row. A search hit already lands on the page, one click from the page star. This can be revisited once the search index has page and card rows (audit A3).
- **Right-click on a rail item.** Rail items are *groups* (`floater_director_v2.xml:5-13`), not features, so a group star has no meaning. A right-click menu on the page tab would be hidden UI that duplicates the visible page star.
- **Row stars and the row "Add to Quick" menu item.** Removed (§4.3). They favourite settings, which the user rejected.

Because the legacy card features have no visible star, the Add feature picker is their in-Quick entry point. If the user wants a star for them on the page, see Q1.

Shell API, replacing `isFavorite/toggleFavorite`:
- `bool isQuickFeature(const std::string& id) const`
- `void toggleQuickFeature(const std::string& id)`
- `void moveQuickFeature(const std::string& id, S32 delta_or_first)`
- `const std::vector<std::string>& quickFeatures() const`
- `const ALKitFeature* feature(const std::string& id) const`
- `const ALKitFeature* pageFeature(const std::string& page_key) const`
- `const ALKitFeature* cardFeature(const std::string& page_key, const std::string& card_id) const`

Every change writes the setting (§4.1), emits `mQuickChanged` (which stars listen to), and rebuilds the tiles on idle, but only if the Quick page is built.

---

## 3. Tile layout and behaviour

### 3.1 Host

Keep the generated `kc_quick` card (kind `quick`).
- Retitle it "Favourite features".
- Make it span all columns: `ALKitCard::spansAllColumns()` returns `mKind == "quick"`; today it always returns false (`alkitcard.h:71`).
- It owns an inner `ALFlowGrid` (`alflowgrid.h`) with `card_width=300`, gap 12 and `fit_cards=true`, holding `ALKitFeatureTile` children.
- `setQuickRows` is replaced by `setQuickTiles(std::vector<std::string> ids)`.
- The header carries `quick_add_feature` on the right. The card has no reset and no enable switch.

New files: `indra/newview/alkitquick.{h,cpp}`. They hold `ALKitFeature` (the registry struct), `ALKitFeatureTile` (`LLPanel` + `ALFlowGridItem`) and `ALKitFeatureStatus` (§3.5).

### 3.2 Tile geometry

The minimum width is 280. The tile is a fixed 98 px tall: pad 10, three rows of 20/24/22, gaps of 6, then pad 10. At 1160 px the Quick grid shows two columns; at 850 px it shows one.

```
┌ qt_<slug> ─────────────────────────────────────────────┐
│ [icon16] Roto Ink •                      Look      [⋮] │  row 1: name (Kit13 semibold, ellipsis) + changed dot; context (muted); menu
│ [switch] On          [ Comic Ink (modified)      ▾ ]   │  row 2: master switch + On/Off label; preset picker (160 wide)
│ Roto Ink is off: this look will not show.     [Open ›] │  row 3: status line (muted or amber, ellipsis); Open button
└────────────────────────────────────────────────────────┘
```

- Row 1. The icon is `feature.icon`. The name is `feature.label`. The " •" dot shows when the target page has changed values, from the same source as the tab pip: `mPageChanged[page_key]` (`alkitshell.cpp:614-646, 1860-1866`). The context label is right-aligned. `qt_<slug>_menu` is a 22×22 "⋮" button.
- Row 2, master (`qt_<slug>_master`). This is an `ALKitSwitch` with `setControlName(feature.master_setting)`: the **same setting** the page master or card enable binds to (`alkitpage.cpp:521`, `alkitcard.cpp:291-292`). It cannot drift, because both read the same `LLControlVariable`. Next to it is a text label, "On" or "Off", like `mMasterState` (`alkitpage.cpp:512-516, 1102-1103`).
  - When the master mode is `none`, the switch slot is collapsed and the preset or summary moves left.
  - When it is `per_actor`, the slot shows the muted text "Per actor".
- Row 2, preset (`qt_<slug>_preset`). This is a compact `ALKitPresetStrip`: combo only, no Save or Delete, because saving belongs on the page. It uses the feature's provider key and the feature's `page` key (not the Quick page key), so the undo gesture is recorded against the owning page (`alkitpresets.cpp:322-325`). It shows "Label (modified)" and "Revert to Label" exactly like the page strip (`:292-303`), from the shared state in §3.3. A tile with no switch and no picker shows `feature.summary` here instead, muted on one line.
- Row 3. The status line comes from §3.5. `qt_<slug>_open` is a 64×22 button labelled "Open ›".

Toggle path: the switch commit calls `shell->history().beginGesture(ALKitHistory::MOUSE, feature.page, "Turn on/off " + label)` (`alkithistory.h:50`), then writes. The page's undo therefore covers changes made from Quick. There is no reset button on tiles.

### 3.3 Shared preset state (required for "cannot go out of sync")

Add the singleton `ALKitPresetState` (in `alkitpresets.{h,cpp}`). It is keyed by provider key and holds:
- `applied_id`, `applied_label` and `applied_values`, moved out of the strip members at `alkitpresets.h:90-100`;
- `modified`;
- `boost::signals2::signal<void()> changed`.

`captureApplied`, `trackedSettingChanged`, `isModified` and `remove` (for clearing a deleted applied preset) move into the state object. There is one setting watch per provider, not one per strip. Each strip subscribes to `changed` and calls `refresh()`. When a page strip's `save()` or `remove()` alters the list, it calls `ALKitPresetState::notifyListChanged(provider)`, so tile combos re-list.

Result: applying a preset on the page relabels the tile, and editing a tracked setting marks both "(modified)".

**Provider availability.** Register **every** provider at Director construction. Move `RigRimPresetProvider` registration (`alpanelrigrim.cpp:68-83`) into `registerLookPresetProviders` (`llfloaterdirector.cpp:610-624`), or into a static registration function called from there. Otherwise the Rim tile's picker is empty until the Rim page is first built.

### 3.4 Open

`qt_<slug>_open` calls `shell->openFeature(id)`:
1. `selectPage(leaf)`, which builds the target page lazily (`alkitshell.cpp:1085-1111`). Only Open builds a page; tiles never do.
2. For a page feature, scroll to the top. There is no highlight.
3. For a kit card feature, `page->revealCard(card, true)` (`alkitpage.cpp:1438-1467`), **plus** a new `ALKitCard::setBodyOpen(true)` so a collapsed card expands (revealCard does not do this today), then flash it with the shell highlight (`mHighlight`, `mHighlightTimer`).
4. For a legacy card feature, `navigateTo(ALKitTarget{page, "", widget})` (the struct overload, never the string one; see §0), which opens accordions, scrolls and highlights (`alkitshell.cpp:1053-1078`).

### 3.5 Status line and unavailable states (reuse only what pages show)

Extract the hard-coded gate logic from `ALKitPage` (`alkitpage.cpp:602-633` for text and action, `:1106-1129` for the condition) into `ALKitFeatureStatus::gate(page_key) -> {bool show; std::string text; std::function<void()> turn_on}`. The page banner and the tiles both call it, so a new gate added later (for example HDR, HIDDEN_GATES.md row 17) appears on both at once. Tiles never invent status text.

Status precedence on a tile:

| # | State | Tile shows |
|---|---|---|
| 1 | Master is off | Muted line "`<master_label>` is off." Same wording source as the page banner (`alkitpage.cpp:575`). The switch is the action |
| 2 | Card feature whose `parent_setting` is off (Rain or Lightning with Weather off; Rig cards with Rig off) | Amber "`<parent_label>` is off." plus a "Turn on" link-button that writes `parent_setting` true through a history gesture on the page. Same text and action as the page banner |
| 3 | Page gate active (`ALKitFeatureStatus::gate`): Rim with the Rig off, light masking (Rig, Atmosphere) | Amber gate text plus its Turn on, if the page has one |
| 4 | `per_actor` master (Actor FX) | "Per actor: open to edit." |
| 5 | `page_only_presets` (Rig, Actors) | "Presets are on the page." (from `preset_note`) |
| 6 | Otherwise | Empty |

Other unavailable cases:
- **Provider key not registered at runtime.** The picker is hidden and a warning is logged once. This is also a selftest failure (§6).
- **Id stored but not in the registry**, for example a renamed page. No tile is shown, but the id **is kept** in the setting so a downgrade or a later rename alias keeps it. Edits (remove, reorder) operate on the full stored list.
- **Target page fails to build on Open.** Toast "Could not open `<label>`" and stay on Quick.

### 3.6 Reorder and remove

The tile menu `qt_<slug>_menu` has four items: "Move earlier" (`move_prev`), "Move later" (`move_next`), "Move to first" (`move_first`) and "Remove from Quick" (`remove`). The first item is disabled on the first tile and "Move later" on the last.
- The menu was chosen over drag because LLView in-panel drag needs new hit-testing in a flow grid. Drag can come later on top of the same `moveQuickFeature`.
- Keyboard: when a tile has focus, Ctrl+Up/Down moves it.
- Favourites are UI state, like `DirectorFavorites` today, and are **not** recorded in `ALKitHistory`. Remove therefore asks for no confirmation; it shows a 4-second toast "Removed Roto Ink · Undo" (reusing the transport toast pattern) whose Undo re-inserts the id at its old index.

### 3.7 Empty state, filter and changed-only

- **Empty state.** Card body text: "No favourite features yet." Second line: "Click ☆ next to a page or card name, or use Add feature above." It replaces `alkitcard.cpp:278`, and the mockup has the final wording. The height rule `alkitcard.cpp:704-705` keeps two rows.
- **Search on Quick.** The page-bar placeholder becomes "Find a feature…". The Quick branch of `runSearch` (`alkitshell.cpp:1382-1388`) keeps the global results popup, and its local filter now filters **tiles**: a case-insensitive token match over label, context and keywords. `ALKitPage::applyLocalFilter` dispatches to `ALKitCard::filterQuickTiles` for kind `quick`. When nothing matches: "No favourite matches '…'".
- **Changed only.** Show only tiles whose `mPageChanged[page]` is true.
- **Navigation.** Quick stays rail item 0 and the default first page. Remove the `sourceRow()` special cases in `refreshChangedMarkers` (`alkitshell.cpp:1899-1906`).

**Performance rule.** Rendering Quick must not build any other page, unlike today's `rebuildQuick` (`alkitshell.cpp:1824-1830`). Tiles bind settings and providers only. The selftest asserts this (§6).

---

## 4. Persistence, migration and removal

### 4.1 Settings (in `app_settings/settings.xml`, next to `:8823`)

- `DirectorQuickFeatures`: LLSD, Persist 1, default `<array />`. An ordered array of feature id strings. Comment: "Director Quick page: ordered favourite feature ids (see kit_shell.feature)."
- `DirectorQuickFeaturesMigrated`: Boolean, Persist 1, default 0.
- `DirectorFavorites`: keep the declaration. Change the comment to "RETIRED 2026-10: read once by the Quick feature migration; never written." Its stored value is left untouched so a downgrade still has the old rows.

### 4.2 Migration (pure function, unit-tested)

```text
ALKitQuick::migrate(old_rows: LLSD array of {page, widget},
                    existing: vector<string>, registry) -> vector<string>
  out = dedupe(existing)                         // keep first occurrence
  for row in old_rows (in order):
      skip if page empty, widget empty, widget starts "quick_proxy_", or page == "quick_group/quick_tab"
      fid = registry.cardFeatureOwning(row.page, row.widget)   // via migrate_widgets
            ?: registry.pageFeature(row.page)                  // owning page feature
      if !fid: LL_INFOS("DirectorKit") << "dropped favourite " ...; continue
      if fid not in out: out.push_back(fid)       // dedupe, first occurrence wins
  return out
```

- It runs in `ALKitShell::postBuild`, replacing `alkitshell.cpp:344-348`, only when `DirectorQuickFeaturesMigrated` is 0. Afterwards the code writes `DirectorQuickFeatures` and sets the flag to 1.
- It is idempotent: re-running yields the same list.
- Order: existing feature ids first, then migrated rows in their original order. Several rows on one page collapse into one feature at the position of the first.
- A starred shortcut row (for example the Rig Easy card's `cine_easy_cone_feather`) maps to the page where it was starred (Rig), not to its home page.

Unit tests (`indra/newview/tests/alkitqol_test.cpp`, which replaces `:41-55`):
- dedupe and first-occurrence order;
- a card-widget row maps to its card feature;
- an unknown page is dropped;
- `quick_proxy_` junk is dropped;
- existing ids are preserved;
- unknown stored ids are preserved on load;
- running twice gives the same result.

### 4.3 Remove the per-row proxy machinery completely

| File | Remove or replace |
|---|---|
| `alkitrow.cpp` | `ALKitProxyBinding` `:68-84`; star creation `:414-431`; star layout `:603-604`; `favoriteKey` `:837-845`; `createProxy` `:847-916`; `refreshFavorite` `:918-928`; its calls `:935, :945`; menu item "favorite" `:1017-1025`; its label update `:1040-1045`. Keep "Show in search" |
| `alkitrow.h` | `favoriteKey`, `sourceRow`, `refreshFavorite`, `createProxy` (`:90-95`); `mFavorite` `:120`; `mSourceRow` `:126` |
| `alkitcard.cpp/.h` | `setQuickRows` `:955-993`, `mQuickRows` and `mQuickAmberConnections` (`alkitcard.h:89,135,140`). Replaced by `setQuickTiles` and `filterQuickTiles` |
| `alkitpage.cpp/.h` | `row->refreshFavorite()` `:838`. `registerDynamicRow/unregisterDynamicRow` (`:1334-1345`, `alkitpage.h:138-139`): the only callers are in `setQuickRows`, so delete them |
| `alkitshell.cpp/.h` | `isFavorite/toggleFavorite/rebuildQuick` `:1783-1843`; `mFavorites`, `mRebuildingQuick` (`alkitshell.h:91-92,135,203,209`); `showRowInSearch` proxy unwrap `:1849-1850`; `sourceRow` branches `:1899-1906`; `startSearchLoad(); rebuildQuick();` `:1372-1373` becomes `rebuildQuickTiles()` |
| `alkitqol.h` | `Favorite`, `sourceFavorite`, `isValidFavorite`, `favoritesFromLLSD/ToLLSD` (`:15-23, 68-104`). Replaced by the feature-list helpers and `migrate` (§4.2). `valuesEqual` and the paste helpers stay |
| Strings | "Star a row…" (`alkitcard.cpp:278`); mockup help "add to Quick panel" (`mockup_src/Main.dc.html:182`) becomes "Right-click any control: copy, paste, reset." |
| Generator and tooling | Comment `director_kit_gen.py:222`; `quick_page` docstring `:889`; the selftest label at `:1326` ("runtime Quick host" becomes "Quick feature host"); manifest comment `director_functional_manifest.py:522` (Quick becomes a functional page, §6) |

The row label gutter (`mLabel->setOrigin(ROW_INSET + 18, …)`, `alkitrow.cpp:478`) **stays** in this change to avoid layout drift on every page. Whether to reclaim those 18 px is a separate visual-match item against the mockup.

Quick search and navigation keep working through §3.7. A grep gate (§6) proves the old symbols are gone.

---

## 5. Mockup (source of truth): data and artifact

Today the mockup has no Quick page: there is no `quick` key, and the rail starts at Cast (`mockup/Director.dc.html:118-120`, `:2096-2111`). **WP-Q0 lands first and the user approves it before any C++ work.**

### 5.1 `director_mockup_data.json` changes

Add one new top-level key. `groups` is untouched, so `validate_data`'s 36-page and 2066-row pins (`director_kit_gen.py:208-231`) do not move.

```json
"quick": {
  "group": {"name": "quick_group", "label": "Quick", "icon": "quick"},
  "page": {"name": "quick_tab", "label": "Quick",
           "sum": "Your favourite features: switch them on or off, pick a preset, or open the full page."},
  "card": {"id": "quick", "t": "Favourite features", "add_label": "Add feature…"},
  "empty": {"title": "No favourite features yet.",
            "body": "Click ☆ next to a page or card name, or use Add feature above."},
  "search_placeholder": "Find a feature…",
  "tile": {"min_w": 280, "h": 98,
           "menu": [{"id": "move_prev", "l": "Move earlier"}, {"id": "move_next", "l": "Move later"},
                    {"id": "move_first", "l": "Move to first"}, {"id": "remove", "l": "Remove from Quick"}],
           "open_label": "Open ›", "per_actor": "Per actor", "page_presets": "Presets are on the page."},
  "star": {"add_tip": "Add {name} to Quick", "remove_tip": "Remove {name} from Quick"},
  "demo_favorites": ["stylize", "stepped", "actors", "vcam", "tron", "filters.polarizer", "weather.rain"],
  "registry": { "...": "exactly the block in §1.3" }
}
```

- `demo_favorites` is used only by the mockup artifact, so it can show a populated Quick. The viewer ignores it, and a new user starts empty (see Q3).
- The `icon: "quick"` value adds `doc/director_kit/icons/rail_quick.svg` (a star outline, same 16/32 grid as `rail_*.svg`). It is generated to `KitIcon_rail_quick_16/_32` with `scripts/code_tools/director_kit/kit_png_gen.py` and needs a `textures.xml` entry. The generator then emits `ICONS["quick"]` instead of re-using the camera icon (`director_kit_gen.py:1252`, `floater_director_v2.xml:5`, `llfloaterdirector.cpp:1217`).
- `director_mockup_data.py` rebuilds the JSON as `{"groups", "chrome"}` only (`scripts/code_tools/director_mockup_data.py:1006`). It must pass `quick` through from a curated `quick.json` in its curated directory, or a regeneration silently deletes the block. Gate: the generator fails if `data["quick"]` is missing.

### 5.2 Artifact (`doc/director_kit/mockup/Director.dc.html`; keep `mockup_src/Director.dc.html` in step)

1. **Rail.** Insert the Quick item at index 0 from `d.quick.group`, with a star glyph icon. Keep `d.groups` indices unchanged internally by treating Quick as a virtual group −1, so the existing `gi/pi` maths (`:2074-2080`) is not disturbed.
2. **Quick page.** The page bar has the search ("Find a feature…") and Changed only, with no preset strip, master or star. One full-width card "Favourite features" holds "Add feature…" at the header right and the tile grid laid out per §3.2. It is driven by a session-state `quickIds` list initialised from `demo_favorites`. The registry is derived in JS from `d.groups` plus `d.quick.registry`, the same rules as the generator. The same 58 ids must result, and the artifact logs a console error otherwise.
3. **Tile behaviour in the mockup:**
   - the switch toggles the same `st.vals` key the page master uses (`masterKeyOf`, `:2114`), so flipping it on Quick shows on the page and the tab pip;
   - the picker reuses `presetOpts`/Custom logic (`:2639`);
   - Open sets `group/pageByGroup` and scrolls to the card's `domId` (`:367`);
   - the menu reorders or removes;
   - the status line follows §3.5 (master-off text, `masterBanner` wording, `:2702`).
4. **Stars:**
   - page star as the first page-bar item (left of the presets at `:275` and the master at `:314`);
   - card star left of the card title (`:369`) on the 22 card features only;
   - hover tooltips from `d.quick.star`.
5. **Help text.** `mockup_src/Main.dc.html:182` drops "add to Quick panel".
6. **Publish.** Republish the artifact for the user's visual sign-off. This is the VISUAL MATCH GATE reference for WP-Q4 and WP-Q5 screenshots.

---

## 6. Gates

All gates fail closed. The reviewer plants adversarial fixtures for each one (Codex has loosened gates before).

**Generator** (`director_kit_gen.py`, run by `--check` and `selftest`, and by `check_kit_pages.py` and the build gate):

| Gate | Check |
|---|---|
| F1 | Every page in `flatten_pages` (36) has exactly one page feature or a `page_exclusions` entry with a non-empty reason. A stale exclusion (no such page) fails |
| F2 | Candidates (§1.1) equal `card_features` ∪ `card_exclusions`, disjoint. Any unlisted candidate, stale entry or non-candidate in `card_features` fails |
| F3 | Ids are unique and match `^[a-z0-9_]+(\.[a-z0-9_]+)?$`; slugs are unique |
| F4 | Each `master_setting` exists in `settings*.xml` and is Boolean (reuse `kit_lint.check_setting_ref`, `kit_lint.py:464`). Each `master_from_row` names a toggle row with that setting on that page |
| F5 | No two features share a `master_setting`, and no card feature equals its page's master |
| F6 | `preset_provider` is the page's `kit_pages.json` provider and appears as a registered key in C++. Scan the literal first arg of `ALLookPresetProvider(`, and the `mKey = "..."` in provider classes, in `llfloaterdirector.cpp` and `alpanelrigrim.cpp`. A widgets-mode page must be in `page_only_presets` |
| F7 | A kit card target exists in the generated page XML. A legacy widget target exists by `name=` in the hosted XUI |
| F8 | `migrate_widgets` ⊆ that card's row widgets |
| F9 | Pinned count: 58 features (36 page, 22 card). Changing it needs a user-approved edit, like the classification pin at `:1225-1234` |
| F10 | `data["quick"]` is present. `demo_favorites` ⊆ registry ids |
| F11 | Selftest: "37 page files" stays; add "58 kit_shell.feature entries" and "Quick card kind=quick, no kit_row" |

**kit_lint** (`scripts/code_tools/kit_lint.py`): a new `check_features(tree)` validates every `kit_shell.feature` attribute set: setting refs and types, the icon texture name in `textures.xml`, and `card`/`widget` exclusivity. The planted fixtures go under `lint_fixtures/kit/`.

**xui_layout_check** (`scripts/code_tools/xui_layout_check.py --all`): zero findings on the regenerated `floater_director_v2.xml` and `panel_dpage_quick.xml`. Tiles are runtime-laid-out, so the runtime **UILayoutAudit** (`aluiperfselftest.cpp`, `UILayoutAuditSizes`) must also report zero `CLIPPED_H`, overflow or label-clip findings on Quick with **all 58 features seeded**, at 850×560 and 1160×720, and on pages carrying the new page star and card stars.

**Parity** (`scripts/code_tools/director_parity.py`): run unchanged. Quick widgets are runtime names. The KITNAMES line for `quick_tab` lists `kc_quick, quick_add_feature` so the supplemental index does not flag them.

**Grep gate**, added to `check_kit_pages.py`: no `createProxy|ALKitProxyBinding|setQuickRows|refreshFavorite|favoriteKey|quick_proxy_|toggleFavorite\(` in `indra/newview/*.{cpp,h}`. `DirectorFavorites` may appear only in settings.xml and the migration call.

**zs_check**: `cxs\zs_check.ps1` (`cl /Zs`) after every Codex round.

**Functional self-test** (`director_functional_manifest.py` plus `aluiperfselftest.cpp` functional mode). Quick becomes a functional page: remove the skip at `director_functional_manifest.py:522`. The run starts with an **arrange** step, `quick_seed`: capture `DirectorQuickFeatures`, set it to all 58 ids, and restore it at teardown (alongside `captureFunctionalUserFiles`). Entries generated per feature:

| Entry id | Exercise | Expect |
|---|---|---|
| `v2:quick_tab:qt_<slug>_master` (features with a `setting` master) | Toggle | The real `master_setting` flips. Then the page master or card enable on the built target page reads back the same value. Restore |
| `v2:quick_tab:qt_<slug>_master:reverse` | Flip the page or card switch | The tile switch reads back the new value without a Quick rebuild |
| `v2:quick_tab:qt_<slug>_open` | Click | `currentPageKey() == feature.page`. Card features: the card is visible, its body is open and the highlight is set. Legacy: the widget is highlighted |
| `v2:quick_tab:qt_<slug>_preset` (provider features) | Select the first built-in | Capture tracked settings S1 (must differ from S0). Restore. Apply the same id through the page strip and capture S2: S1 == S2. The page strip label equals the tile label |
| `v2:quick_tab:qt_<slug>_preset:modified` | Edit one tracked setting | Both the tile and page combo show "(modified)" |
| `v2:quick_tab:qt_<slug>_menu:move_next` / `move_first` / `remove` | Menu | `DirectorQuickFeatures` order or contents change as expected. Restore |
| `v2:<page>:<prefix>_feature_star` (36) and `kc_<card>_feature_star` (19 kit card features) | Click twice | The id is appended, then removed |
| `v2:quick_tab:quick_add_feature` | Select an item | The id is appended and a tile appears |
| `v2:quick_tab:no_page_builds` | Open Quick from cold | `mBuiltPages` contains only Quick and the start page |
| `v2:quick_tab:migration` | Login-screen run with a seeded `DirectorFavorites` fixture and the flag set to 0 | `DirectorQuickFeatures` equals the expected list. The fixture has two rows on one page, one card row, one proxy-junk row and one unknown page |

Expected manifest totals: about 25 master entries plus 25 reverse, 58 open, 6 preset plus 6 modified, 3 menu, 55 star, 1 add, 1 no-build and 1 migration. Actor FX and other `none` masters have no master entries.

The FUNCTIONAL GATE (click every control) and the VISUAL MATCH GATE (screenshot vs the WP-Q0 mockup) both apply before "done".

---

## 7. Work packages, risks and open questions

Order: Q0 → user approves → Q1 → (Q2 ∥ Q3) → Q4 → Q5 → Q6 → Q7. Opus does an adversarial review after each WP, and zs_check runs after each Codex round. **One** checkpoint commit and fresh build (`cxs\fresh_build.ps1`) at the end.

| WP | Owner | Content | Files |
|---|---|---|---|
| **Q0** Mockup | Codex (Opus reviews) + **Haiku** for the icon and help text | §5. Add the `quick` data block; render Quick, tiles, stars and the picker in `Director.dc.html` and `mockup_src`; `rail_quick.svg` → PNG + `textures.xml`; Main.dc.html:182 text; `director_mockup_data.py` pass-through. Publish for user sign-off | `director_mockup_data.json`, `doc/director_kit/mockup*/`, `doc/director_kit/icons/`, `textures.xml`, `director_mockup_data.py` |
| **Q1** Registry generator + gates | Codex | §1.3 emission, `quick_page()` update (card title, placeholder), `ICONS["quick"]`, gen_report FEATURES section, gates F1-F11, kit_lint `check_features`, fixtures, grep gate, regenerate all outputs | `director_kit_gen.py`, `kit_lint.py`, `check_kit_pages.py`, lint and parity fixtures, generated XUI |
| **Q2** Runtime registry, persistence and migration | Codex; **Haiku** for the settings.xml entries | `ALKitShell::Params` `Multiple<Feature>`; `ALKitFeature`; the shell API (§2); the two new settings; `DirectorFavorites` comment; `migrate()` + unit tests | `alkitshell.{h,cpp}`, `alkitqol.h` (or `alkitquick.h`), `settings.xml`, `tests/alkitqol_test.cpp` |
| **Q3** Shared preset state | Codex | `ALKitPresetState`, compact strip mode, `notifyListChanged`, early rig_rim registration | `alkitpresets.{h,cpp}`, `alpanelrigrim.cpp`, `llfloaterdirector.cpp:610-631` |
| **Q4** Tiles + Quick host | Codex | `alkitquick.{h,cpp}` (tile, status), the `kc_quick` grid, Add feature picker, Open with card expand, menu reorder and remove plus undo toast, filter and changed-only, `ALKitFeatureStatus` extracted from `alkitpage.cpp:602-633, 1106-1129` | `alkitquick.*`, `alkitcard.*`, `alkitpage.*`, `alkitshell.cpp`, `CMakeLists.txt` |
| **Q5** Stars | Codex | Page-bar star (shift `bar_x`), card-header star on registered card features, listening to `mQuickChanged` | `alkitpage.cpp:347-411`, `alkitcard.cpp:760-780` |
| **Q6** Removal | Codex | Everything in §4.3; strings | per §4.3 |
| **Q7** Functional + layout self-test | Codex | Manifest generator entries, `quick_seed` arrange and restore, the new exercise kinds in `aluiperfselftest.cpp`, seeded UILayoutAudit pass | `director_functional_manifest.py`, `aluiperfselftest.cpp`, `doc/director_kit/functional/*` |

**Risks**

1. **Preset desync.** Without Q3, a tile strip and the page strip disagree on "(modified)". This is a blocker for the core promise.
2. **Provider registered late.** The Rim tile picker is empty until the Rim page is built (`alpanelrigrim.cpp:68-83`). Gate F6 plus the preset functional entries catch it.
3. **Undo attribution.** Writes from tiles must open history gestures against `feature.page`, or page undo misses them and Quick-page undo shows foreign entries.
4. **Page-header crowding.** The +30 px star shifts the bar. `master_fits` and `search_fits` (`alkitpage.cpp:1178-1210`) may hide the search earlier at 850 px. The runtime layout audit must stay at zero findings.
5. **Hidden masters.** Roto Ink, Tron and Diopter have a tile switch but no page-header master. It is the same setting as their "Enable" row, so there is no drift, but the user may expect a header switch on those pages (out of scope; see Q2).
6. **Mockup regeneration.** `director_mockup_data.py` drops `quick` unless Q0 adds the pass-through. Gate F10 catches it.
7. **Concurrent edits.** Other agents edit `director_kit_gen.py`, `kit_pages.json` and the page XUI. Q1 must rebase and regenerate all outputs in one commit. Never `git add -A`.
8. **Gate loosening.** The reviewer plants a missing page feature, a stale exclusion, a duplicate master, a bad provider and a proxy symbol, and confirms each one fails.
9. **Migration loss.** Only rows whose page no longer exists are dropped, and the old key is kept untouched for rollback.
10. **"Show in search" bug.** The string-path issue (§0) is independent but nearby. Fix it in Q6 or flag it to the search owner. Do not copy the pattern into `openFeature`.

**Open questions for the user** (each has a recommended default; build proceeds on the default unless the user says otherwise):

- **Q1. Card-level features.** Should the 22 curated card features (Polarizer, Rain, Auto switcher and so on) be favourites in their own right, or pages only (36)? **Default: include the 22.** Legacy card features (Onion skin, Path camera, Look at camera) are added from Quick's picker only, with no star on the legacy page.
- **Q2. Context-dependent presets.** Rig setups (per selected rig) and Light Response presets (per selected actor) are page-only, and their tiles say "Presets are on the page". **Default: page-only.** The alternative is a tile picker that acts on the page's current selection, which is invisible from Quick.
- **Q3. First-run Quick for a user with no old favourites.** **Default: empty**, with the empty state and Add feature. The alternative is to seed it with Roto Ink, Stepped, Light Response, VCam and Tron.

# Spec: Light > Exposure as a generated kit page (full Personal Lighting exposure set)

Written 10/15/2026. Spec only: no code, no build, no commit.

Inputs:
- `WIRING_AUDIT_light_exposure.md` (the audit, risks R1-R22, decisions D1-D9);
- the precedent `SPEC_light_actors_kit.md` and its commit `02c2587781f`;
- the Personal Lighting (PL) floater `llfloaterenvironmentadjust.{h,cpp}`;
- `alenvintensity.cpp` (consumer clamps) and `alenvintensitypresets.{h,cpp}`;
- the undo service from `81b2e5cb0b6` (`alsettingsundohistory*.{h,cpp}`);
- the kit sources (`director_kit_gen.py`, `kit_pages.json`, `director_mockup_data.json`, `alkit*.cpp`, `llfloaterdirector.cpp`);
- `COVERAGE_AUDIT_2026-10-15.md` rows 26-27 and `SPEC_search_index.md`.

**Line numbers** are from the working tree on 10/15 (HEAD `02c2587781f`). Other agents have uncommitted edits in `llfloaterdirector.{h,cpp}`, `alkitshell.*`, `kit_pages.json`, `director_mockup_data.json`, `director_kit_gen.py`, `director_page_sources.py`, `director_parity.py` and many generated pages. **Re-anchor every edit by symbol.** Apply pin changes as deltas ("+11", "-1"), never as absolute values copied from here.

User decisions this spec implements:
- the standalone PL floater stays;
- the Director page holds the full exposure set, its presets and its undo/redo;
- ranges keep the legacy range unless it hides consumer-accepted values, never beyond the clamp;
- every control gets value-wiring validation.

## 0. Decisions in one place

1. **Root-class page.** The page is generated with `root_class: panel_exposure_kit`. A new small class, `ALPanelExposureKit` (`alpanelexposurekit.{h,cpp}`), is the page root and owns every code-bound row. There is no shared panel to host:
   - `LLFloaterEnvironmentAdjust` members are not reusable;
   - `ALPanelExposureHub` lacks Kelvin, tints and presets.

   The class is a real owner class, so the parity owner filter applies (Section 6).
2. **PL setting rows are plain setting rows.** They use `control_name` with `ALKitSettingBinding`, so every write goes through `gSavedSettings`. The shared undo service already records every writer (`alsettingsundohistory.cpp:56-80`), so no second stack is needed for the values themselves.
3. **Undo is one shared history.** Section 3 has the details.
   - **Edit finalizers** are installed per row for tracked keys, giving one step per drag.
   - **Batches** wrap every kit multi-row write: page reset, card reset, paste, preset apply and row reset.
   - **Tint pairs** reset together through a new `reset_with` row attribute.
   - **The kit history no longer records tracked keys raw.** It records a *mirror* entry that replays the shared history. The page bar Undo and Ctrl+Z then act on the same stack as the floater, never on a copy.
4. **Presets: one module, two UIs.** Section 4 has the details.
   - The 14 built-ins move into `ALEnvIntensityPresets`.
   - An `env_intensity` kit provider wraps the module.
   - The strip derives its selection and its "(modified)" state from the module. The floater uses the same functions, so both UIs always agree.
   - Module signals refresh both UIs, so a preset saved in either one appears in both.
5. **Night Mask darkness is not a setting row.** Under the mockup's design authority, Night Mask, Rig exposure and Lens filters are **link cards with a read-only summary** (mockup comment: "Link card: read-only summary from shared state + Open › to the control's one home"). The kit already has card summaries (`ALKitCard::Summary`, `alkitcard.h:50-56`, `refreshSummary` `alkitcard.cpp:623-647`), but the generator never emits them. This spec:
   - adds **`scale`/`offset` to the summary** (the minimal display-scale mechanism);
   - makes the generator emit summaries.

   Darkness then shows `(1 - floor) * 100` = "85 %" and is never written from this page (R1 closed). A row-level converter is designed in Appendix A, for use only if Q1 asks for live rows.
6. **The one-home rule is honoured, not bypassed.** `kit_lint` rejects a second writer of a setting on another page unless it is a declared shortcut inside an Easy card or selector option (`kit_lint.py:650-663`, `:755-800`). The five user-approved `image_tab` → `exposure_tab` shortcuts sit in a plain card, so `kit_lint` needs an **enumerated allowance** for approved non-Easy shortcuts. This is not a relaxation: unlisted shortcuts still fail, and a planted fixture proves it (Section 6.3). Bloom rows need the same allowance (Q2).
7. **Advanced = a kit Advanced card** (card `a:1`: badge, Hide/Show, state persisted in `DirectorV2PageState`, `alkitpage.cpp:1566-1567`). `AlchemyEnvIntensityAdvanced` stays the floater's own panel state and is not bound (D5). Binding it would resize the floater from the Director.
8. **Sun/Moon label and the Moon enable rule** are live, as in the floater (`llfloaterenvironmentadjust.cpp:1184-1199`). They are driven by a setting watch in `ALPanelExposureKit` (D2).
9. **Auto exposure** moves to the Light intensity card, where the floater has it (D6). It stays on the page once.
10. **RLV** (D7): status quo, no gating. The legacy hub had none either. See Q4.
11. **`RenderExposure` stays in the shared stack** (D9 / R13, status quo). The Camera exposure row therefore records there too.

## 1. Page attributes and the `kit_pages.json` entry

```json
"light_group/exposure_tab": {
 "v1_sources": [],
 "root_class": "panel_exposure_kit", "root_name": "exposure_tab_logic",
 "preset": {"mode": "provider", "provider": "env_intensity"},
 "link_summaries": true,
 "selector_settings": {}, "card_enables": {},
 "row_overrides": {
  "exp_sun_tint":   {"reset_with": "AlchemyEnvSunTintStrength", "apply_now": true},
  "exp_sun_tint_strength":  {"reset_with": "AlchemyEnvSunTintColor"},
  "exp_moon_tint":  {"reset_with": "AlchemyEnvMoonTintStrength", "apply_now": true},
  "exp_moon_tint_strength": {"reset_with": "AlchemyEnvMoonTintColor"},
  "exp_amb_tint":   {"reset_with": "AlchemyEnvAmbientTintStrength", "apply_now": true},
  "exp_amb_tint_strength":  {"reset_with": "AlchemyEnvAmbientTintColor"}
 },
 "add_rows": [
  {"card": "scene_light", "after": "exp_gi_ev", "row": {"k": "readout", "w": "exp_sun_note", "l": "Sky note",
   "show_if": "sun_cap_note", "kw": "legacy classic sky capped boost ambiance",
   "h": "Shown when this sky caps or ignores the Sun / Moon boost. Raise Reflection Probe Ambiance above 0 to use it."}}
 ],
 "label_names": {}
}
```

Notes:
- Page `watch_settings` is generated from row settings (`director_kit_gen.py:705-708`). Summary settings are not rows and are not watched, which is correct because summaries are read-only.
- In `look_group/image_tab` (`kit_pages.json:193-197`), **delete `omit_rows` and `omit_rows_note`**. The five rows come back as `shortcut_of` rows (Section 2.4).

## 2. Layout: cards in order, every row

Sections (mockup `sections`; card indexes after the edit):

| section | label | cards |
|---|---|---|
| `scene` | Scene | 0, 1, 2, 3 |
| `exposure` | Exposure | 4, 5, 6 |
| `filters` | Filters | 7, 8 |

Column key:
- **bind**: `s` = setting row (`control_name`); `code` = wired by `ALPanelExposureKit`; `act` = generated `floater:`/`lightbox:` action; `sum` = card summary (read-only).
- **range**: UI range after Q3's default (legacy range in brackets when it changes).
- **dp/st**: decimals / increment.
- **a**: advanced row (behind "More options").

Help text is final, one line each, and free of `kit_lint` audit terms (`kit_lint.py:69`). The mockup texts with "type 7", "type 8" and "Lightbox … tab" are replaced.

### Card 0 `scene_light`: "Light intensity" (rows)

| # | widget | kind | label | bind | range | dp/st | default | help | show_if / enable |
|---|---|---|---|---|---|---|---|---|---|
| 1 | `exp_auto_exposure` (moved from `exposure_main`) | toggle | Auto exposure | s `RenderDynamicExposureEnabled` (Boolean, `settings_alchemy.xml:4347`) | bool | - | on | "Adapts to scene brightness, so light changes look softer. Turn off for exact, fixed exposure." | - |
| 2 | `exp_sun_ev` | slider | Sun / Moon EV (live label) | s `AlchemyEnvSunEV` (F32, `:4129`) | -6..6 [-4..6] | 2 / 0.05 | 0 | "Sun brightness in stops; 0 is the sky's own level. Also drives the moon while Link to Sun is on." | label "Sun EV" while unlinked (3.6) |
| 3 | `exp_gi_ev` | slider | Sky light (GI) | s `AlchemyEnvSkyGIEV` (`:4140`) | -6..6 [-4..4] | 2 / 0.05 | 0 | "Master for sky ambient and reflection-probe light together, in stops. 0 is stock." | - |
| 4 | `exp_sun_note` (add_rows) | readout | Sky note | code (text + visibility) | - | - | - | see Section 1 | `sun_cap_note` (3.6) |
| 5 | `exp_hint` (new) | readout, text "Auto exposure softens changes; off = exact control. Viewer-side, not saved to the sky." | Note | none | - | - | - | "Every light control here is a viewer-side multiplier: it is not saved into the sky." | - |
| 6 | `exp_undo` (moved from `exposure_hub`) | button "Undo" | Light undo | code: `ALExposureSettingsHistory::undo()` | - | - | - | "Undoes the last light-intensity or camera-exposure change, made here or in Personal Lighting." | enabled = `canUndo()`; tooltip "Undo: <label>" |
| 7 | `exp_redo` (moved) | button "Redo" | Light redo | code: `redo()` | - | - | - | "Redoes the last undone light-intensity or camera-exposure change." | enabled = `canRedo()`; tooltip "Redo: <label>" |
| 8 | `exp_presets_btn` (new) | button "Presets..." | Manage presets | act `floater:env_intensity_presets` (`llviewerfloaterreg.cpp:438`) | - | - | - | "Opens the light-intensity preset manager: save, load and delete your own presets." | - |

### Card 1 `light_advanced`: "Advanced light" (rows, card `a:1`)

The rows follow the floater's Advanced panel order (`floater_adjust_environment.xml:790-1297`). The card reset equals the floater's "Reset advanced".

| # | widget | kind | label | bind | range | dp/st | default | help | enable |
|---|---|---|---|---|---|---|---|---|---|
| 1 | `exp_moon_linked` | toggle | Link to Sun | s `AlchemyEnvMoonLinked` (`:4154`) | bool | - | on | "On: the Sun / Moon slider drives the moon too. Off: Moon EV is used at night." | - |
| 2 | `exp_moon_ev` | slider | Moon EV | s `AlchemyEnvMoonEV` (`:4165`) | -6..6 [-4..6] | 2 / 0.05 | 0 | "Moon brightness in stops, used at night while Link to Sun is off." | enabled only when unlinked (3.6) |
| 3 | `exp_local_include_rig` | toggle | Rig lights | s `AlchemyEnvLocalLightIncludeRig` (`:4220`) | bool | - | off | "Also applies Local lights to Cine Light Rig lights." | - |
| 4 | `exp_local_ev` (moved from card 0) | slider | Local lights | s `AlchemyEnvLocalLightEV` (`:4209`) | -6..6 [-4..4] | 2 / 0.05 | 0 | "Brightness offset in stops for every point, spot and projector light." | - |
| 5 | `exp_probe_diff_ev` | slider | Probe diffuse | s `AlchemyEnvGIProbeDiffuseEV` (`:4187`) | -6..6 [-3..3] | 2 / 0.05 | 0 | "Indirect light from reflection probes in stops, added to the Sky light master." | - |
| 6 | `exp_probe_spec_ev` | slider | Probe reflections | s `AlchemyEnvGIProbeSpecEV` (`:4198`) | -6..6 [-3..3] | 2 / 0.05 | 0 | "Brightness of glossy and metal reflections from probes. Not affected by the Sky light master." | - |
| 7 | `exp_amb_ev` | slider | Sky ambient | s `AlchemyEnvGIAmbientEV` (`:4176`) | -6..6 [-3..3] | 2 / 0.05 | 0 | "Sky ambient brightness in stops, added to the Sky light master." | - |
| 8 | `exp_shadow_lift` (moved from card 0) | slider | Shadow lift | s `AlchemyEnvShadowLiftEV` (`:4323`) | 0..2 | 2 / 0.05 | 0 | "Lifts ambient light where the sun or moon does not reach, in stops. 0 is off." | - |
| 9 | `exp_sun_kelvin` (new) | slider | Sun temperature (K) | s `AlchemyEnvSunKelvin` (`:4231`) | 1000..15000 [2000..12000] | 0 / 50 | 6500 | "Colour temperature of direct sunlight on surfaces. 6500 K is no change; lower is warmer." | - |
| 10 | `exp_sun_tint` (new) | color, `apply_now` | Sun tint | s `AlchemyEnvSunTintColor` (Color4, `:4242`) | sRGB 0..1 | - | white | "Tint on direct sunlight. White, or strength 0, means no tint." | reset with #11 |
| 11 | `exp_sun_tint_strength` (new) | slider | Sun tint strength | s `AlchemyEnvSunTintStrength` (`:4258`) | 0..1 | 3 / 0.01 | 1 | "How strongly the sun tint applies, 0 to 1." | reset with #10 |
| 12 | `exp_moon_tint` (new) | color, `apply_now` | Moon tint | s `AlchemyEnvMoonTintColor` (`:4269`) | sRGB | - | white | "Tint on direct moonlight at night. White, or strength 0, means no tint." | reset with #13 |
| 13 | `exp_moon_tint_strength` (new) | slider | Moon tint strength | s `AlchemyEnvMoonTintStrength` (`:4285`) | 0..1 | 3 / 0.01 | 1 | "How strongly the moon tint applies, 0 to 1." | reset with #12 |
| 14 | `exp_amb_tint` (new) | color, `apply_now` | Ambient tint | s `AlchemyEnvAmbientTintColor` (`:4296`) | sRGB | - | white | "Tint on sky ambient and probe light: shadow sides, interiors, corners." | reset with #15 |
| 15 | `exp_amb_tint_strength` (new) | slider | Ambient tint strength | s `AlchemyEnvAmbientTintStrength` (`:4312`) | 0..1 | 3 / 0.01 | 1 | "How strongly the ambient tint applies, 0 to 1." | reset with #14 |
| 16 | `exp_adv_reset_all` (new) | button "Reset advanced" | Reset advanced | code: one batch "Reset advanced" (3.5) | - | - | - | "Resets every control in this card and links the moon again. Auto exposure, Sun / Moon and Sky light are not changed." | - |
| 17 | `exp_adv_note` (new) | readout, text "Probe reflections is independent of the Sky light master; mirrors skip it." | Note | none | - | - | - | "Sun, moon, Kelvin and local-light factors reach probes and mirrors; the others apply once, at shading." | - |

### Card 2 `sky_settings`: "Per-sky settings" (custom, kept)

| widget | kind | label | bind | help |
|---|---|---|---|---|
| `exp_sky_note` | readout, text "Scene gamma and probe ambiance are saved with each sky." | Per-sky values | none | "Sky gamma and probe ambiance are stored per sky. Edit them in Personal Lighting." |
| `exp_sky_lighting_btn` | button "Personal Lighting..." | Per-sky look | act `floater:env_adjust_snapshot` (**add** `"target": "env_adjust_snapshot"` to the mockup row) | unchanged |

### Card 3 `exp_night_mask_link`: "Night mask" (link → `light_group/cine_light_rig_tab`, summaries)

| summary | label (mockup `l` edit) | setting | kind | display |
|---|---|---|---|---|
| `exp_nm_enabled` | Night mask | `CineLightRigNightMaskEnabled` | bool | On / Off |
| `exp_nm_darkness` | Darkness | `CineLightRigNightMaskDarkness` (F32 floor 0..1, default 0.15, `settings.xml:2390-2395`) | number, `dc 0`, unit `%`, **`ds -100`, `do 100`** | 100 - 100 × floor = "85 %" (same formula as `alpanelexposurehub.cpp:195-198`) |

### Card 4 `exposure_main`: "Camera exposure" (rows, kept; this page is the writable home, `director_page_sources.py:255-258, 273`)

| widget | kind | label | bind | range | dp/st | default | help | enable |
|---|---|---|---|---|---|---|---|---|
| `exp_exposure` | slider | Exposure | s `RenderExposure` (`settings.xml:21523`) | 0.5..4 (= clamp, `pipeline.cpp:13246`) | 1 / 0.1 | 1 | unchanged | - |
| `exp_tonemap_type` | combo, 9 items, values 0..8 | Tone mapper | s `AlchemyRenderTonemapType` (S32) | 0..8 | - | 7 | unchanged | - |
| `exp_tonemap_mix` | slider | Tone strength | s `RenderTonemapMix` | 0..1 | 1 / 0.1 | 0.7 | unchanged | - |
| `exp_amd_exposure` (a) | slider | AMD exposure | s `AlchemyToneMapAMDExposure` | 1..16 | 2 / 0.1 | 7.4 | "Exposure for the AMD LPM tone mapper only; other tone mappers ignore it." | enabled only when the tone mapper is AMD LPM (3.6) |
| `exp_slog_ev` (a) | slider | S-Log EV | s `AlchemyToneMapSLogEV` | -8..8 | 1 / 0.1 | 0 | "Exposure offset in stops before the log curve. S-Log3 Capture only." | enabled only on S-Log3 Capture (3.6) |
| `exp_legacy_gamma_skies` (a) | toggle | Legacy gamma | s `AlchemyTonemapLegacyGammaSkies` | bool | - | off | unchanged | - |

### Card 5 `rig_exposure_link`: "Rig exposure" (link → `cine_light_rig_tab`, summaries)

| summary | label | setting | kind | display |
|---|---|---|---|---|
| `exp_rig_master_ev` | Master EV | `CineLightRigMasterEV` | number, dc 2, unit `EV` | value |
| `exp_rig_headroom` | Headroom | `CineLightRigHeadroomStops` | number, dc 2, unit `stops` | value |

### Card 6 `exposure_hub`: retitled "Exposure window" (custom, kept)

| widget | kind | label | bind | help |
|---|---|---|---|---|
| `exposure_popout` | button "Open Exposure window..." | Open window | act `floater:exposure_hub` (**add** `"target": "exposure_hub"`) | "Opens the floating Exposure window with the same controls." |

### Card 7 `lens_filters_link`: "Lens filters" (link → `lens_group/filters_tab`, summaries)

| summary | new label (the mockup has "Enable" four times) | setting |
|---|---|---|
| `exp_gnd_enabled` | Graduated ND | `RenderGradNDEnabled` |
| `exp_pol_enabled` | Polarizer | `RenderPolarizerEnabled` |
| `exp_ir_enabled` | Infrared | `RenderInfraredEnabled` |
| `exp_softedge_enabled` | Soft edge | `RenderSoftEdgeEnabled` |

All four are bool (On / Off).

### Card 8 `glow_filters`: "Bloom & filters" (rows)

| widget | kind | label | bind | range | dp/st | help |
|---|---|---|---|---|---|---|
| `exp_bloom_strength` | slider | Bloom strength | s `RenderBloomStrength`, **`shortcut_of: bloom_glow_tab/set_RenderBloomStrength`** (Q2) | 0..2 | 3 / 0.01 | unchanged (the generator appends "Home: Look > Bloom & glow …", `director_kit_gen.py:266-269`) |
| `exp_bloom_threshold` | slider | Threshold | s `RenderBloomThreshold`, **`shortcut_of: bloom_glow_tab/set_RenderBloomThreshold`** (Q2) | 0..4 | 2 / 0.05 | unchanged |
| `exp_filters_btn` | button "Filters..." | All filters | act `lightbox:colortone_settings` (**add** `"target": "lightbox", "target_tab": "colortone_settings"`) | "Opens every Lens Looks control in its own window." |

If Q2 is "no", card 8 becomes a link card to `look_group/bloom_glow_tab`, with two number summaries and the Filters button moved to card 6.

### 2.1 Counts

- **9 cards.**
- **Mockup rows: 25 → 36 (+11).** The 11 new rows are `exp_hint`, `exp_presets_btn`, `exp_sun_kelvin`, the 3 tints, the 3 strengths, `exp_adv_reset_all` and `exp_adv_note`. Five rows only move.
- **Generated rows: 37** (36 + the `exp_sun_note` add_row), plus **8 card summaries**, a 4-part preset strip, page reset and bar undo/redo.
- **PL coverage: all 18 settings**, plus the undo/redo, presets combo, Presets..., Reset advanced, both notes, the label flip and the Moon enable rule.
- **Not carried, by decision:** the `env_adv_toggle` button. The kit card's Hide/Show replaces it (decision 7).

### 2.2 Ranges (rule check; Q3 default = apply the rule literally)

The consumer clamps are `EV_CLAMP = 6` and `LIFT_EV_CLAMP = 2` (`alenvintensity.cpp:38-39`).

| setting | where the clamp applies | legacy range hides consumer values? | new range |
|---|---|---|---|
| Sun EV, Moon EV | per control, `:44-47`, applied `:255-257` | yes, -6..-4 | -6..6 |
| Local lights EV | per control, `:321` | yes | -6..6 |
| Probe reflections EV | per control, `:260` | yes | -6..6 |
| Sky GI, Sky ambient, Probe diffuse | per **sum**, `:258-259`, `:333` | yes (each alone is accepted to ±6) | -6..6. A sum past ±6 saturates silently; the help says so (R12) |
| Shadow lift | `:265` (0..2) | no | 0..2 |
| Kelvin | `:82` (1000..15000) | yes | 1000..15000 |
| Tint strengths | `:185` (0..1) | no | 0..1 |
| `RenderExposure` | `pipeline.cpp:13246` | no | 0.5..4 |

Consequences (WP3):
- Widen `FLOAT_FIELDS` (`alenvintensitypresets.cpp:64-76`) to the same ranges. Otherwise `sanitize` (`:181-204`) rewrites saved values, and a saved preset would not match the live values it came from.
- Also widen the floater sliders in `floater_adjust_environment.xml` (`min_val`/`max_val` on `env_sun_ev` :617, `env_gi_ev` :661, `env_moon_ev` :828, `env_local_ev` :883, `env_probe_diff_ev` :928, `env_probe_spec_ev` :973, `env_amb_ev` :1018, `env_sun_kelvin` :1108). Otherwise the floater shows a pinned thumb for a Director value. These are attribute-only changes; run `xui_layout_check --changed`.
- The non-PL rows keep their legacy ranges. The implementer records each consumer clamp in the validation table (Section 7); these were not verified in this pass.

### 2.3 Mockup edits (`scripts/code_tools/director_kit/director_mockup_data.json`, `light_group`/`exposure_tab`)

Edit the repo file directly with a Python load/modify/dump (`separators=(",", ":")`, `ensure_ascii=False`), as in the precedent (`SPEC_light_actors_kit.md` 1.3). **Do not run `director_mockup_data.py`.** Optionally mirror the edits into `content_curated/light_group.json` and `presets.json`, for bookkeeping only.

1. Insert card `light_advanced` (`"a":1`) after `scene_light`. Then set `sections`: scene `[0,1,2,3]`, exposure `[4,5,6]`, filters `[7,8]`.
2. Move these rows:
   - `exp_auto_exposure` to the head of `scene_light`;
   - `exp_undo` and `exp_redo` to `scene_light`, after `exp_gi_ev`;
   - `exp_moon_linked`, `exp_moon_ev`, `exp_amb_ev`, `exp_probe_diff_ev`, `exp_probe_spec_ev`, `exp_local_include_rig`, `exp_local_ev` and `exp_shadow_lift` into `light_advanced`, in Section 2 order, dropping their `"a"` flags.
3. Add the 11 new rows, with `l`, `k`, `w`, `h`, `kw`, `s`, `d`, `mn`/`mx`/`st`/`dc`, `ty` and `text` for readouts, as in Section 2.
4. Apply the range changes (Q3).
5. Make the help changes for `exp_amd_exposure`, `exp_slog_ev`, `exp_filters_btn` and `exposure_popout`.
6. Add `target` to `exp_sky_lighting_btn`, `exposure_popout` and `exp_filters_btn`.
7. Retitle `exposure_hub` to "Exposure window".
8. In the summaries:
   - set `exp_nm_darkness` to `"ds":-100,"do":100` and keep `"unit":"%"`;
   - set `exp_nm_enabled` `l` to "Night mask";
   - give the four filter summaries their names;
   - add `"unit":"EV"` to Master EV and `"unit":"stops"` to Headroom.
9. Bloom rows get `shortcut_of` (Q2).

Keywords (search, Section 8) on the new rows:

| row | `kw` |
|---|---|
| kelvin | "kelvin temperature white balance warm cool sun colour light intensity" |
| tints | "tint colour gel personal lighting" |
| presets button | "presets save load my presets light intensity personal lighting" |
| undo/redo | "undo redo history personal lighting" |

### 2.4 `image_tab` shortcuts (the five approved 10/13)

In the mockup, `look_group/image_tab`, add `shortcut_of`:

| image row | `shortcut_of` |
|---|---|
| `tonemap_type_combo` | `exposure_tab/exp_tonemap_type` |
| `exposure_slider` | `exposure_tab/exp_exposure` |
| `tonemap_legacy_gamma_skies` | `exposure_tab/exp_legacy_gamma_skies` |
| `lpm_toneexp_slider` | `exposure_tab/exp_amd_exposure` |
| `slog_ev_slider` | `exposure_tab/exp_slog_ev` |

`normalize_shortcuts` copies `k/mn/mx/st/dc/unit` from the home (`director_kit_gen.py:247-269`). The ranges already match. The rows still write through `control_name`, so the mirror undo (3.3) covers `RenderExposure` there too.

## 3. Undo / redo: one history

### 3.1 What exists

The service is a singleton listening on `gSavedSettings`. It tracks 19 keys (`alsettingsundohistory.cpp:26-47`) and has these mechanisms:

| mechanism | where |
|---|---|
| per-key coalesce within 500 ms | `alsettingsundohistorymodel.cpp:13-52` |
| batches | `:54-75` |
| edit groups | `:77-93` |
| `installEditFinalizers` for sliders and spinners | `alsettingsundohistory.cpp:261-301` |

The kit has its own shell-wide history, `ALKitHistory`:
- it watches page settings (`alkithistory.cpp:81-90`, registered at `alkitpage.cpp:870`);
- it records them as raw setting entries during a Director gesture (`captureSetting`, `:135-162`);
- it replays them with `control->set` (`:285-293`).

Today, therefore, a Director change to a tracked key lands in **both** stacks. A kit undo is then recorded by the shared stack as a new change, and a floater undo is captured by the kit as a new step. The two stacks drift apart. The audit missed this; it is fixed here.

### 3.2 Edit finalizers (one step per drag), systemic

In `ALKitRow::resolveBinding` (`alkitrow.cpp:758-807`), after the binding is made:

```cpp
// A slider/spinner bound to a key of the shared exposure history: one undo step per drag.
if (mBind == "setting" && ALExposureSettingsHistory::findTrackedSetting(mMeta.setting))
    ALExposureSettingsHistory::installEditFinalizers(this);   // BFS over this row: its slider/spinner only
```

- This covers this page and the five `image_tab` shortcuts, with no per-page code.
- `installEditFinalizers` connects signals (`LLSliderCtrl::setSliderMouseDownCallback` returns a connection, `llsliderctrl.cpp:594-606`), so it adds to the row's callbacks and does not replace them.
- Colour swatches, toggles and combos commit once per pick, so they need no finalizer. A swatch drag inside the picker coalesces by the 500 ms rule, the same as in the floater (R16, accepted parity).

### 3.3 Kit history mirrors the shared history (no double recording)

**Model (`alsettingsundohistorymodel.{h,cpp}`):**
- `Step` gains `U64 mSerial = 0;`.
- The model gains `U64 mLastSerial = 0;`.
- In `pushUndo` (`:187`), when `clear_redo` is true (a fresh step): `step.mSerial = ++mLastSerial;`. A redo move (`takeRedo`, `:133-144`, `clear_redo=false`) keeps the serial.
- `clear()` does **not** reset `mLastSerial`, so stale mirrors never match.
- New accessors:
  - `U64 lastSerial() const`;
  - `U64 undoSerial() const`: 0 while a pending step exists, else `mUndo.back().mSerial`, or 0;
  - `U64 redoSerial() const`: `mRedo.back().mSerial`, or 0.

**Service (`alsettingsundohistory.{h,cpp}`):** forward `lastSerial()`, `undoSerial()` and `redoSerial()`.

**Kit history (`alkithistory.{h,cpp}`):**
- `Step` gains `U64 exposure_start = 0; bool exposure_touched = false;`.
- `beginGesture` (`:48-70`): when a new `mOpen` is made, set `mOpen->exposure_start = ALExposureSettingsHistory::lastSerial();`.
- `captureSetting` (`:135-162`): after the gesture-open logic and before building `Entry`:
  ```cpp
  if (ALExposureSettingsHistory::findTrackedSetting(name))
  {   // The shared exposure history owns this key; the kit keeps only a mirror (finishGesture).
      mOpen->exposure_touched = true;
      return;
  }
  ```
- `finishGesture` (`:200-216`), before the empty check:
  ```cpp
  if (mOpen->exposure_touched)
  {
      ALExposureSettingsHistory::endEdit();                       // commit a pending step now
      const U64 first = mOpen->exposure_start + 1, last = ALExposureSettingsHistory::lastSerial();
      if (last >= first)
      {
          if (mOpen->label.empty()) mOpen->label = "Light \xE2\x80\xBA " + ALExposureSettingsHistory::undoLabel();
          mOpen->custom.push_back({ "exposure_history", [first, last](bool redo, std::string&)
          {
              ALExposureSettingsHistory::endEdit();
              auto in = [&](U64 s) { return s >= first && s <= last; };
              if (!redo) { while (in(ALExposureSettingsHistory::undoSerial())) ALExposureSettingsHistory::undo(); }
              else       { while (in(ALExposureSettingsHistory::redoSerial())) ALExposureSettingsHistory::redo(); }
              return true;   // already undone/redone elsewhere (floater, Light undo) = no-op, never a partial step
          }});
      }
  }
  ```
  The replay never returns false. `ALKitHistory::replay` reverts raw entries before running customs (`:263-310`), so a false return would leave a half-applied kit step.

**Result:**
- A Director drag of Sun EV makes one shared step (serial N) and one kit step that points at N.
- Ctrl+Z (`LLFloaterDirector::handleKeyHere` 'Z' → `mShell->history().undo()`) or the page bar Undo (`alkitpage.cpp:470-483`) calls `ALExposureSettingsHistory::undo()`. The floater's Undo/Redo state updates through the revision signal.
- An undo made in the floater or with Light undo makes the later kit replay a no-op.
- Clicking Light undo inside the Director touches tracked keys during a pointer gesture, but no fresh serial is pushed, so no mirror is recorded.
- **Known edge** (documented, accepted): a floater step that idle-commits during a Director mouse gesture falls inside `[first, last]`, so the kit's Undo also undoes it.

### 3.4 Batches (one step per multi-row action), systemic

Add `ALExposureSettingsHistory::ScopedBatch` in each place below. A batch that touches no tracked key adds nothing (`endBatch` pushes only non-empty, `alsettingsundohistorymodel.cpp:65-75`), so other pages are unaffected.

| site | label |
|---|---|
| `ALKitPage::performPageReset` (`alkitpage.cpp:1493-1501`), around the loop | "Reset page" |
| `ALKitCard` reset lambda (`alkitcard.cpp:320-334`), around the loop | "Reset " + `mTitle` |
| selector reset-all (`alkitcard.cpp` near :393) | "Reset all …" (same text as the kit gesture) |
| card paste (`alkitcard.cpp` near :592) | "Paste " + `mTitle` |
| `ALKitPresetStrip::onSelect` (`alkitpresets.cpp:308-341`), around `mProvider->apply(id)` | "Preset '" + item label + "'". This matches the floater (`llfloaterenvironmentadjust.cpp:1221-1223`); the module's own inner batch nests (`:54-63`) |
| both row reset-button lambdas (`alkitrow.cpp:442-447` and `:792-797`): fold them into one private `void resetFromButton()` | "Reset " + `mMeta.label` |

### 3.5 Tint pairs and "Reset advanced"

- New row param `reset_with` (comma list), in `ALKitRow::Params` (`alkitrow.h:31-32`) and the ctor (`alkitrow.cpp:156-170`).
- `ALKitSettingBinding` gets an optional `std::vector<std::string> mCompanions`. `reset()` (`alkitbinding.cpp:81-85`) resets itself, then each companion, with `resetToDefault(true)`. `isDefault`/amber stay per key.
- In `resolveBinding` (`alkitrow.cpp:764-765`):
  ```cpp
  make_unique<ALKitSettingBinding>(mMeta.setting, LLStringUtil::getTokens(mResetWith, ","))
  ```
- The generator emits `reset_with` from the row key of the same name (Section 6.1).
- A row reset of either half resets both in one batch. Card and page reset reset both anyway (idempotent).

"Reset advanced" (`exp_adv_reset_all`):
- New `void ALEnvIntensityPresets::resetAdvanced()` resets the 14 keys of `ENV_INTENSITY_ADV_KEYS` and sets `AlchemyEnvMoonLinked` true. The 14 keys move from `llfloaterenvironmentadjust.cpp:132-147` into `alenvintensitypresets.cpp`.
- The floater's `env_adv_reset_all` (`:479-491`) and the kit button both call it inside their own `ScopedBatch` ("Reset All" / "Reset advanced").

### 3.6 `ALPanelExposureKit` (root class) responsibilities

Registration: `static LLPanelInjector<ALPanelExposureKit> t("panel_exposure_kit");`. Add both new files to `indra/newview/CMakeLists.txt` next to `alpanelexposurehub` (`:404`, `:1326`).

Wiring in `postBuild`:
- Use `findChild` plus `LL_WARNS` on a miss. Never use `getChild`, because a dummy would hide a dead row.
- Keep `boost::signals2::scoped_connection` members.
- Do no per-frame work: everything is event-driven (`ui_draw_lint`).

| concern | implementation |
|---|---|
| Light undo/redo | `exp_undo`/`exp_redo` commit → `undo()`/`redo()`. `connectRevisionChanged` → `setEnabledIfChanged` + `setToolTipIfChanged`, the same text as `llfloaterenvironmentadjust.cpp:1265-1283`. |
| Moon link | `ALKitSettingWatch::watch("AlchemyEnvMoonLinked")`. `exp_sun_ev_label` (the row label view, `alkitrow.cpp:253`) shows "Sun / Moon EV" or "Sun EV". `exp_moon_ev` and `exp_moon_ev_reset` are `setEnabled(!linked)`. |
| Sky note (`sun_cap_note`) | Hide first with `ALKitRow::setControlVisible(note, false)` (the precedent: required so the first reveal relayouts, `SPEC_light_actors_kit.md` 2.5a). Evaluate the floater predicate (`llfloaterenvironmentadjust.cpp:1102-1131`) on `LLEnvironment::instance().getCurrentSky()`. **Classic:** `canAutoAdjust() && !RenderSkyAutoAdjustLegacy` → "Classic sky: Local lights only". **Legacy:** `getReflectionProbeAmbiance(auto_adjust) == 0` → "Legacy: Sun boost capped". Tooltips are copied from XML strings 17-18 and 28-29, plus the `AlchemyTonemapLegacyGammaSkies` variant (`:1121-1123`). Re-run on `LLEnvironment::setEnvironmentChanged` (`llenvironment.h:250`), on both settings' watches and on `onVisibilityChange(true)`. |
| Tone-mapper gating | Watch `AlchemyRenderTonemapType`. `exp_amd_exposure` and `_reset` are enabled only on 7; `exp_slog_ev` and `_reset` only on 8. This matches `alpanelexposurehub.cpp:178-187`. |
| Reset advanced | `ScopedBatch b("Reset advanced"); ALEnvIntensityPresets::resetAdvanced();` |
| Preset provider | `ALPanelExposureKit::registerPresetProvider()` (static, idempotent), called from `registerLookPresetProviders()` (`llfloaterdirector.cpp:655-670`). |

Strings are English constants in the `.cpp`, matching the kit's other code-bound text.

**Binder edits (`llfloaterdirector.cpp`):**
- Delete `{ "exposure_tab", &LLFloaterDirector::bindCameraPage }` (`:1435`). The root class owns the page, and the popout is an action row.
- In `bindCameraPage`, delete the v2-only `exposure_popout` block (`:2059-2062`) and the `director_exposure_panel` block (`:2107-2112`). Both are dead after conversion (R20).
- **Keep** the v1 code at `:1053-1054`, `:1158-1167` and `:2390`, because v1 still embeds the hub.

## 4. Presets: `env_intensity` provider, shared state

### 4.1 Module (`alenvintensitypresets.{h,cpp}`)

| addition | purpose |
|---|---|
| `struct BuiltIn { const char* mKey; const char* mLabel; Values mValues; }; const std::vector<BuiltIn>& builtIns();` | The 14 rows move from `llfloaterenvironmentadjust.cpp:186-225` with labels from XUI items 734-747, in that order. `env_intensity_builtin_values` is deleted (R9). |
| `bool applyBuiltIn(const std::string& key);` | `apply(values)` + `setLastApplied("b:"+key, label)` |
| `applyNamed` (existing, `.cpp:1297`) | also `setLastApplied("u:"+name, name)` |
| `std::string matchLiveId(std::string* label = nullptr);` | The floater's rule from `:1330-1366`, verbatim: built-in first (tie rule), then `findMatchingName`. Returns `"b:<key>"`, `"u:<name>"` or `""`. Uses `nearlyEqual` (1e-4). |
| `bool lastApplied(std::string& id, std::string& label);` and `void setLastApplied(...)` | Session-only shared baseline. Cleared when that user preset is removed (`remove`, `.cpp:1262`). |
| `const std::vector<std::string>& settingNames();` | The 16 keys, from `FLOAT_FIELDS`, `COLOR_FIELDS` and `AlchemyEnvMoonLinked` |
| `void resetAdvanced();` | 3.5 |
| `FLOAT_FIELDS` ranges | 2.2 |

Floater refactor (behaviour unchanged):
- `applyEnvIntensityPreset` → `applyBuiltIn`;
- `refreshEnvIntensityPresetCombo` → `matchLiveId`, mapping `b:k` → `k` and `u:n` → `"user:"+n`;
- `ENV_INTENSITY_PRESET_WATCHED_KEYS` → `settingNames()`.

### 4.2 Shared save/delete UX (`alfloaterenvintensitypresets.{h,cpp}`)

Factor the dialog's `onSave` body (`:154-190`) and the delete-failure notice (`:224-230`) into free functions in a new namespace `ALEnvIntensityPresetsUi`:
- `void saveInteractive(const std::string& raw_name)` performs the read-only notice, the name check, the reserved check, the overwrite confirm `EnvIntensityPresetOverwrite`, the save, and the result notices (`notifications.xml:13781-13893`).
- `void notifyWriteFailed()`.

The dialog calls them, so its behaviour is unchanged.

### 4.3 Provider (`ALEnvIntensityPresetProvider`, in `alpanelexposurekit.cpp`)

| member | behaviour |
|---|---|
| `key()` | `"env_intensity"` |
| `list()` | `builtIns()` as `{"b:"+key, label, true}`, then `listNames()` as `{"u:"+name, name, false}` |
| `apply(id)` | `b:` → `applyBuiltIn`; `u:` → `applyNamed` |
| `canSaveUser()` | `!isReadOnly()` |
| `saveUser(name)` | `saveInteractive(name)`. Returns `exists(clean name) && values match` only when it saved synchronously. With the overwrite prompt it returns false, and the list signal refreshes later. |
| `canDelete(id)` | `u:` prefix |
| `deleteUser(id)` | `remove(name)`; on false, while it still exists → `notifyWriteFailed()` |
| `revision()` | counter bumped by the list signal |
| `trackedSettings()` | `settingNames()` |
| **new** `liveSelection(id, label, last_id, last_label)` | `matchLiveId` + `lastApplied`; returns true |
| **new** `connectChanged(cb)` | one internal signal fed by `connectListChanged` and `connectApplied` |
| **new** `busy()` | `isApplying()` |
| **new** `userHeader()` | "-- My presets --" (XML string 25) |

### 4.4 Strip (`alkitpresets.{h,cpp}`), generic and opt-in

New virtuals on `ALKitPresetProvider` (`alkitpresets.h:10-27`), all defaulting to today's behaviour:
- `bool liveSelection(...) const { return false; }`
- `boost::signals2::connection connectChanged(std::function<void()>) { return {}; }`
- `bool busy() const { return false; }`
- `std::string userHeader() const { return "-- Saved --"; }`

Strip changes:
- **`postBuild`** (`:213-263`): `mProviderConnection = mProvider->connectChanged([this]{ queueRefresh(); });`. This fixes R6. `revision()` stays unused, as today.
- **`refresh()`** (`:265-306`): when `liveSelection` is true:
  - select `id`;
  - if `id` is empty, show "Custom". If a last-applied preset exists and is still listed, set the combo label to "<last> (modified)" and add the item "Revert to <last>" (`"revert:"+last_id`).
  - **Do not** use `mAppliedValues` for this provider. This fixes R7: a preset applied in the floater is the strip's baseline too.
- **`trackedSettingChanged`** (`:422-434`), for a live provider: `if (mProvider->busy()) return; queueRefresh();`
  - `queueRefresh` = one `doOnIdleOneTime` (handle-guarded), throttled to 10 Hz with an `LLFrameTimer`;
  - this is no more than the floater's dirty-frame refresh (R17).
- **`save()`** (`:355-371`): unchanged. The provider decides; `captureApplied` is harmless for live providers.

**Result:** both UIs derive "which preset, or Custom, or modified" from the same module functions and the same tolerance. A save, overwrite or delete in either UI fires `connectListChanged`. The floater rebuilds its list (`:540-552`); the strip refreshes through `connectChanged`.

## 5. Display-scale mechanism (kit summaries)

- `ALKitCard::Summary` (`alkitcard.h:50-56`) gains `Optional<F32> scale, offset` (defaults 1, 0). `SummaryData` (`:96-103`) stores them, and the constructor copies them (`alkitcard.cpp:130`).
- In `refreshSummary` (`alkitcard.cpp:623-647`), the number branch becomes:
  ```cpp
  text = llformat("%.*f", sum.decimals, value.asReal() * sum.scale + sum.offset);
  ```
- Summaries are read-only labels and never write. Setting changes refresh them through the existing signal (`:265-269`).

For darkness, `scale -100, offset 100` gives `100 - 100 × floor`:
- the default floor 0.15 → "85 %";
- floor 0 → "100 %";
- floor 1 → "0 %".

This equals the hub (`alpanelexposurehub.cpp:195-198`) and the Rig rows (`alpanelcinelightrig.cpp:1010-1040`), apart from the hub's rounding: `%.0f` rounds the same way.

**Generator:** when the page config has `link_summaries: true`, then for each card with `k == "link"`, emit one `kit_card.summary` child per mockup `summary` row that has `s`:

| summary attribute | source |
|---|---|
| `setting` | `s` |
| `label` | `l` |
| `kind` | `bool` for toggle; `number` for slider/number; else `text` |
| `decimals` | `dc` |
| `unit` | `unit` |
| `scale` | `ds` |
| `offset` | `do` |

Also:
- A summary row without `s` is a `GenError`.
- The flag stays per page, so `shot_tab`'s combo summary is not emitted until combos can show item labels.
- `kit_lint` checks `setting` on any node for existence and type (`kit_lint.py:482-495`). Summaries are not `kit_row` nodes and are not in `WRITER_ATTRS` (`:126`), so they are not counted as writers. That is correct, because they cannot write.

Appendix A gives the editable-row version, for use only if Q1 = live rows.

## 6. Gates, pins and harness

### 6.1 Generator (`scripts/code_tools/director_kit_gen.py`)

| change | where |
|---|---|
| Row pin **+11**, with the comment "+11: Light > Exposure holds the full Personal Lighting set (Kelvin, 3 tint pairs, Reset advanced, presets button, two notes) (10/15)" | `validate_data` `:230-231` (37 pages unchanged) |
| `EXPECTED_SHORTCUTS` += the 5 image triples `("image_tab", "<row>", "exposure_tab/<home>")` (2.4), plus the 2 bloom triples `("exposure_tab", "exp_bloom_strength", "bloom_glow_tab/set_RenderBloomStrength")` and the threshold one if Q2 = yes. Replace the comment at `:46-47`. | `:37-48` |
| `expected` dict: add `"light_group/exposure_tab": "kit (P0)"` | `:1193-1207` |
| Both Counters → `{"kit (P0)": 24, "pure kit (new)": 1, "legacy": 12}`, with the pin comment "10/15 Light Exposure: kit 23 -> 24, legacy 13 -> 12" | `:1214` and `:1281` |
| selftest "12 serialized legacy pages", `legacy_count == 12` | `:1317` |
| "38 page files" **unchanged** (same file name `panel_dpage_light_exposure.xml`) | `:1337` |
| `row_element`: emit `reset_with` from the row key; for a native `color_swatch`, emit `can_apply_immediately="true"` when `apply_now` is set | `:593-663`, `:526-553` |
| `kit_page`: emit `kit_card.summary` under `link_summaries` (Section 5) | card loop `:740-775` |

### 6.2 Other pins and owner data

| file | change |
|---|---|
| `scripts/code_tools/xui_layout_check.py` | `LEGACY_PAGE_COUNT = 12` (`:1681`), with the comment "Light Exposure: 13 -> 12". There is no exposure key in `gate_policy.json` `xui_legacy_v1_findings`. |
| `scripts/code_tools/director_parity.py` | `EXPECTED_PAGES` stays 37 (`:55`). Add `"panel_exposure_kit": {"ALPanelExposureKit"}` to `root_owner` (`:760-764`). **Required**: rows reference names that `alpanelexposurehub.cpp` also requests (`exp_sun_ev`, …). Without the filter, the hub's `getChild` calls (e.g. `exp_ir_preset`, `exp_tab_post`) are checked against this page (check 3). |
| `kit_pages.json` `parity_allow` | Exact names only, each with a reason; add only what the parity run reports. Check 1 now sees the hub's 152 names vanish from v2 (parity follows `filename=` embeds, `director_parity.py:310-312`). Expected categories: **(a)** `panel_exposure_hub`, `director_exposure_panel`, `exposure_tabs`, `exp_tab_*` and `exp_hdr_*`: "v1 hub layout replaced by kit cards"; **(b)** setting-named label texts (`RenderExposure`, `AlchemyEnvSunEV`, …): "v1 label text; the kit row label replaces it"; **(c)** `rst_*`: "replaced by the kit row reset icon"; **(d)** combo item names (`exp_tonemap_type_N`, `exp_gnd_preset_N`, `choose`, `wood`, …): "item names regenerated as <widget>_item_N" or "homed on Lens > Filters"; **(e)** `exp_gnd_*`, `exp_pol_*`, `exp_ir_*`, `exp_softedge_*`, `exp_rig_*`, `exp_light_rig_btn`, `exp_nm_*` and their labels: "homed on <page>; Exposure shows a read-only link summary (one-home rule)"; **(f)** `exp_personal_lighting_btn`: "the Light cards hold the full PL set; Per-sky look opens the floater". Do not add a glob for an existing exact name. |
| `scripts/code_tools/director_page_sources.py` | `PAGE_OWNER_XUI["exposure_tab"]` (`:62`) = `{"floater_adjust_environment.xml", "panel_exposure_hub.xml"}`. `PAGE_OWNER_CLASSES["exposure_tab"]` (`:102`) = `{"ALPanelExposureKit"}`. `ADAPTERS["exposure_tab"]` (`:299`) = `"ALPanelExposureKit"` (fixes R20: `ALExposureKitAdapter` does not exist). Add the 5 (+2) shortcuts to its `EXPECTED_SHORTCUTS` (`:277-286`). `WRITABLE_HOMES` stays: the exposure page is already home for the five; `bloom_glow_tab` is home for bloom. Regenerate `page_sources.json`/`.md` and `pages/exposure_tab.md`. Any new unresolved row is explained in the page doc, not suppressed. |
| `scripts/code_tools/kit_lint.py` + `director_kit/gate_policy.json` | New policy key `approved_shortcuts`: a list of `{"page","row","target","citation"}`. The citation is "user approval 10/13" for image and "user approval <date>" for bloom. In `check_shortcut` (`:650-663`), the "outside an Easy card" error is skipped **only** when `(w.page, w.row, w.shortcut)` is in that list. Metadata drift is still checked. Add `load_policy` support (`:333-350`). |

### 6.3 `kit_lint` self-test (planted fixtures)

In `selftest` (`kit_lint.py:880+`):
- **`R1_K4_easy_location`** (`:980`) must still fail an unlisted non-Easy shortcut.
- **New `R1_K4_approved`**: the same tree, with the triple listed in an in-memory policy, passes.
- **New `R1_K4_approved_drift`**: a listed triple whose `min_val` differs still fails.

### 6.4 Functional manifest and self-test (`director_functional_manifest.py`, `aluiperfselftest.cpp`, `scripts/perf/functional_report.py`)

| item | rule |
|---|---|
| `exp_undo`/`exp_redo` | Names are kept, so the existing `exposure_history` probe applies (`functional_report.py:1777-1779`; C++ witness `aluiperfselftest.cpp:5499-5512`, `:7097-7108`). No change. |
| PL setting rows | Generic set_value/toggle/select with setting oracles and a snapshot restore (harness default). The 7 new setting rows get generic entries automatically. Swatches keep `opens_color_picker` (`classify`, `:725`). |
| `exposure_preset_save`, `exposure_preset_delete`, `exposure_preset_name` (strip names, `alkitpresets.cpp:23-32, 227-251`) | Add to `PERSIST_WRITE_CONTROLS` (`:185-189`): "writes the user's light_intensity_presets.xml", hazard `persistent_setting`. The harness never drives them. |
| Strip apply | Synthetic `preset_strip` entry, built like the rig_rim block (`:1100-1129`): select `b:golden_hour`; oracle `AlchemyEnvSunKelvin == 3600`; restore = snapshot of `settingNames()` (16 keys). The C++ handler resolves `exposure_preset` generically (`aluiperfselftest.cpp:2374-2380`). |
| `exp_adv_reset_all` | RUN, with pre-state `AlchemyEnvSunKelvin = 4000`, `AlchemyEnvMoonLinked = false` and `AlchemyEnvSunTintStrength = 0.5`. Oracle: all 14 keys at default and linked true. Restore: snapshot of the 15 keys. These are in-memory settings, restored; nothing is written to disk by the harness. |
| `exp_presets_btn`, `exp_sky_lighting_btn`, `exposure_popout`, `exp_filters_btn` | Launcher probes; the target floater opens and is closed after. Nothing inside it is clicked. |
| `exp_sun_note`, `exp_hint`, `exp_adv_note` | NO_EXERCISE (readouts). `exp_sun_note`: static check only (T8). |
| Summaries | Not widgets; no entries. T8 checks the static values; T10 checks "85 %". |
| Moon enable rule | Synthetic `enable_rule` entry: set linked true → `exp_moon_ev` disabled; false → enabled; restore. If no generic enable probe exists, record it as an in-world check (T11), with that reason. |
| `TRANSFORMED_VALUE_TESTS` (`:169-172`) | Unchanged (no darkness row on this page). |

**Persistence rule:** the harness never saves or deletes presets, never toggles a file-writing control, and restores every setting it touches.

## 7. Value-wiring validation table (implementer fills the last two columns)

Per the memory rule (value-wiring validation), for each control:
1. Verify the setting/type/range against the clamp, the combo values against the enum, the handler target and the read-back in code.
2. Prove it at runtime.

"Expected" below is from code reading.

| # | widget | writes | type | UI range ↔ consumer clamp | combo values | handler | read-back | runtime oracle | code-verified | runtime result |
|---|---|---|---|---|---|---|---|---|---|---|
| 1 | exp_auto_exposure | RenderDynamicExposureEnabled | Boolean | - | - | control_name | setting | toggle → false/true; shared undo +1 | | |
| 2 | exp_sun_ev | AlchemyEnvSunEV | F32 | -6..6 ↔ ±6 `alenvintensity.cpp:44-47` | - | control_name + finalizer | setting | -5.5 → -5.5; one drag = 1 step | | |
| 3 | exp_gi_ev | AlchemyEnvSkyGIEV | F32 | -6..6 ↔ sum ±6 `:258-259` | - | control_name | setting | 2.0 → 2.0 | | |
| 4 | exp_sun_note | text/visibility | - | - | - | panel (3.6) | - | ambiance 0 sky → "Legacy: Sun boost capped" visible | | |
| 5 | exp_hint | static | - | - | - | none | - | text present | | |
| 6 | exp_undo | shared history | - | - | - | `undo()` | revision signal | after witness: revision changes (existing probe) | | |
| 7 | exp_redo | shared history | - | - | - | `redo()` | revision signal | existing probe | | |
| 8 | exp_presets_btn | opens env_intensity_presets | - | - | - | action | - | floater visible | | |
| 9 | exp_moon_linked | AlchemyEnvMoonLinked | Boolean | - | - | control_name | setting + label flip | off → label "Sun EV", Moon EV enabled | | |
| 10 | exp_moon_ev | AlchemyEnvMoonEV | F32 | -6..6 ↔ ±6 `:255-257` | - | control_name | setting | 1.5 → 1.5 (when unlinked) | | |
| 11 | exp_local_include_rig | AlchemyEnvLocalLightIncludeRig | Boolean | - | - | control_name | setting | on → true | | |
| 12 | exp_local_ev | AlchemyEnvLocalLightEV | F32 | -6..6 ↔ ±6 `:321` | - | control_name | setting | -1 → -1 | | |
| 13 | exp_probe_diff_ev | AlchemyEnvGIProbeDiffuseEV | F32 | -6..6 ↔ sum ±6 `:259` | - | control_name | setting | 0.5 → 0.5 | | |
| 14 | exp_probe_spec_ev | AlchemyEnvGIProbeSpecEV | F32 | -6..6 ↔ ±6 `:260` | - | control_name | setting | 1 → 1 | | |
| 15 | exp_amb_ev | AlchemyEnvGIAmbientEV | F32 | -6..6 ↔ sum ±6 `:258` | - | control_name | setting | -0.5 → -0.5 | | |
| 16 | exp_shadow_lift | AlchemyEnvShadowLiftEV | F32 | 0..2 ↔ 0..2 `:265` | - | control_name | setting | 0.8 → 0.8 | | |
| 17 | exp_sun_kelvin | AlchemyEnvSunKelvin | F32 | 1000..15000 ↔ 1000..15000 `:82` | - | control_name | setting | 1500 → 1500; preset save keeps 1500 | | |
| 18 | exp_sun_tint | AlchemyEnvSunTintColor | Color4 | channels 0..1 `:49-53` | - | control_name, apply_now | setting | (1,0.8,0.6,1) stored; row reset → white **and** strength 1, one step | | |
| 19 | exp_sun_tint_strength | AlchemyEnvSunTintStrength | F32 | 0..1 ↔ `:185` | - | control_name | setting | 0.4 → 0.4; row reset resets the swatch too | | |
| 20 | exp_moon_tint | AlchemyEnvMoonTintColor | Color4 | 0..1 | - | as 18 | setting | as 18 | | |
| 21 | exp_moon_tint_strength | AlchemyEnvMoonTintStrength | F32 | 0..1 | - | as 19 | setting | as 19 | | |
| 22 | exp_amb_tint | AlchemyEnvAmbientTintColor | Color4 | 0..1 | - | as 18 | setting | as 18 | | |
| 23 | exp_amb_tint_strength | AlchemyEnvAmbientTintStrength | F32 | 0..1 | - | as 19 | setting | as 19 | | |
| 24 | exp_adv_reset_all | 14 keys + MoonLinked | - | - | - | panel → `resetAdvanced()` in a batch | settings | Section 6.4 oracle; one shared step | | |
| 25 | exp_adv_note | static | - | - | - | none | - | text present | | |
| 26 | exp_sky_note | static | - | - | - | none | - | text present | | |
| 27 | exp_sky_lighting_btn | opens env_adjust_snapshot | - | - | - | action | - | floater visible | | |
| 28 | summary Night mask | reads CineLightRigNightMaskEnabled | Boolean | - | - | card summary | signal | toggle on Rig page → "On" | | |
| 29 | summary Darkness | reads CineLightRigNightMaskDarkness | F32 | floor 0..1 → 100 - 100x | - | scale -100, offset 100 | signal | floor 0.15 → "85 %"; 0 → "100 %"; never written by this page | | |
| 30 | exp_exposure | RenderExposure | F32 | 0.5..4 ↔ `pipeline.cpp:13246` | - | control_name + finalizer | setting | 2.0 → 2.0; shared undo | | |
| 31 | exp_tonemap_type | AlchemyRenderTonemapType | S32 | 0..8 | Khronos 0 … S-Log3 8 (`exp_tonemap_type_N` order) | control_name | setting | item 7 → 7; AMD row enabled | | |
| 32 | exp_tonemap_mix | RenderTonemapMix | F32 | 0..1 ↔ (verify) | - | control_name | setting | 0.3 → 0.3 | | |
| 33 | exp_amd_exposure | AlchemyToneMapAMDExposure | F32 | 1..16 ↔ (verify) | - | control_name | setting | 10 → 10; disabled unless type 7 | | |
| 34 | exp_slog_ev | AlchemyToneMapSLogEV | F32 | -8..8 ↔ (verify) | - | control_name | setting | 1 → 1; disabled unless type 8 | | |
| 35 | exp_legacy_gamma_skies | AlchemyTonemapLegacyGammaSkies | Boolean | - | - | control_name | setting | on → true; sky note tooltip variant | | |
| 36 | summary Master EV | reads CineLightRigMasterEV | F32 | - | - | summary | signal | 1.25 → "1.25 EV" | | |
| 37 | summary Headroom | reads CineLightRigHeadroomStops | F32 | - | - | summary | signal | "2.00 stops" | | |
| 38 | exposure_popout | opens exposure_hub | - | - | - | action | - | floater visible | | |
| 39-42 | 4 filter summaries | read Render{GradND,Polarizer,Infrared,SoftEdge}Enabled | Boolean | - | - | summary | signal | toggle on Filters page → "On" | | |
| 43 | exp_bloom_strength | RenderBloomStrength | F32 | 0..2 ↔ (verify) | - | control_name (shortcut) | setting | 0.5 → 0.5; enabled by RenderBloomHDR | | |
| 44 | exp_bloom_threshold | RenderBloomThreshold | F32 | 0..4 ↔ (verify) | - | control_name (shortcut) | setting | 1.5 → 1.5 | | |
| 45 | exp_filters_btn | opens Lightbox colortone_settings | - | - | - | action | - | Lightbox on that panel | | |
| 46 | exposure_preset (bar combo) | the 16 keys | - | - | `b:<key>` ×14, `u:<name>` | provider apply in a batch | `matchLiveId` | `b:golden_hour` → Kelvin 3600, label "Golden Hour"; one shared step | | |
| 47 | exposure_preset_save / _name | light_intensity_presets.xml | - | - | - | `saveInteractive` | list signal | SKIP in the harness; in-world manual (T11) | | |
| 48 | exposure_preset_delete | file | - | - | - | yes/cancel → `remove` | list signal | SKIP in the harness; in-world manual | | |
| 49 | page reset (bar) | every bulk-resettable row | - | - | - | `performPageReset` in a batch | - | one shared step + one kit step; Undo restores everything | | |
| 50 | page Undo/Redo (bar), Ctrl+Z | kit history → mirror | - | - | - | 3.3 | - | after a Sun EV drag: bar Undo restores it, the floater's Redo becomes enabled | | |
| 51-55 | image_tab shortcuts (5) | the same 5 settings | as home | as home | as home | control_name | setting | each write equals its home row; one shared step per drag | | |

## 8. Search-index readiness (`SPEC_search_index.md`)

- **Labels, help and keywords.** Every row has `l`, `h` and `kw` (G-labels, 3.5). Summaries are card content and get no rows. The card title "Night mask" is the page's night-mask hit, which keeps the probe "night mask" → `exposure_tab` top 5 (`SPEC_search_index.md:361`). **Verify it when Batch A lands**; if it fails, add a feature alias rather than a fake row.
- **"kelvin" probe.** This page binds `AlchemyEnvSunKelvin`, so the pending "kelvin" probe (`:538`) arms in the same prebuild.
- **New LINK targets.** `env_intensity_presets` and `env_adjust_snapshot` are now `kit_row action`s, so link discovery finds them (`:535`; coverage G8).
- **Proposed `features` entry** for `synonyms.json` v2, once Batch A adds that key: `{"target": "light_group/exposure_tab", "aliases": ["light intensity", "personal lighting", "sun brightness", "light presets", "sun temperature"]}`. Do not add it before the key exists. Today's `synonyms.json` has `groups` only.
- **Index regeneration.** `director_kit_index.xml` is regenerated by the generator; never edit it by hand.

## 9. Work packages (sized for Sonnet/Haiku; Codex/Opus process per memory)

**Process:**
- After every C++ WP: `cxs\zs_check.ps1`, then `ui_draw_lint --changed`.
- Opus adversarial review per WP.
- A Codex review must find no major issues before the build.
- Make a checkpoint commit and an exe backup, then run **one** `cxs\fresh_build.ps1` at the end, with the viewer closed.
- Never loosen a gate. Every pin moves by exactly the delta here.

| WP | size | files (anchor by symbol) | content | depends |
|---|---|---|---|---|
| **WP1** Serials | S (~40 lines + tests) | `alsettingsundohistorymodel.{h,cpp}` (`pushUndo` :187, `Step`), `alsettingsundohistory.{h,cpp}`, `tests/alsettingsundohistorymodel_test.cpp` | 3.3 model + service accessors | - |
| **WP2** Kit undo integration | M (~120) | `alkithistory.{h,cpp}` (`beginGesture` :48, `captureSetting` :135, `finishGesture` :200), `alkitrow.{h,cpp}` (`resolveBinding` :758, reset lambdas :442/:792, Params), `alkitbinding.{h,cpp}` (`ALKitSettingBinding`), `alkitpage.cpp` (`performPageReset` :1493), `alkitcard.cpp` (reset :320, :393, paste :592), `alkitpresets.cpp` (`onSelect` :308) | 3.2, 3.3 kit side, 3.4, `reset_with` | WP1 |
| **WP3** Presets module + floater refactor | M (~150 moved, ~60 new) | `alenvintensitypresets.{h,cpp}`, `llfloaterenvironmentadjust.{h,cpp}` (table :186-225, ADV keys :132-147, `applyEnvIntensityPreset` :1244, `refreshEnvIntensityPresetCombo` :1330, reset-all :479), `alfloaterenvintensitypresets.{h,cpp}` (`onSave` :154, `onDelete` :205), `floater_adjust_environment.xml` (8 `min_val`/`max_val`) | 4.1, 4.2, 3.5 `resetAdvanced`, 2.2 ranges | - |
| **WP4** Page class, provider, strip, summary scale | M (~250) | **new** `alpanelexposurekit.{h,cpp}`, `CMakeLists.txt`, `alkitpresets.{h,cpp}` (virtuals, `postBuild`, `refresh`, `trackedSettingChanged`), `alkitcard.{h,cpp}` (`Summary`, `refreshSummary` :623), `llfloaterdirector.cpp` (bindPage :1435, `registerLookPresetProviders` :655, dead blocks :2059, :2107) | 3.6, 4.3, 4.4, 5 | WP2, WP3 |
| **WP5** Data, generator, gates | M | mockup JSON (2.3, 2.4), `kit_pages.json` (Section 1, image_tab, parity_allow), `director_kit_gen.py` (6.1), `kit_lint.py` + `gate_policy.json` (6.2, 6.3), `director_page_sources.py`, `director_parity.py`, `xui_layout_check.py`. Regenerate in this order: `director_kit_gen.py` → `director_page_sources.py` → `director_wiring_table.py` → `director_functional_manifest.py`. | generation does not need WP1-4 | - |
| **WP6** Harness + docs | S | `director_functional_manifest.py` (6.4), `aluiperfselftest.cpp` (only if the enable-rule probe needs C++), `pages/exposure_tab.md` (regenerated), audit cross-reference | 6.4 | WP5 |

**Tests.** All must pass. Report PASS, FAIL or "not run" with a reason for each (PLAN step 6).

| test | what it checks |
|---|---|
| T1 | `director_kit_gen.py --selftest` and `--check`. The Light Rig and Actors pages are unchanged apart from header hashes. |
| T2 | `kit_lint.py --all`: 0 errors and no new `help-audit-term`. `kit_lint.py --selftest` includes the 3 shortcut fixtures (6.3). |
| T3 | `director_parity.py --groups light_group,look_group`: 0 findings; every allow entry is exact and has a reason. |
| T4 | `xui_layout_check.py --selftest` (pin 12) and `--changed`: 0 findings, including `floater_adjust_environment.xml`. |
| T5 | `director_page_sources.py --check`. |
| T6 | `ui_draw_lint --changed` and zs_check (prebuild guard). |
| T7 | TUT `alsettingsundohistorymodel_test`. New cases: a fresh push gets serial+1; a batch gets one serial; a redo move keeps its serial; `clear()` keeps the counter; pending → `undoSerial()==0`. |
| T8 | A static script over `panel_dpage_light_exposure.xml`: root class `panel_exposure_kit`; `preset_provider="env_intensity"`; 9 cards and 37 rows with the Section 2 names; every `bind="code"` row has no `control_name`; the 3 swatches have `can_apply_immediately="true"`; the 6 tint rows have `reset_with` pointing at the partner; `exp_sun_note` has `visible="false"`; 8 `kit_card.summary` nodes, with darkness `scale="-100" offset="100"`; no `exp_nm_*`/`exp_gnd_*` kit_row. Plus a check that the XUI combo items 734-747 equal `builtIns()` keys and labels, in order (parse the C++ table). |
| T9 | Login functional run: every oracle in Section 7, plus: (a) a 1.5 s Sun EV drag with a 0.8 s pause is **one** shared step; (b) page reset is one shared step, and Undo restores all; (c) a Sun tint row reset sets the swatch and the strength in one step; (d) a preset applied in the floater (driven by the harness through `ALEnvIntensityPresets::applyBuiltIn`) moves the strip to that preset, and a later slider edit shows "<name> (modified)" in the strip and "Custom" in the floater; (e) Ctrl+Z after a Director drag reverts it, and the floater's Redo is enabled. |
| T10 | In-world as 5kvideoeye, auto-login only, server actions blocked. On an ambiance-0 sky, the Sky note appears. Darkness reads "85 %" at the default. Moon EV is disabled while linked. Kelvin 1500 is visibly warmer. |
| T11 | Manual preset-file test. Back up `light_intensity_presets.xml` first. Save "zz kit test" in the Director strip; it appears in the floater combo and in the Presets dialog. Delete it in the dialog; it leaves the strip. Restore and compare the hash to the backup. The floater and v1 Director Exposure hub get a click-every-control regression pass. |
| T12 | Visual hard gate. Compare a screenshot against the mockup at narrow and wide widths, with Advanced open and closed. |

## 10. Open questions (each has a recommended default; implement the default unless the user says otherwise)

- **Q1. Kept non-PL controls (Night Mask, Rig exposure, Lens filters).** Default: **read-only link summaries + Open ›**, as the mockup designs them. Live rows would need cross-page shortcuts. The Rig page writes these settings twice (Easy and Advanced), so declaring a home breaks its lint, and the change would spread into other agents' pages.
- **Q2. Bloom strength and threshold stay live on this page.** This needs two more approved cross-page shortcuts (Exposure → Look > Bloom & glow). Default: **approve**, because the mockup shows live rows. If declined, the card becomes a link card with summaries.
- **Q3. Ranges.** Your rule, applied literally, widens 7 EV sliders to the consumer clamp (±6) and Kelvin to 1000..15000. The preset store and the floater sliders widen to match. Default: **literal**. The alternative is Kelvin only, keeping every EV slider at its legacy range.
- **Q4. RLV `setenv`.** Should the Director's light controls and its Personal Lighting button obey it? Default: **no** (status quo; the old hub did not).

## Appendix A: editable converter row (only if Q1 = live rows)

- **Binding.** A new `ALKitScaledSettingBinding(setting, ctrl, scale, offset)` in `alkitbinding.{h,cpp}`, modelled on `ALKitVecBinding` (`:146-234`):
  - `get()` = `stored * scale + offset`;
  - `set(v)` writes `(v - offset) / scale`, clamped to the setting's range;
  - it connects the control's commit callback to write;
  - `reset()` resets the setting;
  - `connectChanged` watches the setting.
- **Row.** It is selected in `resolveBinding` when the row has `display_scale` (new `ALKitRow::Params` `display_scale`, `display_offset`). The row must be `bind="code"` with a `setting` attribute and no `control_name`, as the Rig darkness rows already are (`panel_dpage_light_cine_light_rig.xml:73, 855`).
- **Rig page.** It could then drop its hand-written conversion (`alpanelcinelightrig.cpp:1010-1040`) as a separate, systemic follow-up.
- **Harness.** Add the row to `TRANSFORMED_VALUE_TESTS` (`director_functional_manifest.py:169-172`) with `(1.0, 0.99)`.

## User decisions (10/15)
- Q1: LIVE controls. Night Mask, Rig exposure and Lens filters are editable on Exposure too (not read-only summaries). Use the appendix's editable converter row for Night Mask darkness (0..1 setting shown as %). The user approves the enumerated gate allowances this needs (listed by name; unlisted duplicates must still fail, with planted fixtures), plus the Rig page change.
- Q2: approve the 2 bloom shortcuts (EXPECTED_SHORTCUTS +2, enumerated).
- Q3: widen literally: 7 EV sliders to +-6 and Kelvin 1000-15000, with the preset store and floater sliders to match.
- Q4: no RLV gating.

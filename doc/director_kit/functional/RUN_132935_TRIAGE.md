# Run 132935 triage (v2 only)

Baseline: temporal `11 PASS / 1 FAIL / 1 SKIP`; cine light rig `104 / 70 / 9`; rim `41 / 6 / 1`. All 77 FAIL rows below are harness defects; no run field supports a product defect or a legitimate in-world defer.

## Evidence and verdict by group

- **G1 — combo open gesture (21, HARNESS).** Every row records the combo button as `down_handler`, `captor_ok=true`, `up_delivered_to_allowed_captor=true`, and `commits=1`; 20 also changed widget/setting during the *opening* click (for example `cine_fill_gobo` `0 -> 9`). The forced hover frame released over the captured list, whose inside-list mouse-up commits (`indra/llui/llscrolllistctrl.cpp:2065-2087`). Release outside the list before the second real click (`indra/newview/aluiperfselftest.cpp:3000-3022`).
- **G2 — popup capacity (17, HARNESS).** All rows have `combo_index >= 1`, `list_item_visible=false`, and no `scroll_before/after/max`; temporal index 1 reports list `[269,1086,419,1124]`, rim preset index 2 `[227,774,417,1152]`. The harness used configured `getPageLines()` (normally zero) instead of live `getLinesPerPage()` (`indra/llui/llscrolllistctrl.cpp:2863-2871`); fixed at `indra/newview/aluiperfselftest.cpp:3337,3391`.
- **G3 — exact pointer target (12 mismatch + 5 C/D rows, HARNESS).** The click committed and moved externally but landed on the nearest pixel: `cine_rig_rim_back_softness .50 -> .52` vs `.51`, `cine_fill_offset_z 0 -> .1999998` vs `.1`, and `cine_bg_pitch -20 -> -18.8` vs `-19`; three C sliders had identical drag endpoints/value, and `cine_bg_yaw` stayed 0 for target 1. Slider hover derives value from integer x (`indra/llui/llslider.cpp:245-271`). A real focused arrow-key nudge now finishes the declared increment (`indra/newview/aluiperfselftest.cpp:3155-3169,3273-3287`). The two darkness rows were additionally a wrong-unit oracle: observed widget `85 -> 1` and setting `.15 -> .99`; product intentionally stores `1-percent/100` (`indra/newview/alpanelcinelightrig.cpp:964-995`), corrected in `scripts/code_tools/director_functional_manifest.py:147-151`.
- **G4 — deferred layout/gesture geometry (9, HARNESS).** Four C toggles and two toggle pre-state rows have exact internal button downs but `commits=0`/unchanged values; a button commits only if the release remains inside its post-layout rect (`indra/llui/llbutton.cpp:540-578`). The three offset rows all report control `[709,772,897,796]`, point `[776,784]`, viewport `[211,616,967,1108]`, but resolver `/director_shell`, proving the selected option moved before dispatch. A second settled reveal frame is required at `indra/newview/aluiperfselftest.cpp:3882-3887`.
- **G5 — missing prerequisites (7, HARNESS).** Each row reports its own `ALKitSwitch`/slider in `enabled_break`. The owner disables per-light Actor-only under the all-lights override/object target (`indra/newview/alpanelcinelightrig.cpp:2418-2448`) and disables Fill EV/ratio state in Easy/ratio-lock modes (`indra/newview/alpanelcinelightrig.cpp:2497-2504`). The harness now restores then enables the selected light/card, exits Easy, and clears the Actor-only-all override (`indra/newview/aluiperfselftest.cpp:1622-1672`).
- **G6 — dynamic fixture oracle (2, HARNESS).** Both rows have `commits=1`, owner run 1, widget change (`5600 -> 5550`, `.10 -> .05`) but `setting.name=""`; the owner writes role-derived `CineLightRig{Role}Kelvin/SourceSizeM` (`indra/newview/alpanelcinelightrig.cpp:1246-1260`). Runtime oracle resolution is at `indra/newview/aluiperfselftest.cpp:3575-3596,3831-3835`.

## Every FAIL row

| Page | Control(s) | Group | Verdict | Evidence/code |
|---|---|---:|---|---|
| temporal | `mode_combo` | G2 | HARNESS | `aluiperfselftest.cpp:3337` |
| rim | `cine_rig_rim_back_softness` | G3 | HARNESS | `aluiperfselftest.cpp:3155` |
| rim | `cine_rimq_units`, `cine_rig_rim_tron_mode`, `cine_rig_rim_tron_color`, `cine_rimq_debug`, `preset_strip:rig_rim` | G2 | HARNESS | `aluiperfselftest.cpp:3337` |
| rig | `cine_fx_combo`, `cine_shadow_mode`, `cine_fixture_gel_0`, `cine_fixture_gel_1`, `cine_fixture_gel_2` | G1 | HARNESS | `aluiperfselftest.cpp:3000` |
| rig | `cine_key_profile`, `cine_key_gel`, `cine_key_gobo`, `cine_key_flicker` | G1 | HARNESS | `aluiperfselftest.cpp:3000` |
| rig | `cine_fill_profile`, `cine_fill_gel`, `cine_fill_gobo`, `cine_fill_flicker` | G1 | HARNESS | `aluiperfselftest.cpp:3000` |
| rig | `cine_rim_profile`, `cine_rim_gel`, `cine_rim_gobo`, `cine_rim_flicker` | G1 | HARNESS | `aluiperfselftest.cpp:3000` |
| rig | `cine_bg_profile`, `cine_bg_gel`, `cine_bg_gobo`, `cine_bg_flicker` | G1 | HARNESS | `aluiperfselftest.cpp:3000` |
| rig | `cine_easy_rim`, `cine_easy_bg`, `cine_easy_probe_target`, `cine_easing`, `cine_fixture_role`, `cine_fixture_preset` | G2 | HARNESS | `aluiperfselftest.cpp:3337` |
| rig | `cine_live_probe_target`, `cine_live_probe_refresh`, `cine_night_mask_preset`, `cine_night_mask_shape`, `cine_night_mask_target` | G2 | HARNESS | `aluiperfselftest.cpp:3337` |
| rig | `cine_easy_brightness`, `cine_easy_shaft_length`, `cine_key_pitch`, `cine_fill_yaw`, `cine_fill_pitch`, `cine_fill_offset_z` | G3 | HARNESS | `aluiperfselftest.cpp:3155,3273` |
| rig | `cine_rim_yaw`, `cine_rim_pitch`, `cine_bg_pitch`, `cine_night_mask_darkness`, `cine_easy_night_mask_darkness` | G3 | HARNESS | `aluiperfselftest.cpp:3155,3273`; `alpanelcinelightrig.cpp:964` |
| rig | `cine_easy_cone_width`, `cine_easy_probe_ambiance`, `cine_night_mask_height` | G3 | HARNESS | `aluiperfselftest.cpp:3155` |
| rig | `cine_easy_flare_preset`, `cine_easy_night_mask_preset`, `cine_track_mode`, `cine_flare_preset` | G2 | HARNESS | `aluiperfselftest.cpp:3337` |
| rig | `cine_gizmo`, `cine_random_cycle`, `cine_manual`, `cine_bounce` | G4 | HARNESS | `aluiperfselftest.cpp:3887` |
| rig | `cine_catchlight_actor_only`, `cine_key_actor_only`, `cine_ratio`, `cine_ratio_lock`, `cine_fill_actor_only`, `cine_rim_actor_only`, `cine_bg_actor_only` | G5 | HARNESS | `aluiperfselftest.cpp:1622` |
| rig | `cine_key_offset_z`, `cine_rim_offset_z`, `cine_bg_offset_z` | G4 | HARNESS | `aluiperfselftest.cpp:3887` |
| rig | `cine_mirror`, `cine_key_shaft` | G4 | HARNESS | `aluiperfselftest.cpp:3887` |
| rig | `cine_key_yaw`, `cine_bg_yaw` | G3 | HARNESS | `aluiperfselftest.cpp:3273` |
| rig | `cine_fixture_kelvin`, `cine_fixture_source_size` | G6 | HARNESS | `aluiperfselftest.cpp:3575` |

## Static next-run prediction

| Page | PASS | FAIL | SKIP | Confidence |
|---|---:|---:|---:|---|
| `temporal_tab` | 12 | 0 | 1 | high (single defect, live-capacity fix) |
| `cine_light_rig_tab` | 174 | 0 | 9 | medium-high; combo capture and idle-settle paths are code-proven, but this page has the broadest state interactions |
| `light_rim_tab` | 47 | 0 | 1 | high |

No product fix was applied. Assertions, foreign modal/captor/menu checks, focus guards, input reset, and user-data restoration remain intact.

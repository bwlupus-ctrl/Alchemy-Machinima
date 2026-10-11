# Run 143205 — v2 Light Rig / Rim triage

Source run: `doc/director_kit/run_functional_20261011_143205/functional.csv` (Rig 143/31, Rim 43/4; Temporal 12/0). This round changes only the functional harness and manifest generation. No product implementation was changed.

## Root causes and fixes

1. **Toggle pre-state was UI-only.** `LLCheckBoxCtrl::setValue()` writes its child button, while `setControlName()` binds that child; calling the outer checkbox's `onCommit()` therefore cannot establish the bound setting (`indra/llui/llcheckboxctrl.cpp:159-165,238-246,280-283`). The harness now writes the already-validated `expect.setting` control and verifies both setting and widget before the click (`indra/newview/aluiperfselftest.cpp:3888-3927`). This is why the 143205 `cine_enabled` JSON could say `prestate=false` while `CineLightRigEnabled` stayed true.
2. **Switch clicks used the drawn track midpoint.** The recorded C rows landed at x=17 in a 34 px switch, while the visible knob centres are x=9/x=25 (`indra/newview/alkitswitch.cpp:51-57`). The click target remains the real child `LLButton`, but its point is now the visible knob (`indra/newview/aluiperfselftest.cpp:2894-2904`); the shipping button writes its bound control in `setToggleState()` (`indra/llui/llbutton.cpp:1071-1077`).
3. **Fixed combos were redirected away from their manifest target.** If the target was already selected, the harness chose any alternate (`functional.csv:36,54` observed index/value 0). Alternate selection is now restricted to runtime-resolved action combos, while fixed combos get an alternate pre-state and still click the declared item (`indra/newview/aluiperfselftest.cpp:3402-3420,3928-3947`).
4. **Code-owned controls had the wrong oracle/state lifetime.** Easy controls now assert the settings their handlers write; fixture fields assert the selected role's dynamic setting; fixture-role restoration commits so `mFixtureRoleIndex` cannot leak into later rows (`scripts/code_tools/director_functional_manifest.py:709-731`, `indra/newview/aluiperfselftest.cpp:2443-2454,3668-3687`).
5. **Derived gates were applied at the wrong time.** Per-light Actor-only is gated by object targeting or the all-lights master (`indra/newview/alpanelcinelightrig.cpp:2418-2448`), so the prerequisite is repeated after lazy-page creation (`indra/newview/aluiperfselftest.cpp:2077-2090`). Tron controls are shown only when `CineRigRimEnabled` reaches `tickRefresh()` (`indra/newview/alrigrimcontroller.cpp:408-417`), so that master is established before page reveal (`indra/newview/aluiperfselftest.cpp:1622-1637`; manifest enrichment at `scripts/perf/functional_report.py:397-401`).

## Row verdicts

| Observed row | Verdict | Code diagnosis and real oracle |
|---|---|---|
| `cine_easy_rim` (`functional.csv:8`) | HARNESS | `onEasyRimCommit()` writes `CineLightRigRimOn` and `CineLightRigRimEV` (`alpanelcinelightrig.cpp:2195-2210`); oracle is Rim EV = -2.5. |
| `cine_easy_bg` (`functional.csv:9`) | HARNESS | `onEasyBgCommit()` writes `CineLightRigBgOn` and `CineLightRigBgEV` (`alpanelcinelightrig.cpp:2213-2228`); oracle is BG On = true. |
| `cine_easy_cone_width` (`functional.csv:10`) | HARNESS | Target equalled the widget, so the zero-length drag committed nothing; alternate slider pre-state is at `aluiperfselftest.cpp:3948-3960`. Handler writes all four Beam settings (`alpanelcinelightrig.cpp:2244-2259`); oracle is Key Beam = 0. |
| `cine_easy_flare_preset` (`functional.csv:13`) | HARNESS | Apply-and-reset action writes the flare table plus `RenderCineLensFlarePreset`, then returns both combos to sentinel 0 (`alpanelcinelightrig.cpp:1734-1763`); oracle is preset = 1. |
| `cine_easy_night_mask_preset` (`functional.csv:16`) | HARNESS | Apply-and-reset action stamps six Night Mask settings then returns both combos to sentinel 0 (`alpanelcinelightrig.cpp:1766-1794`, `396-403`); oracle is Shape = 1. |
| `cine_gizmo` (`functional.csv:22`) | HARNESS | Bound `ALKitSwitch`; real knob click and setting delta are the oracle (`panel_dpage_light_cine_light_rig.xml:118-119`, `aluiperfselftest.cpp:2894-2904`). |
| `cine_track_mode` (`functional.csv:36`) | HARNESS | Manifest target 1 was pre-state 1, but old combo code clicked alternate index 0; fixed-combo pre-state/click fix at `aluiperfselftest.cpp:3402-3420,3928-3947`. |
| `cine_random_cycle` (`functional.csv:39`) | HARNESS | Bound switch (`panel_dpage_light_cine_light_rig.xml:191-192`); same knob gesture fix. |
| `cine_manual` (`functional.csv:45`) | HARNESS | Bound switch plus `onManualCommit()` (`alpanelcinelightrig.cpp:839-840,2289-2294`); same knob gesture fix. |
| `cine_mirror` (`functional.csv:48`) | HARNESS | JSON's widget false / setting true is the broken pre-state signature; binding is `CineLightRigMirror` (`panel_dpage_light_cine_light_rig.xml:511-512`). |
| `cine_shadow_mode` (`functional.csv:54`) | HARNESS | Manifest target 2 was pre-state 2, but old combo code clicked alternate index 0; fixed as for Track mode (`aluiperfselftest.cpp:3402-3420,3928-3947`). |
| `cine_bounce` (`functional.csv:57`) | HARNESS | Bound switch (`panel_dpage_light_cine_light_rig.xml:549-550`); adjacent `cine_bounce_lights_set` passed the same product binding path, and knob gesture now uses the visible affordance. |
| `cine_actor_only_all` (`functional.csv:60`) | HARNESS | Setting was already true because pre-state did not write it; real handler also writes five per-light/catchlight settings (`alpanelcinelightrig.cpp:2452-2464`). Oracle remains `CineLightRigActorOnlyAll`. |
| `cine_catchlight_actor_only` (`functional.csv:64`) | HARNESS | Setting was already true while widget was forced false; corrected bound-setting pre-state. Binding is explicit at `panel_dpage_light_cine_light_rig.xml:572-573`. |
| `cine_fixture_role` (`functional.csv:65`) | HARNESS | Handler changes `mFixtureRoleIndex` and refreshes fixture/gobo editors (`alpanelcinelightrig.cpp:1206-1213`). A distinct Fill Kelvin witness proves the refresh; committed restoration prevents role leakage (`director_functional_manifest.py:724-730`, `aluiperfselftest.cpp:2443-2454`). |
| `cine_fixture_mode` (`functional.csv:66`) | HARNESS | Writes `<selected role>FixtureMode` (`alpanelcinelightrig.cpp:1215-1221`); dynamic selected-role oracle at `aluiperfselftest.cpp:3668-3687`. |
| `cine_fixture_preset` (`functional.csv:67`) | HARNESS | Writes selected role Mode, Preset, Kelvin, SourceSize, Beam, and three GelSlots (`alpanelcinelightrig.cpp:1223-1243`); oracle is selected-role FixturePreset. |
| `cine_fixture_kelvin` (`functional.csv:68`) | HARNESS | Writes selected-role Kelvin (and current Source/Gels) (`alpanelcinelightrig.cpp:1246-1261`); old stale role made the harness inspect Key while the handler addressed Fill. |
| `cine_fixture_source_size` (`functional.csv:69`) | HARNESS | Same handler; oracle is selected-role SourceSizeM (`alpanelcinelightrig.cpp:1246-1261`). |
| `cine_fixture_gel_0` (`functional.csv:70`) | HARNESS | Same handler; oracle is selected-role GelSlot0 (`alpanelcinelightrig.cpp:1255-1261`). |
| `cine_fixture_gel_1` (`functional.csv:71`) | HARNESS | Same handler; oracle is selected-role GelSlot1 (`alpanelcinelightrig.cpp:1255-1261`). |
| `cine_fixture_gel_2` (`functional.csv:72`) | HARNESS | Same handler; oracle is selected-role GelSlot2 (`alpanelcinelightrig.cpp:1255-1261`). |
| `cine_rig_rim_include_probes` (`functional.csv:122`) | HARNESS | Bound switch (`panel_dpage_light_rim.xml:37-38`); D failure was the UI-only pre-state. |
| `cine_rig_rim_tron_mix` (`functional.csv:137`) | HARNESS | Visibility is refreshed from `CineRigRimEnabled` (`alrigrimcontroller.cpp:408-417`); prerequisite now precedes lazy-page reveal (`aluiperfselftest.cpp:1622-1637`). |
| `cine_key_shaft` (`functional.csv:160`) | HARNESS | Not a product gap: `postBuild()` binds the control to `selected().setShaftEnabled()` (`alpanelcinelightrig.cpp:841-856`), and enrichment supplies `selected_rig_light_shaft` (`functional_report.py:493-500`). Knob click now reaches that handler. |
| `cine_ratio_lock` (`functional.csv:169`) | HARNESS | JSON's widget false / setting true is the broken pre-state signature; explicit binding at `panel_dpage_light_cine_light_rig.xml:312-313`. |
| `cine_fill_actor_only` (`functional.csv:183`) | HARNESS | Disabled by `CineLightRigActorOnlyAll` or object-target gate (`alpanelcinelightrig.cpp:2418-2448`); gate is re-established after page load (`aluiperfselftest.cpp:2077-2090`). |
| `cine_rim_actor_only` (`functional.csv:197`) | HARNESS | Same derived gate and fix (`alpanelcinelightrig.cpp:2418-2448`, `aluiperfselftest.cpp:2077-2090`). |
| `cine_bg_actor_only` (`functional.csv:211`) | HARNESS | Same derived gate and fix (`alpanelcinelightrig.cpp:2418-2448`, `aluiperfselftest.cpp:2077-2090`). |
| `cine_enabled` (`functional.csv:228`) | HARNESS | Page constructs and binds the master switch to `CineLightRigEnabled` (`alkitpage.cpp:454-475`); the failed programmatic pre-state never touched that child binding. |
| `cine_easy_probe_enable` (`functional.csv:229`) | HARNESS | Card-created switch binds `CineLightRigLiveProbeEnabled` (`alkitcard.cpp:263-273`); corrected setting pre-state. |
| `cine_bg_on` (`functional.csv:233`) | HARNESS | Option-created switch binds its `enable_setting` (`alkitcard.cpp:276-290`; XUI declaration `panel_dpage_light_cine_light_rig.xml:443`); corrected setting pre-state. |
| `cine_live_probe_enable` (`functional.csv:235`) | HARNESS | Card-created switch binding (`alkitcard.cpp:263-273`; card declaration `panel_dpage_light_cine_light_rig.xml:731`); corrected setting pre-state. |
| `cine_rig_rim_enable` (`functional.csv:241`) | HARNESS | Page master is generated and bound by `ALKitPage` (`alkitpage.cpp:454-475`; setting declared at `panel_dpage_light_rim.xml:4`); corrected setting pre-state. |
| `cine_rimq_wrap_enable` (`functional.csv:242`) | HARNESS | Card enable switch is generated/bound by `ALKitCard` (`alkitcard.cpp:263-273`; setting declared at `panel_dpage_light_rim.xml:151`); corrected setting pre-state. |

## Product bugs

None found in these 35 rows. Each affected product path has a concrete binding or handler and a real setting/model effect; therefore no product fix was applied and there is no minimal product patch to propose.

## Static verification

- Manifest generation and R5 enrichment completed with 0 unresolved owner gates and 57 consumers.
- `ui_draw_lint.py --changed HEAD --json`: `new_errors: 0`, baseline unchanged.
- Python syntax compilation and `git diff --check` passed. Per instruction, no viewer build or functional rerun was performed.

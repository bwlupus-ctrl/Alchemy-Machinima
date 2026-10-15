# Director v2 coverage and search audit (2026-10-15)

Read-only audit. Scope: Director v2 only (`floater_director_v2.xml`). The v1 redesign and the shelved Equirect 360 video mode are out of scope. Light > Actors is being converted to kit by another agent at the time of writing, so its row counts reflect the in-progress tree.

Paths are relative to the repo root. XUI paths are under `indra/newview/skins/default/xui/en/`. When something could not be confirmed from source alone it is marked **unverified**.

## 0. How this was measured

The figures come from scripts run against the working tree. Nothing in the repo was changed except this file.

1. **Pages.** There are 36 lazy pages in `floater_director_v2.xml:36-110`. The classification comes from `scripts/code_tools/director_kit/gen_report.txt` (`classify()` at `scripts/code_tools/director_kit_gen.py:174`). The counts are 23 kit (22 "kit (P0)" plus 1 "pure kit"), 13 "legacy" and the Quick proxy page.
2. **Reachable settings.** The audit walked the v2 floater, every `page_file` and every `filename=` include, and collected every attribute value that names a setting (`control_name`, `setting`, `enable_setting`, `mode_setting`, `watch_settings`, `master_setting` and so on). That set was compared with every setting key in `app_settings/settings.xml`, `settings_alchemy.xml` and `settings_per_account.xml`, and with the controls of each custom floater registered in `llviewerfloaterreg.cpp`.
3. **Search.** The audit parsed `director_kit_index.xml`, which has 4,252 entries: 2,059 Director entries and 2,197 "external" entries. Each page's index rows were diffed against the named controls actually present in that page's XUI tree. The scorer was then ported to Python (`alkitsearch.cpp:190-250`, `aldirectorqol.cpp:51-78`) and about 60 feature-name queries were replayed through it.
4. **Code-bound settings.** Settings bound only in C++ (for example `alpanellensgaze.cpp` and `alghoststudio.cpp`) were traced with grep. Where only C++ binds a setting, the setting is counted as covered when its panel is embedded in the Director.

## 1. Feature coverage table

**Status key:**
- **KIT**: the feature is a generated kit page.
- **LEGACY-EMBED**: an old panel is hosted inside a Director page.
- **LINK**: the Director only has a button that opens another floater.
- **PARTIAL**: some controls are in the Director and others are missing.
- **MISSING**: the feature cannot be reached from Director v2.
- **SETTINGS-ONLY**: the feature has no UI anywhere.

**Searchable key:**
- **Y**: typing the feature name or its main controls finds a Director row.
- **Ext**: search finds it only as an "Opens in <floater>" external hit.
- **Partial**: some of its controls are found and others are not.
- **N**: search does not find it.

| # | Feature | What it does | Current entry points | v2 status | Searchable? | Gap / recommendation |
|---|---|---|---|---|---|---|
| 1 | Director shell (ACTION/CUT transport, arm popover, scenes, marks, cast roster, hotkeys legend) | Runs the take: what ACTION starts, the countdown, scene save/load, marks, cast | `floater_director_v2.xml:112-195` | KIT (shell) | **N**: no index rows for `DirectorArmMoves`, `DirectorActionDelay`, `btn_scene_*`, `show_paths_global_check` and similar | Emit shell index rows (batch A2) |
| 2 | Actor Mover (straight walk, groups, custom walk anim) | Walks cast members | Machinima > Actor Mover (`menu_viewer.xml:220`); Cast > Move | LEGACY-EMBED (`panel_actor_mover.xml`) | Partial: 19 mockup rows; `custom_anim_check` (`panel_actor_mover.xml:251`) not indexed | Batch A1; later C5 |
| 3 | Character Mover Path V2 / Path editor (nodes, primitives, path camera, sync to take, follow leader, onion skin) | Authored walk paths and camera on path | Actor Mover floater; Cast > Path | LEGACY-EMBED (`panel_path_editor.xml`) | Partial: 38 of 94 controls not indexed, including `edit_mode_check` (:31), `onion_skin_check` (:83), the path camera enable (:194) and the sync-take enable (:282) plus 34 reset buttons. "onion skin" returns 0 Director hits | A1; C5 |
| 4 | Prop Mover + rotation (orientation, oscillation, spin, shuttle, hover, rock) | Moves and animates props, carrying seated avatars | Machinima > Prop Mover (`menu_viewer.xml:227`); Cast > Props | LEGACY-EMBED (`panel_prop_mover.xml`) | Partial: the master toggles `hover_on_check` (:313), `rock_on_check` (:371) and `spin_on_check` (:446) are not indexed | A1 |
| 5 | Ghost Studio (clones, crowds, formations, true mirror, chaos, ghost look, lens gaze) | Clone and crowd authoring | Machinima > Ghost Studio (`menu_viewer.xml:234`); Cast > Ghosts | LEGACY-EMBED (`panel_ghost_studio.xml`, which has an inner `tab_container`) | Partial: 28 of 143 controls not indexed, including `chaos_check` (:225), `autoanim_check` (:264) and `lens_gaze_check` (:276). "true mirror" has no good hit | A1 + A8 (inner-tab jump) |
| 6 | Ghost AutoAnimate | Clones learn and keep playing the source's performance | Ghosts page | LEGACY-EMBED | **Ext only**: "autoanimate" returns 0 Director hits and 1 Ghost Studio external hit | A1 |
| 7 | Dance Ball + Blocking | Client-side forced dances, local blocking of other avatars | Machinima > Dance Ball (`menu_viewer.xml:241`); Cast > Dance plus a pop-out button | LEGACY-EMBED (`panel_dance_ball.xml`, inner tabs) + LINK | Partial: `db_auto_advance` (:573), `db_blocking_on` (:880) and `db_wave_enable` (:909) are not indexed. The top "blocking" hit is the external floater | A1 + A8 |
| 8 | **Client-side avatar height + hover height** | Render-only local height offset for any avatar; own hover height | Avatar > Hover height (`menu_viewer.xml:462`); avatar right-click > Local Height (`menu_avatar_other.xml:212`, `alviewermenu.cpp:1057,1348`); `floater_edit_hover_height.xml:85,134` | **MISSING** | **N**: "client height", "local height" and "hover height" give only fuzzy noise | Batch B2: "Height" card on Cast > Move, acting on the selected cast member |
| 9 | Animate (anim preview, Animation Explorer) | Plays and auditions animations on cast | Machinima > Animation Explorer (`menu_viewer.xml:272`); Performance > Animate | LEGACY-EMBED + LINK (`btn_open_animexp`) | Y | C6 later |
| 10 | Switchboard (unified pose/anim switchboard, freeze) | Slot banks of poses and anims | Performance > Switchboard | LEGACY-EMBED (`panel_director_anim_switcher.xml`) | Y (32 rows, 0 dead) | C6 later |
| 11 | Actor Gaze (look-at camera, movement style, near-lens, persona, limits, lean, IK) | Eye, head and torso gaze performance | Machinima > Actor Gaze (`menu_viewer.xml:251`); Performance > Gaze; `btn_actor_gaze` | LEGACY-EMBED (`panel_lens_gaze.xml`; the floater has 132 of 132 of the same controls) + LINK | Partial: `look_at_camera_enabled` (:28) and `gaze_performance_combo` (:233) are not indexed. The `actor_gaze` floater is **not** in `external_search.json`. The global `DirectorGaze{Valence,Arousal,Dominance}` defaults are read only in `llactormover.cpp:7412` (settings-only; whether they are meant to be user-facing is **unverified**) | A1, A5 |
| 12 | Pose Polish | Inertialization, foot IK, loop seams | Machinima > Pose Polish (`menu_viewer.xml:265`); Performance > Polish | LEGACY-EMBED (`panel_pose_polish.xml`) + LINK | Partial: `polish_master_check` (:10) is not indexed. The page master `ALPolishEnabled` has no Director row | A1, A2 |
| 13 | Gaze Cues | Keyframed gaze cue list and transport | Machinima > Gaze Cues (`menu_viewer.xml:258`); Performance > Gaze Cues | **LINK**: the page is one "Open Gaze Cues..." card | Ext: the floater is indexed (29 rows), but the 5 Director rows (`cue_list`, `btn_cue_add`...) are **dead**, because those widgets do not exist in the page | C3; meanwhile A6 |
| 14 | **Poser (BD/FS) + pose stand** | Joint-level posing | Machinima > Poser (`menu_viewer.xml:283`); `llviewerfloaterreg.cpp:914-915` | **MISSING** | N | B6: decide with the user (it is an upstream port); at least a LINK plus external index |
| 15 | Cinematic Camera (modes, moves, auto switcher, cut transitions, world slow-mo) | Automated camera moves on subjects A to D | Machinima > Cinematic Camera (`menu_viewer.xml:297`); Camera > Shot and Moves | KIT (all 323 floater settings are present) | Partial: the card master switches `CinematicCamEnabled` (`panel_dpage_camera_shot.xml:958`), `DirectorSwitcherArmed` (:1046), `DirectorSwitcherEaseCuts` (:1281) and `DirectorSwitcherEaseFreeze` (:1343) are not indexed | A2 |
| 16 | Camera Operator (handheld, shake, rigs) | Handheld operator feel | Camera > Operator | KIT | Partial: **"camera shake" finds nothing relevant**, because the card switch `CameraShakeApplyActive` (`panel_dpage_camera_operator.xml:9`) is not indexed and "camera" appears in none of the shake rows | A2 + synonyms |
| 17 | Frame & Orbit (auto-reframe, frame guides/aspect, flycam orbit) | Framing and orbit | Machinima > Flycam Orbit (`menu_viewer.xml:304`); Camera > Frame & Orbit | KIT | Partial: switches `CinematicAutoFrameEnabled` (:51) and `FlycamOrbitEnabled` (:148) are not indexed. Code-bound `CinematicFrameAspectRatio` is **unverified** | A2 |
| 18 | Flycam Recorder / Takes | Records and plays camera takes | Machinima > Flycam Recorder (`menu_viewer.xml:311`); Camera > Takes | KIT | Y | none |
| 19 | VCam / Prism (lens, feeds, monitors, screen FX, scanlines, bone POV, gate) | Virtual cameras and displays | Machinima > VCam (`menu_viewer.xml:328`); Camera > VCam | KIT + LINK (`vc_open_manager`) | Y. `PrismProtectedMainFPS` and `PrismCaptureRefreshCeilingHz` live only in the manager (Ext). 1 dead row: `vc_monitor_tile_manifest` | Minor |
| 20 | **Flight model (Drone, Weighted Classic, trigger momentum) + XInput gamepad + axis/button mapping** | Flycam physics and controller | Machinima > Flight Model & Gamepad (`menu_viewer.xml:318`); `floater_joystick.xml:13,142,628,690`; 108 settings (for example `FlycamDroneCruise` at `settings.xml:13221` and `JoystickXInputEnabled` at :14212) | **MISSING** (5 of 113 floater settings reachable, all via Operator) | **N**: "gamepad" 0 hits; "xbox", "drone" and "joystick" give only fuzzy noise. `pref_joystick` is not in `external_search.json` | B1: new Camera > "Flycam" page |
| 21 | DoF / focus lock / crosshair / composition guide | Depth of field and focus | Machinima > Focus Lock and Crosshair (`menu_viewer.xml:349,361`); Lens > Focus | KIT | Y. The card switches `RenderDepthOfField` (`panel_dpage_lens_focus.xml:7`) and `RenderCompositionGuide` (:58) are not indexed | A2 |
| 22 | Lens filters (Grad ND, Polarizer, Infrared/Aerochrome, Soft edge) | On-lens filters | Lens > Filters; duplicated in the Exposure hub | KIT | Y through card and row names. Four card switches are not indexed (`panel_dpage_lens_filters.xml:8,69,142,174`) | A2 |
| 23 | Ultimate Diopter + Kaleidoscope | Lens-front diopter, halo and kaleidoscope | Machinima > Ultimate Diopter (`menu_viewer.xml:335`); Lens > Diopter (5 rows) + "Full editor" LINK | **PARTIAL**: 6 of 184 settings in the Director; 178 only in the floater | Ext: the full editor is indexed (377 rows). "kaleidoscope" scores 145 on unrelated rows, because the tool-mode combo **item labels are not indexed**, and the same applies to presets such as "Heart Bokeh" | A4; C4 (decide whether the full editor stays a LINK) |
| 24 | Cine Light Rig (Easy/Manual, looks, gobo, catchlight, night mask, live probe, instances, setups) | Cinematic lighting | Machinima > Cinematic Light Rig (`menu_viewer.xml:375`); Light > Rig | KIT (P0 hosting v1 nodes; all 161 floater settings present) | Y. The setups trio (`cine_setup_combo`, `cine_setup_save`, `cine_setup_delete`) at `panel_dpage_light_cine_light_rig.xml:916` is not indexed, and neither is the page master `CineLightRigEnabled` | A2 |
| 25 | Rig Rim Light + quality + presets | Per-pixel rim on rig lights | Light > Rim | KIT | Y (the `CineRigRimWrapEnabled` switch is not indexed) | A2 |
| 26 | Exposure hub (exposure, tonemap, Light Intensity EVs, Night Mask, lens filters, undo/redo) | Global exposure | Machinima > Exposure (`menu_viewer.xml:396`); Light > Exposure | LEGACY-EMBED (`panel_exposure_hub.xml`, inner tabs) | Partial: **39 of 63 controls are not indexed**, including GND/Pol/IR/Soft-edge/Night-mask toggles and presets (`panel_exposure_hub.xml:76,102,106`), rig Master EV (:56), undo/redo (:3-4) and both Personal Lighting buttons | C1 (convert to kit); meanwhile A1 |
| 27 | Light Intensity v2 (Sun Kelvin, Sun/Moon/Ambient tints) + user presets | Scene light colour and presets | Machinima > Personal Lighting (`menu_viewer.xml:403`) and Light Intensity Presets (:413); `floater_adjust_environment.xml:1097,1143`; `floater_env_intensity_presets.xml` | **PARTIAL**: EVs are in the Exposure hub; 7 Kelvin/tint settings and the presets floater are not in the Director. The only path is the unindexed `exp_personal_lighting_btn` (`panel_exposure_hub.xml:52`, which opens `env_adjust_snapshot`) | N: "kelvin" finds rig fixture Kelvin and "light intensity" finds Tron Intensity. `env_adjust_snapshot` and `env_intensity_presets` are not in `external_search.json` | B4 (inside C1) |
| 28 | Practicals / Selected Light (gobo, volumetric, cast shadows on any in-world light) | Edits the selected SL light | Light > Practicals (one "Open Selected Light..." card); Lightbox `sellight_settings` (`floater_lightbox_settings.xml:6345`) | **LINK** | Ext (14 rows). The 16 Director rows (`sl_gobo`, `sl_capture`...) are **dead**. "practicals" (the page name) finds nothing, because page labels are not tokenized | C2; A3, A6 |
| 29 | Avatar Light Response + Actor Mask + Silhouette | Per-avatar light response, actor-only lights | Machinima > Avatar Light Response (`menu_viewer.xml:389`); Light > Actors | KIT (**conversion in progress**) + LINK (`open_lightbox_actormask`) | Y. In-progress rows `combo_preset` and `btn_apply_preset` (`panel_dpage_light_actors.xml:151,164`) are not yet indexed | Let the converting agent finish; then A-gate |
| 30 | Lighting Cue Stack | Lighting cues | Machinima > Lighting Cue Stack (`menu_viewer.xml:382`); Light > Light Cues; Rig "Cue stack..." | **LINK** | Ext (21 rows). The 5 Director rows are **dead** | C3; A6 |
| 31 | Light class toggles (world, projector and attached lights), Stable spot shadows, Global light scale | Light culling and safety switches for shoots | Lightbox Avatar tab (`floater_lightbox_settings.xml:4855`) and Rendering tab | **LINK** (Lightbox, no Director button for these tabs) | Ext ("light toggles" opens the Lightbox) | B8 |
| 32 | Image (tonemappers, grade, LUT, Khronos, AMD LPM, S-Log3, vignette, grain, CA) | Final image look | Look > Image | KIT | Y | none |
| 33 | Bloom & Glow | Bloom | Look > Bloom & Glow | KIT | Y | none |
| 34 | Roto Ink (Cine Outline) | Outline and ink styles | Look > Roto Ink | KIT | Y | none |
| 35 | Tron (world pass, traces, trails, suit) | Tron look | Look > Tron | KIT | Y. `TronTrailTarget` is in the Lightbox only (`floater_lightbox_settings.xml:6175`) | Small add to Tron |
| 36 | Stepped animation (on twos/threes, particles, texture anim) | Stop-motion cadence | Look > Stepped | KIT | Y. The page master `StepAnimEnabled` has no Director row | A2 |
| 37 | Actor FX | Per-actor looks (shimmer, glitch, distortion) | Look > Actor FX | KIT | Y | none |
| 38 | Weather (rain, splash, rain shelter, lightning, cinematic lightning) | Client weather | Look/World > Weather | KIT | Partial: 5 card switches are not indexed (`panel_dpage_world_weather.xml:15,77,135,165,215`); "lightning" only finds rows inside the card. Rain/lightning colour, seed and trigger are settings-only | A2 |
| 39 | Cine Haze, projector cones/volumetrics, dust, fog volumes, soft projector shadows | Atmosphere | World > Atmosphere | KIT | Partial: the switches at `panel_dpage_world_atmosphere.xml:103,191,224` are not indexed. Projector rim and bloom-feed are only in Lightbox "Froxel Air (legacy)" (froxel is excluded because the user rejected it) | A2 |
| 40 | Shadow distance control (sharp range, softness, follow subject, alpha caster cap, bias/resolution) | Shadows | World > Shadows | KIT | Y | none |
| 41 | Reflection probe refresh (on-demand, sun-shadow mode, capture scale, update rate) | Probe cost and quality | Lightbox Rendering (`floater_lightbox_settings.xml:1508`); Rig > Live probe | **PARTIAL**: the page is internally `shadows_probes_tab` but has no probe rows; 12 `RenderProbe*` settings are Lightbox-only | Ext only ("probe on demand" gives 0 Director and 2 external hits) | B7 |
| 42 | **Custom water level (visual, session-only)** | Raises or lowers the rendered water | Personal Lighting water floater (`floater_adjust_water.xml:302-310`, `settings.xml:22442`) | **MISSING** | N ("water level" gives only noise) | B3: World > Weather or Atmosphere card |
| 43 | Capture-mode render switches: Machinima High LOD, pin scene textures, near clip, forced alpha mask | Shoot-quality overrides | Lightbox Rendering/Machinima (`floater_lightbox_settings.xml:2006,4622,4632,4680`) | **LINK** (Lightbox only, no Director button) | Ext | B5 |
| 44 | **Auto cinematic capture mode** (`BDMergeCineModeAuto` + On-Action/On-Flycam/On-CineCam triggers) | Hides the UI while shooting | Preferences > Graphics only (`panel_preferences_graphics1.xml:387`) | **MISSING** | N | B5 |
| 45 | Temporal capture / world clock | Slow-mo and world time for capture | Machinima > Temporal Capture (`menu_viewer.xml:433`); Capture > Time | KIT | Y | none |
| 46 | Scene options + Snapshot | Scene file, hotkeys, auto-cast-self; snapshot | Capture > Scene (4 rows) + snapshot card LINK | KIT + LINK | Y / Ext | none |
| 47 | Phototools (BD port) | DoF and framing helpers | Machinima > Phototools (`menu_viewer.xml:342`) | covered: all 9 floater settings are already on Director pages, with no launcher | Y via those rows | none, or add a LINK for parity |
| 48 | Motion vectors / motion blur (`BDMergeMotionBlur`, `BDMergeVelocity*`) | Velocity buffer and blur | none (`settings.xml:9859`; `pipeline.cpp:1577`) | **SETTINGS-ONLY** | N | B9 (ask the user whether it should be exposed) |
| 49 | ReShade bridge (cut detect, history reset, depth) | ReShade integration | none (`llreshadebridge.cpp:92`) | **SETTINGS-ONLY** | N | B9 |
| 50 | Hide look-at beacons (`BDMergeHideLookAt`) | Hides look-at targets in shots | none (`llhudeffectlookat.cpp:422`) | **SETTINGS-ONLY** | N | B9 |

Not included: the Avatar Staging engine (dormant), the Actor Chroma Key (design doc only, no settings), the Star filter (idea), Planned Shots / Shot Builder (`doc/SHOT_BUILDER_PLANNED_SHOTS_DESIGN.md`: "Design proposal"), PTVL native volumetrics (still being designed), the froxel fog (rejected by the user), the Scene Explorer (upstream Alchemy, 2026-06-09), and the VRAM governor / texture and mesh pools (performance plumbing, not creative controls).

### Counts

| Status | Count | Rows |
|---|---:|---|
| KIT | 22 | 1, 15-19, 21, 22, 24, 25, 29, 32-40, 45, 46 |
| LEGACY-EMBED | 11 | 2-7, 9-12, 26 |
| LINK | 5 | 13, 28, 30, 31, 43 |
| PARTIAL | 3 | 23, 27, 41 |
| MISSING | 5 | 8, 14, 20, 42, 44 |
| SETTINGS-ONLY | 3 | 48-50 |

Row 47 (Phototools) is covered through other pages and is not counted. KIT here means the feature's settings are reachable; most KIT pages still have unindexed master or card switches (section 3).

## 2. How Director v2 search works today

**Index format.** `director_kit_index.xml` is a CDATA payload of tab-separated lines with 22 fields (`alkitsearch.cpp:17,77-120`). It is generated by `index_bytes()` (`director_kit_gen.py:948`) from two sources:
- **Director rows.** These come from `director_mockup_data.json` cards, rows and opts rows for every page in `flatten_pages()`, legacy pages included (flag `x`, `:974`). The fields are widget, label, kind, setting, keywords (`kw`), help (`h`) and synonym aliases (`synonym_aliases()`, `:893`, which reads `synonyms.json`, 22 groups).
- **External rows.** `external_index_rows()` (`:913`) walks each floater listed in `external_search.json` (13 floaters) and every `filename=` include. It emits one row per named control tag or `control_name` node, with tab context. When a control has no label, the label falls back to the widget name (`:921`).

**Loading and scoring.** `ALKitSearch::tokenizeBatch` tokenizes these fields: label, card title, keywords, help, setting, widget and synonyms (`alkitsearch.cpp:147-153`). **It does not tokenize the page label, the group label or combo items.**
- Every query token must match some field (AND semantics).
- Tokens of 2 characters or fewer only match label, card and keyword (`:209`).
- Scoring is fuzzy: exact, prefix, substring, *subsequence* for 3 or more characters, or edit distance (`aldirectorqol.cpp:51-78`).
- Field weights are 80/55/45/25/90/15/5 (`:75`).
- Director hits rank before external hits on ties (`searchResultBefore`).

**Activation.**
- A Director hit calls `navigateTo()` (`alkitshell.cpp:1036`). On a kit page it reveals the row. On a legacy page it finds the child by name, opens accordions and scrolls to it, but it **does not select inner `tab_container` tabs** (`:1053-1080`).
- An external hit calls `openExternalResult()` (`:1626`), which opens the floater and selects the tab (`:1632`).
- In-page filtering only applies to kit rows (`page->findRow`, `:1411-1418`). Legacy pages get the global result list only.

**Existing gates.**
- The drift gate checks that the index equals the generated rows **for non-legacy pages only** (`director_kit_gen.py:1001-1006`).
- The launcher gate only regex-scans `LLFloaterReg::showInstance("...")` in `llfloaterdirector.cpp` (`:991`).

## 3. Search gaps (concrete)

| Gap | Evidence | Effect |
|---|---|---|
| G1. Legacy-embedded controls are indexed from mockup rows, not from the real hosted XUI | Unindexed real controls: Path 38, Exposure 39, Ghosts 28, Gaze 15, Props 3, Dance 3, Move 1, Polish 1 (128 in total). Many are the feature's on/off toggle (see rows 3-7, 11, 12, 26) | "autoanimate", "onion skin", "auto advance" and "blocking" land in external floaters or nowhere |
| G2. Dead Director rows on LINK pages | `gaze_cues_tab` 5, `light_cues_tab` 5 and `practicals_tab` 16 index widgets do not exist in the page tree; `vcam_tab` has 1 (`vc_monitor_tile_manifest`) | The hit opens the page, `navigateTo` finds no widget and nothing is highlighted |
| G3. Card master switches are not indexed | 25 of 26 `kit_card enable_setting` switches have no row (list in rows 15-17, 21, 22, 24, 25, 38, 39) | "camera shake", "enable rain" and "auto switcher on" fail or rank poorly |
| G4. Page and rail master switches are not indexed | No Director rows for `AlchemyWeatherEnabled`, `StepAnimEnabled`, `PrismLensEnabled`, `CineLightRigEnabled`, `CineRigRimEnabled`, `AvatarLightResponseEnabled`, `ALPolishEnabled` | The master on/off switches cannot be found by search |
| G5. Shell controls are not indexed | Arm popover (`DirectorArm*`, `DirectorActionDelay`), scene save/load, marks, paths toggle (`ActorMoverShowHeading`), cast roster tools | "countdown", "save scene" and "show paths" fail |
| G6. Page and group names are not searchable | `page_label` and `group_label` are not tokenized (`alkitsearch.cpp:147-153`); there are no page rows | "practicals" scores 95 on Flicker rows; a page name only matches by accident |
| G7. Combo and radio item labels are not indexed | 227 mockup rows carry `it` items; none are in index fields; external rows likewise | "kaleidoscope", "heart bokeh", "tripod" and named looks are not findable |
| G8. Launcher discovery misses floaters | `actor_gaze` (opened via `showTypedInstance`, `llfloaterdirector.cpp:5719`) and `env_adjust_snapshot` (an XUI `Floater.ShowOrBringToFront` in `panel_exposure_hub.xml:53,66`) are reachable from the Director but are not in `external_search.json`, and the gate cannot see them | Their controls are unsearchable |
| G9. External-row noise | 1,392 of 2,197 external rows use the widget-name fallback label (for example "fx reset BDMergeFroxelDensity"); 265 are reset buttons; subsequence matching makes "drone" return 1,048 hits and "xbox" match froxel rows | Real hits are buried; labels in the results are unreadable |
| G10. Synonyms are thin | `synonyms.json` has 22 groups. There are none for gamepad/controller/joystick/xbox, height/hover/elevation, water/sea level, practical/selected light, letterbox/aspect, pose/poser, shake/handheld/camera | Film and SL vocabulary misses |
| G11. Legacy jump does not switch inner tabs | `navigateTo` only opens accordions (`alkitshell.cpp:1053-1060`); Ghosts, Dance and Exposure hosts have inner tab containers. Whether hidden-tab targets still highlight is **unverified** | A search jump may scroll to a hidden control |
| G12. MISSING and settings-only features | Rows 8, 14, 20, 42, 44 and 48-50 | Not in the Director, so not searchable at all |

## 4. Prioritized build batches

The effort scale is S (up to half a day), M (1-2 days) and L (3 days or more). Each batch should end with the zero-finding layout gate and the click-every-control functional gate before it is called done.

**Batch A: search index completeness (generator + gate). Do this first because it makes everything else verifiable.**

| ID | Work | Group/page | Effort |
|---|---|---|---|
| A1 | Index legacy-embedded pages from the **real hosted XUI**, reusing the `external_index_rows` walker with Director coordinates (group, page, inner-tab name) instead of the mockup. Keep the mockup `kw` and `h` as enrichment joined by widget. This fixes the 128 unindexed controls | all 11 legacy pages | M |
| A2 | Emit rows for card `enable_setting` switches (card title + " on/off"), lazy-page `master_setting` and rail `master_settings`, and shell controls (arm popover, scene, marks, paths, roster) | shell + all kit pages | S |
| A3 | Emit one page row and one card row per page and card (label, summary and section labels tokenized), so page names such as "Practicals" and "Stepped" hit | all | S |
| A4 | Add an `options` field (combo, radio and `opts` labels), tokenized at CARD-level weight. This needs `INDEX_FIELDS` 22 to 23 and a version bump in the header check (`alkitsearch.cpp:83`) | all + external | S/M |
| A5 | Launcher discovery: scan `showInstance`, `showTypedInstance`, `toggleInstance` and `ShowOrBringToFront` in `llfloaterdirector.cpp` **and** in every hosted panel's .cpp, XUI `button.commit_callback function="Floater.*"` parameters inside the Director tree, and kit `action="floater:..."`/`"lightbox:..."`. Fail if a target is missing from `external_search.json`. This adds `actor_gaze` and `env_adjust_snapshot`, plus `pref_joystick`, `edit_hover_height`, `env_adjust_water`, `env_intensity_presets` and `fs_poser` once B adds links | gen | S |
| A6 | Dead-row gate: every non-external widget must resolve to a named node in its page tree. On LINK pages, rewrite the mockup rows as external rows pointing at the target floater and tab (`gaze_cues`, `cine_light_cues`, `lightbox:sellight_settings`) | Gaze Cues, Light Cues, Practicals | S |
| A7 | Noise: skip `reset_*`/`rst_*` buttons (or label them "Reset <sibling label>"); derive labels from the adjacent `text` sibling when `label` is empty; add a minimum score for subsequence matches on long tokens; extend `synonyms.json` (G10) | gen + `aldirectorqol.cpp` | S |
| A8 | `navigateTo` on legacy hosts: call `selectTabRecursive` on the hosted panel for the row's recorded inner-tab name before scrolling | `alkitshell.cpp` | S |

**Batch B: missing features to build into the Director.** These should be kit pages or cards, never new legacy hosts.

| ID | Work | Group/page | Effort |
|---|---|---|---|
| B1 | **Flycam page**. Cards: Flight model (Classic / Weighted / Drone + their curves), Triggers and momentum, Xbox pad (enable, D-pad actions, trigger mode, throttle, rumble), Axis mapping/dead zones/scales (advanced). Source: `floater_joystick.xml` tabs at :13, :142, :628, :690 | Camera > new "Flycam" (after Operator) | L |
| B2 | **Height card**: client-side local height for the selected cast member (ALClientHeight key), own Hover Height, `HoverHeightAffectsCamera`. Plus the matching LINK to `edit_hover_height` | Cast > Move (or new Cast > "Height") | M |
| B3 | **Water level card**: `RenderWaterLevelOverride`, `RenderWaterLevel` (session-only note) | World > Weather (or Atmosphere) | S |
| B4 | **Light Intensity colour + presets**: Sun Kelvin, Sun/Moon/Ambient tint colour and strength, user preset list/save/load/delete (from `floater_env_intensity_presets.xml`) | Light > Exposure (fold into C1) | M |
| B5 | **Capture mode card**: Auto cinematic capture mode + its triggers (`BDMergeCineMode*`), Machinima High LOD, Pin scene textures (`BDMergeCaptureModePin`), Near clip, Forced alpha mask | Capture > Scene (or a new Capture > "Render") | S/M |
| B6 | Poser: LINK row + external index for `fs_poser` (and pose stand). Confirm with the user first, because it is upstream-ported | Performance > Animate | S |
| B7 | Probes card: `RenderProbeOnDemand`, sun-shadow mode, capture scale, update cadence, min interval/max age | World > Shadows (makes the internal `shadows_probes` name true) | S/M |
| B8 | Light classes card: world, projector, own and others' attached lights; Stable spot shadows; Global light scale | Light > Practicals (as part of C2) or Light > Rig | S |
| B9 | Settings-only items: Motion blur / velocity (Look > Image or Capture > Time), ReShade bridge cut/history (Capture), Hide look-at (Capture). **Ask the user first.** Tron trail target goes on Look > Tron | Look / Capture | S each |

**Batch C: convert legacy and LINK pages to kit.** These are larger; Batch A makes them searchable in the meantime.

| ID | Work | Group/page | Effort |
|---|---|---|---|
| C1 | Exposure hub to kit. It duplicates the Lens > Filters and Rig master-EV rows, so link to those instead of duplicating; add undo/redo to the page bar | Light > Exposure | M |
| C2 | Practicals (Selected Light) to a kit page | Light > Practicals | M |
| C3 | Light Cues and Gaze Cues to kit pages (cue list + transport) | Light > Light Cues, Performance > Gaze Cues | M each |
| C4 | Diopter: decide whether the 178 floater-only settings stay a LINK (already searchable externally) or grow into kit sections | Lens > Diopter | L |
| C5 | Cast pages (Move, Path, Props, Ghosts, Dance) to kit | Cast | L each (Path and Ghosts are the largest) |
| C6 | Performance pages (Animate, Switchboard, Gaze, Polish) to kit | Performance | M-L each |

Suggested order: A (all) first, then B3, B5, B7 and B8 (small), then B2, B4 + C1, then B1. C2 and C3 come after that, and C4-C6 last. Do one build at the end of each batch, following the checkpoint-commit-then-fresh-build rule.

## 5. Search-index fix design (every Director feature searchable, gate-checked)

**Principle.** The generator owns the index. It emits a row for everything a user can reach from Director v2, and a fail-closed gate proves it.

1. **One reachability walker** (shared by the generator and the gate). Start at `floater_director_v2.xml`.
   - Follow every `page_file`, every `filename=` include, `kit_popover` and `kit_bar`.
   - Record: (a) every named interactive node (the `EXTERNAL_CONTROL_TAGS` set plus `kit_switch`, `kit_row`, `radar_list`, `multi_slider`, `text_editor`); (b) every `kit_card@enable_setting`, `kit_lazy_page@master_setting` and `rail_item@master_settings`; (c) the inner `tab_container` panel that contains each node.
   - Collect link targets from kit `action`, XUI `commit_callback` `Floater.*` parameters, and a C++ scan of `showInstance`/`showTypedInstance`/`toggleInstance`/`ShowOrBringToFront` and `ALFloaterLightBox::showTab` in `llfloaterdirector.cpp` plus the .cpp of every hosted panel class (the class names are in the walked XUI `class=` attributes).
2. **Director rows.**
   - On kit pages, emit from generated rows as today.
   - On legacy pages, emit from the walker, carrying the inner-tab name in the field that external rows already use (fields 19/20). Merge mockup `kw`, `h` and items by widget.
   - Add page rows, card rows, switch rows and shell rows (A2, A3).
   - Add a tokenized `options` field (A4).
   - Labels resolve as: `label`, else the adjacent `text` sibling, else the tool tip's first clause. A widget name is never used as a label.
3. **External rows.** Emit one row set per discovered link target. A target found by the walker but missing from `external_search.json` is a generator error, so the JSON becomes an explicit allowlist of *how* to label a floater, not *whether* to index it. Lightbox rows use the tab that the link opened.
4. **Synonyms.** Move to `synonyms.json` v2 with feature aliases (`{"feature": "Flycam", "aliases": ["gamepad", "controller", "xbox", "joystick", "drone", "flight model"]}`) that attach to page and card rows, plus the existing bidirectional groups (extended per G10).
5. **Gates** (in `director_kit_gen.py --check`, run by `check_kit_pages.py` and the build gate):
   - **G-coverage**: every walker-reachable control or switch has at least one index row matched by widget or setting. Exemptions go through `search_allow.json` only, entry by entry with a reason; pattern-wide exemptions are not accepted. Reset buttons are exempt by kind (`reset_`/`rst_` and the `kit_row` reset), with a fixed count asserted.
   - **G-dead**: every Director row resolves to a node on its page, and every external row resolves in its floater and tab.
   - **G-links**: walker link targets are a subset of indexed floaters.
   - **G-feature-probe**: `search_probes.json` lists about 80 feature-name queries with the expected page or floater (for example `"camera shake" -> camera_group/operator_tab`, `"water level" -> world_group/weather_tab`, `"gamepad" -> camera_group/flycam_tab`, `"practicals" -> light_group/practicals_tab`). A Python port of `ALKitSearch::query` and the scorer must put the expected target in the top 3. A C++ unit test runs the same probes against the real `ALKitSearch` so the port cannot drift.
   - **G-noise**: no row label equals its widget name, and no single-token feature probe returns more than N (for example 150) hits.
   - **Adversarial fixtures** (per the "Codex relaxes gates" rule): the reviewer plants an unindexed toggle in a hosted panel, a dead mockup row, a `showTypedInstance` launcher not in the JSON, and a page whose label is only in `page_label`. Each must fail its gate.
6. **Runtime.** Add `selectTabRecursive` for legacy jumps (A8). Tokenize the page label through the page rows (no format change), or tokenize `fields[2]`/`fields[3]` at CARD weight.

## 6. Notes and unverified items

- Light > Actors: the conversion was mid-flight during the audit; its two unindexed controls and the earlier `actors_tab` legacy classification may change.
- Settings bound only in C++ inside hosted panels are counted as covered without per-control verification. In particular, whether every `GhostStudio*`/`GhostAutoAnimate*` tuning setting has a visible control is **unverified**; `alghoststudio.cpp` reads them.
- `CinematicFrameAspectRatio` and `CinematicCamMode` are assumed code-bound on Frame & Orbit and Shot (**unverified**).
- Weather rain and lightning colour, seed and trigger (`AlchemyWeatherRainColor` and similar) have no XUI anywhere; it is unclear whether they are meant to be user-facing.
- The search simulation reproduces the C++ scorer, but its results are a model and were not run in the viewer.

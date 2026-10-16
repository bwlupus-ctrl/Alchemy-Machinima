# Wiring audit: Cast > Ghosts kit conversion

Static audit of the Ghosts page after it became a generated kit page (10/15, legacy conversion batch 3, the last legacy page). No build, no commit, no viewer run: every row is checked against the generated XML (`panel_dpage_cast_ghosts.xml`) and the C++ source. Sources: mockup page `ghosts_tab` (25 cards, 125 rows in the mockup; 128 now, see below), `kit_pages.json` entry `cast_group/ghosts_tab` (hosts the v1 nodes of `panel_ghost_studio.xml`; page root class `panel_ghost_studio`, root name `ghosts_tab_logic`).

## Result

- The page is no longer legacy: classification `kit (P0)`, pin 1 -> 0, `KNOWN_LEGACY_PAGES` is now empty and `KNOWN_LEGACY_MAX` is 0. The legacy count is 0 everywhere (`director_kit_gen.py` classification table, `director_functional_manifest.py`, `xui_layout_check.py` `LEGACY_PAGE_COUNT`, `gate_policy.json` `xui_legacy_v1_findings` is empty).
- 128 generated rows = the 125 mockup rows + 3 added (below), 25 cards, 6 sections (Ghosts, Look, Motion, Crowd, Groups, Advanced). Three cards carry the v1 checkboxes as card switches: `chaos_check`, `autoanim_check`, `lens_gaze_check` (`ALKitSwitch` is an `LLCheckBoxCtrl`, so the lookups resolve).
- The page root is `ALPanelGhostStudio`, the class the standalone Ghost Studio floater and the v1 Director tab embed. The panel owns all state (`ALGhostStudio` instances, crowd draft); the setting-bound rows are `mirror_hold_check`, `mirror_repeat_held_check` and `formation_overlay_palette_combo`, everything else is code-bound.
- The v1 Director tab (`floater_director.xml`) and the standalone floater (`floater_ghost_studio.xml`, `panel_ghost_studio.xml`) are untouched.

## Mockup rows added (the v1 panel had them, the mockup did not)

- `studio_hint` (Tip, readout, first row of the Ghost list card): the v1 hint text; `refreshStatus` rewrites it with the edit-mode state (`mHint`).
- `ghost_type` (Type, readout, first row of the Position card): the v1 "Type: Overlay ghost / Entity clone" line (`mTypeText`).
- `heading_dial` (Heading dial, dial, after Yaw in the Position card): the v1 live heading dial (`mHeadingDial`, an `ALCompassDial`); the C++ keeps it and the Yaw spinner in step.
- Mockup row pin in `director_kit_gen.py`: 2109 -> 2112. Every Ghosts row also gained search keywords (`kw`). The 24 rows that had an XUI reset button now carry `bind="code"` and `d` (the control's XUI `initial_value`), so the kit draws the reset glyph (`default_value`).
- Card ids `playback`, `look`, `diagnostics` renamed `ghost_playback`, `ghost_look`, `ghost_diagnostics` in the mockup (generated card names must be unique across pages).

## Renames and C++ changes

- `btn_freeze` -> `ghost_btn_freeze` (Capture > Time owns `btn_freeze`); the four Gaze combo items `gaze_tangent/camera/cast/point` -> `ghost_gaze_*` (Performance > Gaze owns those item names); ghost list columns `on/name/kind/state/source/style/pose` -> `ghost_on/ghost_name_col/ghost_kind/ghost_state/ghost_source/ghost_style/ghost_pose`.
- `alpanelghoststudio.cpp`: `column_name(list, index, v1_name)` (:66) and the list rebuild addresses columns by position (`refreshList`, rows and group header, :1362-1430), so one code path fills the floater and the kit page.
- `alpanelghoststudio.cpp:424-428`: `mFreezeBtn = findChild("btn_freeze")` with the `ghost_btn_freeze` fallback (same idiom as Props).
- `alpanelghoststudio.cpp:80,494-525,778`: the tab container is found by type (`find_tab_container`) instead of `getChild("ghost_studio_tabs")`; the Pose/Style/Crowd tab reordering, `installForTabs` and the `reshape` fit run only when a tab container exists (the floater). The lazy page adoption fits the kit page and gives its sliders the wheel. `reshape` recursively finds `instances_section`.
- `alpanelghoststudio.cpp:405-421,1521-1527`: the overlay-only style controls (hidden for entity clones) were one panel (`look_section`, found as the parent of the Look combo); on the kit page the same 12 controls sit in rows, so `mLookControls` is hidden control by control with `ALKitRow::setControlVisible` (`setRowVisibleIfChanged`, :99). Every other `setVisibleIfChanged` on a control that sits in a kit row (hint, refresh, directed-animation rows, brigade and formation rows) uses the same helper so the row and the card re-lay out.
- `alpanelghoststudio.cpp:352,440,640-647,755-766`: the 25 icon reset buttons are looked up with `findChild` and skipped when absent; `reset_array_parameter` is the floater's, and on the kit page `registerKitParameterReset` (:4343) registers a `FormationParamBinding` whose default follows the current formation and whose reset runs the new `onFormationParamReset` (:4285), the former lambda body.
- `alpanelghoststudio.cpp:1718,1874`: new read-back for `pose_rate_spinner` (`inst->mPoseRateHz`); the v1 spinner was write-only, so it showed the last typed value whichever ghost was selected.
- `alpanelghoststudio.h`: `onFormationParamReset`, `registerKitParameterReset`, `mLookControls`, `mTabs`, forward declaration of `LLTabContainer`.
- `director_page_sources.py`: four `alpanelghoststudio.cpp` binding line numbers refreshed (already stale).
- No change in `llfloaterdirector.cpp` (its `ghost_studio_panel` lookups return null on the kit page and are skipped; `fillPage` ignores null).

## parity_allow additions (explicit names, no patterns)

Static labels and headings replaced by card titles or row labels: `array_section_label`, `autoanim_label`, `lens_gaze_label`, `look_lbl`, `look_target_label`, `motion_section_label`, `place_lbl`, `pose_lbl`, `studio_header`.

Icon reset buttons replaced by the kit row's own reset glyph (default = the control's XUI initial value, same commit path): `reset_yaw`, `reset_scale`, `reset_effect_fps`, `reset_anim_speed`, `reset_autoanim_window`, `reset_array_count`, `reset_array_spacing`, `reset_array_parameter`, `reset_strip_count`, `reset_strip_interval`, `reset_motion_speed`, `reset_motion_amplitude`, `reset_hue`, `reset_alpha`, `reset_pixel`, `reset_shimmer_speed`, `reset_shimmer_amount`, `reset_glitch`, `reset_distort_amount`, `reset_brightness`, `reset_chaos`, `reset_lens_gaze_torso`, `reset_lens_gaze_head_eye`, `reset_lens_gaze_intensity`, `reset_lens_gaze_smoothing`.

## Undo ownership (final review 10/16)

Selected ghost/group values, source selectors, crowd draft and placement inputs, array/formation,
brigade, strip and motion controls use `widget_history=false`. Ghosts has no page history.
Only Show ghosts, Edit ghosts and Names retain global widget history. Mirror hold/repeat and
overlay palette keep their setting history. Buttons and output rows never record widget steps.

The overlay-only Ghost look, Colour & tint and Shimmer & glitch cards use `setCodeVisible`.
The entity note is a normal-width wrapping row in Position, visible for entity clones.
Yaw keeps two decimals. Kit reset checks the enabled control before invoking its existing commit
side effects. Formation Parameter binds only after registry installation and adds a changed signal
without replacing the panel commit handler. The instances section lookup is recursive.

## Controls

All handlers are in `alpanelghoststudio.cpp` (postBuild wiring :560-760). Combo data values are the v1 `value` attributes, hosted unchanged. Read-back is the gated `refresh*` function listed.

| Card | Widget | Type / range | Combo data values | Handler / target | Read-back | Undo owner | Status |
|---|---|---|---|---|---|---|---|
| Ghost list | `studio_hint` | readout | - | none (output) | refreshStatus: setTextIfChanged(mHint) (edit-mode text) | n/a (action/output, no widget step) | OK |
| Ghost list | `show_all_check` | toggle | - | onShowAllToggle() | postBuild: set(ALGhostStudio::getShowAll()) | kit widget history (global, no selected subject) | OK |
| Ghost list | `edit_ghosts_check` | toggle | - | onToggleEditMode() | onToggleEditMode / exitEditMode keep it in step with the tool | kit widget history (global, no selected subject) | OK |
| Ghost list | `ghost_list` | list (7 cols) | - | onListSelect() | refreshList (list revision gate): rows rebuilt, selection restored; columns by position | none (widget_history=false; subject/selection state) | OK |
| Ghost list | `studio_status` | readout | - | none (output) | refreshStatus: setTextIfChanged(mStatusText) | n/a (action/output, no widget step) | OK |
| Ghost list | `btn_ghost_dup` | button | - | onClickDuplicate() | refreshDetail enable/visibility; result in the list/status | n/a (action/output, no widget step) | OK |
| Ghost list | `btn_ghost_del` | button | - | onClickDelete() | refreshDetail enable/visibility; result in the list/status | n/a (action/output, no widget step) | OK |
| Ghost list | `btn_ghost_refresh` | button | - | onClickRefresh | refreshDetail enable/visibility; result in the list/status | n/a (action/output, no widget step) | OK |
| Add & copy | `source_combo` | combo | - | read by onClickAdd (no commit handler) | refreshSourceCombo (cast revision), keeps the previous choice | none (widget_history=false; subject/selection state) | OK |
| Add & copy | `clone_type_combo` | combo | Overlay ghost=0; Entity clone=1 | read by onClickAdd (no commit handler) | none (local choice read by onClickAdd) | none (widget_history=false; subject/selection state) | OK |
| Add & copy | `btn_ghost_add` | button | - | onClickAdd() | refreshDetail enable/visibility; result in the list/status | n/a (action/output, no widget step) | OK |
| Add & copy | `btn_duplicate_in_place` | button | - | onClickDuplicateInPlace() | refreshDetail enable/visibility; result in the list/status | n/a (action/output, no widget step) | OK |
| Add & copy | `btn_copy_transform` | button | - | onClickCopyTransform() | refreshDetail enable/visibility; result in the list/status | n/a (action/output, no widget step) | OK |
| Add & copy | `btn_paste_transform` | button | - | onClickPasteTransform() | refreshDetail enable/visibility; result in the list/status | n/a (action/output, no widget step) | OK |
| Position | `ghost_type` | readout | - | none (output) | refreshDetail: setTextIfChanged(mTypeText) | n/a (action/output, no widget step) | OK |
| Position | `ghost_name` | text | - | onNameCommit() | refreshDetail (selected instance, 10 Hz, diff-guarded; skipped while the control is being edited) | none (widget_history=false; subject/selection state) | OK |
| Position | `pos_x_spinner` | number -1000..9000 | - | onPosCommit() | refreshDetail (selected instance, 10 Hz, diff-guarded; skipped while the control is being edited) | none (widget_history=false; subject/selection state) | OK |
| Position | `pos_y_spinner` | number -1000..9000 | - | onPosCommit() | refreshDetail (selected instance, 10 Hz, diff-guarded; skipped while the control is being edited) | none (widget_history=false; subject/selection state) | OK |
| Position | `pos_z_spinner` | number -1000..9000 | - | onPosCommit() | refreshDetail (selected instance, 10 Hz, diff-guarded; skipped while the control is being edited) | none (widget_history=false; subject/selection state) | OK |
| Position | `yaw_spinner` | number -180..180 | - | onYawCommit() | refreshDetail (selected instance, 10 Hz, diff-guarded; skipped while the control is being edited) | none (widget_history=false; subject/selection state) | OK |
| Position | `heading_dial` | custom | - | onHeadingDialCommit() | refreshDetail (selected instance, 10 Hz, diff-guarded; skipped while the control is being edited) | none (widget_history=false; subject/selection state) | OK |
| Position | `scale_spinner` | number 0.05..150 | - | onScaleCommit() | refreshDetail (selected instance, 10 Hz, diff-guarded; skipped while the control is being edited) | none (widget_history=false; subject/selection state) | OK |
| Place & snap | `btn_place` | button | - | onClickPlace() | refreshDetail enable/visibility; result in the list/status | n/a (action/output, no widget step) | OK |
| Place & snap | `btn_to_actor` | button | - | onClickToActor() | refreshDetail enable/visibility; result in the list/status | n/a (action/output, no widget step) | OK |
| Place & snap | `btn_to_me` | button | - | onClickToMe() | refreshDetail enable/visibility; result in the list/status | n/a (action/output, no widget step) | OK |
| Place & snap | `btn_drop_ground` | button | - | onClickDrop() | refreshDetail enable/visibility; result in the list/status | n/a (action/output, no widget step) | OK |
| Place & snap | `btn_align_feet` | button | - | onClickAlignFeet() | refreshDetail enable/visibility; result in the list/status | n/a (action/output, no widget step) | OK |
| Facing | `look_target_combo` | combo | - | onLookTargetCommit() | refreshLookTargetCombo + refreshDetail (selected instance, 10 Hz, diff-guarded; skipped while the control is being edited) | none (widget_history=false; subject/selection state) | OK |
| Facing | `btn_face_now` | button | - | onClickFaceNow() | refreshDetail enable/visibility; result in the list/status | n/a (action/output, no widget step) | OK |
| Facing | `keep_facing_check` | toggle | - | onKeepFacingCommit() | refreshDetail (selected instance, 10 Hz, diff-guarded; skipped while the control is being edited) | none (widget_history=false; subject/selection state) | OK |
| Facing | `btn_reset_upright` | button | - | onClickUpright() | refreshDetail enable/visibility; result in the list/status | n/a (action/output, no widget step) | OK |
| Ghost look | `style_combo` | combo | Ghost=0; Clone=1; Hologram=2; Wireframe=3; X-ray=4; Thermal=5; Neon Outline=6; Silhouette=7; Toon/Ink=8; Chrome=9; Dissolve=10; Negative=11; Gold Statue=12; Night-vision=13; Blueprint=14; Ectoplasm=15; Frost/Ice=16; Prism=17; Thermal Scope=18; Wallhack/ESP=19; Night-vision Tube=20; Damage Overlay=21; Killcam=22; Oil Slick=23; Vaporwave=24; Halftone/Comic=25; Sonar Reveal=26; Hologram Echo=27 | onStyleCommit() | refreshDetail (selected instance, 10 Hz, diff-guarded; skipped while the control is being edited) | none (widget_history=false; subject/selection state) | OK |
| Ghost look | `distort_combo` | combo | Distort: None=0; Distort: Pixelate=1; Distort: Voxel=2; Distort: Lens/Magnify=3; Distort: Wave/Ripple=4; Distort: RGB Split=5; Distort: Block Glitch=6; Distort: Vertical Tear=7; Distort: VHS=8 | onDistortCommit() | refreshDetail (selected instance, 10 Hz, diff-guarded; skipped while the control is being edited) | none (widget_history=false; subject/selection state) | OK |
| Ghost look | `distort_amount_slider` | slider 0..1 | - | onDistortAmountCommit() | refreshDetail (selected instance, 10 Hz, diff-guarded; skipped while the control is being edited) | none (widget_history=false; subject/selection state) | OK |
| Ghost look | `entity_style_hint` | readout | - | none (output) | XML text, shown for entity clones (setRowVisibleIfChanged) | n/a (action/output, no widget step) | OK |
| Colour & tint | `actor_tint_check` | toggle | - | onActorTintToggle() | refreshDetail (selected instance, 10 Hz, diff-guarded; skipped while the control is being edited) | none (widget_history=false; subject/selection state) | OK |
| Colour & tint | `hue_slider` | slider 0..360 | - | onHueCommit() | refreshDetail (selected instance, 10 Hz, diff-guarded; skipped while the control is being edited) | none (widget_history=false; subject/selection state) | OK |
| Colour & tint | `alpha_slider` | slider 0..1 | - | onAlphaCommit() | refreshDetail (selected instance, 10 Hz, diff-guarded; skipped while the control is being edited) | none (widget_history=false; subject/selection state) | OK |
| Colour & tint | `brightness_slider` | slider 0.05..1.5 | - | onBrightnessCommit() | refreshDetail (selected instance, 10 Hz, diff-guarded; skipped while the control is being edited) | none (widget_history=false; subject/selection state) | OK |
| Shimmer & glitch | `pixel_slider` | slider 0..64 | - | onPixelCommit() | refreshDetail (selected instance, 10 Hz, diff-guarded; skipped while the control is being edited) | none (widget_history=false; subject/selection state) | OK |
| Shimmer & glitch | `shimmer_amount_slider` | slider 0..1 | - | onShimmerAmountCommit() | refreshDetail (selected instance, 10 Hz, diff-guarded; skipped while the control is being edited) | none (widget_history=false; subject/selection state) | OK |
| Shimmer & glitch | `shimmer_speed_slider` | slider 0..10 | - | onShimmerSpeedCommit() | refreshDetail (selected instance, 10 Hz, diff-guarded; skipped while the control is being edited) | none (widget_history=false; subject/selection state) | OK |
| Shimmer & glitch | `glitch_slider` | slider 0..1 | - | onGlitchCommit() | refreshDetail (selected instance, 10 Hz, diff-guarded; skipped while the control is being edited) | none (widget_history=false; subject/selection state) | OK |
| Shimmer & glitch | `effect_fps_spinner` | number 0..30 | - | onEffectFpsCommit() | refreshDetail (selected instance, 10 Hz, diff-guarded; skipped while the control is being edited) | none (widget_history=false; subject/selection state) | OK |
| Pose & drive | `ghost_btn_freeze` | button | - | onClickFreeze (floater name btn_freeze) | refreshDetail enable/visibility; result in the list/status | n/a (action/output, no widget step) | OK |
| Pose & drive | `btn_live` | button | - | onClickLive() | refreshDetail enable/visibility; result in the list/status | n/a (action/output, no widget step) | OK |
| Pose & drive | `drive_mode_combo` | combo | Mirror=0; Directed=1; Frozen=2; True mirror=3 | onDriveModeCommit() | refreshDetail (selected instance, 10 Hz, diff-guarded; skipped while the control is being edited) | none (widget_history=false; subject/selection state) | OK |
| Pose & drive | `mirror_hold_check` | toggle | - | kit setting binding writes GhostMirrorHoldOnSourceChange; ALGhostStudio reads it | kit setting binding (GhostMirrorHoldOnSourceChange) | kit setting history (GhostMirrorHoldOnSourceChange) | OK |
| Pose & drive | `pose_status` | readout | - | none (output) | refreshDetail slow tick: setTextIfChanged(mPoseStatus) | n/a (action/output, no widget step) | OK |
| Pose & drive | `mirror_repeat_held_check` | toggle | - | kit setting binding writes GhostMirrorRepeatHeldAnimesh; ALGhostStudio reads it | kit setting binding (GhostMirrorRepeatHeldAnimesh) | kit setting history (GhostMirrorRepeatHeldAnimesh) | OK |
| Chaos & speed | `chaos_slider` | slider 0..1 | - | onChaosCommit() | refreshDetail (selected instance, 10 Hz, diff-guarded; skipped while the control is being edited) | none (widget_history=false; subject/selection state) | OK |
| Chaos & speed | `anim_speed_spinner` | number 0.05..4 | - | onAnimSpeedCommit() | refreshDetail (selected instance, 10 Hz, diff-guarded; skipped while the control is being edited) | none (widget_history=false; subject/selection state) | OK |
| Clone behaviour | `entity_look_combo` | combo | Normal=0; Apparition=1; Hologram*=2; Chrome*=3; Toon*=4; Silhouette*=5 | onEntityLookCommit() | refreshDetail (selected instance, 10 Hz, diff-guarded; skipped while the control is being edited) | none (widget_history=false; subject/selection state) | OK |
| Clone behaviour | `physics_check` | toggle | - | onPhysicsCommit() | refreshDetail (selected instance, 10 Hz, diff-guarded; skipped while the control is being edited) | none (widget_history=false; subject/selection state) | OK |
| Clone behaviour | `anim_loop_mode_combo` | combo | Loop (re-trigger)=0; Play once=1 | onLoopModeCommit() | refreshDetail (selected instance, 10 Hz, diff-guarded; skipped while the control is being edited) | none (widget_history=false; subject/selection state) | OK |
| Playback | `btn_anim_pause` | button | - | onClickAnimPause() | refreshDetail enable/visibility; result in the list/status | n/a (action/output, no widget step) | OK |
| Playback | `btn_anim_resume` | button | - | onClickAnimResume() | refreshDetail enable/visibility; result in the list/status | n/a (action/output, no widget step) | OK |
| Playback | `btn_anim_sync` | button | - | onClickAnimSync() | refreshDetail enable/visibility; result in the list/status | n/a (action/output, no widget step) | OK |
| Animation asset | `directed_anim_editor` | text | - | onDirectedAnimCommit() | refreshDetail: setTextIfChanged(mDirectedAnimEdit) | none (widget_history=false; subject/selection state) | OK |
| Animation asset | `animation_library_combo` | combo | - | onAnimationLibraryCommit() | populateAnimationLibrary + refreshDetail (selected instance, 10 Hz, diff-guarded; skipped while the control is being edited) | none (widget_history=false; subject/selection state) | OK |
| Animation asset | `btn_anim_library_refresh` | button | - | inline: ALGhostStudio::scanAnimLibrary() then populateAnimationLibrary() | refreshDetail enable/visibility; result in the list/status | n/a (action/output, no widget step) | OK |
| Animation asset | `anim_metadata_text` | readout | - | none (output) | refreshDetail slow tick | n/a (action/output, no widget step) | OK |
| Auto-animate | `autoanim_window_spinner` | number 15..600 | - | onAutoAnimWindow() | refreshDetail (selected instance, 10 Hz, diff-guarded; skipped while the control is being edited) | none (widget_history=false; subject/selection state) | OK |
| Auto-animate | `btn_autoanim_go` | button | - | onAutoAnimAction(ALGhostAutoAnim::Action::GoAutonomous) | refreshDetail enable/visibility; result in the list/status | n/a (action/output, no widget step) | OK |
| Auto-animate | `btn_autoanim_follow` | button | - | onAutoAnimAction(ALGhostAutoAnim::Action::FollowSource) | refreshDetail enable/visibility; result in the list/status | n/a (action/output, no widget step) | OK |
| Auto-animate | `btn_autoanim_stop` | button | - | onAutoAnimAction(ALGhostAutoAnim::Action::StopLearning) | refreshDetail enable/visibility; result in the list/status | n/a (action/output, no widget step) | OK |
| Auto-animate | `btn_autoanim_relearn` | button | - | onAutoAnimAction(ALGhostAutoAnim::Action::Relearn) | refreshDetail enable/visibility; result in the list/status | n/a (action/output, no widget step) | OK |
| Auto-animate | `autoanim_status` | readout | - | none (output) | refreshAutoAnimate | n/a (action/output, no widget step) | OK |
| Auto-animate | `btn_autoanim_reseed` | button | - | onAutoAnimAction(ALGhostAutoAnim::Action::Reseed) | refreshDetail enable/visibility; result in the list/status | n/a (action/output, no widget step) | OK |
| Auto-animate | `btn_autoanim_save` | button | - | onAutoAnimFile(true) | refreshDetail enable/visibility; result in the list/status | n/a (action/output, no widget step) | OK |
| Auto-animate | `btn_autoanim_load` | button | - | onAutoAnimFile(false) | refreshDetail enable/visibility; result in the list/status | n/a (action/output, no widget step) | OK |
| Head gaze | `btn_lens_gaze_selection` | button | - | onClickLensGazeSelection | refreshDetail enable/visibility; result in the list/status | n/a (action/output, no widget step) | OK |
| Head gaze | `lens_gaze_target_mode` | combo | Travel tangent=0; Camera=1; Cast target=2; World point=3 | onLensGazeSettingsCommit() | refreshDetail (selected instance, 10 Hz, diff-guarded; skipped while the control is being edited) | none (widget_history=false; subject/selection state) | OK |
| Head gaze | `lens_gaze_cast_target` | combo | - | onLensGazeSettingsCommit() | populateLensGazeCastTargets + refreshDetail (selected instance, 10 Hz, diff-guarded; skipped while the control is being edited) | none (widget_history=false; subject/selection state) | OK |
| Head gaze | `lens_gaze_torso` | slider 0..1 | - | onLensGazeTorsoCommit() | refreshDetail (selected instance, 10 Hz, diff-guarded; skipped while the control is being edited) | none (widget_history=false; subject/selection state) | OK |
| Head gaze | `lens_gaze_head_eye` | slider 0..1 | - | onLensGazeSettingsCommit() | refreshDetail (selected instance, 10 Hz, diff-guarded; skipped while the control is being edited) | none (widget_history=false; subject/selection state) | OK |
| Head gaze | `lens_gaze_intensity` | slider 0..1 | - | onLensGazeSettingsCommit() | refreshDetail (selected instance, 10 Hz, diff-guarded; skipped while the control is being edited) | none (widget_history=false; subject/selection state) | OK |
| Head gaze | `lens_gaze_smoothing` | slider 0..1 | - | onLensGazeSettingsCommit() | refreshDetail (selected instance, 10 Hz, diff-guarded; skipped while the control is being edited) | none (widget_history=false; subject/selection state) | OK |
| Head gaze | `lens_gaze_status` | readout | - | none (output) | refreshDetail slow tick | n/a (action/output, no widget step) | OK |
| Crowd pattern | `btn_array_line` | button | - | onClickArray(ALGhostStudio::FORMATION_LINE) | refreshDetail enable/visibility; result in the list/status | n/a (action/output, no widget step) | OK |
| Crowd pattern | `btn_array_ring` | button | - | onClickArray(ALGhostStudio::FORMATION_RING) | refreshDetail enable/visibility; result in the list/status | n/a (action/output, no widget step) | OK |
| Crowd pattern | `array_formation_combo` | combo | Line=0; Grid=3; Staggered rows=17; Split row=18; Soul Train=19; Ring=1; Arc=2; Concentric rings=11; Spiral=5; Chevron=20; Zigzag=21; Horseshoe=22; Infinity=23; Pentagram=24; Star outline=25; Diamond=26; Cross=27; Arrow=28; Heart=29; Sunburst=30; Military Brigade=31; Soul Train (military)=32; Staggered ranks (military)=33; Ring brigade (military)=34; Organic=8 | onFormationCommit() | refreshCrowdPlacement: setValueIfChanged(mFormationCombo) from the active draft | none (widget_history=false; subject/selection state) | OK |
| Crowd pattern | `array_count_spinner` | number 2..256 | - | updateOwnedCrowdPlacement (re-resolves the owned crowd draft) | refreshCrowdPlacement: shows the active draft values; otherwise keeps the entry | none (widget_history=false; subject/selection state) | OK |
| Crowd pattern | `array_spacing_spinner` | number 0.01..100 | - | updateOwnedCrowdPlacement (re-resolves the owned crowd draft) | refreshCrowdPlacement: shows the active draft values; otherwise keeps the entry | none (widget_history=false; subject/selection state) | OK |
| Layout & heading | `formation_edge_gap_spinner` | number 0..20 | - | updateOwnedCrowdPlacement (re-resolves the owned crowd draft) | refreshCrowdPlacement: shows the active draft values; otherwise keeps the entry | none (widget_history=false; subject/selection state) | OK |
| Layout & heading | `formation_jitter_spinner` | number 0..20 | - | updateOwnedCrowdPlacement (re-resolves the owned crowd draft) | refreshCrowdPlacement: shows the active draft values; otherwise keeps the entry | none (widget_history=false; subject/selection state) | OK |
| Layout & heading | `formation_seed_spinner` | number 0..999999 | - | updateOwnedCrowdPlacement (re-resolves the owned crowd draft) | refreshCrowdPlacement: shows the active draft values; otherwise keeps the entry | none (widget_history=false; subject/selection state) | OK |
| Layout & heading | `formation_terrain_check` | toggle | - | updateOwnedCrowdPlacement (re-resolves the owned crowd draft) | refreshCrowdPlacement: shows the active draft values; otherwise keeps the entry | none (widget_history=false; subject/selection state) | OK |
| Layout & heading | `formation_facing_combo` | combo | Rotate with formation=0; Source yaw=1; Camera / stage=2; Centroid in=3; Outward=4; World point=5; Mirror source yaw=7; Look-at target actor=8; Random=9; Face the aisle=10; Watch subject, live=11; Two ranks across=12; Cluster inward=13; Loose toward focal=14; Face previous slot=15; Animated look wave=16 | updateOwnedCrowdPlacement (re-resolves the owned crowd draft) | refreshCrowdPlacement: shows the active draft values; otherwise keeps the entry | none (widget_history=false; subject/selection state) | OK |
| Layout & heading | `facing_yaw_offset_spinner` | number -180..180 | - | inline: ALGhostStudio::setCrowdPlacementFacingOffset (active draft owned by this panel), refreshCrowdPlacement | refreshCrowdPlacement: shows the active draft values; otherwise keeps the entry | none (widget_history=false; subject/selection state) | OK |
| Layout & heading | `btn_pick_facing_target` | button | - | onClickPickFacingTarget() | refreshDetail enable/visibility; result in the list/status | n/a (action/output, no widget step) | OK |
| Layout & heading | `preserve_source_tilt_check` | toggle | - | updateOwnedCrowdPlacement (re-resolves the owned crowd draft) | refreshCrowdPlacement: shows the active draft values; otherwise keeps the entry | none (widget_history=false; subject/selection state) | OK |
| Guide & names | `formation_guide_shape_combo` | combo | Guide: Off=0; Guide: Circle=1; Guide: Square=2 | updateOwnedCrowdPlacement (re-resolves the owned crowd draft) | refreshCrowdPlacement: shows the active draft values; otherwise keeps the entry | none (widget_history=false; subject/selection state) | OK |
| Guide & names | `formation_guide_size_spinner` | number 0.01..1000 | - | updateOwnedCrowdPlacement (re-resolves the owned crowd draft) | refreshCrowdPlacement: shows the active draft values; otherwise keeps the entry | none (widget_history=false; subject/selection state) | OK |
| Guide & names | `crowd_copies_as_combo` | combo | Copies as: Select clone=-1; Copies as: Overlay ghost=0; Copies as: Entity clone=1 | none (disabled: shows the prototype kind) | refreshDetail: setValueIfChanged(mCrowdCopiesAs) (disabled; shows the prototype kind) | none (widget_history=false; subject/selection state) | OK |
| Guide & names | `crowd_create_mode_combo` | combo | Create: Separate=0; Create: Rigid group=1; Create: Move+Face group=2 | updateOwnedCrowdPlacement (re-resolves the owned crowd draft) | refreshCrowdPlacement: shows the active draft values; otherwise keeps the entry | none (widget_history=false; subject/selection state) | OK |
| Guide & names | `nameplate_mode_combo` | combo | Names: Off=0; Names: Smart=1; Names: All=2 | onNameplateModeCommit() | postBuild + onNameplateModeCommit: GhostStudioNameplateMode | kit widget history (global, no selected subject) | OK |
| Guide & names | `formation_overlay_palette_combo` | combo | Preview: Neon green=0; Preview: Neon yellow=1; Preview: Cyan=2; Preview: White=3; Preview: Magenta=4 | kit setting binding writes GhostStudioOverlayPalette; the crowd tool reads it | kit setting binding (GhostStudioOverlayPalette) | kit setting history (GhostStudioOverlayPalette) | OK |
| Preview & create | `btn_array_build` | button | - | onClickBuildArray() | refreshDetail enable/visibility; result in the list/status | n/a (action/output, no widget step) | OK |
| Preview & create | `btn_crowd_confirm` | button | - | onClickCrowdConfirm() | refreshDetail enable/visibility; result in the list/status | n/a (action/output, no widget step) | OK |
| Preview & create | `btn_crowd_unpin` | button | - | onClickCrowdUnpin() | refreshDetail enable/visibility; result in the list/status | n/a (action/output, no widget step) | OK |
| Preview & create | `btn_crowd_cancel` | button | - | onClickCrowdCancel() | refreshDetail enable/visibility; result in the list/status | n/a (action/output, no widget step) | OK |
| Preview & create | `crowd_placement_status` | readout | - | none (output) | refreshCrowdPlacement | n/a (action/output, no widget step) | OK |
| Preview & create | `crowd_placement_instructions` | readout | - | none (output) | refreshCrowdPlacement | n/a (action/output, no widget step) | OK |
| Group members | `group_edit_members_check` | toggle | - | onGroupEditMembersCommit() | refreshDetail (selected instance, 10 Hz, diff-guarded; skipped while the control is being edited) | none (widget_history=false; subject/selection state) | OK |
| Group members | `lock_mode_combo` | combo | Selected group: Off=0; Selected group: Rigid=1; Selected group: Move+Face=2 | inline: onLockModeCommit when no draft is active, then updateOwnedCrowdPlacement | refreshDetail: setValueIfChanged(mLockMode) (group members, no active draft) | none (widget_history=false; subject/selection state) | OK |
| Group members | `group_member_pinned_check` | toggle | - | onGroupMemberPinnedCommit() | refreshDetail (selected instance, 10 Hz, diff-guarded; skipped while the control is being edited) | none (widget_history=false; subject/selection state) | OK |
| Group members | `btn_group_member_reset` | button | - | onClickGroupMemberReset() | refreshDetail enable/visibility; result in the list/status | n/a (action/output, no widget step) | OK |
| Military brigade | `brigade_columns_spinner` | number 1..256 | - | updateOwnedCrowdPlacement (re-resolves the owned crowd draft) | refreshCrowdPlacement: shows the active draft values; otherwise keeps the entry | none (widget_history=false; subject/selection state) | OK |
| Military brigade | `brigade_rows_spinner` | number 1..256 | - | updateOwnedCrowdPlacement (re-resolves the owned crowd draft) | refreshCrowdPlacement: shows the active draft values; otherwise keeps the entry | none (widget_history=false; subject/selection state) | OK |
| Military brigade | `brigade_file_spacing_spinner` | number 0.01..100 | - | updateOwnedCrowdPlacement (re-resolves the owned crowd draft) | refreshCrowdPlacement: shows the active draft values; otherwise keeps the entry | none (widget_history=false; subject/selection state) | OK |
| Military brigade | `brigade_rank_spacing_spinner` | number 0.01..100 | - | updateOwnedCrowdPlacement (re-resolves the owned crowd draft) | refreshCrowdPlacement: shows the active draft values; otherwise keeps the entry | none (widget_history=false; subject/selection state) | OK |
| Military brigade | `array_parameter_spinner` | number 0..256 | - | captureFormationParameter + updateOwnedCrowdPlacement (reset: onFormationParamReset via FormationParamBinding) | configureFormationParameter / refreshCrowdPlacement (label, range and value follow the formation) | none (widget_history=false; subject/selection state) | OK |
| Pose strip | `strip_count_spinner` | number 1..64 | - | read by onClickStartStrip (no commit handler) | none (local value read by onClickStartStrip) | none (widget_history=false; subject/selection state) | OK |
| Pose strip | `strip_interval_spinner` | number 0.01..60 | - | read by onClickStartStrip (no commit handler) | none (local value read by onClickStartStrip) | none (widget_history=false; subject/selection state) | OK |
| Pose strip | `btn_strip_start` | button | - | onClickStartStrip() | refreshDetail enable/visibility; result in the list/status | n/a (action/output, no widget step) | OK |
| Pose strip | `btn_strip_cancel` | button | - | onClickCancelStrip() | refreshDetail enable/visibility; result in the list/status | n/a (action/output, no widget step) | OK |
| Pose strip | `strip_status` | readout | - | none (output) | refreshDetail slow tick: setTextIfChanged(mStripStatus) | n/a (action/output, no widget step) | OK |
| Formation motion | `formation_motion_combo` | combo | Off=0; Orbit=1; Spin=2; Breathe=3; Ripple=4; March actor path=5 | read by onMotionCommit (Apply) | none (local choice read by onMotionCommit) | none (widget_history=false; subject/selection state) | OK |
| Formation motion | `formation_motion_speed` | number 0..10 | - | read by onMotionCommit (Apply) | none (local value read by onMotionCommit) | none (widget_history=false; subject/selection state) | OK |
| Formation motion | `formation_motion_amplitude` | number 0..100 | - | read by onMotionCommit (Apply) | none (local value read by onMotionCommit) | none (widget_history=false; subject/selection state) | OK |
| Formation motion | `btn_formation_motion_apply` | button | - | onMotionCommit() | refreshDetail enable/visibility; result in the list/status | n/a (action/output, no widget step) | OK |
| Formation motion | `pose_rate_spinner` | number 0..30 | - | onPoseRateCommit() | refreshDetail: setValueIfChanged(mPoseRate, inst->mPoseRateHz) (added in this conversion; it had no read-back) | none (widget_history=false; subject/selection state) | OK |
| Group as one unit | `btn_group_unit` | button | - | onClickGroup() | refreshDetail enable/visibility; result in the list/status | n/a (action/output, no widget step) | OK |
| Group as one unit | `btn_ungroup_unit` | button | - | onClickUngroup() | refreshDetail enable/visibility; result in the list/status | n/a (action/output, no widget step) | OK |
| Group as one unit | `motion_precedence_help` | readout | - | none (output) | XML text | n/a (action/output, no widget step) | OK |
| Diagnostics | `btn_rig_diag_refresh` | button | - | onClickRigDiagRefresh() | refreshDetail enable/visibility; result in the list/status | n/a (action/output, no widget step) | OK |
| Diagnostics | `rig_diagnostics` | custom | - | none (output) | refreshRigDiagnostics (visible only, revision driven, 240 px max) | none (widget_history=false; subject/selection state) | OK |
| Chaos & speed (card switch) | `chaos_check` | switch | on/off | onChaosCommit() | refreshDetail (selected instance, 10 Hz, diff-guarded; skipped while the control is being edited) | none: card enable switches are not recorded by kit widget history (the handler writes the selected ghost, so recording would be unsafe) | OK |
| Auto-animate (card switch) | `autoanim_check` | switch | on/off | onAutoAnimToggle() | refreshAutoAnimate (selected instance) | none: card enable switches are not recorded by kit widget history (the handler writes the selected ghost, so recording would be unsafe) | OK |
| Head gaze (card switch) | `lens_gaze_check` | switch | on/off | onLensGazeToggle() | refreshDetail: setValueIfChanged(mLensGazeCheck) | none: card enable switches are not recorded by kit widget history (the handler writes the selected ghost, so recording would be unsafe) | OK |

## Notes

- Every row above is OK. Fixes made during the audit: `pose_rate_spinner` gained a read-back; the Formation Parameter reset keeps its per-formation default through `FormationParamBinding`; the overlay-only style rows are hidden row by row for entity clones.
- The aluiperfselftest functional probe for Ghosts needs no column names (it drives widgets by name).

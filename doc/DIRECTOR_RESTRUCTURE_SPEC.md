# Director restructure (P2b) + shared-panel extractions (P2a): implementation spec

Opus, 2026-10-09, read at HEAD 4d4279b8c93. Source audit: C:\Users\xianw\cxs\director_integration_audit.md.

USER DECISIONS:
- Director: 8 groups with sub-tabs.
- Lightbox: stays the deep editor, sharing the Director's panels.
- Machinima menu: done in 85dccf6cbae.
- Scope: P1+P2 in one build; Phase 3 later.

## Checker baseline
- floater_director.xml: 7 findings (fixed in B4).
- 0 findings: Lightbox, Exposure hub, Pose Polish, Light Response, Dance Ball, Actor Gaze floater.
- Embedded panels that must NOT be edited: panel_lens_gaze 11, panel_cinecam_params 56, panel_cinecam_operator 4, panel_director_anim_switcher 2, panel_weather_settings 2, panel_atmospheric_volumes 1, panel_cinecam_frame 1, floater_phototools 2. The checker merges only an embedded file's root attributes into the host.

## Order
P2a, then P2b. The file sets do not overlap. The only cross-package API is `ALFloaterLightBox::showTab()` (P2a adds it; P2b calls it). Build once after both are reviewed.

## Global decisions
1. **Owner tokens:** none new.
   - Dance Ball Blocking handlers are stateless commands (alpaneldanceball.cpp:1464-1607).
   - Gaze state is per instance; commits go through editGazeTargetsFor.
   - Exposure, Light Response, Shafts, DoF, Lens Looks and Polish are settings/store only.
   - Path editor: keep mEditOwner (P2b calls revealFor on inner tab changes).
   - Ghost Studio: keep mPlacementOwner.
   - Light Response's per-instance Copy clipboard becomes static.
2. **Reset buttons in new panels:** `commit_callback.function="Machinima.ResetControl"`. Each new class constructor calls `alRegisterMachinimaResetControl()` (alpanelcinecamparams.h, idempotent; precedent: the ALPanelLensGaze ctor). This replaces LightBox.ResetControlDefault / Director.ResetControlDefault in moved blocks.
3. **Embedding:**
   - No `height=` on a `<panel filename>` node.
   - Panel roots carry their natural size.
   - Each new .cpp names its XML in the header comment.
   - `LLPanelInjector<Class> t_x("panel_x")` must match the host's `class=`.
4. **Director build cost: eager this round.**
   - An LLTimer mBuildTimer starts in the ctor. At the end of postBuild, log once: `LL_INFOS("Director") << "Director build " << ms << " ms " << (ms<250?"PASS":"LAZY-NEEDED")`.
   - Zones: `LL_PROFILE_ZONE_NAMED_CATEGORY_UI("Director postBuild")`, plus "ALPanelX postBuild" in every new ALPanel::postBuild.
   - Lazy embedding is Phase 3, only if needed.
5. **Card pattern:** flow_grid with fit_cards="true".
   - New sub-tab grids: `card_width="360" max_columns="3" column_gap="12" row_gap="12" pad="8"`.
   - Camera sub-tabs keep card_width 340; their cards move verbatim.

Launcher card snippet (vary ID/Title/label/tooltip):
```xml
<panel background_visible="false" border="true" follows="left|top" layout="topleft" left="0" name="card_ID" top="0" width="360">
    <text follows="left|top|right" font="SansSerifBold" height="16" layout="topleft" left="8" name="card_ID_title" top="6" width="344">Title</text>
    <text follows="left|top|right" height="16" layout="topleft" left="8" name="card_ID_summary" text_color="LtGray_50" top="26" use_ellipses="true" width="344">-</text>
    <button follows="left|top" height="22" label="Open Title..." layout="topleft" left="8" name="card_ID_open" tool_tip="..." top="48" width="220" />
</panel>
```
Embedding card: title text at top 6, then `<panel class=… filename=… follows="left|top|right" left="0" top="28" width="360" name=…/>`.

Card grid page:
```xml
<panel background_visible="false" follows="all" height="525" label="L" layout="topleft" name="X_tab" width="711">
  <scroll_container follows="all" height="525" layout="topleft" left="0" name="X_flow_scroll" opaque="false" reserve_scroll_corner="false" top="0" width="711">
    <flow_grid card_width="360" column_gap="12" fit_cards="true" follows="all" height="8" layout="topleft" left="0" max_columns="3" name="X_flow_grid" pad="8" row_gap="12" top="0" width="695">…cards…</flow_grid>
  </scroll_container>
</panel>
```

---
# PACKAGE P2a: shared-panel extractions + Lightbox Phase-1 fixes

**New files:**
- panel_pose_polish.xml
- panel_exposure_hub.xml + alpanelexposurehub.{h,cpp}
- panel_avatar_light_response.xml + alpanelavatarlightresponse.{h,cpp}
- panel_projector_shafts.xml + alpanelprojectorshafts.{h,cpp}
- panel_dof.xml + alpaneldof.{h,cpp}
- panel_lens_looks.xml + alpanellenslooks.{h,cpp}
- CMakeLists: 5 .cpp near line 371 and 5 .h near line 1255, alphabetical.

**Modified:** floater_pose_polish.xml; floater_exposure_hub.xml + alfloaterexposurehub.{h,cpp}; floater_avatar_light_response.xml + alfloateravatarlightresponse.{h,cpp}; floater_lightbox_settings.xml + alfloaterlightbox.{h,cpp}.

**Off-limits:** floater_director.xml, llfloaterdirector.*, alscrollfit.*, panel_lens_gaze.xml, panel_dance_ball.xml, panel_cinecam_*.xml, panel_weather_settings.xml, panel_atmospheric_volumes.xml, floater_phototools.xml, alfloaterphototools.*, alpanellensgaze.*, alpaneldanceball.*, settings*.xml, menu_viewer.xml, CLAUDE.md.

### A1. Pose Polish → panel_pose_polish.xml (plain LLPanel, no class)
- All `control_name`, no C++ (llviewerfloaterreg.cpp:780).
- Move floater_pose_polish.xml lines 19-277 (14 control_names, 21 named widgets) verbatim, with every top −22.
- Root: `<panel follows="left|top|right" height="484" layout="topleft" name="panel_pose_polish" width="360">`.
- The floater keeps its root; its only child: `<panel filename="panel_pose_polish.xml" follows="all" layout="topleft" left="0" name="pose_polish_panel" top="22" width="360"/>`.

### A2. Exposure hub → ALPanelExposureHub, panel_exposure_hub.xml
**Root:** `<panel class="panel_exposure_hub" follows="all" height="488" layout="topleft" name="panel_exposure_hub" width="372">`

**Children:**
- `<button name="exp_undo" label="Undo" left="4" top="4" width="60" height="20" follows="left|top" tool_tip="Nothing to undo"/>`
- `<button name="exp_redo" label="Redo" left="68" top="4" width="60" height="20" follows="left|top" tool_tip="Nothing to redo"/>`
- `<tab_container follows="all" height="460" layout="topleft" left="0" name="exposure_tabs" tab_position="top" top="28" width="372">` holding the three pages from floater_exposure_hub.xml:30-160 verbatim (names unchanged).
- New in the Light page: `<check_box control_name="AlchemyEnvLocalLightIncludeRig" follows="left|top" height="18" label="Local lights EV includes rig lights" layout="topleft" left="10" name="exp_local_include_rig" tool_tip="Apply Local lights EV to Cine Light Rig lights too. Same setting as Personal Lighting (incl. rig lights)." top="258" width="322"/>`. Add +24 to every Light-page top ≥ 264 (rows 75-90).

**C++:** move all of alfloaterexposurehub.cpp into ALPanelExposureHub: RESET_SETTINGS, the reset loop, tonemap gating, the Night Mask percent mapping, and the IR/Soft-edge preset combos.

Members:
- `ALScrollFit::Controller mScrollFit`;
- `LLTabContainer* mTabs`;
- `LLButton* mUndo, *mRedo`;
- cached pointers exp_amd_exposure, rst_AlchemyToneMapAMDExposure, exp_slog_ev, rst_AlchemyToneMapSLogEV, exp_nm_darkness (resolved in postBuild);
- `scoped_connection`s for tonemap, night mask, history.

postBuild:
1. `mScrollFit.installForTabs(mTabs, this);`
2. Reset loop. When `ALExposureSettingsHistory::findTrackedSetting(setting)`, wrap resetToDefault in `ALExposureSettingsHistory::ScopedBatch b("Reset " + std::string(tracked->mLabel));`
3. Existing tonemap/night-mask signals.
4. Preset combos.
5. mUndo → `ALExposureSettingsHistory::undo()`; mRedo → `redo()`.
6. `mHistoryConn = ALExposureSettingsHistory::connectRevisionChanged([this](U64){ updateHistoryButtons(); });`
7. `ALExposureSettingsHistory::installEditFinalizers(findChild<LLPanel>("exp_tab_camera"))` and the same for "exp_tab_light", AFTER installForTabs.
8. `updateHistoryButtons()` (copy llfloaterenvironmentadjust.cpp:1190-1208; signals only, never in draw).

`onVisibilityChange(true)`: `mScrollFit.fit(this, mTabs); updateTonemapGating(); syncNightMaskDarkness(); updateHistoryButtons();`. No draw override.

**Floater becomes a thin host:**
- floater_exposure_hub.xml keeps its root; its only child: `<panel class="panel_exposure_hub" filename="panel_exposure_hub.xml" follows="all" layout="topleft" left="4" name="exposure_hub_panel" top="24" width="372"/>`.
- ALFloaterExposureHub stays (the registry uses it): postBuild returns `LLFloater::postBuild()`; onOpen calls only LLFloater::onOpen. Delete everything else.

### A3. Avatar Light Response → ALPanelAvatarLightResponse, panel_avatar_light_response.xml
- Move floater_avatar_light_response.xml:15-380 verbatim, every top −24.
- Root: `<panel class="panel_avatar_light_response" follows="left|top|right" height="556" layout="topleft" name="panel_avatar_light_response" width="360">`; move the floater's root tool_tip onto it.

**C++:** move the whole ALFloaterAvatarLightResponse body into `ALPanelAvatarLightResponse : LLPanel`.
- LLFloater:: calls become LLPanel::.
- `getDerivedHandle<ALPanelAvatarLightResponse>()` replaces the floater handle (alfloateravatarlightresponse.cpp:919).
- Keep the idle callback with idleUpdate's `isWidgetOnScreen(this)` early-out, the flag-only draw() with its zone, and `onVisibilityChange(true)` forcing a refresh.
- The old onOpen body becomes `public: void focusTarget(const LLUUID&)` (pinned key, refreshRows, selectKeys, refreshEditors, refreshStatus).
- mClip / mHaveClip become static.

**Floater:**
- XML keeps its root (360×580, can_resize false, root tool_tip dropped); its only child: `<panel class="panel_avatar_light_response" filename="panel_avatar_light_response.xml" follows="all" layout="topleft" left="0" name="light_response_panel" top="24" width="360"/>`.
- postBuild: `mPanel = findChild<ALPanelAvatarLightResponse>("light_response_panel")`.
- onOpen(key): `LLFloater::onOpen(key); if (mPanel) mPanel->focusTarget(key.isUUID()?key.asUUID():LLUUID::null);`
- onClose: `ALAvatarLightResponse::instance().saveIfPersist();` then LLFloater::onClose.
- Remove the rest.

### A4. Projector Shafts → ALPanelProjectorShafts, panel_projector_shafts.xml
- Source: Lightbox floater_lightbox_settings.xml:5723-5835 (39 control_names + the ps_preset combo; richer tooltips). The Director copy (floater_director.xml:1431-1636) binds the identical 39 settings.
- Moving edits:
  - resets → Machinima.ResetControl;
  - ps_BDMergeProjectorVolumetricsTemporalBlend_slider gets `enabled_control="BDMergeProjectorVolumetricsTemporal"`;
  - every check box / header width → 330, so every child's right edge is ≤ 348;
  - keep the ps_* names, top_pad flow, ps_preset items and tooltip.
- Root: `<panel class="panel_projector_shafts" follows="left|top|right" height="(content bottom+8 per checker)" layout="topleft" name="panel_projector_shafts" width="352">`.
- C++: the ctor calls `alRegisterMachinimaResetControl()`. postBuild: `getChild<LLComboBox>("ps_preset")->setCommitCallback([](LLUICtrl* c, const LLSD&){ ALProjectorShaftPresets::apply(c->getValue().asInteger()); }); return true;`
- Delete the preset wiring at alfloaterlightbox.cpp:178-182 (P2b deletes llfloaterdirector.cpp:280-284).
- Lightbox host (lines 5718-5838) becomes `<panel follows="all" label="Proj Shafts" layout="topleft" name="projshaft_settings" can_resize="false"><panel class="panel_projector_shafts" filename="panel_projector_shafts.xml" follows="left|top|right" layout="topleft" left="0" name="projector_shafts_shared" top="0" width="352"/></panel>`. Remove `{ "projshaft_settings_scroll", … }` from alfloaterlightbox.cpp:170.

### A5. DoF → ALPanelDoF, panel_dof.xml
- Source: the Lightbox DoF page floater_lightbox_settings.xml:4456-4827 (15 control_names / 10 unique), verbatim; resets → Machinima.ResetControl.
- Add the Phototools guide controls (pure control_name, no C++):
  - after "Lock Focus": FocusCrosshair (floater_phototools.xml:217-227, RenderFocusPointCrosshair);
  - at the end: phototools 229-342 (guides_section_label, ShowGuides, GuideStyle, GuideColor, guide_thickness_label + GuideThickness, guide_opacity_label + GuideOpacity), `left="15"`→`left="5"`, keep top_pad/left_pad.
  - Expected: 21 control_names.
- Root: `<panel class="panel_dof" follows="left|top|right" height="(checker)" layout="topleft" name="panel_dof" width="400">`.
- C++: the ctor calls `alRegisterMachinimaResetControl()`.
- Lightbox DoF page (4452-4828) keeps only `<panel class="panel_dof" filename="panel_dof.xml" follows="left|top|right" layout="topleft" left="0" name="dof_shared" top="0" width="400"/>`.
- Phototools unchanged (Phase 3).

### A6. Lens Looks → ALPanelLensLooks, panel_lens_looks.xml
- Source: Lightbox Image page floater_lightbox_settings.xml:912-1083 verbatim: Grad ND + tint + Lens Filter preset/amount, Polarizer, Infrared, Soft-Edge. That is 44 control_names (30 unique) + ir_preset_action, softedge_preset_action, ir_tag_hint, ir_enable_tag, ir_limit_hint, softedge_dof_hint.
- Root: `<panel class="panel_lens_looks" follows="left|top|right" height="(checker)" layout="topleft" name="panel_lens_looks" width="400">`; the first child (gradnd_border) gets `top_pad="0"`.
- C++ moved out of the Lightbox:
  - lines 220-251: hints, preset actions, the tag-confirm notification, 5 setting connections (RenderInfraredEnabled, RenderGBufferAvatarTag, RenderSoftEdgeEnabled, RenderSoftEdgeBlurType, RenderDepthOfField);
  - updateLensLookHints (317-330) → `ALPanelLensLooks::updateHints()`;
  - header members (alfloaterlightbox.h:69-77), destructor disconnects (cpp 151-155), the onOpen call (cpp 314).
- Change: `setVisibleIfChanged(mInfraredEnableTag, !tag_wanted)`, so the Enable-tag button shows whenever the tag is off, independent of IR.
- `onVisibilityChange(true)` calls `updateHints()` (re-reads gbufferAvatarTagActive, which has no signal).
- Includes: allenslooks.h, llviewershadermgr.h (gOnLensNoIR), llnotificationsutil.h, aluirefresh.h.
- Lightbox host: replace 912-1083 with `<panel class="panel_lens_looks" filename="panel_lens_looks.xml" follows="left|top|right" layout="topleft" left="0" name="lens_looks_shared" top_pad="10" width="400"/>`. Leave the commented block (1084-1342) alone.

### A7. Launcher cards now; panels in Phase 3
Roto Ink, Tron, Selected Light, Actor Mask and Depth Haze are not cleanly separable:
- Roto Ink: 3 handlers + 10 connections + a findChild visibility pass.
- Tron: 8 handlers + 16 connections + updateTronVisibility (~40 names) + a draw() readout.
- Selected Light: updated from ALFloaterLightBox::draw() every frame.

### A8. Lightbox Phase-1 fixes
1. Line 10: `title="Lightbox"`. The status-bar tooltip is already done.
2. Line 6814 tr_suit_hint → "Actor FX look for cast members: Director > Look > Actor FX > Look: Tron Suit. Layer keeps clothing and adds seams; Cover gives the black suit."
3. Avatar-tag tooltips:
   - 6200 ri_subject_source_combo: replace "needs 'Avatar tag in G-buffer' (RenderGBufferAvatarTag) and HDR" with "needs the avatar tag (click 'Enable avatar tag' in the Infrared Film row of Lightbox > Image or Director > Lens > Filters) and HDR";
   - 6963 / 6970: "Needs RenderGBufferAvatarTag + HDR" → "Needs the avatar tag ('Enable avatar tag' button, Infrared Film row) + HDR";
   - 6976 tr_subject_source_hint → "(tag unavailable: click 'Enable avatar tag' in Lightbox > Image; needs HDR)".
4. Glow tab: label `Prim Glow` (line 4208). First child: `<text follows="left|top" font="SansSerifSmall" height="30" layout="topleft" left="5" name="glow_prim_hint" top_pad="6" width="380" word_wrap="true" value="Prim Glow: the legacy per-face Glow on objects. Scene bloom (HDR) is in Exposure > Post." />`. Keep the tab.
5. Froxel (line 5843): label `Froxel Air (legacy)`. Keep the tab.
6. Weather page (5914-5931) → `<panel follows="all" label="Weather" layout="topleft" name="weather_settings" can_resize="false"><panel class="panel_weather_settings" filename="panel_weather_settings.xml" follows="left|top|right" layout="topleft" left="10" name="weather_settings_shared" top="4" width="350"/></panel>`. Atmos page (5932-5938): same form with panel_atmospheric_volumes, name="atmospheric_volumes_shared". This removes the typed heights 710/650 and the hand-typed doc heights 724/660. Remove `{ "weather_settings_scroll", … }` (cpp 172).
7. New API in alfloaterlightbox.h: `public: static void showTab(const std::string& tab_panel_name);`, implemented as `if (auto* lb = LLFloaterReg::showTypedInstance<ALFloaterLightBox>("lightbox")) if (auto* t = lb->findChild<LLTabContainer>("lightbox_tab_container")) t->selectTabByName(tab_panel_name);`. Use the DEFAULT key: Lightbox is not single-instance, so a string key opens a second one.
8. Out of scope (Phase 3): the untouched tabs' hand-typed doc heights (render 872, shadow 1130, froxel 674, cinehaze 700, actor_mask 640, rotoink 2520, tron 4100) and the ALFloaterLightBox::draw() rule 1/8 fixes.

### P2a gates
1. `--selftest`: all PASS.
2. `--changed HEAD`: 0 (tracked files only).
3. EXPLICIT per-file runs, because untracked files are invisible to `--changed`:
   `foreach ($f in 'panel_pose_polish','panel_exposure_hub','panel_avatar_light_response','panel_projector_shafts','panel_dof','panel_lens_looks','floater_pose_polish','floater_exposure_hub','floater_avatar_light_response','floater_lightbox_settings') { python scripts/code_tools/xui_layout_check.py "indra/newview/skins/default/xui/en/$f.xml" }`
   Each must report 0. Each panel root height = checker content bottom + 8.
4. control_name counts:

   | Panel | Count |
   |---|---|
   | shafts | 39 |
   | lens_looks | 44 |
   | dof | 21 |
   | pose_polish | 14 |
   | exposure_hub | 32 |
   | avatar_light_response | 2 |

5. Set equality: each new panel's sorted control_name set equals its source block (`git show HEAD:<file>`); Shafts also equals Director block 1431-1636.
6. Lightbox tab count stays 17.
7. alfloaterlightbox.cpp has no remaining ir_ / softedge_ / ps_preset lookups.

---
# PACKAGE P2b: Director restructure

**Modified:** floater_director.xml, llfloaterdirector.{h,cpp}, alscrollfit.{h,cpp}, app_settings/settings.xml (comments only).
**Off-limits:** all P2a files and every embedded panel_*.xml (lens_gaze, dance_ball, cinecam_*, director_*switcher, weather, atmospheric_volumes, ghost_studio, prop_mover, path_editor, cine_light_rig, temporal_capture, flycam_*), menu_viewer.xml, CLAUDE.md.
**Depends on:** P2a XML names/classes and `ALFloaterLightBox::showTab`.

### B1. ALScrollFit additions (opt-in; defaults unchanged)
- Problem: wrapTabPanel would wrap group pages (whose content is a follows-all nested tab container) and fitContainer's FIT_PAD of 12 leaves a permanent outer scrollbar. Self-scrolling panels (Dance Ball, Exposure, which own their tabs and ALScrollFit) must fill their page, not be wrapped.
- Add to ALScrollFit::Controller:
  ```cpp
  // Tab pages installForTabs must not wrap (by LLPanel name): group pages holding a follows="all"
  // nested tab container, and pages hosting a self-scrolling shared panel. Call before installForTabs.
  void setUnwrappedPages(std::vector<std::string> names) { mUnwrapped = std::move(names); }
  private: std::vector<std::string> mUnwrapped;
  ```
  installForTabs' wrap loop skips pages whose getName() is in mUnwrapped.
- Free function `void fillPage(LLView* page, LLView* child, S32 top_px);`:
  `w = page->getLocalRect().getWidth() - child->getRect().mLeft; h = page->getLocalRect().getHeight() - top_px; child->reshape(llmax(1,w), llmax(1,h), true); child->setOrigin(child->getRect().mLeft, 0);`
- No other caller sets mUnwrapped (Lightbox, Exposure, env-adjust, joystick and Dance Ball are unaffected; none has a tab page whose direct child is a tab container).

### B2. XML structure
**Root:** `height="650" width="1020" min_height="560" min_width="850"`. Gaze is 516 wide; the sub-page width is floater − 309, so the minimum is ~841. 850 also fixes the path editor (500), clipped at today's 660. The transport bar, arming, cast column, status and legend keep their geometry except the B4 fixes.

**Outer rail:**
```xml
<tab_container follows="all" height="550" layout="topleft" left="196" top="74" name="director_tabs" tab_height="22" tab_width="100" tab_group="1" tab_position="left" width="816">
```

**Group page template (×8):**
```xml
<panel background_visible="false" follows="all" height="548" label="Cast" layout="topleft" name="cast_group" top="0" width="713">
  <tab_container follows="all" height="548" layout="topleft" left="0" name="cast_tabs" tab_height="22" tab_min_width="60" tab_position="top" top="0" width="713"> …sub-pages… </tab_container>
</panel>
```
- New sub-pages: `width="711" height="525"`; explicit scrollers inside: `left="0" top="0" width="711" height="525"`.
- Verbatim pages keep their current declared sizes.

**Groups ("verbatim" = the existing block moves unchanged):**

1. **Cast** (cast_group / cast_tabs, Command_People_Icon)
   - Move: move_tab (327-501 verbatim)
   - Path: path_tab (509-545)
   - Props: props_tab (578-596)
   - Ghosts: ghosts_tab (553-571)
   - Dance: dance_tab (new, UNWRAPPED): button dance_popout "Open Dance Ball window..." (left 8, top 4, w 200, h 20) + `<panel class="panel_dance_ball" filename="panel_dance_ball.xml" follows="all" layout="topleft" left="0" name="director_dance_ball_panel" top="30" width="711"/>`
2. **Performance** (performance_group / performance_tabs, Command_Poser_Icon)
   - Animate: animate_tab (605-907; B4 text fixes)
   - Switchboard: switchboard_tab (card grid with anim_switcher_card 1222-1251 verbatim; grid switchboard_flow_grid, card_width 340)
   - Gaze: gaze_tab (new, wrapped): btn_actor_gaze moved from 1039-1048 (label "Open Actor Gaze window...", left 8, top 4, w 200, h 20, follows left|top) + `<panel class="panel_lens_gaze" filename="panel_lens_gaze.xml" follows="left|top" layout="topleft" left="0" name="director_gaze_panel" top="30" width="516"/>`
   - Polish: polish_tab (new, wrapped): button polish_popout "Open Pose Polish window..." + `<panel filename="panel_pose_polish.xml" follows="left|top" left="0" name="director_pose_polish" top="30" width="360"/>`
   - Gaze Cues: gaze_cues_tab (card grid: launcher card gaze_cues)
3. **Camera** (camera_group / camera_tabs, Command_View_Icon; grids card_width 340; cards verbatim)
   - Shot: shot_tab: new card shot_subjects_card (title "Subjects" + 919-1038, subject_a_text … btn_clear_d, every top +22), camera_switcher_card (1116-1145), camera_cinematic_card (1252-1281)
   - Moves: moves_tab (camera_shot_builders_card 1146-1221)
   - Operator: operator_tab (camera_shake_card 1342-1371)
   - Frame & Orbit: frame_orbit_tab (camera_frame_card 1312-1341, camera_flycam_orbit_card 1282-1311)
   - Takes: takes_tab (1381-1399 verbatim)
4. **Lens** (lens_group / lens_tabs, Command_Search_Icon)
   - Focus: focus_tab (wrapped): `<panel class="panel_dof" filename="panel_dof.xml" follows="left|top" left="0" name="director_dof_panel" top="4" width="400"/>`
   - Filters: filters_tab (wrapped): panel_lens_looks, name director_lens_looks_panel, same attributes
   - VCam: vcam_tab, card lens_vcam_card: title "Virtual Cam" + prism_lens_enabled (moved from 1059-1069; left 8, top 28) + prism_summary (1070-1081; left 150, top 30, w 202) + btn_prism_manage (1049-1058; label "Open Virtual Cam Manager...", left 8, top 52, w 220, follows left|top)
   - Diopter: diopter_tab (launcher diopter; delete btn_ultimate_diopter 1082-1091)
5. **Light** (light_group / light_tabs, Command_PersonalLighting_Icon)
   - Rig: cine_light_rig_tab (1642-1670 verbatim; label "Rig")
   - Exposure: exposure_tab (new, UNWRAPPED): button exposure_popout "Open Exposure window..." + `<panel class="panel_exposure_hub" filename="panel_exposure_hub.xml" follows="all" left="0" name="director_exposure_panel" top="30" width="711"/>`
   - Practicals: practicals_tab (launcher practicals)
   - Actors: actors_tab with two cards:
     - actors_light_response_card: title "Avatar Light Response" (w 200) + button light_response_popout "Open in window..." (left 212, top 3, w 140) + `<panel class="panel_avatar_light_response" filename="panel_avatar_light_response.xml" follows="left|top|right" left="0" name="director_light_response_panel" top="28" width="360"/>`
     - launcher actor_mask
   - Light Cues: light_cues_tab (launcher light_cues)
6. **Look** (look_group / look_tabs, Command_Lightbox_Icon)
   - Image: image_tab (launcher image)
   - Stylize: stylize_tab (launchers rotoink, tron)
   - Actor FX: actor_style_tab (1782-1960 verbatim, inline; header text "Actor FX"; B4 hint)
7. **World** (world_group / world_tabs, Command_Environments_Icon)
   - Weather: weather_tab (1675-1711 verbatim)
   - Atmosphere: atmosphere_tab with three cards:
     - atmos_volumes_card: title "Atmospheric Volumes" + `<panel class="panel_atmospheric_volumes" filename="panel_atmospheric_volumes.xml" follows="left|top|right" left="4" name="director_atmospheric_volumes_shared" top="28" width="350"/>`
     - atmos_shafts_card: title "Projector Shafts" + `<panel class="panel_projector_shafts" filename="panel_projector_shafts.xml" follows="left|top|right" left="0" name="director_projector_shafts" top="28" width="352"/>`
     - launcher haze
   - Shadows & Probes: shadows_probes_tab (launcher shadows_probes)
8. **Capture** (capture_group / capture_tabs, Command_Snapshot_Icon)
   - Time: temporal_tab (1759-1777 verbatim)
   - Capture settings: capture_settings_tab (launcher snapshot)
   - Scene options: scene_options_tab (wrapped, inline):
     - bold text "Hotkeys";
     - `<check_box control_name="DirectorHotkeysEnabled" name="scene_hotkeys_enabled" label="Enable Director hotkeys" …/>`;
     - word-wrap scene_hotkeys_list (follows left|top|right, w 680): "F2 toggle console · F3 ACTION / CUT · F4 CUT · F5 reset to marks · F6 pose ghosts on/off · F7 take play/pause · F8 set marks · 1-9 punch camera-switcher slots (console visible, switcher armed, no text focus) · Alt+1-9 punch Anim Switchboard slots (console open, switchboard armed, no text focus) · Esc CUT while running (console focused). F-keys work while the Director or Actor Mover is open; F2 works always.";
     - bold text "What a scene saves";
     - word-wrap scene_categories_text: "Cast, marks, groups and start delays, locomotion roles, subjects A-D, actor paths, per-actor gaze targets, performance and gaze cues, Actor FX styles; transport arming; camera switcher program; Flycam Orbit; Cinematic camera mode and preset; frame/lens guide; Actor Mover settings; Temporal capture modes; Virtual Cam captures/displays/gate; light rigs, Rig Rim, Roto Ink, Tron. Not saved yet: exposure/tonemap, lens looks, DoF, depth haze, projector shafts, weather, scene-light EVs, Pose Polish, Diopter.";
     - heights per the checker.

**Deleted blocks:** camera_tab (910-1374, dismantled as above), projector_volumetrics_tab (1401-1639), atmospheric_volumes_tab (1713-1750; content moved to the Atmosphere card).

**Launcher cards (title / button / target):**
- gaze_cues: "Gaze Cues" / "Open Gaze Cues..."
- diopter: "Ultimate Diopter" / "Open Ultimate Diopter..."
- practicals: "Selected Light" / "Open Selected Light..."
- actor_mask: "Actor Mask" / "Open Actor Mask..."
- light_cues: "Lighting Cue Stack" / "Open Cue Stack..."
- image: "Image (tone, LUT, sharpen)" / "Open Lightbox Image..."
- rotoink: "Roto Ink" / "Open Roto Ink..."
- tron: "Tron" / "Open Tron..."
- haze: "Depth Haze" / "Open Depth Haze..."
- shadows_probes: "Shadows & Probes" / "Open Shadows..."
- snapshot: "Snapshot" / "Open Snapshot..."

### B3. C++ in llfloaterdirector.{h,cpp}
1. **Delete:** the Director.ResetControlDefault registrar (242-251); the director_ps_preset wiring (280-284) + `#include "alprojectorshaftpresets.h"`; the btn_ultimate_diopter wiring (598-599); the TAB_ICON_* table (80-90, 322-342).
2. **Rail icons** by group page name via `mTabContainer->setTabImage`.
3. **Groups.** Member `std::vector<std::pair<LLPanel*, LLTabContainer*>> mGroups`, built in postBuild from each outer page + its direct-child LLTabContainer. Connect `[this]{ onTabChanged(); }` to the outer container AND each inner container's commit callback.
   - `LLPanel* currentLeafPanel()` (non-const).
   - `bool selectLeafTab(const std::string&)`: for each (g, t), if `t->getPanelByName(name)` then `mTabContainer->selectTabPanel(g); t->selectTabPanel(leaf); return true;`
4. **Scroll fit.** Before installForTabs (line 620): `mScrollFit.setUnwrappedPages({"cast_group","performance_group","camera_group","lens_group","light_group","look_group","world_group","capture_group","dance_tab","exposure_tab"});`. After it, `ALScrollFit::fillPage(dance_tab, director_dance_ball_panel, 30)` and the same for exposure. Repeat both fillPage calls in onOpen after `mScrollFit.fit`.
5. **DirectorLastTabName migration** (anonymous namespace):
   - `OLD_FLAT_TABS[] = {"move_tab","path_tab","ghosts_tab","props_tab","animate_tab","camera_tab","takes_tab","projector_volumetrics_tab","cine_light_rig_tab","weather_tab","atmospheric_volumes_tab","temporal_tab","actor_style_tab"};`
   - migrate_tab_name: camera_tab→shot_tab; projector_volumetrics_tab and atmospheric_volumes_tab→atmosphere_tab; everything else passes through.
   - onOpen:
     1. name = migrate(DirectorLastTabName); try selectLeafTab;
     2. else idx = clamp(DirectorLastTab, 0, 12); try selectLeafTab(migrate(OLD_FLAT_TABS[idx]));
     3. else selectLeafTab("move_tab");
     4. write DirectorLastTabName = the leaf name.
   - onTabChanged writes the leaf name. It writes DirectorLastTab only when the leaf (reverse-mapped shot_tab→camera_tab, atmosphere_tab→projector_volumetrics_tab) is in OLD_FLAT_TABS. It keeps the dirty flags + `mPathPanel->revealFor(firstSelectedCastId())`.
   - settings.xml comments: DirectorLastTabName ("leaf sub-tab panel name, e.g. move_tab, shot_tab"); DirectorHotkeysEnabled (add Alt+1-9 and Esc).
6. **Gaze.** `ALPanelLensGaze* mGazePanel = findChild<ALPanelLensGaze>("director_gaze_panel")`. In the existing selection-change push (3168-3173), also call `mGazePanel->setSelectedActors(selected)` (store-only; equivalent to ESelectionSource::DIRECTOR). onOpen pushes selectedCastIds() once. onOpenActorGaze is unchanged and the button keeps its name.
7. **Pop-outs:**
   - dance_popout → showInstance("dance_ball")
   - polish_popout → "pose_polish"
   - exposure_popout → "exposure_hub"
   - light_response_popout → `showInstance("avatar_light_response", LLSD(firstSelectedCastId().notNull() ? firstSelectedCastId() : gAgentID))`
8. **Launcher cards.**
   - Members: `struct LauncherCard { LLTextBox* mSummary = nullptr; std::function<std::string()> mFn; }; std::vector<LauncherCard> mLauncherCards; ALLiveReadoutClock mLauncherClock{2.f};`
   - postBuild: `addLauncherCard(id, open_fn, summary_fn)` resolves card_<id>_open and card_<id>_summary once.
   - draw: `refreshLauncherCards()` with zone "Director refreshLauncherCards": `if (!mLauncherClock.due()) return; for (c) if (isWidgetOnScreen(c.mSummary)) setTextIfChanged(c.mSummary, c.mFn());`
   - Summaries use `static LLCachedControl` only:

   | Card | Opens | Summary |
   |---|---|---|
   | gaze_cues | `showInstance("gaze_cues", LLSD(actor.asString()))`; actor = first selected cast member, else gAgentID | "<name>: N gaze cues" (`LLDirectorCast::getGazeCues(actor).size()`) |
   | diopter | "ultimate_diopter" | CineDiopterEnabled → "Diopter: on/off" |
   | practicals | `ALFloaterLightBox::showTab("sellight_settings")` | primary-else-first-root selected object as LLVOVolume: "Projector selected" / "Point light selected (needs a projector)" / "Selection is not a light" / "Select a projector in world" (getIsLight, isLightSpotlight) |
   | actor_mask | showTab("actor_mask_settings") | CineLightRigActorMaskDisable → "Actor-only lights: disabled/active" |
   | light_cues | "cine_light_cues" | `ALCineLightRigManager::instance().selected().cueList()` → "N cues · <name>" |
   | image | showTab("colortone_settings") | tonemapper label from AlchemyRenderTonemapType 0-8 + " · Exposure %.2f" (RenderExposure) + " · LUT" when RenderColorGrade |
   | rotoink | showTab("rotoink_settings") | CineOutlineEnabled On/Off |
   | tron | showTab("tron_settings") | TronEnabled On/Off |
   | haze | showTab("cinehaze_settings") | CineHazeEnable On/Off |
   | shadows_probes | showTab("shadow_settings") | RenderShadowDetail 0 Off / 1 Sun/moon / 2 Sun/moon + projectors |
   | snapshot | showInstance("snapshot") | static "Stills and high-resolution frames" |

9. **Rule 8 / rule 1:**
   - Split refreshCameraTab into:
     - (a) the always-on gaze push;
     - (b) refreshSubjects(), early-out unless `mSubjectAText->isInVisibleChain()`;
     - (c) refreshPrismDependents(), early-out unless `mOtsPairBtn->isInVisibleChain() || mPrismSummaryText->isInVisibleChain()`, keeping the 0.25 s poll.
   - setEnabled → setEnabledIfChanged; `mPrismSummaryText->setText` → setTextIfChanged.
   - refreshGroupControls: keep ungated the name-set rebuild (2048-2068) and the cast-column assign combo (2072-2082). Gate everything after 2083 on `mGroupsList->isInVisibleChain()`. On the first visible pass after onTabChanged (new `bool mGroupsTabDirty`), clear mGroupsListSnapshot and mGroupDelayShownFor. All controls use setEnabledIfChanged.
   - Actor FX / Animate gating stays; the dirty flags are now set by inner tab changes too.
10. **Other:**
    - onClose adds `ALAvatarLightResponse::instance().saveIfPersist();`
    - Remove "CineLightRigPower" from sceneSettingsList() (cpp:978). It is deprecated; light_rig sceneData still carries it, so old scenes are fine.
    - No other scene key or control_name changes.
    - Build timer + zones per global decision 4.

### B4. Fix the 7 existing floater_director.xml findings
1. arming_lbl width 74→88; shift arm_moves_check 84→98, arm_camera_check 182→196, arm_play_check 262→276, arm_capture_check 370→384.
2. `<!-- xui-check: overlay cast_hint cast_list -->`
3. `<!-- xui-check: allow-overlap director_tabs legend_panel -->`. This annotation must actually be supported by the checker; if it is not, use the supported `exclusive` / `overlay` / `independent` annotation that expresses a popup-over-content relationship, and verify with the checker.
4. `<!-- xui-check: overlay anim_hint anim_list -->`
5. paste_lbl text → "Play any animation locally on the selected member:"
6. actor_style_hint: `word_wrap="true"`, text → "Style You or a cast member with cinematic looks. Client-side only."
7. Legend: legend_panel height 280→400, legend_body 234→326, btn_legend_close top 252→368. The Hotkeys paragraph → "Hotkeys: see Capture > Scene options."

### P2b gates
1. The checker on floater_director.xml gives 0 (from 7); `--changed HEAD` gives 0; `--selftest` passes.
2. control_name= count is exactly 20 (58 − 39 shafts + DirectorHotkeysEnabled); the set equals old − shafts + DirectorHotkeysEnabled.
3. Every getChild/findChild name in llfloaterdirector.cpp exists in the XML:
   `$x=Get-Content floater_director.xml -Raw; [regex]::Matches((Get-Content llfloaterdirector.cpp -Raw),'(?:getChild|findChild)<[^>]+>\("([^"]+)"\)')|%{$_.Groups[1].Value}|sort -u|?{$x -notmatch ('name="'+[regex]::Escape($_)+'"')}` must print nothing.
4. The sceneSettingsList diff shows only the CineLightRigPower line removed.
5. No `height=` on any `<panel … filename=…>` in the Director.

## Risks
- **Shared ALScrollFit:** the skip list is opt-in. Unwrapped self-scrolling pages need fillPage, or Dance (568) overflows a 495-px page.
- **Two controllers on Dance/Exposure:** both refit the same docs. Their documents have no follows top+bottom children, so nothing collapses.
- **Nested commits:** selectLeafTab fires onTabChanged twice. That is idempotent; there must be no tab selection inside onTabChanged.
- **Path editor:** reclaims via revealFor on inner tab changes; its own onVisibilityChange claim stays as a fallback.
- **ALPanelLensGaze::draw():** already refreshes every frame (rule-1 debt); with two hosts visible it costs double. Phase 3.
- **Build hitch:** measured by the log verdict.
- **min_width 850:** saved rects are clamped up.
- **Lightbox showTab:** must use the DEFAULT key.
- **Machinima.ResetControl:** must be registered in the ctor, or the Lightbox DoF/Shafts resets silently do nothing.
- **Exposure undo:** installEditFinalizers only on the Camera and Light pages.
- **Checker blind spots:** host checks don't see embedded children, and `--changed` skips untracked files. Run explicit per-file checks.

## In-world test checklist
**Director:**
- 8 rail groups with icons; every sub-tab opens.
- No double scrollbars on Dance/Exposure; Gaze not clipped at 850.
- Reopen restores the last sub-tab; old camera_tab → Shot, projector_volumetrics_tab → Atmosphere.
- Path tab claims the edit node via the sub-tab.
- Alt+1-9 punches the Switchboard; 1-9 punches the camera switcher; the DirectorHotkeysEnabled box toggles F2.
- Launcher summaries update ≤ 2 Hz and open the right Lightbox tab.
- Light Response in the Director and the floater edit the same avatar; Copy in one host, Paste in the other.
- Exposure Undo/Redo enable after a Sun EV drag and revert it.
- The shafts preset applies from both hosts, and the Lightbox shafts tab mirrors the Director.
- Scene save/load round-trips; an old scene with CineLightRigPower loads with no error.
- The log shows the Director build ms verdict and no UIFitDiagnostics warning for floater_director.

**Lightbox:**
- Title "Lightbox"; Prim Glow hint; Froxel Air (legacy).
- Image › Lens Looks: the IR tag button shows whenever the tag is off; the Soft-edge DoF hint toggles with DoF.
- The DoF tab crosshair and composition guides work.
- Weather and Atmos scroll.
- The Pose Polish, Exposure and Light Response floaters look and behave as before.

## Phase 3
- Roto Ink / Tron / Selected Light / Actor Mask / Depth Haze panels.
- Lightbox grouped rail; the remaining 7 typed doc heights; Lightbox draw() rule 1/8 fixes.
- Phototools Focus & Guides embeds panel_dof.
- Gaze draw() rework + its 11 findings.
- Fisheye/Pedro card; gamepad summary; Bloom & Glow + advanced bloom; grade UI; motion blur; Godrays.
- Scene v7.
- Lazy embedding if LAZY-NEEDED.
- Actor FX extraction if a second host appears.

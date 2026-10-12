# Static wiring audit: Director Camera and Capture pages

Scope: read-only static review of the eight page files in `indra/newview/skins/default/xui/en/` (panel_dpage_camera_frame_orbit, _moves, _operator, _shot, _takes, _vcam; panel_dpage_capture_scene_options, _temporal). No build, run, commit or fetch was performed. Only this file was written.

Verdict key: OK / UNBOUND / UNREACHABLE / MISSING-SETTING / TYPE-MISMATCH / RANGE / LABEL-MISMATCH / UNSURE.

## Problems first

| Page | Widget | Kind | Label | Binding | Verdict | Reason |
|---|---|---|---|---|---|---|
| panel_dpage_camera_vcam.xml | vc_tv_preset | combo | Presets (TV effects card) | commitDisplay, alpaneldirectorvcam.cpp:1234-1241 | LABEL-MISMATCH | Help says "Apply Clean, Old CRT, Broken TV, VHS Tape, or Security Feed". Value "clean" hits `return` at 1237-1240 before any effect reset, so Clean does nothing and the combo keeps showing Clean. Other four presets work (1243-1274). |
| panel_dpage_camera_vcam.xml | vc_proj_tv_preset | combo | Presets (More effects TV card) | commitProjector, alpaneldirectorvcam.cpp:1351-1357 | LABEL-MISMATCH | Same defect: "clean" returns at 1354-1358 with no effect change. Also this path never resets the combo to "Pick..." (unlike vc_tv_preset at 1274), so the combo stays on the applied preset. |

No UNBOUND, UNREACHABLE, MISSING-SETTING, TYPE-MISMATCH or RANGE findings in the eight pages.

## Notes that do not change the verdict

- vc_bone_enabled, vc_bone_scale, vc_bone_* (Skeleton card): commitCamera writes `mBonePov` values, but `vc_bone_enabled` is `value && settings.mVirtual` (alpaneldirectorvcam.cpp:1180), so the toggle is inert on object and test-pattern cameras. Help text says "virtual camera", so this is a UX gap, not a broken binding.
- vc_capture_budget (Performance): item 0 is labelled "Adaptive 30 / Manual every frame". Setting comment (settings.xml:29583) says 0 means Every Frame in Manual mode. llprismlens.cpp:332 accepts 0. Label matches the comment; the Adaptive part is not independently verified.
- vc_new_face_fit (Faces): not read back in refresh(). The value is panel-level (mNewFaceFit), so the widget keeps its own state. Consistent, not a fault.
- vc_proj_tv_preset: does not reset to "Pick..." after applying (see Problems).
- Dead code: `LLFloaterDirector::bindCameraPage` (llfloaterdirector.cpp:1163-1258, declared llfloaterdirector.h:90) is never called. The Shot and Moves buttons are bound in postBuild (llfloaterdirector.cpp:580-629) and work. Only a hygiene issue; calling bindCameraPage later would double-attach callbacks.

## Page: panel_dpage_camera_vcam.xml

Wiring basis: `ALPanelDirectorVCam::postBuild` (alpaneldirectorvcam.cpp:167) calls `collectControls(this)` (152-165), which collects every LLUICtrl named `vc_*`, then attaches `onControl` as commit callback (204-206). Every `vc_*` widget that is not a text readout goes through `onControl` (869-1134), which dispatches to commitCamera (1136), commitDisplay (1197) or commitProjector (1280). Readouts are written in refresh() (357-800) or in postBuild info rows (208-228). Custom widgets are in alvcammonitorstrip.cpp and are registered at lines 22-37. Every `vc_*` name used in XML is also referenced in the C++ file (checked against the listing of 324 references).

| Page | Widget | Kind | Label | Binding | Verdict | Reason |
|---|---|---|---|---|---|---|
| vcam | vc_enable | kit master switch | Virtual Cam (page master) | ALKitSwitch via ALKitPage::postBuild (alkitpage.cpp:477-501), setControlName PrismLensEnabled; onControl 878 writes setBOOL | OK | Master is created in ALKitPage::postBuild, so it exists before the parent collectControls. Boolean setting exists (settings.xml, PrismLensEnabled, Boolean, default 0). |
| vcam | vc_summary | readout | Status | refresh 374 | OK | Text written each refresh. |
| vcam | vc_perf | readout | Performance | refresh 377 | OK | Text written. |
| vcam | vc_status | readout (hidden until used) | Action | setStatus 801-822 | OK | Shown by setStatus; cleared on timer (draw 263). |
| vcam | vc_open_manager | button | Open full manager | onControl 879 | OK | LLFloaterReg "prism_manager" registered (llviewerfloaterreg.cpp:782). |
| vcam | vc_monitor_header | custom | (monitor header) | ALVCamMonitorHeader::handleMouseDown (alvcammonitorstrip.cpp:172-200) | OK | Emits size / previews / speed; onControl 880-893 writes PrismMonitorPreviewFPS (F32, default 1.0; header speeds 0.5/1/2/5 match settings comment). |
| vcam | vc_monitor_pager | custom (hidden unless >4 cams) | Pages | ALVCamMonitorPager::handleMouseDown (526-533) | OK | onControl 894 refreshes page. Row visible toggled in refresh (454). |
| vcam | vc_monitor_strip | custom | (monitor strip) | ALVCamMonitorStrip::handleMouseDown (330-360) and drag handling | OK | Menu and drag_rejected actions handled at onControl 909-912. |
| vcam | vc_monitor_footer | readout | Preview cost | refresh 779-790 | OK | Text written. |
| vcam | vc_camera_selector | custom | Settings for | ALVCamCameraSelector::handleMouseDown (619-631) | OK | onControl 900-908 selects capture. |
| vcam | vc_add_camera | combo (child of selector) | + Add camera... | Created in ALVCamCameraSelector::postBuild (alvcammonitorstrip.cpp:549-558) | OK | Child exists before panel collectControls. Clicks reach it: selector calls LLUICtrl::handleMouseDown first (623), which runs childrenHandleMouseDown (llview.cpp:1174). onControl 970-980 dispatches object / virtual / ots / lens. |
| vcam | vc_remove_capture | button (child of selector) | Remove capture | Created in ALVCamCameraSelector::postBuild (560-566) | OK | Same reachability as vc_add_camera. onControl 981-995 confirms, then removeCapture. |
| vcam | vc_source_type | segmented radio | Source | onControl 1093-1117 | OK | Object/Virtual/Skeleton/Test mapped to mTestPattern, mVirtual, mBonePov.mEnabled. Refresh 593-595. |
| vcam | vc_source_bound | readout | Bound to | refresh 587 | OK | Text. |
| vcam | vc_source_note_object | readout | Object source note | info row 209; refresh 611 | OK | Visibility toggled in refresh. |
| vcam | vc_source_note_virtual | readout | Virtual source note | info row 210; refresh 612 | OK | Visibility toggled in refresh. |
| vcam | vc_source_note_skeleton | readout | Skeleton source note | info row 211; refresh 613 | OK | Visibility toggled in refresh. |
| vcam | vc_lens_type | readout | Type | info 212; refresh 622 | OK | Text. |
| vcam | vc_lens_aperture | readout | Aperture | refresh 605, 623 | OK | Text. |
| vcam | vc_lens_locate | button | Locate aperture | onControl 1008-1028 | OK | Selects and frames aperture object. |
| vcam | vc_lens_note | readout | Surface Lens note | info 213; refresh 625 | OK | Text. |
| vcam | vc_set_selected_camera | button | Set selected camera | onControl 996-1000 (LLPrismLens::setSelectedCamera) | OK | Visibility toggled in refresh 614. |
| vcam | vc_place_eye_front | readout | Place eye in front | info 214; refresh 615 | OK | Info text. |
| vcam | vc_placement_note | readout | Marker convention | info 215; refresh 616 | OK | Info text. |
| vcam | vc_eye_x | slider -64..64 m | Eye offset X | commitCamera 1143 -> mLocalEyeOffset.mV[0] | OK | Label and units match. |
| vcam | vc_eye_y | slider -64..64 m | Eye offset Y | commitCamera 1144 | OK | Matches. |
| vcam | vc_eye_z | slider -64..64 m | Eye offset Z | commitCamera 1145 | OK | Matches. |
| vcam | vc_snap_view | button | Snap to my view | onControl 1001-1007 (currentViewTransform) | OK | Stores view as virtual transform. |
| vcam | vc_virtual_note | readout | Virtual placement note | info 216; refresh 621 | OK | Text. |
| vcam | vc_bone_enabled | kit switch | Bone POV | commitCamera 1180 | OK | Inert on non-virtual cameras (see Notes). |
| vcam | vc_bone_anchor | combo Me/A/B/C/D | Anchor subject | commitCamera 1162 -> mAnchorSlot | OK | BONE_ANCHOR_ME..D = 0..4 (llprismlens.h:161-165). |
| vcam | vc_bone_joint | combo | POV joint | commitCamera 1163-1170 | OK | "" -> EYELINE; names -> NAMED; spine test sets stabilized aim. |
| vcam | vc_bone_aim | combo | Aim mode | commitCamera 1171 -> mAimMode | OK | Full-follow 0 / Stabilized 1 = BONE_AIM_FULL_FOLLOW / STABILIZED (llprismlens.h:213-214). |
| vcam | vc_bone_roll | combo | Roll mode | commitCamera 1172 -> mRollMode | OK | Horizon-lock 0 / Inherit 1 (llprismlens.h:216-217). |
| vcam | vc_bone_smooth | slider 0..10 s | Smooth | commitCamera 1173 | OK | Default 0.15 in range. |
| vcam | vc_bone_status | readout | Status | refresh 680 | OK | Text. |
| vcam | vc_bone_x | slider -64..64 m | Offset X | commitCamera 1174 | OK | Label matches (joint-local). |
| vcam | vc_bone_y | slider -64..64 m | Offset Y | commitCamera 1175 | OK | Matches. |
| vcam | vc_bone_z | slider -64..64 m | Offset Z | commitCamera 1176 | OK | Matches. |
| vcam | vc_bone_pitch | slider -180..180 deg | Trim pitch | commitCamera 1177 | OK | Matches. |
| vcam | vc_bone_yaw | slider -180..180 deg | Trim yaw | commitCamera 1178 | OK | Matches. |
| vcam | vc_bone_scale | kit switch | Clone-scale aware | commitCamera 1179 -> mScaleAware | OK | Matches label. |
| vcam | vc_bone_reset | button | Reset attach | onControl 1086-1092 | OK | Restores default BonePovSettings. |
| vcam | vc_lens_equiv | readout | Lens equiv. | refresh 659 | OK | Text. |
| vcam | vc_fov_mode | combo Fixed / Follow projector | Field of view | commitCamera 1146 -> mFovMode | OK | FIXED 0 / FOLLOW_PROJECTOR 1 (llprismlens.h:63-67). |
| vcam | vc_fov_deg | slider 5..175 deg | Degrees | commitCamera 1147 (deg -> rad) | OK | Default 60 deg in range. |
| vcam | vc_fov_projector | readout | Degrees | refresh 628, 642 | OK | Text. |
| vcam | vc_fov_attached | slider 10..150 deg | FOV (attached) | commitCamera 1148 -> mBonePov.mFovDeg | OK | Default 60 in range. |
| vcam | vc_near_clip | slider 0.01..10 m | Near clip | commitCamera 1149 | OK | Label matches. |
| vcam | vc_far_clip | slider 0.2..512 m | Far clip | commitCamera 1150 | OK | Label matches. |
| vcam | vc_lens_zoom | slider 1..8 x | Lens zoom | onControl 895 -> PrismLensZoom | OK | PrismLensZoom F32, default 2.0, 1..8 (settings.xml:29539). |
| vcam | vc_lens_zoom_note | readout | Lens zoom note | info 217 | OK | Info text. |
| vcam | vc_guide_show | kit switch | Show guide | commitCamera 1151 -> mShowGuide | OK | Matches. |
| vcam | vc_guide_thirds | kit switch | Rule of thirds | commitCamera 1152 -> mGuideThirds | OK | Matches. |
| vcam | vc_guide_up | kit switch | Up / roll | commitCamera 1153 -> mGuideUpRoll | OK | Matches. |
| vcam | vc_guide_cross | kit switch | Center crosshair | commitCamera 1154 -> mGuideCrosshair | OK | Matches. |
| vcam | vc_guide_clip | kit switch | Clip range markers | commitCamera 1155 -> mGuideClipMarkers | OK | Matches. |
| vcam | vc_guide_note | readout | Guide note | info 218; refresh 634 | OK | Text. |
| vcam | vc_exposure | slider -4..4 EV | Exposure EV | commitCamera 1156 -> mExposureBias | OK | Documented -4..+4 EV (llprismlens.h:158). |
| vcam | vc_grain | slider 0..1 | Film grain | commitCamera 1157 -> mFilmGrain | OK | Matches. |
| vcam | vc_chromatic | slider 0..1 | Chromatic ab. | commitCamera 1158 -> mChromaticAberration | OK | Matches. |
| vcam | vc_camera_crt | slider 0..1 | CRT lines | commitCamera 1159 -> mCRTScanlines | OK | Matches. |
| vcam | vc_look_feeds | readout | Feeds | refresh 664 | OK | Text. |
| vcam | vc_look_note | readout | Look note | info 219 | OK | Text. |
| vcam | vc_go_output | button | Go to Output | onControl 1041 -> setChipIndex(3) | OK | Output is the fourth section (kit_page section list). |
| vcam | vc_output_aspect | combo | Output aspect | onControl 1029-1034 + commitCamera 1160 -> mOutputAspect | OK | Values 1.7778 / 1.3333 / 1 / -1 (Custom) handled by ratioFromValue. |
| vcam | vc_output_custom | slider 0.25..4 | Custom W/H | commitCamera 1160 | OK | Matches. |
| vcam | vc_output_picture | readout | Picture | refresh 661 | OK | Text. |
| vcam | vc_face_list | custom | Faces | ALVCamFaceList::handleMouseDown (697-712) | OK | Row select; raw output uses LLSD() -> null display (onControl 913-950). |
| vcam | vc_add_face | button | Add selected face | onControl 1042-1047 (bindSelectedFace) | OK | Matches. |
| vcam | vc_new_face_fit | combo Fit/Fill/Stretch | Mapping for new face | onControl 1048-1052 -> mNewFaceFit | OK | EFitMode 0/1/2 (llprismlens.h:56-61). Panel-level state, not read back (see Notes). |
| vcam | vc_add_screen | button | Add virtual screen | onControl 1053-1062 | OK | Matches. |
| vcam | vc_face_count | readout | Face capacity | refresh 565 | OK | Text. |
| vcam | vc_projector_list | custom | Projectors | ALVCamProjectorList::handleMouseDown (899-919) | OK | select / remove actions handled at onControl 951-969. |
| vcam | vc_proj_look | combo (Custom and 5 presets) | Look preset | commitProjector 1285-1296 -> setProjectorLook | OK | EProjectorLookPreset: CLEAN_DIGITAL 0 .. WORN_FILM_REEL 4 match XML values (alvcampagemodel.h:20-29). |
| vcam | vc_proj_fit | segmented Fit/Fill/Stretch | Framing | commitProjector 1303 | OK | EFitMode 0/1/2. |
| vcam | vc_proj_anchor_x | slider 0..1 | Anchor X | commitProjector 1305 | OK | Matches. |
| vcam | vc_proj_anchor_y | slider 0..1 | Anchor Y | commitProjector 1306 | OK | Matches. |
| vcam | vc_proj_flip_h | kit switch | Flip H | commitProjector 1307 | OK | Matches. |
| vcam | vc_proj_flip_v | kit switch | Flip V | commitProjector 1308 | OK | Matches. |
| vcam | vc_proj_rotate | segmented 0/90/180/270 | Rotate | commitProjector 1309 -> mRotate (quarter turns) | OK | Values 0..3 match quarter turns. |
| vcam | vc_proj_keystone_h | slider -1..1 | Keystone H | commitProjector 1310 -> look.mKeystoneH | OK | Matches. |
| vcam | vc_proj_keystone_v | slider -1..1 | Keystone V | commitProjector 1311 | OK | Matches. |
| vcam | vc_proj_throw | segmented Off/Light/Camera | Throw | commitProjector 1312 | OK | EProjectorThrow OFF/LIGHT_FOV/CAMERA_FOV = 0/1/2 (llprismlens.h:119-124). |
| vcam | vc_proj_brightness | slider -4..4 EV | Brightness (feed EV) | commitProjector 1314 -> mFeedEV | OK | Matches. |
| vcam | vc_proj_follow_exposure | kit switch | Follow main exposure | commitProjector 1315 | OK | Matches. |
| vcam | vc_proj_lamp_kelvin | slider 2700..10000 K | Lamp colour | commitProjector 1316 -> mLampKelvin | OK | Range matches the model clamp (llprismlens.h:687). |
| vcam | vc_proj_gel_tint | color swatch | Gel tint | commitProjector 1317 -> LLColor3(value) | OK | Color swatch commits through onControl. |
| vcam | vc_proj_hotspot | slider 0..1 | Hot spot | commitProjector 1318 | OK | Matches. |
| vcam | vc_proj_shutter_flicker | slider 0..1 | Shutter flicker | commitProjector 1319 | OK | Matches. |
| vcam | vc_proj_shutter_rate | slider 12..48 Hz | Shutter rate | commitProjector 1320 -> mShutterHz | OK | Range matches model (llprismlens.h:684). |
| vcam | vc_proj_gate_weave | slider 0..1 | Gate weave | commitProjector 1321 | OK | Matches. |
| vcam | vc_proj_grain | slider 0..1 | Grain | commitProjector 1322 | OK | Matches. |
| vcam | vc_proj_dust | slider 0..1 | Dust | commitProjector 1323 | OK | Matches. |
| vcam | vc_proj_scratches | slider 0..1 | Scratches | commitProjector 1324 | OK | Matches. |
| vcam | vc_proj_burn | slider 0..1 | Burn | commitProjector 1325 | OK | Matches. |
| vcam | vc_proj_halation | slider 0..1 | Halation | commitProjector 1326 | OK | Matches. |
| vcam | vc_proj_resolution | segmented 256/512/1024 | Resolution | commitProjector 1327 -> mResolution | OK | Values match XML. |
| vcam | vc_proj_no_signal | combo Cookie/Black/White | When camera is off | commitProjector 1328 -> mNoSignal | OK | ENoSignal OWN_COOKIE/BLACK/WHITE = 0/1/2 (llprismlens.h:112-117). |
| vcam | vc_proj_keep_gobo | kit switch | Keep the light's gobo | commitProjector 1330 -> mKeepGobo | OK | Matches. |
| vcam | vc_proj_tv_preset | combo | Presets (More effects TV) | commitProjector 1351-1357 | LABEL-MISMATCH | "Clean" is a no-op (see Problems). |
| vcam | vc_proj_tv_scan | slider 0..1 | Scanlines | commitProjector 1331 | OK | Clamped 0..1 (llprismlens.h:450). |
| vcam | vc_proj_tv_gray | slider 0..1 | Grayscale | commitProjector 1332 | OK | Matches. |
| vcam | vc_proj_tv_static | slider 0..1 | Static Noise | commitProjector 1333 | OK | Matches. |
| vcam | vc_proj_tv_vignette | slider 0..1 | Vignette | commitProjector 1334 | OK | Matches. |
| vcam | vc_proj_tv_brightness | slider -1..1 | Brightness | commitProjector 1335 | OK | Clamp -1..1 (llprismlens.h:468). |
| vcam | vc_proj_tv_lines | slider 20..1200 | Lines | commitProjector 1336-1337 -> (v-100)/500 | OK | Maps to -0.16..2.2 (llprismlens.h:415, 452) which matches. |
| vcam | vc_proj_tv_dark | slider 0..100 % | Darkness | commitProjector 1338 | OK | Percent divided by 100 (depth 0..1). |
| vcam | vc_proj_tv_sharp | slider 0..100 % | Sharpness | commitProjector 1339 | OK | Percent divided by 100. |
| vcam | vc_proj_tv_thick | slider 10..90 % | Line thickness | commitProjector 1340 | OK | Clamp 0.1..0.9 (llprismlens.h:455). |
| vcam | vc_proj_tv_pixel | slider 0..1 | Pixelate | commitProjector 1341 | OK | Matches. |
| vcam | vc_proj_tv_sepia | slider 0..1 | Sepia Tint | commitProjector 1342 | OK | Matches. |
| vcam | vc_proj_tv_chroma | slider 0..1 | Chroma Bleed | commitProjector 1343 | OK | Matches. |
| vcam | vc_proj_tv_vroll | slider 0..1 | Vertical Roll | commitProjector 1344 | OK | Matches. |
| vcam | vc_proj_tv_roll_speed | slider 0..1 | Roll Speed | commitProjector 1345 | OK | Matches. |
| vcam | vc_proj_tv_tracking | slider 0..1 | VHS Tracking | commitProjector 1346 | OK | Matches. |
| vcam | vc_proj_tv_flicker | slider 0..1 | Flicker | commitProjector 1347 | OK | Matches. |
| vcam | vc_proj_tv_interlace | slider 0..1 | Interlace | commitProjector 1348 | OK | Matches. |
| vcam | vc_proj_tv_dropout | slider 0..1 | Dropout | commitProjector 1349 | OK | Matches. |
| vcam | vc_proj_tv_sheen | slider 0..1 | Sheen | commitProjector 1350 | OK | Matches. |
| vcam | vc_face_feed | segmented This camera / Gate program | Feed from | onControl 1079-1085 -> setDisplayGateSubscribed | OK | Value 1 = gate. Matches labels. |
| vcam | vc_face_fit | combo Fit/Fill/Stretch | Mapping | commitDisplay 1204 -> mFitMode | OK | EFitMode 0/1/2. |
| vcam | vc_anchor_x | slider 0..1 | Anchor X | commitDisplay 1205 | OK | Matches. |
| vcam | vc_anchor_y | slider 0..1 | Anchor Y | commitDisplay 1206 | OK | Matches. |
| vcam | vc_face_shape | readout | Face shape | refresh 698 | OK | Text. |
| vcam | vc_screen_height | slider 0.1..20 m | Height | commitDisplay 1207 | OK | Width follows aspect. |
| vcam | vc_screen_aspect | combo | Screen aspect | onControl 1035-1040 + commitDisplay 1208 | OK | Values 1.7778 / 1.3333 / 1 / -1 (Custom). |
| vcam | vc_face_locate | button | Locate | onControl 1008-1028 | OK | Matches. |
| vcam | vc_screen_reposition | button | Reposition to my view | onControl 1063-1073 (virtual only) | OK | Gated on mSettings.mVirtual; visibility toggled in refresh 704, 743. |
| vcam | vc_remove_face | button | Remove face | onControl 1074-1078 (LLPrismLens::removeDisplay) | OK | Matches. |
| vcam | vc_gate_note | readout | Gate note | info 220; refresh 707 | OK | Text. |
| vcam | vc_bar_r | slider 0..1 | Bar R | commitDisplay 1209 -> mBarColorLinear[0] | OK | Matches. |
| vcam | vc_bar_g | slider 0..1 | Bar G | commitDisplay 1210 | OK | Matches. |
| vcam | vc_bar_b | slider 0..1 | Bar B | commitDisplay 1211 | OK | Matches. |
| vcam | vc_tv_preset | combo | Presets (TV effects) | commitDisplay 1234-1275 | LABEL-MISMATCH | "Clean" is a no-op (see Problems). |
| vcam | vc_tv_scan | slider 0..1 | Scanlines | commitDisplay 1212 | OK | Matches. |
| vcam | vc_tv_gray | slider 0..1 | Grayscale | commitDisplay 1213 | OK | Matches. |
| vcam | vc_tv_static | slider 0..1 | Static Noise | commitDisplay 1214 | OK | Matches. |
| vcam | vc_tv_vignette | slider 0..1 | Vignette | commitDisplay 1215 | OK | Matches. |
| vcam | vc_tv_brightness | slider -1..1 | Brightness | commitDisplay 1216 | OK | Matches. |
| vcam | vc_tv_flip_h | kit switch | Flip H | commitDisplay 1217 | OK | Matches. |
| vcam | vc_tv_flip_v | kit switch | Flip V | commitDisplay 1218 | OK | Matches. |
| vcam | vc_tv_rotate | kit switch | Rotate 90 | commitDisplay 1219 | OK | Matches. |
| vcam | vc_tv_lines | slider 20..1200 | Lines | commitDisplay 1220 | OK | Same mapping as the projector copy. |
| vcam | vc_tv_dark | slider 0..100 % | Darkness | commitDisplay 1221 | OK | Matches. |
| vcam | vc_tv_sharp | slider 0..100 % | Sharpness | commitDisplay 1222 | OK | Matches. |
| vcam | vc_tv_thick | slider 10..90 % | Line thickness | commitDisplay 1223 | OK | Matches. |
| vcam | vc_tv_pixel | slider 0..1 | Pixelate | commitDisplay 1224 | OK | Matches. |
| vcam | vc_tv_sepia | slider 0..1 | Sepia Tint | commitDisplay 1225 | OK | Matches. |
| vcam | vc_tv_chroma | slider 0..1 | Chroma Bleed | commitDisplay 1226 | OK | Matches. |
| vcam | vc_tv_vroll | slider 0..1 | Vertical Roll | commitDisplay 1227 | OK | Matches. |
| vcam | vc_tv_roll_speed | slider 0..1 | Roll Speed | commitDisplay 1228 | OK | Matches. |
| vcam | vc_tv_tracking | slider 0..1 | VHS Tracking | commitDisplay 1229 | OK | Matches. |
| vcam | vc_tv_flicker | slider 0..1 | Flicker | commitDisplay 1230 | OK | Matches. |
| vcam | vc_tv_interlace | slider 0..1 | Interlace | commitDisplay 1231 | OK | Matches. |
| vcam | vc_tv_dropout | slider 0..1 | Dropout | commitDisplay 1232 | OK | Matches. |
| vcam | vc_tv_sheen | slider 0..1 | Sheen | commitDisplay 1233 | OK | Matches. |
| vcam | vc_raw_note | readout | Raw output | info 221; refresh 708, 746 | OK | Text. |
| vcam | vc_rate_mode | combo Automatic/Target FPS | Mode | commitCamera 1181 -> mModel.setRate | OK | AUTOMATIC 0 / TARGET_FPS 1 (llprismlens.h:69-73). |
| vcam | vc_target_fps | slider 1..30 fps | Target FPS | commitCamera 1187 | OK | Matches. |
| vcam | vc_rate_preset | combo | Preset | onControl 1118-1125 (TARGET_FPS) | OK | Row hidden in XML; visibility toggled in refresh 687. |
| vcam | vc_rate_readout | readout | Req/Ent/Obs/Age | refresh 683 | OK | Text. |
| vcam | vc_rate_note | readout | Rate note | info 222 | OK | Text. |
| vcam | vc_protect | kit switch | Protect main view | onControl 896 -> PrismAdaptivePerformance | OK | Boolean setting exists (default 1). |
| vcam | vc_main_fps | combo 30/45/60 | Main-view target | onControl 897 -> PrismProtectedMainFPS | OK | F32, default 30, valid 30/45/60 (settings.xml:29574). |
| vcam | vc_capture_budget | combo 0/5/10/15/20/30 | Capture budget | onControl 898 -> PrismCaptureRefreshCeilingHz | OK | F32, default 30, valid set matches (settings.xml:29581). Label for 0 see Notes. |
| vcam | vc_quality_ceiling | slider 0.25..2 | Quality ceiling | onControl 899 -> PrismLensResolutionScale | OK | F32, default 1.0, 0.25..2 matches (settings.xml:29546). |
| vcam | vc_admitted | readout | Admitted | refresh 758 | OK | Text. |
| vcam | vc_preview_cost | readout | Previews | refresh 760 | OK | Text. |
| vcam | vc_performance_note_1 | readout | Adaptive note | info 223 | OK | Text. |
| vcam | vc_performance_note_2 | readout | Presented FPS note | info 224 | OK | Text. |
| vcam | vc_performance_note_3 | readout | Face cost note | info 225 | OK | Text. |

VCam tally: 172 rows, 2 LABEL-MISMATCH, 170 OK (several with notes).

## Page: panel_dpage_camera_moves.xml (Shot Builders)

Flat XUI widgets (no kit_row). Callbacks for the buttons are attached in `LLFloaterDirector::postBuild` (llfloaterdirector.cpp:624-627). Spinner and check_box bindings use control_name, so the settings are read directly by the code that uses them.

| Page | Widget | Kind | Label | Binding | Verdict | Reason |
|---|---|---|---|---|---|---|
| moves | btn_build_ots_pair | button | OTS from A/B | onClickBuildOtsPair (llfloaterdirector.cpp:4183) -> buildOtsPairFromSubjects | OK | Callback attached at 624-625. |
| moves | prism_ots_shoulder | spinner 0.05..2 | Shoulder | control_name PrismOtsShoulderOffset (F32, default 0.45) | OK | Read at llprismlens.cpp:8856. |
| moves | prism_ots_distance | spinner 0.25..8 | Distance | control_name PrismOtsCameraDistance (F32, default 1.1) | OK | Read at llprismlens.cpp:8858. |
| moves | prism_ots_fov | spinner 5..175 | Lens FOV | control_name PrismOtsVerticalFovDeg (F32, default 45) | OK | Read at llprismlens.cpp:8860. |
| moves | prism_ots_height_bias | spinner -1..1 | Height bias | control_name PrismOtsHeightBias (F32, default 0.05) | OK | Read at llprismlens.cpp:8862. |
| moves | btn_start_zolly | button | Start Zolly | onClickZolly (4197) | OK | Callback at 626-627. Sets Vertigo start/end and triggers MODE_DOLLY_ZOOM. |
| moves | zolly_duration | spinner 0.1..60 s | Duration | control_name CinematicCamVertigoDuration (F32, default 8) | OK | Read at llcinematiccamera.cpp:1162. |
| moves | zolly_direction | combo Dolly out / Dolly in | Direction | control_name CinematicCamVertigoDirection (S32, default 0) | OK | onClickZolly line 4211: value != 0 is dolly in; label and setting comment agree. |
| moves | btn_start_hero_arc | button | Start Hero Arc | onClickHeroArc (4222) | OK | Callback at 628-629. |
| moves | hero_arc_angle | spinner 30..180 | Angle | control_name CinematicCamArcGeneratorAngle (F32, default 80) | OK | Clamped to 30..180 at 4232-4233, matching the spinner range. |
| moves | hero_arc_duration | spinner 0.1..60 s | Sec | control_name CinematicCamArcDuration (F32, default 9) | OK | Read at llcinematiccamera.cpp:1286. |
| moves | hero_arc_height_drift | spinner -5..5 m | Height drift | control_name CinematicCamArcHeightDrift (F32, default 0) | OK | Read at llcinematiccamera.cpp:1289. |
| moves | hero_arc_speed_ramp | check_box | Speed ramp | control_name CinematicCamSpeedRampEnabled (Boolean, default 0) | OK | Read at llcinematiccamera.cpp:1291. |
| moves | hero_arc_ramp_strength | spinner 0..0.95 | Ramp strength | control_name CinematicCamSpeedRampStrength (F32, default 0.65) | OK | Read at llcinematiccamera.cpp:1292. |

Moves tally: 14 rows, 14 OK.

## Page: panel_dpage_camera_shot.xml (Subjects card; embedded panels pending below)

| Page | Widget | Kind | Label | Binding | Verdict | Reason |
|---|---|---|---|---|---|---|
| shot | btn_set_a | button | Set from selection | postBuild commit callback (llfloaterdirector.cpp:593) -> onClickSetSubjectFromSelection(SUBJECT_A) (4154) | OK | assign_subject writes the cast subject. |
| shot | btn_set_b | button | Set from selection | llfloaterdirector.cpp:594 | OK | Same path, SUBJECT_B. |
| shot | btn_set_c | button | Set from selection | llfloaterdirector.cpp:595 | OK | Same path, SUBJECT_C. |
| shot | btn_set_d | button | Set from selection | llfloaterdirector.cpp:596 | OK | Same path, SUBJECT_D. |
| shot | btn_clear_a | button | Clear | llfloaterdirector.cpp:597 -> onClickClearSubject (4166) | OK | clear_subject. |
| shot | btn_clear_b | button | Clear | llfloaterdirector.cpp:598 | OK | Matches. |
| shot | btn_clear_c | button | Clear | llfloaterdirector.cpp:599 | OK | Matches. |
| shot | btn_clear_d | button | Clear | llfloaterdirector.cpp:600 | OK | Matches. |
| shot | subject_a_text | readout | A: | setTextIfChanged (llfloaterdirector.cpp:4272) | OK | Shows subject name or dash. |
| shot | subject_b_text | readout | B: | llfloaterdirector.cpp:4274 | OK | Same. |
| shot | subject_c_text | readout | C: | llfloaterdirector.cpp:4276 | OK | Same. |
| shot | subject_d_text | readout | D: | llfloaterdirector.cpp:4278 | OK | Same. |

Shot tally (this file's own widgets): 12 rows, 12 OK. Embedded panel_director_switcher.xml and panel_cinecam_params.xml are covered in the embedded section below.

## Page: panel_dpage_capture_scene_options.xml

| Page | Widget | Kind | Label | Binding | Verdict | Reason |
|---|---|---|---|---|---|---|
| scene_options | scene_hotkeys_enabled | check_box | Enable Director hotkeys | control_name DirectorHotkeysEnabled (Boolean, default 1, settings.xml:8838-8847) | OK | Read at aldirectorhotkeys.cpp:50 and 69. |

Scene options tally: 1 row, 1 OK. The text blocks are static copy with no controls.

## Pages with no own controls (embedded panels only)

- panel_dpage_camera_frame_orbit.xml: embeds panel_cinecam_frame.xml and panel_flycam_orbit.xml. Own widgets: titles only.
- panel_dpage_camera_operator.xml: embeds panel_cinecam_operator.xml. Own widgets: none.
- panel_dpage_camera_takes.xml: embeds panel_flycam_recorder.xml. Own widgets: none.
- panel_dpage_capture_temporal.xml: embeds panel_temporal_capture.xml. Own widgets: none.

## Embedded panels (verdicts pending)

The controls inside the embedded panels were audited by a separate read-only pass and will be appended here.

# gaze: dependency evidence

Landing layer 3. Actor and group targeting, blink, lens tracking; disable restores ordinary head motion.

Candidate associations require review. Includes/calls are discovery edges, not a complete call graph or proof of build closure.

## owned

- `indra/llcharacter/alavataranimationtime.h`
- `indra/newview/aldirectoralloff.cpp`
- `indra/newview/alfloateractorgaze.cpp`
- `indra/newview/alfloateractorgaze.h`
- `indra/newview/alfloatergazecues.cpp`
- `indra/newview/alfloatergazecues.h`
- `indra/newview/algazeblink.h`
- `indra/newview/algazehot.h`
- `indra/newview/algazehotslots.h`
- `indra/newview/algazemath.h`
- `indra/newview/algazemotor.h`
- `indra/newview/algazenoise.h`
- `indra/newview/algazepolicy.h`
- `indra/newview/algazerecruit.h`
- `indra/newview/alghostanimmirrormath.h`
- `indra/newview/alghostclearanim.h`
- `indra/newview/alghoststudio.cpp`
- `indra/newview/alghoststudio.h`
- `indra/newview/alkitpage.cpp`
- `indra/newview/alkitshell.cpp`
- `indra/newview/alpanelcinelightrig.cpp`
- `indra/newview/alpanelgazecues.cpp`
- `indra/newview/alpanelgazecues.h`
- `indra/newview/alpanelghoststudio.cpp`
- `indra/newview/alpanelghoststudio.h`
- `indra/newview/alpanellensgaze.cpp`
- `indra/newview/alpanellensgaze.h`
- `indra/newview/alposepolish.h`
- `indra/newview/alposesecondary.h`
- `indra/newview/alpovblinkmath.h`
- `indra/newview/alsteppedanim.cpp`
- `indra/newview/alsteppedanim.h`
- `indra/newview/alsteppedanimruntime.cpp`
- `indra/newview/altrajectory.h`
- `indra/newview/alvcambonepov.cpp`
- `indra/newview/alvcambonepovmath.h`
- `indra/newview/llactormover.cpp`
- `indra/newview/llactormover.h`
- `indra/newview/llcinematiccamera.cpp`
- `indra/newview/llcinematiccamera.h`
- `indra/newview/lldirectorcast.cpp`
- `indra/newview/lldirectorcast.h`
- `indra/newview/llfloateractormover.cpp`
- `indra/newview/llfloateractormover.h`
- `indra/newview/llfloaterdirector.cpp`
- `indra/newview/llfloaterdirector.h`
- `indra/newview/llghostavatar.cpp`
- `indra/newview/llghostavatar.h`
- `indra/newview/llpresentationtime.cpp`
- `indra/newview/skins/default/xui/en/control_table_contents_machinima.xml`
- `indra/newview/skins/default/xui/en/floater_actor_gaze.xml`
- `indra/newview/skins/default/xui/en/floater_actor_mover.xml`
- `indra/newview/skins/default/xui/en/floater_director.xml`
- `indra/newview/skins/default/xui/en/floater_gaze_cues.xml`
- `indra/newview/skins/default/xui/en/panel_gaze_cues_src.xml`
- `indra/newview/skins/default/xui/en/panel_ghost_studio.xml`
- `indra/newview/skins/default/xui/en/panel_lens_gaze.xml`
- `indra/newview/skins/default/xui/en/panel_path_editor.xml`
- `indra/newview/skins/default/xui/en/panel_temporal_capture.xml`

## hooks

- {"path": "indra/newview/CMakeLists.txt", "hunks": ["indra/newview/CMakeLists.txt@334,119", "indra/newview/CMakeLists.txt@1294,171", "indra/newview/CMakeLists.txt@2965,197"]}
- {"path": "indra/newview/app_settings/settings.xml", "hunks": ["indra/newview/app_settings/settings.xml@2,3455", "indra/newview/app_settings/settings.xml@6575,14788", "indra/newview/app_settings/settings.xml@29404,2466"]}
- {"path": "indra/newview/llappviewer.cpp", "hunks": ["indra/newview/llappviewer.cpp@72,35", "indra/newview/llappviewer.cpp@1728,12", "indra/newview/llappviewer.cpp@5383,37", "indra/newview/llappviewer.cpp@5671,49", "indra/newview/llappviewer.cpp@5993,30"]}
- {"path": "indra/newview/llstartup.cpp", "hunks": ["indra/newview/llstartup.cpp@201,9", "indra/newview/llstartup.cpp@2263,11"]}
- {"path": "indra/newview/alchatcommand.cpp", "hunks": ["indra/newview/alchatcommand.cpp@372,747"]}
- {"path": "indra/newview/app_settings/key_bindings.xml", "hunks": ["indra/newview/app_settings/key_bindings.xml@1,14", "indra/newview/app_settings/key_bindings.xml@48,14", "indra/newview/app_settings/key_bindings.xml@138,14", "indra/newview/app_settings/key_bindings.xml@235,14"]}
- {"path": "indra/newview/llfloaterpreference.cpp", "hunks": ["indra/newview/llfloaterpreference.cpp@3676,10"]}
- {"path": "indra/newview/llviewerfloaterreg.cpp", "hunks": ["indra/newview/llviewerfloaterreg.cpp@30,46", "indra/newview/llviewerfloaterreg.cpp@538,266"]}
- {"path": "indra/newview/llviewerinput.cpp", "hunks": ["indra/newview/llviewerinput.cpp@37,18", "indra/newview/llviewerinput.cpp@506,115", "indra/newview/llviewerinput.cpp@1222,28", "indra/newview/llviewerinput.cpp@1387,19", "indra/newview/llviewerinput.cpp@1656,84", "indra/newview/llviewerinput.cpp@1937,19", "indra/newview/llviewerinput.cpp@1980,13"]}
- {"path": "indra/newview/llviewerjoystick.cpp", "hunks": ["indra/newview/llviewerjoystick.cpp@1502,43", "indra/newview/llviewerjoystick.cpp@1887,2171", "indra/newview/llviewerjoystick.cpp@4332,242"]}
- {"path": "indra/newview/llvoavatar.cpp", "hunks": ["indra/newview/llvoavatar.cpp@5227,596", "indra/newview/llvoavatar.cpp@5923,45"]}
- {"path": "indra/newview/skins/default/xui/en/menu_viewer.xml", "hunks": ["indra/newview/skins/default/xui/en/menu_viewer.xml@200,243"]}
- {"path": "indra/llcharacter/llheadrotmotion.cpp", "hunks": ["indra/llcharacter/llheadrotmotion.cpp@24,12", "indra/llcharacter/llheadrotmotion.cpp@69,22", "indra/llcharacter/llheadrotmotion.cpp@198,21", "indra/llcharacter/llheadrotmotion.cpp@262,7", "indra/llcharacter/llheadrotmotion.cpp@395,7", "indra/llcharacter/llheadrotmotion.cpp@431,7", "indra/llcharacter/llheadrotmotion.cpp@492,45", "indra/llcharacter/llheadrotmotion.cpp@547,9", "indra/llcharacter/llheadrotmotion.cpp@563,15", "indra/llcharacter/llheadrotmotion.cpp@585,12"]}
- {"path": "indra/llcharacter/llheadrotmotion.h", "hunks": ["indra/llcharacter/llheadrotmotion.h@31,7", "indra/llcharacter/llheadrotmotion.h@102,12", "indra/llcharacter/llheadrotmotion.h@124,9", "indra/llcharacter/llheadrotmotion.h@196,12", "indra/llcharacter/llheadrotmotion.h@214,11", "indra/llcharacter/llheadrotmotion.h@228,12"]}
- {"path": "indra/llcharacter/llkeyframemotion.h", "hunks": ["indra/llcharacter/llkeyframemotion.h@226,21"]}
- {"path": "indra/llcharacter/llkeyframewalkmotion.h", "hunks": ["indra/llcharacter/llkeyframewalkmotion.h@70,21"]}
- {"path": "indra/llcharacter/llmotioncontroller.h", "hunks": ["indra/llcharacter/llmotioncontroller.h@145,13"]}
- {"path": "indra/llcharacter/llpose.cpp", "hunks": ["indra/llcharacter/llpose.cpp@235,15", "indra/llcharacter/llpose.cpp@294,17", "indra/llcharacter/llpose.cpp@413,15", "indra/llcharacter/llpose.cpp@547,14", "indra/llcharacter/llpose.cpp@566,9"]}
- {"path": "indra/llcharacter/llpose.h", "hunks": ["indra/llcharacter/llpose.h@88,18", "indra/llcharacter/llpose.h@121,12", "indra/llcharacter/llpose.h@148,13"]}
- {"path": "indra/llcharacter/lltargetingmotion.cpp", "hunks": ["indra/llcharacter/lltargetingmotion.cpp@28,7", "indra/llcharacter/lltargetingmotion.cpp@105,13"]}
- {"path": "indra/newview/llviewerjoystick.h", "hunks": ["indra/newview/llviewerjoystick.h@172,83"]}
- {"path": "indra/newview/llviewerpartsim.cpp", "hunks": ["indra/newview/llviewerpartsim.cpp@1114,60"]}
- {"path": "indra/newview/llviewerpartsim.h", "hunks": ["indra/newview/llviewerpartsim.h@157,11"]}

## settings

- `BDMergeEyeRotationLimit`
- `BDMergeGazeBehindAngle`
- `BDMergeGazeBehindPolicy`
- `BDMergeGazeDeadZone`
- `BDMergeGazeEyePitchMax`
- `BDMergeGazeEyeYawMax`
- `BDMergeGazeHeadPitchMax`
- `BDMergeGazeHeadSlewRate`
- `BDMergeGazeHeadYawMax`
- `BDMergeGazeTorsoLagRatio`
- `BDMergeHeadRotationLimit`
- `DirectorGazeAnimationPriority`
- `DirectorGazeArousal`
- `DirectorGazeBlinks`
- `DirectorGazeBodyTurnThresholdDeg`
- `DirectorGazeBreakFrequency`
- `DirectorGazeCameraRoll`
- `DirectorGazeChestShare`
- `DirectorGazeComfortPitchDeg`
- `DirectorGazeComfortYawDeg`
- `DirectorGazeComposition`
- `DirectorGazeCompositionMix`
- `DirectorGazeCueButtonHoldFramesPast`
- `DirectorGazeCueDefaultDurationSec`
- `DirectorGazeCueDoubleTakeGapSec`
- `DirectorGazeCueObjectDwellSec`
- `DirectorGazeCueThoughtResidue`
- `DirectorGazeCustomLimitsEnabled`
- `DirectorGazeDominance`
- `DirectorGazeEaseAcquireSec`
- `DirectorGazeEaseReleaseSec`
- `DirectorGazeEasing`
- `DirectorGazeExaggerate`
- `DirectorGazeEyeDurationBaseMs`
- `DirectorGazeEyeDurationPerDegMs`
- `DirectorGazeHeadDurationBaseMs`
- `DirectorGazeHeadLatencyMs`
- `DirectorGazeLeanCurve`
- `DirectorGazeLeanMaxDeg`
- `DirectorGazeLeanSoftnessDeg`
- `DirectorGazeLeanThresholdDeg`
- `DirectorGazeLidFollow`
- `DirectorGazeLimitEyeApplyPitchDeg`
- `DirectorGazeLimitEyeApplyPitchUpDeg`
- `DirectorGazeLimitEyeApplyYawDeg`
- `DirectorGazeLimitEyePitchDeg`
- `DirectorGazeLimitEyePitchUpDeg`
- `DirectorGazeLimitEyeRadialDeg`
- `DirectorGazeLimitEyeYawDeg`
- `DirectorGazeLimitHeadPitchDeg`
- `DirectorGazeLimitHeadPitchUpDeg`
- `DirectorGazeLimitHeadYawDeg`
- `DirectorGazeLimitHipsPitchDeg`
- `DirectorGazeLimitHipsPitchUpDeg`
- `DirectorGazeLimitHipsYawDeg`
- `DirectorGazeLimitNeckPitchDeg`
- `DirectorGazeLimitNeckPitchUpDeg`
- `DirectorGazeLimitNeckYawDeg`
- `DirectorGazeLimitSpinePitchDeg`
- `DirectorGazeLimitSpinePitchUpDeg`
- `DirectorGazeLimitSpineYawDeg`
- `DirectorGazeMicroLife`
- `DirectorGazeMotionPrograms`
- `DirectorGazeNearLensDeg`
- `DirectorGazePitchUpScale`
- `DirectorGazePriority`
- `DirectorGazeRandomInterval`
- `DirectorGazeRandomMode`
- `DirectorGazeReactionMax`
- `DirectorGazeReactionMin`
- `DirectorGazeRestraint`
- `DirectorGazeSoftRecruitDeg`
- `DirectorGazeStillness`
- `DirectorGazeTransitionSec`
- `DirectorGazeValence`
- `DirectorGazeVariation`
- `DirectorHotGazeGamepad`
- `DirectorLookAtCameraEaseTime`
- `DirectorLookAtCameraHeadEye`
- `DirectorLookAtCameraMode`
- `DirectorLookAtCameraSmoothing`
- `DirectorLookAtCameraStrength`
- `DirectorLookAtCameraTorso`
- `DirectorLookAtCameraTorsoAmount`
- `StepAnimGaze`
- `StepAnimPartFace`
- `StepAnimPartHead`
- `TemporalDriveAnimation`

## shaders

None identified by local evidence; not a claim that none is needed.

## xui

- `indra/newview/skins/default/xui/en/control_table_contents_machinima.xml`
- `indra/newview/skins/default/xui/en/director_kit_index.xml`
- `indra/newview/skins/default/xui/en/floater_actor_gaze.xml`
- `indra/newview/skins/default/xui/en/floater_actor_mover.xml`
- `indra/newview/skins/default/xui/en/floater_director.xml`
- `indra/newview/skins/default/xui/en/floater_director_v2.xml`
- `indra/newview/skins/default/xui/en/floater_gaze_cues.xml`
- `indra/newview/skins/default/xui/en/menu_viewer.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_capture_temporal.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_cast_ghosts.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_cast_groups.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_cast_path.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_performance_gaze.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_performance_gaze_cues.xml`
- `indra/newview/skins/default/xui/en/panel_gaze_cues_src.xml`
- `indra/newview/skins/default/xui/en/panel_ghost_studio.xml`
- `indra/newview/skins/default/xui/en/panel_lens_gaze.xml`
- `indra/newview/skins/default/xui/en/panel_path_editor.xml`
- `indra/newview/skins/default/xui/en/panel_temporal_capture.xml`

## generated runtime

- `indra/newview/skins/default/xui/en/director_kit_index.xml`
- `indra/newview/skins/default/xui/en/floater_director_v2.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_capture_temporal.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_cast_ghosts.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_cast_groups.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_cast_path.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_performance_gaze.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_performance_gaze_cues.xml`

## cmake entries

- {"file": "indra/llcharacter/CMakeLists.txt", "line": 33, "text": "alavataranimationtime.h", "source": "indra/llcharacter/alavataranimationtime.h"}
- {"file": "indra/llcharacter/CMakeLists.txt", "line": 39, "text": "llcharacter.h", "source": "indra/llcharacter/llcharacter.h"}
- {"file": "indra/llcharacter/CMakeLists.txt", "line": 83, "text": "# whose llcharacter.h -> llvisualparam.h chain needs llxmltree.h.", "source": "indra/llcharacter/llcharacter.h"}
- {"file": "indra/llcharacter/CMakeLists.txt", "line": 13, "text": "llheadrotmotion.cpp", "source": "indra/llcharacter/llheadrotmotion.cpp"}
- {"file": "indra/llcharacter/CMakeLists.txt", "line": 43, "text": "llheadrotmotion.h", "source": "indra/llcharacter/llheadrotmotion.h"}
- {"file": "indra/llcharacter/CMakeLists.txt", "line": 48, "text": "llkeyframemotion.h", "source": "indra/llcharacter/llkeyframemotion.h"}
- {"file": "indra/llcharacter/CMakeLists.txt", "line": 50, "text": "llkeyframewalkmotion.h", "source": "indra/llcharacter/llkeyframewalkmotion.h"}
- {"file": "indra/llcharacter/CMakeLists.txt", "line": 51, "text": "llmotion.h", "source": "indra/llcharacter/llmotion.h"}
- {"file": "indra/llcharacter/CMakeLists.txt", "line": 20, "text": "llmotioncontroller.cpp", "source": "indra/llcharacter/llmotioncontroller.cpp"}
- {"file": "indra/llcharacter/CMakeLists.txt", "line": 52, "text": "llmotioncontroller.h", "source": "indra/llcharacter/llmotioncontroller.h"}
- {"file": "indra/llcharacter/CMakeLists.txt", "line": 82, "text": "# llxml: the test includes llmotioncontroller.h (inline usesNativeLifetime),", "source": "indra/llcharacter/llmotioncontroller.h"}
- {"file": "indra/llcharacter/CMakeLists.txt", "line": 23, "text": "llpose.cpp", "source": "indra/llcharacter/llpose.cpp"}
- {"file": "indra/llcharacter/CMakeLists.txt", "line": 84, "text": "LL_ADD_INTEGRATION_TEST(llmotionpresentation \"llmotion.cpp;llpose.cpp;lljoint.cpp\" \"${test_libs};llxml\" \"${test_project}\")", "source": "indra/llcharacter/llpose.cpp"}
- {"file": "indra/llcharacter/CMakeLists.txt", "line": 54, "text": "llpose.h", "source": "indra/llcharacter/llpose.h"}
- {"file": "indra/llcharacter/CMakeLists.txt", "line": 24, "text": "lltargetingmotion.cpp", "source": "indra/llcharacter/lltargetingmotion.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 185, "text": "alchatcommand.cpp", "source": "indra/newview/alchatcommand.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 202, "text": "aldirectoralloff.cpp", "source": "indra/newview/aldirectoralloff.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 414, "text": "alfloateractorgaze.cpp", "source": "indra/newview/alfloateractorgaze.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1400, "text": "alfloateractorgaze.h", "source": "indra/newview/alfloateractorgaze.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 415, "text": "alfloatergazecues.cpp", "source": "indra/newview/alfloatergazecues.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1401, "text": "alfloatergazecues.h", "source": "indra/newview/alfloatergazecues.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1445, "text": "algazeblink.h", "source": "indra/newview/algazeblink.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1448, "text": "algazehot.h", "source": "indra/newview/algazehot.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1449, "text": "algazehotslots.h", "source": "indra/newview/algazehotslots.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1440, "text": "algazemath.h", "source": "indra/newview/algazemath.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1450, "text": "algazemotor.h", "source": "indra/newview/algazemotor.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1446, "text": "algazenoise.h", "source": "indra/newview/algazenoise.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1447, "text": "algazepolicy.h", "source": "indra/newview/algazepolicy.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1444, "text": "algazerecruit.h", "source": "indra/newview/algazerecruit.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1368, "text": "alghostanimmirrormath.h", "source": "indra/newview/alghostanimmirrormath.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1375, "text": "alghostclearanim.h # [ClearAnim]", "source": "indra/newview/alghostclearanim.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 395, "text": "alghoststudio.cpp", "source": "indra/newview/alghoststudio.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1372, "text": "alghoststudio.h", "source": "indra/newview/alghoststudio.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 212, "text": "alkitpage.cpp", "source": "indra/newview/alkitpage.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 217, "text": "alkitshell.cpp", "source": "indra/newview/alkitshell.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 434, "text": "alpanelcinelightrig.cpp", "source": "indra/newview/alpanelcinelightrig.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 429, "text": "alpanelgazecues.cpp", "source": "indra/newview/alpanelgazecues.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1417, "text": "alpanelgazecues.h", "source": "indra/newview/alpanelgazecues.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 403, "text": "alpanelghoststudio.cpp", "source": "indra/newview/alpanelghoststudio.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1384, "text": "alpanelghoststudio.h", "source": "indra/newview/alpanelghoststudio.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 439, "text": "alpanellensgaze.cpp", "source": "indra/newview/alpanellensgaze.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1451, "text": "alpanellensgaze.h", "source": "indra/newview/alpanellensgaze.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1149, "text": "alpovblinkmath.h", "source": "indra/newview/alpovblinkmath.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 248, "text": "alsteppedanim.cpp", "source": "indra/newview/alsteppedanim.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 2971, "text": "LL_ADD_INTEGRATION_TEST(alsteppedanim \"alsteppedanim.cpp\" \"${test_libs}\" \"${test_project}\")", "source": "indra/newview/alsteppedanim.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1205, "text": "alsteppedanim.h", "source": "indra/newview/alsteppedanim.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 249, "text": "alsteppedanimruntime.cpp", "source": "indra/newview/alsteppedanimruntime.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1443, "text": "altrajectory.h", "source": "indra/newview/altrajectory.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 375, "text": "alvcambonepov.cpp", "source": "indra/newview/alvcambonepov.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1330, "text": "alvcambonepovmath.h", "source": "indra/newview/alvcambonepovmath.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 2317, "text": "app_settings/key_bindings.xml", "source": "indra/newview/app_settings/key_bindings.xml"}
- {"file": "indra/newview/CMakeLists.txt", "line": 2319, "text": "app_settings/settings.xml", "source": "indra/newview/app_settings/settings.xml"}
- {"file": "indra/newview/CMakeLists.txt", "line": 341, "text": "llactormover.cpp", "source": "indra/newview/llactormover.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1304, "text": "llactormover.h", "source": "indra/newview/llactormover.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 319, "text": "llappviewer.cpp", "source": "indra/newview/llappviewer.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 410, "text": "llcinematiccamera.cpp", "source": "indra/newview/llcinematiccamera.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1393, "text": "llcinematiccamera.h", "source": "indra/newview/llcinematiccamera.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 411, "text": "lldirectorcast.cpp", "source": "indra/newview/lldirectorcast.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1394, "text": "lldirectorcast.h", "source": "indra/newview/lldirectorcast.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 412, "text": "llfloateractormover.cpp", "source": "indra/newview/llfloateractormover.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1398, "text": "llfloateractormover.h", "source": "indra/newview/llfloateractormover.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 413, "text": "llfloaterdirector.cpp", "source": "indra/newview/llfloaterdirector.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1399, "text": "llfloaterdirector.h", "source": "indra/newview/llfloaterdirector.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 621, "text": "llfloaterpreference.cpp", "source": "indra/newview/llfloaterpreference.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 669, "text": "llghostavatar.cpp", "source": "indra/newview/llghostavatar.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1687, "text": "llghostavatar.h", "source": "indra/newview/llghostavatar.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 911, "text": "llheroprobemanager.cpp", "source": "indra/newview/llheroprobemanager.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 687, "text": "llhudeffecttrail.cpp", "source": "indra/newview/llhudeffecttrail.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 419, "text": "llpresentationtime.cpp", "source": "indra/newview/llpresentationtime.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 917, "text": "llscenemonitor.cpp", "source": "indra/newview/llscenemonitor.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 951, "text": "llstartup.cpp", "source": "indra/newview/llstartup.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1029, "text": "llviewerfloaterreg.cpp", "source": "indra/newview/llviewerfloaterreg.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1041, "text": "llviewerinput.cpp", "source": "indra/newview/llviewerinput.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1040, "text": "llviewerjoystick.cpp", "source": "indra/newview/llviewerjoystick.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 2050, "text": "llviewerjoystick.h", "source": "indra/newview/llviewerjoystick.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1058, "text": "llviewerpartsim.cpp", "source": "indra/newview/llviewerpartsim.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 2068, "text": "llviewerpartsim.h", "source": "indra/newview/llviewerpartsim.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1076, "text": "llvoavatar.cpp", "source": "indra/newview/llvoavatar.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1086, "text": "llvopartgroup.cpp", "source": "indra/newview/llvopartgroup.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 2110, "text": "llworld.h", "source": "indra/newview/llworld.h"}

## kit wiring pages

- `gaze_cues_tab`
- `gaze_tab`
- `ghosts_tab`
- `stepped_tab`

## checks

- `scripts/code_tools/director_gaze_hot_check.py`

Full include/call edges: ../features.json. Shared hooks: ../hooks.md. Setting values and save coverage: ../settings.csv.

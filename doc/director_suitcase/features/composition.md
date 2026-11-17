# composition: dependency evidence

Landing layer 3. Change guide/aspect, resize viewport and enter capture; overlays align and clear when disabled.

Candidate associations require review. Includes/calls are discovery edges, not a complete call graph or proof of build closure.

## owned

- `indra/newview/alavataranimspeed.cpp`
- `indra/newview/alavatarkey.cpp`
- `indra/newview/alavatarkey.h`
- `indra/newview/alavatarlightresponse.cpp`
- `indra/newview/alavatarlightresponsemodel.h`
- `indra/newview/alcleantelemetry.h`
- `indra/newview/alclientheight.cpp`
- `indra/newview/alclientheight.h`
- `indra/newview/alclientheightmodel.h`
- `indra/newview/alfloaterphototools.cpp`
- `indra/newview/alfloaterphototools.h`
- `indra/newview/alfloaterultimatediopter.cpp`
- `indra/newview/alfloaterultimatediopter.h`
- `indra/newview/algazeblink.h`
- `indra/newview/algazemath.h`
- `indra/newview/algazemotor.h`
- `indra/newview/alghostclonemenu.cpp`
- `indra/newview/alghoststudio.cpp`
- `indra/newview/alkeyframeinputgate.h`
- `indra/newview/allocalfogmanager.cpp`
- `indra/newview/alpanelactorkey.cpp`
- `indra/newview/alpanelghoststudio.cpp`
- `indra/newview/alpanellensgaze.cpp`
- `indra/newview/alpanellensgaze.h`
- `indra/newview/altooldirectorpick.h`
- `indra/newview/altoolfocuspick.cpp`
- `indra/newview/altoolfocuspick.h`
- `indra/newview/alviewfinder.h`
- `indra/newview/alworldoverlayviz.cpp`
- `indra/newview/alworldoverlayviz.h`
- `indra/newview/app_settings/shaders/class1/deferred/projectorVolumetricClassicShadowUtil.glsl`
- `indra/newview/app_settings/shaders/class1/deferred/projectorVolumetricClassicShadowUtil16.glsl`
- `indra/newview/app_settings/shaders/class3/deferred/projectorVolumetricClassicF.glsl`
- `indra/newview/app_settings/shaders/class3/deferred/projectorVolumetricF.glsl`
- `indra/newview/llactormover.cpp`
- `indra/newview/llactormover.h`
- `indra/newview/llcinematiccamera.cpp`
- `indra/newview/lldirectorcast.cpp`
- `indra/newview/llghostavatar.cpp`
- `indra/newview/llghostavatar.h`
- `indra/newview/llprismgate.h`
- `indra/newview/llprismlens.cpp`
- `indra/newview/skins/default/xui/en/floater_phototools.xml`
- `indra/newview/skins/default/xui/en/floater_ultimate_diopter.xml`
- `indra/newview/skins/default/xui/en/panel_dof.xml`
- `indra/newview/skins/default/xui/en/panel_lens_gaze.xml`

## hooks

- {"path": "indra/newview/CMakeLists.txt", "hunks": ["indra/newview/CMakeLists.txt@347,126", "indra/newview/CMakeLists.txt@1331,186", "indra/newview/CMakeLists.txt@3018,198"]}
- {"path": "indra/newview/app_settings/settings.xml", "hunks": ["indra/newview/app_settings/settings.xml@5590,32", "indra/newview/app_settings/settings.xml@5623,21", "indra/newview/app_settings/settings.xml@6624,14888"]}
- {"path": "indra/newview/llappviewer.cpp", "hunks": ["indra/newview/llappviewer.cpp@25,20", "indra/newview/llappviewer.cpp@2053,34", "indra/newview/llappviewer.cpp@2167,9", "indra/newview/llappviewer.cpp@6569,45"]}
- {"path": "indra/newview/llfloaterhoverheight.cpp", "hunks": ["indra/newview/llfloaterhoverheight.cpp@1,7", "indra/newview/llfloaterhoverheight.cpp@29,50", "indra/newview/llfloaterhoverheight.cpp@93,41", "indra/newview/llfloaterhoverheight.cpp@228,216"]}
- {"path": "indra/newview/alviewermenu.cpp", "hunks": ["indra/newview/alviewermenu.cpp@18,37", "indra/newview/alviewermenu.cpp@606,774", "indra/newview/alviewermenu.cpp@1417,79"]}
- {"path": "indra/newview/app_settings/key_bindings.xml", "hunks": ["indra/newview/app_settings/key_bindings.xml@1,14", "indra/newview/app_settings/key_bindings.xml@48,14", "indra/newview/app_settings/key_bindings.xml@138,14", "indra/newview/app_settings/key_bindings.xml@235,14"]}
- {"path": "indra/newview/app_settings/settings_alchemy.xml", "hunks": ["indra/newview/app_settings/settings_alchemy.xml@2792,235"]}
- {"path": "indra/newview/llcontrolavatar.cpp", "hunks": ["indra/newview/llcontrolavatar.cpp@194,9", "indra/newview/llcontrolavatar.cpp@248,21"]}
- {"path": "indra/newview/llviewerdisplay.cpp", "hunks": ["indra/newview/llviewerdisplay.cpp@1979,14"]}
- {"path": "indra/newview/llviewerinput.cpp", "hunks": ["indra/newview/llviewerinput.cpp@1040,27", "indra/newview/llviewerinput.cpp@1247,28", "indra/newview/llviewerinput.cpp@1683,84", "indra/newview/llviewerinput.cpp@1836,14"]}
- {"path": "indra/newview/llviewermenu.cpp", "hunks": ["indra/newview/llviewermenu.cpp@4730,10"]}
- {"path": "indra/newview/llviewerwindow.cpp", "hunks": ["indra/newview/llviewerwindow.cpp@209,7", "indra/newview/llviewerwindow.cpp@270,272", "indra/newview/llviewerwindow.cpp@3376,8", "indra/newview/llviewerwindow.cpp@3386,16"]}
- {"path": "indra/newview/llvoavatar.cpp", "hunks": ["indra/newview/llvoavatar.cpp@50,24", "indra/newview/llvoavatar.cpp@2952,33", "indra/newview/llvoavatar.cpp@5120,21", "indra/newview/llvoavatar.cpp@5203,25", "indra/newview/llvoavatar.cpp@5881,13", "indra/newview/llvoavatar.cpp@5973,45", "indra/newview/llvoavatar.cpp@9481,10", "indra/newview/llvoavatar.cpp@9525,15", "indra/newview/llvoavatar.cpp@9546,12", "indra/newview/llvoavatar.cpp@12960,177"]}
- {"path": "indra/newview/llvoavatar.h", "hunks": ["indra/newview/llvoavatar.h@56,8", "indra/newview/llvoavatar.h@552,52", "indra/newview/llvoavatar.h@1286,17"]}
- {"path": "indra/newview/pipeline.cpp", "hunks": ["indra/newview/pipeline.cpp@95,17", "indra/newview/pipeline.cpp@1707,30357"]}
- {"path": "indra/newview/pipeline.h", "hunks": ["indra/newview/pipeline.h@924,100"]}
- {"path": "indra/newview/skins/default/xui/en/fonts.xml", "hunks": ["indra/newview/skins/default/xui/en/fonts.xml@663,26"]}
- {"path": "indra/llcommon/llstring.cpp", "hunks": ["indra/llcommon/llstring.cpp@858,131"]}
- {"path": "indra/llphysicsextensionsos/llconvexdecomposition.cpp", "hunks": ["indra/llphysicsextensionsos/llconvexdecomposition.cpp@77,12"]}
- {"path": "indra/newview/app_settings/shaders/class1/deferred/shadowUtil.glsl", "hunks": ["indra/newview/app_settings/shaders/class1/deferred/shadowUtil.glsl@387,774"]}
- {"path": "indra/newview/llagentcamera.cpp", "hunks": ["indra/newview/llagentcamera.cpp@2514,15"]}
- {"path": "indra/newview/llfloaterhoverheight.h", "hunks": ["indra/newview/llfloaterhoverheight.h@1,7", "indra/newview/llfloaterhoverheight.h@56,37"]}
- {"path": "indra/newview/llmeshrepository.cpp", "hunks": ["indra/newview/llmeshrepository.cpp@4358,22", "indra/newview/llmeshrepository.cpp@5803,17"]}
- {"path": "indra/newview/llmeshrepository.h", "hunks": ["indra/newview/llmeshrepository.h@864,7"]}
- {"path": "indra/newview/llsurfacepatch.cpp", "hunks": ["indra/newview/llsurfacepatch.cpp@1057,9"]}
- {"path": "indra/newview/skins/default/xui/en/control_table_contents_movement.xml", "hunks": ["indra/newview/skins/default/xui/en/control_table_contents_movement.xml@205,20"]}
- {"path": "indra/newview/skins/default/xui/en/floater_edit_hover_height.xml", "hunks": ["indra/newview/skins/default/xui/en/floater_edit_hover_height.xml@1,14", "indra/newview/skins/default/xui/en/floater_edit_hover_height.xml@17,127"]}

## settings

- `CinematicViewfinderEnabled`
- `CinematicViewfinderScale`
- `DirectorGazeComposition`
- `DirectorGazeCompositionMix`
- `RenderCompositionGuide`
- `RenderCompositionGuideCenter`
- `RenderCompositionGuideColor`
- `RenderCompositionGuideSafeAreas`
- `RenderCompositionGuideStyle`
- `RenderCompositionGuideVisibility`
- `RenderCompositionGuideWidth`
- `RenderCompositionGuideWithUIHidden`

## shaders

- `indra/newview/app_settings/shaders/class1/deferred/projectorVolumetricClassicShadowUtil.glsl`
- `indra/newview/app_settings/shaders/class1/deferred/projectorVolumetricClassicShadowUtil16.glsl`
- `indra/newview/app_settings/shaders/class1/deferred/shadowUtil.glsl`
- `indra/newview/app_settings/shaders/class3/deferred/projectorVolumetricClassicF.glsl`
- `indra/newview/app_settings/shaders/class3/deferred/projectorVolumetricF.glsl`

## xui

- `indra/newview/skins/default/xui/en/control_table_contents_movement.xml`
- `indra/newview/skins/default/xui/en/director_kit_index.xml`
- `indra/newview/skins/default/xui/en/floater_edit_hover_height.xml`
- `indra/newview/skins/default/xui/en/floater_phototools.xml`
- `indra/newview/skins/default/xui/en/floater_ultimate_diopter.xml`
- `indra/newview/skins/default/xui/en/fonts.xml`
- `indra/newview/skins/default/xui/en/panel_dof.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_camera_frame_orbit.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_capture_key.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_lens_focus.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_performance_gaze.xml`
- `indra/newview/skins/default/xui/en/panel_lens_gaze.xml`

## generated runtime

- `indra/newview/skins/default/xui/en/director_kit_index.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_camera_frame_orbit.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_capture_key.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_lens_focus.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_performance_gaze.xml`

## cmake entries

- {"file": "indra/llcommon/CMakeLists.txt", "line": 102, "text": "llstring.cpp", "source": "indra/llcommon/llstring.cpp"}
- {"file": "indra/llphysicsextensionsos/CMakeLists.txt", "line": 7, "text": "llconvexdecomposition.cpp", "source": "indra/llphysicsextensionsos/llconvexdecomposition.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 409, "text": "alavataranimspeed.cpp", "source": "indra/newview/alavataranimspeed.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 379, "text": "alavatarkey.cpp # [ClientHeight]", "source": "indra/newview/alavatarkey.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1356, "text": "alavatarkey.h # [ClientHeight]", "source": "indra/newview/alavatarkey.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 376, "text": "alavatarlightresponse.cpp # [AvatarLightResponse]", "source": "indra/newview/alavatarlightresponse.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1354, "text": "alavatarlightresponsemodel.h # [AvatarLightResponse]", "source": "indra/newview/alavatarlightresponsemodel.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1231, "text": "alcleantelemetry.h", "source": "indra/newview/alcleantelemetry.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 380, "text": "alclientheight.cpp # [ClientHeight]", "source": "indra/newview/alclientheight.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1357, "text": "alclientheight.h # [ClientHeight]", "source": "indra/newview/alclientheight.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1358, "text": "alclientheightmodel.h # [ClientHeight]", "source": "indra/newview/alclientheightmodel.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 3070, "text": "# (alclientheightmodel.h), so its TUT is registered directly like ALGazeMath.", "source": "indra/newview/alclientheightmodel.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 239, "text": "alfloaterblocked.cpp", "source": "indra/newview/alfloaterblocked.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 249, "text": "alfloaterphototools.cpp", "source": "indra/newview/alfloaterphototools.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1214, "text": "alfloaterphototools.h", "source": "indra/newview/alfloaterphototools.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 386, "text": "alfloaterultimatediopter.cpp", "source": "indra/newview/alfloaterultimatediopter.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1362, "text": "alfloaterultimatediopter.h", "source": "indra/newview/alfloaterultimatediopter.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1495, "text": "algazeblink.h", "source": "indra/newview/algazeblink.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1490, "text": "algazemath.h", "source": "indra/newview/algazemath.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1502, "text": "algazemotor.h", "source": "indra/newview/algazemotor.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 419, "text": "alghostclonemenu.cpp # [CloneMenu] clone right-click menu", "source": "indra/newview/alghostclonemenu.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 410, "text": "alghoststudio.cpp", "source": "indra/newview/alghoststudio.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1244, "text": "alkeyframeinputgate.h", "source": "indra/newview/alkeyframeinputgate.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 206, "text": "allocalfogmanager.cpp", "source": "indra/newview/allocalfogmanager.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 447, "text": "alpanelactorkey.cpp", "source": "indra/newview/alpanelactorkey.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 421, "text": "alpanelghoststudio.cpp", "source": "indra/newview/alpanelghoststudio.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 459, "text": "alpanellensgaze.cpp", "source": "indra/newview/alpanellensgaze.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1503, "text": "alpanellensgaze.h", "source": "indra/newview/alpanellensgaze.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1438, "text": "altooldirectorpick.h", "source": "indra/newview/altooldirectorpick.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 425, "text": "altoolfocuspick.cpp", "source": "indra/newview/altoolfocuspick.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1439, "text": "altoolfocuspick.h", "source": "indra/newview/altoolfocuspick.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 292, "text": "alviewermenu.cpp", "source": "indra/newview/alviewermenu.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1370, "text": "alviewfinder.h", "source": "indra/newview/alviewfinder.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 414, "text": "alworldoverlayviz.cpp", "source": "indra/newview/alworldoverlayviz.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1422, "text": "alworldoverlayviz.h", "source": "indra/newview/alworldoverlayviz.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 2369, "text": "app_settings/key_bindings.xml", "source": "indra/newview/app_settings/key_bindings.xml"}
- {"file": "indra/newview/CMakeLists.txt", "line": 2371, "text": "app_settings/settings.xml", "source": "indra/newview/app_settings/settings.xml"}
- {"file": "indra/newview/CMakeLists.txt", "line": 2372, "text": "app_settings/settings_alchemy.xml", "source": "indra/newview/app_settings/settings_alchemy.xml"}
- {"file": "indra/newview/CMakeLists.txt", "line": 297, "text": "fsmaniprotatejoint.cpp", "source": "indra/newview/fsmaniprotatejoint.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 298, "text": "fsmaniptranslatejoint.cpp", "source": "indra/newview/fsmaniptranslatejoint.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 354, "text": "llactormover.cpp", "source": "indra/newview/llactormover.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1341, "text": "llactormover.h", "source": "indra/newview/llactormover.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 319, "text": "llagentcamera.cpp", "source": "indra/newview/llagentcamera.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 315, "text": "llaisapi.cpp", "source": "indra/newview/llaisapi.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 331, "text": "llappearancemgr.cpp", "source": "indra/newview/llappearancemgr.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 332, "text": "llappviewer.cpp", "source": "indra/newview/llappviewer.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 429, "text": "llcinematiccamera.cpp", "source": "indra/newview/llcinematiccamera.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 488, "text": "llcontrolavatar.cpp", "source": "indra/newview/llcontrolavatar.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 430, "text": "lldirectorcast.cpp", "source": "indra/newview/lldirectorcast.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 515, "text": "lldrawpoolwater.cpp", "source": "indra/newview/lldrawpoolwater.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 518, "text": "lldynamictexture.cpp", "source": "indra/newview/lldynamictexture.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 533, "text": "llfetchedgltfmaterial.cpp", "source": "indra/newview/llfetchedgltfmaterial.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 598, "text": "llfloaterhoverheight.cpp", "source": "indra/newview/llfloaterhoverheight.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1644, "text": "llfloaterhoverheight.h", "source": "indra/newview/llfloaterhoverheight.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 604, "text": "llfloaterimcontainer.cpp", "source": "indra/newview/llfloaterimcontainer.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 642, "text": "llfloaterpreferencesgraphicsadvanced.cpp", "source": "indra/newview/llfloaterpreferencesgraphicsadvanced.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 689, "text": "llghostavatar.cpp", "source": "indra/newview/llghostavatar.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1739, "text": "llghostavatar.h", "source": "indra/newview/llghostavatar.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 931, "text": "llheroprobemanager.cpp", "source": "indra/newview/llheroprobemanager.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 705, "text": "llhudeffectlookat.cpp", "source": "indra/newview/llhudeffectlookat.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 707, "text": "llhudeffecttrail.cpp", "source": "indra/newview/llhudeffecttrail.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 750, "text": "lllocalmesh.cpp", "source": "indra/newview/lllocalmesh.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 771, "text": "llmeshrepository.cpp", "source": "indra/newview/llmeshrepository.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1818, "text": "llmeshrepository.h", "source": "indra/newview/llmeshrepository.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 905, "text": "llpipelinelistener.cpp", "source": "indra/newview/llpipelinelistener.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1955, "text": "llprismgate.h", "source": "indra/newview/llprismgate.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 922, "text": "llprismlens.cpp", "source": "indra/newview/llprismlens.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 937, "text": "llscenemonitor.cpp", "source": "indra/newview/llscenemonitor.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 975, "text": "llsurface.cpp", "source": "indra/newview/llsurface.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 976, "text": "llsurfacepatch.cpp", "source": "indra/newview/llsurfacepatch.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 989, "text": "lltextureview.cpp", "source": "indra/newview/lltextureview.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1047, "text": "llviewerdisplay.cpp", "source": "indra/newview/llviewerdisplay.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1061, "text": "llviewerinput.cpp", "source": "indra/newview/llviewerinput.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1057, "text": "llviewerjoint.cpp", "source": "indra/newview/llviewerjoint.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1066, "text": "llviewermenu.cpp", "source": "indra/newview/llviewermenu.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1072, "text": "llvieweroctree.cpp", "source": "indra/newview/llvieweroctree.cpp"}
- {"file": "indra/llwindow/CMakeLists.txt", "line": 107, "text": "# llviewerwindow.cpp's (guarded) include still resolves.", "source": "indra/newview/llviewerwindow.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1091, "text": "llviewerwindow.cpp", "source": "indra/newview/llviewerwindow.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1096, "text": "llvoavatar.cpp", "source": "indra/newview/llvoavatar.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 2139, "text": "llvoavatar.h", "source": "indra/newview/llvoavatar.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 2140, "text": "llvoavatarself.h", "source": "indra/newview/llvoavatarself.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1106, "text": "llvopartgroup.cpp", "source": "indra/newview/llvopartgroup.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1130, "text": "pipeline.cpp", "source": "indra/newview/pipeline.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 2172, "text": "pipeline.h", "source": "indra/newview/pipeline.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1134, "text": "rlveffects.cpp", "source": "indra/newview/rlveffects.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1137, "text": "rlvhandler.cpp", "source": "indra/newview/rlvhandler.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1140, "text": "rlvlocks.cpp", "source": "indra/newview/rlvlocks.cpp"}
- {"file": "indra/llrender/CMakeLists.txt", "line": 171, "text": "\"LLFONT_TEST_FONTS_XML=\\\"${INDRA_SOURCE_DIR}/newview/skins/default/xui/en/fonts.xml\\\"\")", "source": "indra/newview/skins/default/xui/en/fonts.xml"}

## kit wiring pages

- `focus_tab`
- `frame_orbit_tab`
- `gaze_tab`

## checks

None identified by local evidence; not a claim that none is needed.

Full include/call edges: ../features.json. Shared hooks: ../hooks.md. Setting values and save coverage: ../settings.csv.

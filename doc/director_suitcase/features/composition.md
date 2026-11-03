# composition: dependency evidence

Landing layer 3. Change guide/aspect, resize viewport and enter capture; overlays align and clear when disabled.

Candidate associations require review. Includes/calls are discovery edges, not a complete call graph or proof of build closure.

## owned

- `indra/newview/alavataranimspeed.cpp`
- `indra/newview/alavatarkey.cpp`
- `indra/newview/alavatarkey.h`
- `indra/newview/alavatarlightresponse.cpp`
- `indra/newview/alavatarlightresponsemodel.h`
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
- `indra/newview/alghoststudio.cpp`
- `indra/newview/allocalfogmanager.cpp`
- `indra/newview/alpanelghoststudio.cpp`
- `indra/newview/alpanellensgaze.cpp`
- `indra/newview/alpanellensgaze.h`
- `indra/newview/altoolfocuspick.cpp`
- `indra/newview/altoolfocuspick.h`
- `indra/newview/alviewfinder.h`
- `indra/newview/alworldoverlayviz.cpp`
- `indra/newview/alworldoverlayviz.h`
- `indra/newview/app_settings/shaders/class1/deferred/projectorVolumetricClassicShadowUtil.glsl`
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

- {"path": "indra/newview/CMakeLists.txt", "hunks": ["indra/newview/CMakeLists.txt@333,115", "indra/newview/CMakeLists.txt@1286,162", "indra/newview/CMakeLists.txt@2947,185"]}
- {"path": "indra/newview/app_settings/settings.xml", "hunks": ["indra/newview/app_settings/settings.xml@5522,32", "indra/newview/app_settings/settings.xml@5555,21", "indra/newview/app_settings/settings.xml@6556,15959"]}
- {"path": "indra/newview/llappviewer.cpp", "hunks": ["indra/newview/llappviewer.cpp@25,19", "indra/newview/llappviewer.cpp@6468,45"]}
- {"path": "indra/newview/llfloaterhoverheight.cpp", "hunks": ["indra/newview/llfloaterhoverheight.cpp@1,7", "indra/newview/llfloaterhoverheight.cpp@29,50", "indra/newview/llfloaterhoverheight.cpp@93,41", "indra/newview/llfloaterhoverheight.cpp@228,216"]}
- {"path": "indra/newview/alviewermenu.cpp", "hunks": ["indra/newview/alviewermenu.cpp@18,36", "indra/newview/alviewermenu.cpp@604,830", "indra/newview/alviewermenu.cpp@1471,83"]}
- {"path": "indra/newview/app_settings/key_bindings.xml", "hunks": ["indra/newview/app_settings/key_bindings.xml@1,13", "indra/newview/app_settings/key_bindings.xml@47,13", "indra/newview/app_settings/key_bindings.xml@136,13", "indra/newview/app_settings/key_bindings.xml@232,13"]}
- {"path": "indra/newview/app_settings/settings_alchemy.xml", "hunks": ["indra/newview/app_settings/settings_alchemy.xml@2771,233"]}
- {"path": "indra/newview/llcontrolavatar.cpp", "hunks": ["indra/newview/llcontrolavatar.cpp@193,9", "indra/newview/llcontrolavatar.cpp@247,21"]}
- {"path": "indra/newview/llviewerinput.cpp", "hunks": ["indra/newview/llviewerinput.cpp@1025,20", "indra/newview/llviewerinput.cpp@1215,27", "indra/newview/llviewerinput.cpp@1648,84", "indra/newview/llviewerinput.cpp@1801,14"]}
- {"path": "indra/newview/llviewermenu.cpp", "hunks": ["indra/newview/llviewermenu.cpp@4723,10"]}
- {"path": "indra/newview/llviewerwindow.cpp", "hunks": ["indra/newview/llviewerwindow.cpp@201,7", "indra/newview/llviewerwindow.cpp@262,272", "indra/newview/llviewerwindow.cpp@3366,14"]}
- {"path": "indra/newview/llvoavatar.cpp", "hunks": ["indra/newview/llvoavatar.cpp@49,23", "indra/newview/llvoavatar.cpp@2947,33", "indra/newview/llvoavatar.cpp@5059,21", "indra/newview/llvoavatar.cpp@5142,25", "indra/newview/llvoavatar.cpp@5820,13", "indra/newview/llvoavatar.cpp@5912,45", "indra/newview/llvoavatar.cpp@9412,10", "indra/newview/llvoavatar.cpp@9456,15", "indra/newview/llvoavatar.cpp@9477,12", "indra/newview/llvoavatar.cpp@12825,163"]}
- {"path": "indra/newview/llvoavatar.h", "hunks": ["indra/newview/llvoavatar.h@56,8", "indra/newview/llvoavatar.h@550,45", "indra/newview/llvoavatar.h@1259,17"]}
- {"path": "indra/newview/pipeline.cpp", "hunks": ["indra/newview/pipeline.cpp@85,17", "indra/newview/pipeline.cpp@1693,29165"]}
- {"path": "indra/newview/pipeline.h", "hunks": ["indra/newview/pipeline.h@878,67"]}
- {"path": "indra/newview/skins/default/xui/en/fonts.xml", "hunks": ["indra/newview/skins/default/xui/en/fonts.xml@663,26"]}
- {"path": "indra/llcommon/llstring.cpp", "hunks": ["indra/llcommon/llstring.cpp@858,131"]}
- {"path": "indra/newview/app_settings/shaders/class1/deferred/shadowUtil.glsl", "hunks": ["indra/newview/app_settings/shaders/class1/deferred/shadowUtil.glsl@256,496"]}
- {"path": "indra/newview/llagentcamera.cpp", "hunks": ["indra/newview/llagentcamera.cpp@2509,15"]}
- {"path": "indra/newview/llfloaterhoverheight.h", "hunks": ["indra/newview/llfloaterhoverheight.h@1,7", "indra/newview/llfloaterhoverheight.h@56,37"]}
- {"path": "indra/newview/llsurfacepatch.cpp", "hunks": ["indra/newview/llsurfacepatch.cpp@1057,9"]}
- {"path": "indra/newview/skins/default/xui/en/control_table_contents_movement.xml", "hunks": ["indra/newview/skins/default/xui/en/control_table_contents_movement.xml@205,17"]}
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
- `indra/newview/skins/default/xui/en/panel_dpage_lens_focus.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_performance_gaze.xml`
- `indra/newview/skins/default/xui/en/panel_lens_gaze.xml`

## generated runtime

- `indra/newview/skins/default/xui/en/director_kit_index.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_camera_frame_orbit.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_lens_focus.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_performance_gaze.xml`

## cmake entries

- {"file": "indra/llcommon/CMakeLists.txt", "line": 102, "text": "llstring.cpp", "source": "indra/llcommon/llstring.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 395, "text": "alavataranimspeed.cpp", "source": "indra/newview/alavataranimspeed.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 367, "text": "alavatarkey.cpp # [ClientHeight]", "source": "indra/newview/alavatarkey.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1315, "text": "alavatarkey.h # [ClientHeight]", "source": "indra/newview/alavatarkey.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 364, "text": "alavatarlightresponse.cpp # [AvatarLightResponse]", "source": "indra/newview/alavatarlightresponse.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1313, "text": "alavatarlightresponsemodel.h # [AvatarLightResponse]", "source": "indra/newview/alavatarlightresponsemodel.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 368, "text": "alclientheight.cpp # [ClientHeight]", "source": "indra/newview/alclientheight.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1316, "text": "alclientheight.h # [ClientHeight]", "source": "indra/newview/alclientheight.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1317, "text": "alclientheightmodel.h # [ClientHeight]", "source": "indra/newview/alclientheightmodel.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 3002, "text": "# (alclientheightmodel.h), so its TUT is registered directly like ALGazeMath.", "source": "indra/newview/alclientheightmodel.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 227, "text": "alfloaterblocked.cpp", "source": "indra/newview/alfloaterblocked.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 237, "text": "alfloaterphototools.cpp", "source": "indra/newview/alfloaterphototools.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1190, "text": "alfloaterphototools.h", "source": "indra/newview/alfloaterphototools.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 374, "text": "alfloaterultimatediopter.cpp", "source": "indra/newview/alfloaterultimatediopter.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1321, "text": "alfloaterultimatediopter.h", "source": "indra/newview/alfloaterultimatediopter.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1437, "text": "algazeblink.h", "source": "indra/newview/algazeblink.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1432, "text": "algazemath.h", "source": "indra/newview/algazemath.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1442, "text": "algazemotor.h", "source": "indra/newview/algazemotor.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 396, "text": "alghoststudio.cpp", "source": "indra/newview/alghoststudio.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 196, "text": "allocalfogmanager.cpp", "source": "indra/newview/allocalfogmanager.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 402, "text": "alpanelghoststudio.cpp", "source": "indra/newview/alpanelghoststudio.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 438, "text": "alpanellensgaze.cpp", "source": "indra/newview/alpanellensgaze.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1443, "text": "alpanellensgaze.h", "source": "indra/newview/alpanellensgaze.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 405, "text": "altoolfocuspick.cpp", "source": "indra/newview/altoolfocuspick.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1382, "text": "altoolfocuspick.h", "source": "indra/newview/altoolfocuspick.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 280, "text": "alviewermenu.cpp", "source": "indra/newview/alviewermenu.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1329, "text": "alviewfinder.h", "source": "indra/newview/alviewfinder.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 398, "text": "alworldoverlayviz.cpp", "source": "indra/newview/alworldoverlayviz.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1370, "text": "alworldoverlayviz.h", "source": "indra/newview/alworldoverlayviz.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 2309, "text": "app_settings/key_bindings.xml", "source": "indra/newview/app_settings/key_bindings.xml"}
- {"file": "indra/newview/CMakeLists.txt", "line": 2311, "text": "app_settings/settings.xml", "source": "indra/newview/app_settings/settings.xml"}
- {"file": "indra/newview/CMakeLists.txt", "line": 2312, "text": "app_settings/settings_alchemy.xml", "source": "indra/newview/app_settings/settings_alchemy.xml"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1243, "text": "fsposeranimator.h", "source": "indra/newview/fsposeranimator.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 342, "text": "llactormover.cpp", "source": "indra/newview/llactormover.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1300, "text": "llactormover.h", "source": "indra/newview/llactormover.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 307, "text": "llagentcamera.cpp", "source": "indra/newview/llagentcamera.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 303, "text": "llaisapi.cpp", "source": "indra/newview/llaisapi.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 320, "text": "llappviewer.cpp", "source": "indra/newview/llappviewer.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 409, "text": "llcinematiccamera.cpp", "source": "indra/newview/llcinematiccamera.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 467, "text": "llcontrolavatar.cpp", "source": "indra/newview/llcontrolavatar.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 410, "text": "lldirectorcast.cpp", "source": "indra/newview/lldirectorcast.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 494, "text": "lldrawpoolwater.cpp", "source": "indra/newview/lldrawpoolwater.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 497, "text": "lldynamictexture.cpp", "source": "indra/newview/lldynamictexture.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 512, "text": "llfetchedgltfmaterial.cpp", "source": "indra/newview/llfetchedgltfmaterial.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 577, "text": "llfloaterhoverheight.cpp", "source": "indra/newview/llfloaterhoverheight.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1584, "text": "llfloaterhoverheight.h", "source": "indra/newview/llfloaterhoverheight.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 621, "text": "llfloaterpreferencesgraphicsadvanced.cpp", "source": "indra/newview/llfloaterpreferencesgraphicsadvanced.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 668, "text": "llghostavatar.cpp", "source": "indra/newview/llghostavatar.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1679, "text": "llghostavatar.h", "source": "indra/newview/llghostavatar.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 684, "text": "llhudeffectlookat.cpp", "source": "indra/newview/llhudeffectlookat.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 686, "text": "llhudeffecttrail.cpp", "source": "indra/newview/llhudeffecttrail.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 729, "text": "lllocalmesh.cpp", "source": "indra/newview/lllocalmesh.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 750, "text": "llmeshrepository.cpp", "source": "indra/newview/llmeshrepository.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1895, "text": "llprismgate.h", "source": "indra/newview/llprismgate.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 901, "text": "llprismlens.cpp", "source": "indra/newview/llprismlens.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 916, "text": "llscenemonitor.cpp", "source": "indra/newview/llscenemonitor.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 954, "text": "llsurface.cpp", "source": "indra/newview/llsurface.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 955, "text": "llsurfacepatch.cpp", "source": "indra/newview/llsurfacepatch.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 992, "text": "lltoolpie.cpp", "source": "indra/newview/lltoolpie.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1040, "text": "llviewerinput.cpp", "source": "indra/newview/llviewerinput.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1036, "text": "llviewerjoint.cpp", "source": "indra/newview/llviewerjoint.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1045, "text": "llviewermenu.cpp", "source": "indra/newview/llviewermenu.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1051, "text": "llvieweroctree.cpp", "source": "indra/newview/llvieweroctree.cpp"}
- {"file": "indra/llwindow/CMakeLists.txt", "line": 107, "text": "# llviewerwindow.cpp's (guarded) include still resolves.", "source": "indra/newview/llviewerwindow.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1070, "text": "llviewerwindow.cpp", "source": "indra/newview/llviewerwindow.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1075, "text": "llvoavatar.cpp", "source": "indra/newview/llvoavatar.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 2079, "text": "llvoavatar.h", "source": "indra/newview/llvoavatar.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1085, "text": "llvopartgroup.cpp", "source": "indra/newview/llvopartgroup.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1109, "text": "pipeline.cpp", "source": "indra/newview/pipeline.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 2112, "text": "pipeline.h", "source": "indra/newview/pipeline.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1113, "text": "rlveffects.cpp", "source": "indra/newview/rlveffects.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1116, "text": "rlvhandler.cpp", "source": "indra/newview/rlvhandler.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1119, "text": "rlvlocks.cpp", "source": "indra/newview/rlvlocks.cpp"}
- {"file": "indra/llrender/CMakeLists.txt", "line": 169, "text": "\"LLFONT_TEST_FONTS_XML=\\\"${INDRA_SOURCE_DIR}/newview/skins/default/xui/en/fonts.xml\\\"\")", "source": "indra/newview/skins/default/xui/en/fonts.xml"}

## kit wiring pages

- `focus_tab`
- `frame_orbit_tab`
- `gaze_tab`

## checks

None identified by local evidence; not a claim that none is needed.

Full include/call edges: ../features.json. Shared hooks: ../hooks.md. Setting values and save coverage: ../settings.csv.

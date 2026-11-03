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

- {"path": "indra/newview/CMakeLists.txt", "hunks": ["indra/newview/CMakeLists.txt@333,116", "indra/newview/CMakeLists.txt@1287,165", "indra/newview/CMakeLists.txt@2952,185"]}
- {"path": "indra/newview/app_settings/settings.xml", "hunks": ["indra/newview/app_settings/settings.xml@5522,32", "indra/newview/app_settings/settings.xml@5555,21", "indra/newview/app_settings/settings.xml@6556,14744"]}
- {"path": "indra/newview/llappviewer.cpp", "hunks": ["indra/newview/llappviewer.cpp@25,19", "indra/newview/llappviewer.cpp@6470,45"]}
- {"path": "indra/newview/llfloaterhoverheight.cpp", "hunks": ["indra/newview/llfloaterhoverheight.cpp@1,7", "indra/newview/llfloaterhoverheight.cpp@29,50", "indra/newview/llfloaterhoverheight.cpp@93,41", "indra/newview/llfloaterhoverheight.cpp@228,216"]}
- {"path": "indra/newview/alviewermenu.cpp", "hunks": ["indra/newview/alviewermenu.cpp@18,36", "indra/newview/alviewermenu.cpp@604,830", "indra/newview/alviewermenu.cpp@1471,83"]}
- {"path": "indra/newview/app_settings/key_bindings.xml", "hunks": ["indra/newview/app_settings/key_bindings.xml@1,13", "indra/newview/app_settings/key_bindings.xml@47,13", "indra/newview/app_settings/key_bindings.xml@136,13", "indra/newview/app_settings/key_bindings.xml@232,13"]}
- {"path": "indra/newview/app_settings/settings_alchemy.xml", "hunks": ["indra/newview/app_settings/settings_alchemy.xml@2771,233"]}
- {"path": "indra/newview/llcontrolavatar.cpp", "hunks": ["indra/newview/llcontrolavatar.cpp@194,9", "indra/newview/llcontrolavatar.cpp@248,21"]}
- {"path": "indra/newview/llviewerinput.cpp", "hunks": ["indra/newview/llviewerinput.cpp@1025,20", "indra/newview/llviewerinput.cpp@1215,27", "indra/newview/llviewerinput.cpp@1648,84", "indra/newview/llviewerinput.cpp@1801,14"]}
- {"path": "indra/newview/llviewermenu.cpp", "hunks": ["indra/newview/llviewermenu.cpp@4723,10"]}
- {"path": "indra/newview/llviewerwindow.cpp", "hunks": ["indra/newview/llviewerwindow.cpp@202,7", "indra/newview/llviewerwindow.cpp@263,272", "indra/newview/llviewerwindow.cpp@3367,14"]}
- {"path": "indra/newview/llvoavatar.cpp", "hunks": ["indra/newview/llvoavatar.cpp@49,24", "indra/newview/llvoavatar.cpp@2948,33", "indra/newview/llvoavatar.cpp@5061,21", "indra/newview/llvoavatar.cpp@5144,25", "indra/newview/llvoavatar.cpp@5822,13", "indra/newview/llvoavatar.cpp@5914,45", "indra/newview/llvoavatar.cpp@9414,10", "indra/newview/llvoavatar.cpp@9458,15", "indra/newview/llvoavatar.cpp@9479,12", "indra/newview/llvoavatar.cpp@12827,163"]}
- {"path": "indra/newview/llvoavatar.h", "hunks": ["indra/newview/llvoavatar.h@56,8", "indra/newview/llvoavatar.h@550,45", "indra/newview/llvoavatar.h@1259,17"]}
- {"path": "indra/newview/pipeline.cpp", "hunks": ["indra/newview/pipeline.cpp@87,17", "indra/newview/pipeline.cpp@1697,29674"]}
- {"path": "indra/newview/pipeline.h", "hunks": ["indra/newview/pipeline.h@905,67"]}
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
- {"file": "indra/newview/CMakeLists.txt", "line": 393, "text": "alavataranimspeed.cpp", "source": "indra/newview/alavataranimspeed.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 365, "text": "alavatarkey.cpp # [ClientHeight]", "source": "indra/newview/alavatarkey.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1311, "text": "alavatarkey.h # [ClientHeight]", "source": "indra/newview/alavatarkey.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 362, "text": "alavatarlightresponse.cpp # [AvatarLightResponse]", "source": "indra/newview/alavatarlightresponse.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1309, "text": "alavatarlightresponsemodel.h # [AvatarLightResponse]", "source": "indra/newview/alavatarlightresponsemodel.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 366, "text": "alclientheight.cpp # [ClientHeight]", "source": "indra/newview/alclientheight.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1312, "text": "alclientheight.h # [ClientHeight]", "source": "indra/newview/alclientheight.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1313, "text": "alclientheightmodel.h # [ClientHeight]", "source": "indra/newview/alclientheightmodel.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 2997, "text": "# (alclientheightmodel.h), so its TUT is registered directly like ALGazeMath.", "source": "indra/newview/alclientheightmodel.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 225, "text": "alfloaterblocked.cpp", "source": "indra/newview/alfloaterblocked.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 235, "text": "alfloaterphototools.cpp", "source": "indra/newview/alfloaterphototools.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1187, "text": "alfloaterphototools.h", "source": "indra/newview/alfloaterphototools.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 372, "text": "alfloaterultimatediopter.cpp", "source": "indra/newview/alfloaterultimatediopter.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1317, "text": "alfloaterultimatediopter.h", "source": "indra/newview/alfloaterultimatediopter.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1432, "text": "algazeblink.h", "source": "indra/newview/algazeblink.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1427, "text": "algazemath.h", "source": "indra/newview/algazemath.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1437, "text": "algazemotor.h", "source": "indra/newview/algazemotor.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 394, "text": "alghoststudio.cpp", "source": "indra/newview/alghoststudio.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 194, "text": "allocalfogmanager.cpp", "source": "indra/newview/allocalfogmanager.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 400, "text": "alpanelghoststudio.cpp", "source": "indra/newview/alpanelghoststudio.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 435, "text": "alpanellensgaze.cpp", "source": "indra/newview/alpanellensgaze.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1438, "text": "alpanellensgaze.h", "source": "indra/newview/alpanellensgaze.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 403, "text": "altoolfocuspick.cpp", "source": "indra/newview/altoolfocuspick.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1378, "text": "altoolfocuspick.h", "source": "indra/newview/altoolfocuspick.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 278, "text": "alviewermenu.cpp", "source": "indra/newview/alviewermenu.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1325, "text": "alviewfinder.h", "source": "indra/newview/alviewfinder.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 396, "text": "alworldoverlayviz.cpp", "source": "indra/newview/alworldoverlayviz.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1366, "text": "alworldoverlayviz.h", "source": "indra/newview/alworldoverlayviz.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 2304, "text": "app_settings/key_bindings.xml", "source": "indra/newview/app_settings/key_bindings.xml"}
- {"file": "indra/newview/CMakeLists.txt", "line": 2306, "text": "app_settings/settings.xml", "source": "indra/newview/app_settings/settings.xml"}
- {"file": "indra/newview/CMakeLists.txt", "line": 2307, "text": "app_settings/settings_alchemy.xml", "source": "indra/newview/app_settings/settings_alchemy.xml"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1240, "text": "fsposeranimator.h", "source": "indra/newview/fsposeranimator.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 340, "text": "llactormover.cpp", "source": "indra/newview/llactormover.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1296, "text": "llactormover.h", "source": "indra/newview/llactormover.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 305, "text": "llagentcamera.cpp", "source": "indra/newview/llagentcamera.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 301, "text": "llaisapi.cpp", "source": "indra/newview/llaisapi.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 318, "text": "llappviewer.cpp", "source": "indra/newview/llappviewer.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 407, "text": "llcinematiccamera.cpp", "source": "indra/newview/llcinematiccamera.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 464, "text": "llcontrolavatar.cpp", "source": "indra/newview/llcontrolavatar.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 408, "text": "lldirectorcast.cpp", "source": "indra/newview/lldirectorcast.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 491, "text": "lldrawpoolwater.cpp", "source": "indra/newview/lldrawpoolwater.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 494, "text": "lldynamictexture.cpp", "source": "indra/newview/lldynamictexture.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 509, "text": "llfetchedgltfmaterial.cpp", "source": "indra/newview/llfetchedgltfmaterial.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 574, "text": "llfloaterhoverheight.cpp", "source": "indra/newview/llfloaterhoverheight.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1579, "text": "llfloaterhoverheight.h", "source": "indra/newview/llfloaterhoverheight.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 618, "text": "llfloaterpreferencesgraphicsadvanced.cpp", "source": "indra/newview/llfloaterpreferencesgraphicsadvanced.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 665, "text": "llghostavatar.cpp", "source": "indra/newview/llghostavatar.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1674, "text": "llghostavatar.h", "source": "indra/newview/llghostavatar.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 681, "text": "llhudeffectlookat.cpp", "source": "indra/newview/llhudeffectlookat.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 683, "text": "llhudeffecttrail.cpp", "source": "indra/newview/llhudeffecttrail.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 726, "text": "lllocalmesh.cpp", "source": "indra/newview/lllocalmesh.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 747, "text": "llmeshrepository.cpp", "source": "indra/newview/llmeshrepository.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1890, "text": "llprismgate.h", "source": "indra/newview/llprismgate.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 898, "text": "llprismlens.cpp", "source": "indra/newview/llprismlens.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 913, "text": "llscenemonitor.cpp", "source": "indra/newview/llscenemonitor.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 951, "text": "llsurface.cpp", "source": "indra/newview/llsurface.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 952, "text": "llsurfacepatch.cpp", "source": "indra/newview/llsurfacepatch.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 989, "text": "lltoolpie.cpp", "source": "indra/newview/lltoolpie.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1037, "text": "llviewerinput.cpp", "source": "indra/newview/llviewerinput.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1033, "text": "llviewerjoint.cpp", "source": "indra/newview/llviewerjoint.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1042, "text": "llviewermenu.cpp", "source": "indra/newview/llviewermenu.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1048, "text": "llvieweroctree.cpp", "source": "indra/newview/llvieweroctree.cpp"}
- {"file": "indra/llwindow/CMakeLists.txt", "line": 107, "text": "# llviewerwindow.cpp's (guarded) include still resolves.", "source": "indra/newview/llviewerwindow.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1067, "text": "llviewerwindow.cpp", "source": "indra/newview/llviewerwindow.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1072, "text": "llvoavatar.cpp", "source": "indra/newview/llvoavatar.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 2074, "text": "llvoavatar.h", "source": "indra/newview/llvoavatar.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1082, "text": "llvopartgroup.cpp", "source": "indra/newview/llvopartgroup.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1106, "text": "pipeline.cpp", "source": "indra/newview/pipeline.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 2107, "text": "pipeline.h", "source": "indra/newview/pipeline.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1110, "text": "rlveffects.cpp", "source": "indra/newview/rlveffects.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1113, "text": "rlvhandler.cpp", "source": "indra/newview/rlvhandler.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1116, "text": "rlvlocks.cpp", "source": "indra/newview/rlvlocks.cpp"}
- {"file": "indra/llrender/CMakeLists.txt", "line": 169, "text": "\"LLFONT_TEST_FONTS_XML=\\\"${INDRA_SOURCE_DIR}/newview/skins/default/xui/en/fonts.xml\\\"\")", "source": "indra/newview/skins/default/xui/en/fonts.xml"}

## kit wiring pages

- `focus_tab`
- `frame_orbit_tab`
- `gaze_tab`

## checks

None identified by local evidence; not a claim that none is needed.

Full include/call edges: ../features.json. Shared hooks: ../hooks.md. Setting values and save coverage: ../settings.csv.

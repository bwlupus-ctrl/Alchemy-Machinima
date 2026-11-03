# crowd: dependency evidence

Landing layer 4. Place a group; vary Width/Depth/Columns/Rows; move/delete/reload group and preserve stable IDs.

Candidate associations require review. Includes/calls are discovery edges, not a complete call graph or proof of build closure.

## owned

- `indra/newview/alavatarstaging.cpp`
- `indra/newview/alavatarstaging.h`
- `indra/newview/alavatarstagingmodel.cpp`
- `indra/newview/alavatarstagingmodel.h`
- `indra/newview/aldanceball.cpp`
- `indra/newview/aldanceball.h`
- `indra/newview/alformationsolver.cpp`
- `indra/newview/alformationsolver.h`
- `indra/newview/algazemotor.h`
- `indra/newview/alghostgroupmodel.cpp`
- `indra/newview/alghostgroupmodel.h`
- `indra/newview/alghostinteractionstate.cpp`
- `indra/newview/alghostinteractionstate.h`
- `indra/newview/alghostmanipproxy.cpp`
- `indra/newview/alghostnameplates.cpp`
- `indra/newview/alghostnameplates.h`
- `indra/newview/alghostplacementadapter.cpp`
- `indra/newview/alghostplacementadapter.h`
- `indra/newview/alghostplacementresolver.h`
- `indra/newview/alghostspawnengine.cpp`
- `indra/newview/alghostspawnengine.h`
- `indra/newview/alghoststudio.cpp`
- `indra/newview/alghoststudio.h`
- `indra/newview/alpaneldanceball.cpp`
- `indra/newview/alpaneldanceball.h`
- `indra/newview/alpanelghoststudio.cpp`
- `indra/newview/alpanelghoststudio.h`
- `indra/newview/alsteppedanimruntime.cpp`
- `indra/newview/altoolcrowdplace.cpp`
- `indra/newview/altoolcrowdplace.h`
- `indra/newview/altoolfocuspick.cpp`
- `indra/newview/altoolghostedit.cpp`
- `indra/newview/altoolghostplace.cpp`
- `indra/newview/altoolghostplace.h`
- `indra/newview/alvcammediafileio.cpp`
- `indra/newview/app_settings/shaders/class1/deferred/tronWorldF.glsl`
- `indra/newview/app_settings/shaders/class1/interface/actorghostF.glsl`
- `indra/newview/llactormover.cpp`
- `indra/newview/lldirectorcast.h`
- `indra/newview/llfloaterdirector.cpp`
- `indra/newview/llghostavatar.cpp`
- `indra/newview/llghostavatar.h`
- `indra/newview/llreshadebridge.cpp`
- `indra/newview/llreshadebridgeabi.h`
- `indra/newview/skins/default/xui/en/panel_dance_ball.xml`
- `indra/newview/skins/default/xui/en/panel_ghost_studio.xml`
- `reshade-addon/external/reshade/reshade_api.hpp`
- `reshade-addon/external/reshade/reshade_api_device.hpp`
- `reshade-addon/external/reshade/reshade_api_pipeline.hpp`

## hooks

- {"path": "indra/newview/CMakeLists.txt", "hunks": ["indra/newview/CMakeLists.txt@335,117", "indra/newview/CMakeLists.txt@1290,167", "indra/newview/CMakeLists.txt@2851,32"]}
- {"path": "indra/newview/app_settings/settings.xml", "hunks": ["indra/newview/app_settings/settings.xml@2,3458", "indra/newview/app_settings/settings.xml@6556,14744", "indra/newview/app_settings/settings.xml@22957,412", "indra/newview/app_settings/settings.xml@23480,362", "indra/newview/app_settings/settings.xml@29160,100", "indra/newview/app_settings/settings.xml@29305,2466"]}
- {"path": "indra/newview/alchatcommand.cpp", "hunks": ["indra/newview/alchatcommand.cpp@372,747"]}
- {"path": "indra/newview/app_settings/settings_alchemy.xml", "hunks": ["indra/newview/app_settings/settings_alchemy.xml@1789,13"]}
- {"path": "indra/newview/llviewerdisplay.cpp", "hunks": ["indra/newview/llviewerdisplay.cpp@2083,28"]}
- {"path": "indra/newview/llviewerwindow.cpp", "hunks": ["indra/newview/llviewerwindow.cpp@179,8", "indra/newview/llviewerwindow.cpp@4049,19"]}
- {"path": "indra/newview/skins/default/xui/en/floater_lightbox_settings.xml", "hunks": ["indra/newview/skins/default/xui/en/floater_lightbox_settings.xml@4360,2151"]}
- {"path": "indra/newview/app_settings/shaders/class2/deferred/alphaF.glsl", "hunks": ["indra/newview/app_settings/shaders/class2/deferred/alphaF.glsl@485,30"]}
- {"path": "indra/newview/llselectmgr.cpp", "hunks": ["indra/newview/llselectmgr.cpp@4376,35", "indra/newview/llselectmgr.cpp@4559,28"]}
- {"path": "indra/newview/llviewershadermgr.cpp", "hunks": ["indra/newview/llviewershadermgr.cpp@617,35", "indra/newview/llviewershadermgr.cpp@2694,18", "indra/newview/llviewershadermgr.cpp@4168,11"]}

## settings

- `GhostMotionAnimations`
- `GhostStudioCrowdHeightUnitsPerPixel`
- `GhostStudioCrowdRotationStepDegrees`
- `GhostStudioFormationPreviewHeight`
- `GhostStudioFormationPreviewScale`
- `GhostStudioGuideWidthPixels`
- `GhostStudioLookSpread`
- `GhostStudioMixLook1`
- `GhostStudioMixLook2`
- `GhostStudioMixLook3`
- `GhostStudioMixLook4`
- `GhostStudioMixLook5`
- `GhostStudioMixLook6`
- `GhostStudioOverlayPalette`
- `GhostStudioRefreshAlways`
- `GhostStudioRippleFrom`
- `GhostStudioShowFormationPreview`
- `GhostStudioStartMode`
- `GhostTrueMirrorCopyFacing`

## shaders

- `indra/newview/app_settings/shaders/class1/deferred/tronWorldF.glsl`
- `indra/newview/app_settings/shaders/class1/interface/actorghostF.glsl`
- `indra/newview/app_settings/shaders/class2/deferred/alphaF.glsl`

## xui

- `indra/newview/skins/default/xui/en/director_kit_index.xml`
- `indra/newview/skins/default/xui/en/floater_about.xml`
- `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml`
- `indra/newview/skins/default/xui/en/panel_dance_ball.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_cast_dance.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_cast_ghosts.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_cast_move.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml`
- `indra/newview/skins/default/xui/en/panel_ghost_studio.xml`

## generated runtime

- `indra/newview/skins/default/xui/en/director_kit_index.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_cast_dance.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_cast_ghosts.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_cast_move.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml`

## cmake entries

- {"file": "indra/newview/CMakeLists.txt", "line": 193, "text": "alavatarstaging.cpp", "source": "indra/newview/alavatarstaging.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1143, "text": "alavatarstaging.h", "source": "indra/newview/alavatarstaging.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 194, "text": "alavatarstagingmodel.cpp", "source": "indra/newview/alavatarstagingmodel.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 2865, "text": "alavatarstagingmodel.cpp", "source": "indra/newview/alavatarstagingmodel.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1144, "text": "alavatarstagingmodel.h", "source": "indra/newview/alavatarstagingmodel.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 186, "text": "alchatcommand.cpp", "source": "indra/newview/alchatcommand.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 191, "text": "aldanceball.cpp", "source": "indra/newview/aldanceball.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1141, "text": "aldanceball.h", "source": "indra/newview/aldanceball.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 347, "text": "alformationsolver.cpp", "source": "indra/newview/alformationsolver.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 2857, "text": "alformationsolver.cpp", "source": "indra/newview/alformationsolver.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1306, "text": "alformationsolver.h", "source": "indra/newview/alformationsolver.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1441, "text": "algazemotor.h", "source": "indra/newview/algazemotor.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 385, "text": "alghostgroupmodel.cpp", "source": "indra/newview/alghostgroupmodel.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 2873, "text": "alghostgroupmodel.cpp", "source": "indra/newview/alghostgroupmodel.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1340, "text": "alghostgroupmodel.h", "source": "indra/newview/alghostgroupmodel.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 386, "text": "alghostinteractionstate.cpp", "source": "indra/newview/alghostinteractionstate.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 2874, "text": "alghostinteractionstate.cpp", "source": "indra/newview/alghostinteractionstate.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1342, "text": "alghostinteractionstate.h", "source": "indra/newview/alghostinteractionstate.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 389, "text": "alghostmanipproxy.cpp", "source": "indra/newview/alghostmanipproxy.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 391, "text": "alghostnameplates.cpp", "source": "indra/newview/alghostnameplates.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1349, "text": "alghostnameplates.h", "source": "indra/newview/alghostnameplates.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 388, "text": "alghostplacementadapter.cpp", "source": "indra/newview/alghostplacementadapter.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1344, "text": "alghostplacementadapter.h", "source": "indra/newview/alghostplacementadapter.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1343, "text": "alghostplacementresolver.h", "source": "indra/newview/alghostplacementresolver.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 344, "text": "alghostspawnengine.cpp", "source": "indra/newview/alghostspawnengine.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1302, "text": "alghostspawnengine.h", "source": "indra/newview/alghostspawnengine.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 395, "text": "alghoststudio.cpp", "source": "indra/newview/alghoststudio.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1367, "text": "alghoststudio.h", "source": "indra/newview/alghoststudio.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 438, "text": "alpaneldanceball.cpp", "source": "indra/newview/alpaneldanceball.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1443, "text": "alpaneldanceball.h", "source": "indra/newview/alpaneldanceball.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 401, "text": "alpanelghoststudio.cpp", "source": "indra/newview/alpanelghoststudio.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1376, "text": "alpanelghoststudio.h", "source": "indra/newview/alpanelghoststudio.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 249, "text": "alsteppedanimruntime.cpp", "source": "indra/newview/alsteppedanimruntime.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 403, "text": "altoolcrowdplace.cpp", "source": "indra/newview/altoolcrowdplace.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1380, "text": "altoolcrowdplace.h", "source": "indra/newview/altoolcrowdplace.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 404, "text": "altoolfocuspick.cpp", "source": "indra/newview/altoolfocuspick.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 405, "text": "altoolghostedit.cpp", "source": "indra/newview/altoolghostedit.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 406, "text": "altoolghostplace.cpp", "source": "indra/newview/altoolghostplace.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1383, "text": "altoolghostplace.h", "source": "indra/newview/altoolghostplace.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 901, "text": "alvcammediafileio.cpp", "source": "indra/newview/alvcammediafileio.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 2310, "text": "app_settings/settings.xml", "source": "indra/newview/app_settings/settings.xml"}
- {"file": "indra/newview/CMakeLists.txt", "line": 2311, "text": "app_settings/settings_alchemy.xml", "source": "indra/newview/app_settings/settings_alchemy.xml"}
- {"file": "indra/newview/CMakeLists.txt", "line": 341, "text": "llactormover.cpp", "source": "indra/newview/llactormover.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1386, "text": "lldirectorcast.h", "source": "indra/newview/lldirectorcast.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 411, "text": "llfloaterdirector.cpp", "source": "indra/newview/llfloaterdirector.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 667, "text": "llghostavatar.cpp", "source": "indra/newview/llghostavatar.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1678, "text": "llghostavatar.h", "source": "indra/newview/llghostavatar.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 913, "text": "llreshadebridge.cpp", "source": "indra/newview/llreshadebridge.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1911, "text": "llreshadebridgeabi.h", "source": "indra/newview/llreshadebridgeabi.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 929, "text": "llselectmgr.cpp", "source": "indra/newview/llselectmgr.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1025, "text": "llviewerdisplay.cpp", "source": "indra/newview/llviewerdisplay.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1049, "text": "llviewerobjectlist.cpp", "source": "indra/newview/llviewerobjectlist.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1059, "text": "llviewershadermgr.cpp", "source": "indra/newview/llviewershadermgr.cpp"}
- {"file": "indra/llwindow/CMakeLists.txt", "line": 107, "text": "# llviewerwindow.cpp's (guarded) include still resolves.", "source": "indra/newview/llviewerwindow.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1069, "text": "llviewerwindow.cpp", "source": "indra/newview/llviewerwindow.cpp"}

## kit wiring pages

- `dance_tab`
- `ghosts_tab`

## checks

None identified by local evidence; not a claim that none is needed.

Full include/call edges: ../features.json. Shared hooks: ../hooks.md. Setting values and save coverage: ../settings.csv.

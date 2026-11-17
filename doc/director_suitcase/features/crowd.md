# crowd: dependency evidence

Landing layer 4. Place a group; vary Width/Depth/Columns/Rows; move/delete/reload group and preserve stable IDs.

Candidate associations require review. Includes/calls are discovery edges, not a complete call graph or proof of build closure.

## owned

- `indra/newview/alavatarstaging.cpp`
- `indra/newview/alavatarstaging.h`
- `indra/newview/alavatarstagingmodel.cpp`
- `indra/newview/alavatarstagingmodel.h`
- `indra/newview/alcineflycamhandover.h`
- `indra/newview/aldanceball.cpp`
- `indra/newview/aldanceball.h`
- `indra/newview/aldirectorselect.cpp`
- `indra/newview/alformationsolver.cpp`
- `indra/newview/alformationsolver.h`
- `indra/newview/algazemotor.h`
- `indra/newview/algazenearby.h`
- `indra/newview/alghostcloneactions.cpp`
- `indra/newview/alghostcloneactions.h`
- `indra/newview/alghostclonemenu.cpp`
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
- `indra/newview/altooldirectorpick.cpp`
- `indra/newview/altooldirectorpick.h`
- `indra/newview/altoolfocuspick.cpp`
- `indra/newview/altoolghostedit.cpp`
- `indra/newview/altoolghostplace.cpp`
- `indra/newview/altoolghostplace.h`
- `indra/newview/alvcammediafileio.cpp`
- `indra/newview/app_settings/shaders/class1/deferred/tronWorldF.glsl`
- `indra/newview/llactormover.cpp`
- `indra/newview/lldirectorcast.h`
- `indra/newview/llghostavatar.cpp`
- `indra/newview/llghostavatar.h`
- `indra/newview/llreshadebridgeabi.h`
- `indra/newview/skins/default/xui/en/menu_ghost_clone.xml`
- `indra/newview/skins/default/xui/en/panel_dance_ball.xml`
- `indra/newview/skins/default/xui/en/panel_ghost_studio.xml`
- `reshade-addon/external/reshade/reshade_api.hpp`
- `reshade-addon/external/reshade/reshade_api_device.hpp`
- `reshade-addon/external/reshade/reshade_api_pipeline.hpp`

## hooks

- {"path": "indra/newview/CMakeLists.txt", "hunks": ["indra/newview/CMakeLists.txt@335,126", "indra/newview/CMakeLists.txt@1303,186", "indra/newview/CMakeLists.txt@2883,33"]}
- {"path": "indra/newview/app_settings/settings.xml", "hunks": ["indra/newview/app_settings/settings.xml@2,3497", "indra/newview/app_settings/settings.xml@6617,14888", "indra/newview/app_settings/settings.xml@22775,831", "indra/newview/app_settings/settings.xml@23742,362", "indra/newview/app_settings/settings.xml@29433,100", "indra/newview/app_settings/settings.xml@29578,2466"]}
- {"path": "indra/newview/alchatcommand.cpp", "hunks": ["indra/newview/alchatcommand.cpp@372,747"]}
- {"path": "indra/newview/app_settings/settings_alchemy.xml", "hunks": ["indra/newview/app_settings/settings_alchemy.xml@1800,13"]}
- {"path": "indra/newview/llviewerdisplay.cpp", "hunks": ["indra/newview/llviewerdisplay.cpp@2146,28"]}
- {"path": "indra/newview/llviewerwindow.cpp", "hunks": ["indra/newview/llviewerwindow.cpp@182,8", "indra/newview/llviewerwindow.cpp@4070,19"]}
- {"path": "indra/newview/skins/default/xui/en/floater_lightbox_settings.xml", "hunks": ["indra/newview/skins/default/xui/en/floater_lightbox_settings.xml@4360,2151"]}
- {"path": "indra/newview/skins/default/xui/en/strings.xml", "hunks": ["indra/newview/skins/default/xui/en/strings.xml@4544,64"]}
- {"path": "indra/newview/app_settings/shaders/class2/deferred/alphaF.glsl", "hunks": ["indra/newview/app_settings/shaders/class2/deferred/alphaF.glsl@485,30"]}
- {"path": "indra/newview/llselectmgr.cpp", "hunks": ["indra/newview/llselectmgr.cpp@4387,35", "indra/newview/llselectmgr.cpp@4570,28"]}
- {"path": "indra/newview/llviewershadermgr.cpp", "hunks": ["indra/newview/llviewershadermgr.cpp@650,35", "indra/newview/llviewershadermgr.cpp@2760,58", "indra/newview/llviewershadermgr.cpp@4257,11"]}

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
- `indra/newview/app_settings/shaders/class2/deferred/alphaF.glsl`

## xui

- `indra/newview/skins/default/xui/en/director_kit_index.xml`
- `indra/newview/skins/default/xui/en/floater_about.xml`
- `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml`
- `indra/newview/skins/default/xui/en/menu_ghost_clone.xml`
- `indra/newview/skins/default/xui/en/panel_dance_ball.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_cast_dance.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_cast_ghosts.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_cast_groups.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml`
- `indra/newview/skins/default/xui/en/panel_ghost_studio.xml`
- `indra/newview/skins/default/xui/en/strings.xml`

## generated runtime

- `indra/newview/skins/default/xui/en/director_kit_index.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_cast_dance.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_cast_ghosts.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_cast_groups.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml`

## cmake entries

- {"file": "indra/newview/CMakeLists.txt", "line": 192, "text": "alavatarstaging.cpp", "source": "indra/newview/alavatarstaging.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1153, "text": "alavatarstaging.h", "source": "indra/newview/alavatarstaging.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 193, "text": "alavatarstagingmodel.cpp", "source": "indra/newview/alavatarstagingmodel.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 2898, "text": "alavatarstagingmodel.cpp", "source": "indra/newview/alavatarstagingmodel.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1154, "text": "alavatarstagingmodel.h", "source": "indra/newview/alavatarstagingmodel.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 185, "text": "alchatcommand.cpp", "source": "indra/newview/alchatcommand.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1218, "text": "alcineflycamhandover.h", "source": "indra/newview/alcineflycamhandover.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 190, "text": "aldanceball.cpp", "source": "indra/newview/aldanceball.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1151, "text": "aldanceball.h", "source": "indra/newview/aldanceball.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 408, "text": "aldirectorselect.cpp # [CloneMenu] Select in Director helper", "source": "indra/newview/aldirectorselect.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 348, "text": "alformationsolver.cpp", "source": "indra/newview/alformationsolver.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 2890, "text": "alformationsolver.cpp", "source": "indra/newview/alformationsolver.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1320, "text": "alformationsolver.h", "source": "indra/newview/alformationsolver.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1474, "text": "algazemotor.h", "source": "indra/newview/algazemotor.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1472, "text": "algazenearby.h", "source": "indra/newview/algazenearby.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 406, "text": "alghostcloneactions.cpp # [CloneMenu] Clones page row actions shared with the menus", "source": "indra/newview/alghostcloneactions.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1401, "text": "alghostcloneactions.h", "source": "indra/newview/alghostcloneactions.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 407, "text": "alghostclonemenu.cpp # [CloneMenu] clone right-click menu", "source": "indra/newview/alghostclonemenu.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 388, "text": "alghostgroupmodel.cpp", "source": "indra/newview/alghostgroupmodel.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 2907, "text": "alghostgroupmodel.cpp", "source": "indra/newview/alghostgroupmodel.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1356, "text": "alghostgroupmodel.h", "source": "indra/newview/alghostgroupmodel.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 389, "text": "alghostinteractionstate.cpp", "source": "indra/newview/alghostinteractionstate.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 2908, "text": "alghostinteractionstate.cpp", "source": "indra/newview/alghostinteractionstate.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1358, "text": "alghostinteractionstate.h", "source": "indra/newview/alghostinteractionstate.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 392, "text": "alghostmanipproxy.cpp", "source": "indra/newview/alghostmanipproxy.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 394, "text": "alghostnameplates.cpp", "source": "indra/newview/alghostnameplates.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1365, "text": "alghostnameplates.h", "source": "indra/newview/alghostnameplates.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 391, "text": "alghostplacementadapter.cpp", "source": "indra/newview/alghostplacementadapter.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1360, "text": "alghostplacementadapter.h", "source": "indra/newview/alghostplacementadapter.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1359, "text": "alghostplacementresolver.h", "source": "indra/newview/alghostplacementresolver.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 345, "text": "alghostspawnengine.cpp", "source": "indra/newview/alghostspawnengine.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1316, "text": "alghostspawnengine.h", "source": "indra/newview/alghostspawnengine.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 398, "text": "alghoststudio.cpp", "source": "indra/newview/alghoststudio.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1388, "text": "alghoststudio.h", "source": "indra/newview/alghoststudio.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 448, "text": "alpaneldanceball.cpp", "source": "indra/newview/alpaneldanceball.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1476, "text": "alpaneldanceball.h", "source": "indra/newview/alpaneldanceball.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 409, "text": "alpanelghoststudio.cpp", "source": "indra/newview/alpanelghoststudio.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1405, "text": "alpanelghoststudio.h", "source": "indra/newview/alpanelghoststudio.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 250, "text": "alsteppedanimruntime.cpp", "source": "indra/newview/alsteppedanimruntime.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 411, "text": "altoolcrowdplace.cpp", "source": "indra/newview/altoolcrowdplace.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1409, "text": "altoolcrowdplace.h", "source": "indra/newview/altoolcrowdplace.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 412, "text": "altooldirectorpick.cpp # [CloneMenu] Actor bar Select pick mode", "source": "indra/newview/altooldirectorpick.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1410, "text": "altooldirectorpick.h", "source": "indra/newview/altooldirectorpick.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 413, "text": "altoolfocuspick.cpp", "source": "indra/newview/altoolfocuspick.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 414, "text": "altoolghostedit.cpp", "source": "indra/newview/altoolghostedit.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 415, "text": "altoolghostplace.cpp", "source": "indra/newview/altoolghostplace.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1413, "text": "altoolghostplace.h", "source": "indra/newview/altoolghostplace.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 911, "text": "alvcammediafileio.cpp", "source": "indra/newview/alvcammediafileio.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 2343, "text": "app_settings/settings.xml", "source": "indra/newview/app_settings/settings.xml"}
- {"file": "indra/newview/CMakeLists.txt", "line": 2344, "text": "app_settings/settings_alchemy.xml", "source": "indra/newview/app_settings/settings_alchemy.xml"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1256, "text": "fsposeranimator.h", "source": "indra/newview/fsposeranimator.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 342, "text": "llactormover.cpp", "source": "indra/newview/llactormover.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1416, "text": "lldirectorcast.h", "source": "indra/newview/lldirectorcast.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 677, "text": "llghostavatar.cpp", "source": "indra/newview/llghostavatar.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1711, "text": "llghostavatar.h", "source": "indra/newview/llghostavatar.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1944, "text": "llreshadebridgeabi.h", "source": "indra/newview/llreshadebridgeabi.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 939, "text": "llselectmgr.cpp", "source": "indra/newview/llselectmgr.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1035, "text": "llviewerdisplay.cpp", "source": "indra/newview/llviewerdisplay.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1059, "text": "llviewerobjectlist.cpp", "source": "indra/newview/llviewerobjectlist.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1069, "text": "llviewershadermgr.cpp", "source": "indra/newview/llviewershadermgr.cpp"}
- {"file": "indra/llwindow/CMakeLists.txt", "line": 107, "text": "# llviewerwindow.cpp's (guarded) include still resolves.", "source": "indra/newview/llviewerwindow.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1079, "text": "llviewerwindow.cpp", "source": "indra/newview/llviewerwindow.cpp"}

## kit wiring pages

- `dance_tab`
- `ghosts_tab`

## checks

None identified by local evidence; not a claim that none is needed.

Full include/call edges: ../features.json. Shared hooks: ../hooks.md. Setting values and save coverage: ../settings.csv.

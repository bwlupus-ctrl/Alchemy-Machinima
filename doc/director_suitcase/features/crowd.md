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

- {"path": "indra/newview/CMakeLists.txt", "hunks": ["indra/newview/CMakeLists.txt@347,126", "indra/newview/CMakeLists.txt@1331,186", "indra/newview/CMakeLists.txt@2911,33"]}
- {"path": "indra/newview/app_settings/settings.xml", "hunks": ["indra/newview/app_settings/settings.xml@2,3504", "indra/newview/app_settings/settings.xml@6624,14888", "indra/newview/app_settings/settings.xml@22782,831", "indra/newview/app_settings/settings.xml@23749,362", "indra/newview/app_settings/settings.xml@29440,100", "indra/newview/app_settings/settings.xml@29585,2466"]}
- {"path": "indra/newview/alchatcommand.cpp", "hunks": ["indra/newview/alchatcommand.cpp@372,747"]}
- {"path": "indra/newview/app_settings/settings_alchemy.xml", "hunks": ["indra/newview/app_settings/settings_alchemy.xml@1800,13"]}
- {"path": "indra/newview/llviewerdisplay.cpp", "hunks": ["indra/newview/llviewerdisplay.cpp@2153,35"]}
- {"path": "indra/newview/llviewerwindow.cpp", "hunks": ["indra/newview/llviewerwindow.cpp@186,8", "indra/newview/llviewerwindow.cpp@4104,19"]}
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

- {"file": "indra/newview/CMakeLists.txt", "line": 204, "text": "alavatarstaging.cpp", "source": "indra/newview/alavatarstaging.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1165, "text": "alavatarstaging.h", "source": "indra/newview/alavatarstaging.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 205, "text": "alavatarstagingmodel.cpp", "source": "indra/newview/alavatarstagingmodel.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 2926, "text": "alavatarstagingmodel.cpp", "source": "indra/newview/alavatarstagingmodel.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1166, "text": "alavatarstagingmodel.h", "source": "indra/newview/alavatarstagingmodel.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 197, "text": "alchatcommand.cpp", "source": "indra/newview/alchatcommand.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1246, "text": "alcineflycamhandover.h", "source": "indra/newview/alcineflycamhandover.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 202, "text": "aldanceball.cpp", "source": "indra/newview/aldanceball.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1163, "text": "aldanceball.h", "source": "indra/newview/aldanceball.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 420, "text": "aldirectorselect.cpp # [CloneMenu] Select in Director helper", "source": "indra/newview/aldirectorselect.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 360, "text": "alformationsolver.cpp", "source": "indra/newview/alformationsolver.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 2918, "text": "alformationsolver.cpp", "source": "indra/newview/alformationsolver.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1348, "text": "alformationsolver.h", "source": "indra/newview/alformationsolver.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1502, "text": "algazemotor.h", "source": "indra/newview/algazemotor.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1500, "text": "algazenearby.h", "source": "indra/newview/algazenearby.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 418, "text": "alghostcloneactions.cpp # [CloneMenu] Clones page row actions shared with the menus", "source": "indra/newview/alghostcloneactions.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1429, "text": "alghostcloneactions.h", "source": "indra/newview/alghostcloneactions.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 419, "text": "alghostclonemenu.cpp # [CloneMenu] clone right-click menu", "source": "indra/newview/alghostclonemenu.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 400, "text": "alghostgroupmodel.cpp", "source": "indra/newview/alghostgroupmodel.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 2935, "text": "alghostgroupmodel.cpp", "source": "indra/newview/alghostgroupmodel.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1384, "text": "alghostgroupmodel.h", "source": "indra/newview/alghostgroupmodel.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 401, "text": "alghostinteractionstate.cpp", "source": "indra/newview/alghostinteractionstate.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 2936, "text": "alghostinteractionstate.cpp", "source": "indra/newview/alghostinteractionstate.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1386, "text": "alghostinteractionstate.h", "source": "indra/newview/alghostinteractionstate.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 404, "text": "alghostmanipproxy.cpp", "source": "indra/newview/alghostmanipproxy.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 406, "text": "alghostnameplates.cpp", "source": "indra/newview/alghostnameplates.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1393, "text": "alghostnameplates.h", "source": "indra/newview/alghostnameplates.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 403, "text": "alghostplacementadapter.cpp", "source": "indra/newview/alghostplacementadapter.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1388, "text": "alghostplacementadapter.h", "source": "indra/newview/alghostplacementadapter.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1387, "text": "alghostplacementresolver.h", "source": "indra/newview/alghostplacementresolver.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 357, "text": "alghostspawnengine.cpp", "source": "indra/newview/alghostspawnengine.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1344, "text": "alghostspawnengine.h", "source": "indra/newview/alghostspawnengine.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 410, "text": "alghoststudio.cpp", "source": "indra/newview/alghoststudio.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1416, "text": "alghoststudio.h", "source": "indra/newview/alghoststudio.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 460, "text": "alpaneldanceball.cpp", "source": "indra/newview/alpaneldanceball.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1504, "text": "alpaneldanceball.h", "source": "indra/newview/alpaneldanceball.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 421, "text": "alpanelghoststudio.cpp", "source": "indra/newview/alpanelghoststudio.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1433, "text": "alpanelghoststudio.h", "source": "indra/newview/alpanelghoststudio.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 262, "text": "alsteppedanimruntime.cpp", "source": "indra/newview/alsteppedanimruntime.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 423, "text": "altoolcrowdplace.cpp", "source": "indra/newview/altoolcrowdplace.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1437, "text": "altoolcrowdplace.h", "source": "indra/newview/altoolcrowdplace.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 424, "text": "altooldirectorpick.cpp # [CloneMenu] Actor bar Select pick mode", "source": "indra/newview/altooldirectorpick.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1438, "text": "altooldirectorpick.h", "source": "indra/newview/altooldirectorpick.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 425, "text": "altoolfocuspick.cpp", "source": "indra/newview/altoolfocuspick.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 426, "text": "altoolghostedit.cpp", "source": "indra/newview/altoolghostedit.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 427, "text": "altoolghostplace.cpp", "source": "indra/newview/altoolghostplace.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1441, "text": "altoolghostplace.h", "source": "indra/newview/altoolghostplace.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 923, "text": "alvcammediafileio.cpp", "source": "indra/newview/alvcammediafileio.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 2371, "text": "app_settings/settings.xml", "source": "indra/newview/app_settings/settings.xml"}
- {"file": "indra/newview/CMakeLists.txt", "line": 2372, "text": "app_settings/settings_alchemy.xml", "source": "indra/newview/app_settings/settings_alchemy.xml"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1284, "text": "fsposeranimator.h", "source": "indra/newview/fsposeranimator.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 354, "text": "llactormover.cpp", "source": "indra/newview/llactormover.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1444, "text": "lldirectorcast.h", "source": "indra/newview/lldirectorcast.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 689, "text": "llghostavatar.cpp", "source": "indra/newview/llghostavatar.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1739, "text": "llghostavatar.h", "source": "indra/newview/llghostavatar.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1972, "text": "llreshadebridgeabi.h", "source": "indra/newview/llreshadebridgeabi.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 951, "text": "llselectmgr.cpp", "source": "indra/newview/llselectmgr.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1047, "text": "llviewerdisplay.cpp", "source": "indra/newview/llviewerdisplay.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1071, "text": "llviewerobjectlist.cpp", "source": "indra/newview/llviewerobjectlist.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1081, "text": "llviewershadermgr.cpp", "source": "indra/newview/llviewershadermgr.cpp"}
- {"file": "indra/llwindow/CMakeLists.txt", "line": 107, "text": "# llviewerwindow.cpp's (guarded) include still resolves.", "source": "indra/newview/llviewerwindow.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1091, "text": "llviewerwindow.cpp", "source": "indra/newview/llviewerwindow.cpp"}

## kit wiring pages

- `dance_tab`
- `ghosts_tab`

## checks

None identified by local evidence; not a claim that none is needed.

Full include/call edges: ../features.json. Shared hooks: ../hooks.md. Setting values and save coverage: ../settings.csv.

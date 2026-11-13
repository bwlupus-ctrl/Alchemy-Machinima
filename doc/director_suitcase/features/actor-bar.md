# actor-bar: dependency evidence

Landing layer 3. Change actor once; all single-actor pages follow; delete/rekey/load a clone without stale selection.

Candidate associations require review. Includes/calls are discovery edges, not a complete call graph or proof of build closure.

## owned

- `indra/newview/alactorchromakey.cpp`
- `indra/newview/alavataranimspeed.cpp`
- `indra/newview/alavatarkey.cpp`
- `indra/newview/alavatarkey.h`
- `indra/newview/alavatarlightresponse.cpp`
- `indra/newview/alavatarstaging.cpp`
- `indra/newview/alavatarstaging.h`
- `indra/newview/alavatarstagingmodel.cpp`
- `indra/newview/alavatarstagingmodel.h`
- `indra/newview/alavatarsubjectpicker.cpp`
- `indra/newview/alavatarsubjectpicker.h`
- `indra/newview/alcinelightrig.cpp`
- `indra/newview/alcinelightrigmanager.cpp`
- `indra/newview/alclientheight.cpp`
- `indra/newview/alclientheight.h`
- `indra/newview/aldanceball.cpp`
- `indra/newview/aldanceball.h`
- `indra/newview/aldirectoractorchoice.cpp`
- `indra/newview/aldirectoractorchoice.h`
- `indra/newview/aldirectoralloff.cpp`
- `indra/newview/aldirectoranimswitcher.cpp`
- `indra/newview/aldirectorcastui.cpp`
- `indra/newview/aldirectorcastui.h`
- `indra/newview/aldirectorhotkeys.cpp`
- `indra/newview/aldirectorselect.cpp`
- `indra/newview/aldirectorselect.h`
- `indra/newview/aldirectorswitcher.cpp`
- `indra/newview/aldirectortagtargets.cpp`
- `indra/newview/aldirectortagtargets.h`
- `indra/newview/alfloatergazecues.cpp`
- `indra/newview/alfloatergazecues.h`
- `indra/newview/algazeactor.h`
- `indra/newview/algazehotslots.h`
- `indra/newview/alghostcloneactions.h`
- `indra/newview/alghostclonemenu.cpp`
- `indra/newview/alghostclonemenulogic.h`
- `indra/newview/alghostspawnengine.cpp`
- `indra/newview/alghoststudio.cpp`
- `indra/newview/alkitpage.cpp`
- `indra/newview/alkitrow.cpp`
- `indra/newview/alkitshell.cpp`
- `indra/newview/allenslooks.cpp`
- `indra/newview/alpanelactormover.cpp`
- `indra/newview/alpanelavatarlightresponse.cpp`
- `indra/newview/alpanelcinelightrig.cpp`
- `indra/newview/alpaneldanceball.cpp`
- `indra/newview/alpaneldirectorvcam.cpp`
- `indra/newview/alpanelgazecues.cpp`
- `indra/newview/alpanelgazecues.h`
- `indra/newview/alpanelghoststudio.cpp`
- `indra/newview/alpanellensgaze.cpp`
- `indra/newview/alpanellensgaze.h`
- `indra/newview/alpanelpatheditor.cpp`
- `indra/newview/alpanelpatheditor.h`
- `indra/newview/alpanelpracticals.cpp`
- `indra/newview/alpanelsteppedanim.cpp`
- `indra/newview/alprojectortracker.cpp`
- `indra/newview/alsteppedanimruntime.cpp`
- `indra/newview/alsteppedanimruntime.h`
- `indra/newview/altooldirectorpick.cpp`
- `indra/newview/altooldirectorpick.h`
- `indra/newview/alvcambonepov.cpp`
- `indra/newview/llactormover.cpp`
- `indra/newview/llactormover.h`
- `indra/newview/llcinematiccamera.cpp`
- `indra/newview/llclonefidelityaudit.cpp`
- `indra/newview/lldirectorcast.cpp`
- `indra/newview/lldirectorcast.h`
- `indra/newview/llfloateractormover.cpp`
- `indra/newview/llfloaterdirector.cpp`
- `indra/newview/llfloaterdirector.h`
- `indra/newview/llfloaterprismmanager.cpp`
- `indra/newview/llpathcamera.cpp`
- `indra/newview/llprismlens.cpp`
- `indra/newview/skins/default/xui/en/floater_director.xml`
- `indra/newview/skins/default/xui/en/menu_director_cast.xml`
- `indra/newview/skins/default/xui/en/panel_lens_gaze.xml`

## hooks

- {"path": "indra/newview/CMakeLists.txt", "hunks": ["indra/newview/CMakeLists.txt@186,55", "indra/newview/CMakeLists.txt@335,124", "indra/newview/CMakeLists.txt@1145,59", "indra/newview/CMakeLists.txt@1301,183", "indra/newview/CMakeLists.txt@2878,32", "indra/newview/CMakeLists.txt@2984,198"]}
- {"path": "indra/newview/llappviewer.cpp", "hunks": ["indra/newview/llappviewer.cpp@72,35", "indra/newview/llappviewer.cpp@2650,53", "indra/newview/llappviewer.cpp@5696,49", "indra/newview/llappviewer.cpp@6518,45"]}
- {"path": "indra/newview/llstartup.cpp", "hunks": ["indra/newview/llstartup.cpp@201,9", "indra/newview/llstartup.cpp@2647,19"]}
- {"path": "indra/newview/alviewermenu.cpp", "hunks": ["indra/newview/alviewermenu.cpp@18,37", "indra/newview/alviewermenu.cpp@606,774"]}
- {"path": "indra/newview/llcontrolavatar.cpp", "hunks": ["indra/newview/llcontrolavatar.cpp@25,12", "indra/newview/llcontrolavatar.cpp@412,32"]}
- {"path": "indra/newview/lldrawpool.cpp", "hunks": ["indra/newview/lldrawpool.cpp@71,82", "indra/newview/lldrawpool.cpp@499,1399"]}
- {"path": "indra/newview/lldrawpoolavatar.cpp", "hunks": ["indra/newview/lldrawpoolavatar.cpp@25,14", "indra/newview/lldrawpoolavatar.cpp@83,102"]}
- {"path": "indra/newview/llviewerinput.cpp", "hunks": ["indra/newview/llviewerinput.cpp@37,18", "indra/newview/llviewerinput.cpp@506,119"]}
- {"path": "indra/newview/llviewerjoystick.cpp", "hunks": ["indra/newview/llviewerjoystick.cpp@27,24", "indra/newview/llviewerjoystick.cpp@4332,243"]}
- {"path": "indra/newview/llviewertexture.cpp", "hunks": ["indra/newview/llviewertexture.cpp@63,11", "indra/newview/llviewertexture.cpp@493,629"]}
- {"path": "indra/newview/llvoavatar.cpp", "hunks": ["indra/newview/llvoavatar.cpp@50,24", "indra/newview/llvoavatar.cpp@4723,28", "indra/newview/llvoavatar.cpp@12960,163"]}
- {"path": "indra/newview/llvoavatar.h", "hunks": ["indra/newview/llvoavatar.h@552,45"]}
- {"path": "indra/newview/llvovolume.cpp", "hunks": ["indra/newview/llvovolume.cpp@83,8", "indra/newview/llvovolume.cpp@5690,87"]}
- {"path": "indra/newview/pipeline.cpp", "hunks": ["indra/newview/pipeline.cpp@110,8", "indra/newview/pipeline.cpp@1704,30224", "indra/newview/pipeline.cpp@32059,430"]}

## settings

None identified by local evidence; not a claim that none is needed.

## shaders

None identified by local evidence; not a claim that none is needed.

## xui

- `indra/newview/skins/default/xui/en/director_kit_index.xml`
- `indra/newview/skins/default/xui/en/floater_director.xml`
- `indra/newview/skins/default/xui/en/floater_director_v2.xml`
- `indra/newview/skins/default/xui/en/menu_director_cast.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_performance_gaze.xml`
- `indra/newview/skins/default/xui/en/panel_lens_gaze.xml`

## generated runtime

- `indra/newview/skins/default/xui/en/director_kit_index.xml`
- `indra/newview/skins/default/xui/en/floater_director_v2.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_performance_gaze.xml`

## cmake entries

- {"file": "indra/newview/CMakeLists.txt", "line": 340, "text": "alactorchromakey.cpp # [ActorKey] WP0/WP1", "source": "indra/newview/alactorchromakey.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 395, "text": "alavataranimspeed.cpp", "source": "indra/newview/alavataranimspeed.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 367, "text": "alavatarkey.cpp # [ClientHeight]", "source": "indra/newview/alavatarkey.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1326, "text": "alavatarkey.h # [ClientHeight]", "source": "indra/newview/alavatarkey.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 364, "text": "alavatarlightresponse.cpp # [AvatarLightResponse]", "source": "indra/newview/alavatarlightresponse.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 192, "text": "alavatarstaging.cpp", "source": "indra/newview/alavatarstaging.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1151, "text": "alavatarstaging.h", "source": "indra/newview/alavatarstaging.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 193, "text": "alavatarstagingmodel.cpp", "source": "indra/newview/alavatarstagingmodel.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 2893, "text": "alavatarstagingmodel.cpp", "source": "indra/newview/alavatarstagingmodel.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1152, "text": "alavatarstagingmodel.h", "source": "indra/newview/alavatarstagingmodel.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 401, "text": "alavatarsubjectpicker.cpp", "source": "indra/newview/alavatarsubjectpicker.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1392, "text": "alavatarsubjectpicker.h", "source": "indra/newview/alavatarsubjectpicker.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 349, "text": "alcinelightrig.cpp", "source": "indra/newview/alcinelightrig.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 350, "text": "alcinelightrigmanager.cpp", "source": "indra/newview/alcinelightrigmanager.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 368, "text": "alclientheight.cpp # [ClientHeight]", "source": "indra/newview/alclientheight.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1327, "text": "alclientheight.h # [ClientHeight]", "source": "indra/newview/alclientheight.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 190, "text": "aldanceball.cpp", "source": "indra/newview/aldanceball.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1149, "text": "aldanceball.h", "source": "indra/newview/aldanceball.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 371, "text": "aldirectoractorchoice.cpp", "source": "indra/newview/aldirectoractorchoice.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 2991, "text": "LL_ADD_INTEGRATION_TEST(lldirectorcast_choice \"aldirectoractorchoice.cpp\" \"${test_libs}\" \"${test_project}\")", "source": "indra/newview/aldirectoractorchoice.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1412, "text": "aldirectoractorchoice.h", "source": "indra/newview/aldirectoractorchoice.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 203, "text": "aldirectoralloff.cpp", "source": "indra/newview/aldirectoralloff.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 189, "text": "aldirectoranimswitcher.cpp", "source": "indra/newview/aldirectoranimswitcher.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 372, "text": "aldirectorcastui.cpp", "source": "indra/newview/aldirectorcastui.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1414, "text": "aldirectorcastui.h", "source": "indra/newview/aldirectorcastui.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 196, "text": "aldirectorhotkeys.cpp", "source": "indra/newview/aldirectorhotkeys.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 406, "text": "aldirectorselect.cpp # [CloneMenu] Select in Director helper", "source": "indra/newview/aldirectorselect.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1399, "text": "aldirectorselect.h", "source": "indra/newview/aldirectorselect.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 197, "text": "aldirectorswitcher.cpp", "source": "indra/newview/aldirectorswitcher.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 201, "text": "aldirectortagtargets.cpp", "source": "indra/newview/aldirectortagtargets.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1161, "text": "aldirectortagtargets.h", "source": "indra/newview/aldirectortagtargets.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 227, "text": "alfloaterblocked.cpp", "source": "indra/newview/alfloaterblocked.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 420, "text": "alfloatergazecues.cpp", "source": "indra/newview/alfloatergazecues.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1418, "text": "alfloatergazecues.h", "source": "indra/newview/alfloatergazecues.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1466, "text": "algazehotslots.h", "source": "indra/newview/algazehotslots.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1396, "text": "alghostcloneactions.h", "source": "indra/newview/alghostcloneactions.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 405, "text": "alghostclonemenu.cpp # [CloneMenu] clone right-click menu", "source": "indra/newview/alghostclonemenu.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1398, "text": "alghostclonemenulogic.h", "source": "indra/newview/alghostclonemenulogic.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 345, "text": "alghostspawnengine.cpp", "source": "indra/newview/alghostspawnengine.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 396, "text": "alghoststudio.cpp", "source": "indra/newview/alghoststudio.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 213, "text": "alkitpage.cpp", "source": "indra/newview/alkitpage.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 216, "text": "alkitrow.cpp", "source": "indra/newview/alkitrow.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 218, "text": "alkitshell.cpp", "source": "indra/newview/alkitshell.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 353, "text": "allenslooks.cpp", "source": "indra/newview/allenslooks.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 425, "text": "alpanelactormover.cpp", "source": "indra/newview/alpanelactormover.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 429, "text": "alpanelavatarlightresponse.cpp", "source": "indra/newview/alpanelavatarlightresponse.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 439, "text": "alpanelcinelightrig.cpp", "source": "indra/newview/alpanelcinelightrig.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 446, "text": "alpaneldanceball.cpp", "source": "indra/newview/alpaneldanceball.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 221, "text": "alpaneldirectorvcam.cpp", "source": "indra/newview/alpaneldirectorvcam.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 434, "text": "alpanelgazecues.cpp", "source": "indra/newview/alpanelgazecues.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1434, "text": "alpanelgazecues.h", "source": "indra/newview/alpanelgazecues.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 407, "text": "alpanelghoststudio.cpp", "source": "indra/newview/alpanelghoststudio.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 445, "text": "alpanellensgaze.cpp", "source": "indra/newview/alpanellensgaze.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1470, "text": "alpanellensgaze.h", "source": "indra/newview/alpanellensgaze.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 452, "text": "alpanelpatheditor.cpp", "source": "indra/newview/alpanelpatheditor.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1477, "text": "alpanelpatheditor.h", "source": "indra/newview/alpanelpatheditor.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 437, "text": "alpanelpracticals.cpp", "source": "indra/newview/alpanelpracticals.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 426, "text": "alpanelsteppedanim.cpp", "source": "indra/newview/alpanelsteppedanim.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 402, "text": "alprojectortracker.cpp", "source": "indra/newview/alprojectortracker.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 250, "text": "alsteppedanimruntime.cpp", "source": "indra/newview/alsteppedanimruntime.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1213, "text": "alsteppedanimruntime.h", "source": "indra/newview/alsteppedanimruntime.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 410, "text": "altooldirectorpick.cpp # [CloneMenu] Actor bar Select pick mode", "source": "indra/newview/altooldirectorpick.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1405, "text": "altooldirectorpick.h", "source": "indra/newview/altooldirectorpick.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 376, "text": "alvcambonepov.cpp", "source": "indra/newview/alvcambonepov.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 280, "text": "alviewermenu.cpp", "source": "indra/newview/alviewermenu.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 285, "text": "fsmaniprotatejoint.cpp", "source": "indra/newview/fsmaniprotatejoint.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 286, "text": "fsmaniptranslatejoint.cpp", "source": "indra/newview/fsmaniptranslatejoint.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 342, "text": "llactormover.cpp", "source": "indra/newview/llactormover.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1311, "text": "llactormover.h", "source": "indra/newview/llactormover.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 303, "text": "llaisapi.cpp", "source": "indra/newview/llaisapi.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 320, "text": "llappviewer.cpp", "source": "indra/newview/llappviewer.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 415, "text": "llcinematiccamera.cpp", "source": "indra/newview/llcinematiccamera.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 346, "text": "llclonefidelityaudit.cpp", "source": "indra/newview/llclonefidelityaudit.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 474, "text": "llcontrolavatar.cpp", "source": "indra/newview/llcontrolavatar.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 416, "text": "lldirectorcast.cpp", "source": "indra/newview/lldirectorcast.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1411, "text": "lldirectorcast.h", "source": "indra/newview/lldirectorcast.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 491, "text": "lldrawpool.cpp", "source": "indra/newview/lldrawpool.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 493, "text": "lldrawpoolavatar.cpp", "source": "indra/newview/lldrawpoolavatar.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 417, "text": "llfloateractormover.cpp", "source": "indra/newview/llfloateractormover.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 418, "text": "llfloaterdirector.cpp", "source": "indra/newview/llfloaterdirector.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1416, "text": "llfloaterdirector.h", "source": "indra/newview/llfloaterdirector.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 590, "text": "llfloaterimcontainer.cpp", "source": "indra/newview/llfloaterimcontainer.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 628, "text": "llfloaterpreferencesgraphicsadvanced.cpp", "source": "indra/newview/llfloaterpreferencesgraphicsadvanced.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 421, "text": "llfloaterprismmanager.cpp", "source": "indra/newview/llfloaterprismmanager.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 691, "text": "llhudeffectlookat.cpp", "source": "indra/newview/llhudeffectlookat.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 693, "text": "llhudeffecttrail.cpp", "source": "indra/newview/llhudeffecttrail.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 455, "text": "llpathcamera.cpp", "source": "indra/newview/llpathcamera.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 908, "text": "llprismlens.cpp", "source": "indra/newview/llprismlens.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 957, "text": "llstartup.cpp", "source": "indra/newview/llstartup.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1047, "text": "llviewerinput.cpp", "source": "indra/newview/llviewerinput.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1043, "text": "llviewerjoint.cpp", "source": "indra/newview/llviewerjoint.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1046, "text": "llviewerjoystick.cpp", "source": "indra/newview/llviewerjoystick.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1072, "text": "llviewertexture.cpp", "source": "indra/newview/llviewertexture.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1082, "text": "llvoavatar.cpp", "source": "indra/newview/llvoavatar.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 2106, "text": "llvoavatar.h", "source": "indra/newview/llvoavatar.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 2107, "text": "llvoavatarself.h", "source": "indra/newview/llvoavatarself.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1096, "text": "llvovolume.cpp", "source": "indra/newview/llvovolume.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1116, "text": "pipeline.cpp", "source": "indra/newview/pipeline.cpp"}

## kit wiring pages

None identified by local evidence; not a claim that none is needed.

## checks

- `scripts/code_tools/director_actor_bar_check.py`

Full include/call edges: ../features.json. Shared hooks: ../hooks.md. Setting values and save coverage: ../settings.csv.

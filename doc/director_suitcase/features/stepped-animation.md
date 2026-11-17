# stepped-animation: dependency evidence

Landing layer 3. Real/clone scope, independent animation speed, step cadence and exclusion of camera-mounted actors.

Candidate associations require review. Includes/calls are discovery edges, not a complete call graph or proof of build closure.

## owned

- `indra/llcharacter/alloopseam.h`
- `indra/llplugin/llpluginloopseam.h`
- `indra/newview/alavataranimspeed.cpp`
- `indra/newview/alavataranimspeed.h`
- `indra/newview/alcinelightrig.cpp`
- `indra/newview/alcinelightrigmanager.cpp`
- `indra/newview/aldirectoralloff.h`
- `indra/newview/alghostanimmirrormath.h`
- `indra/newview/alghostcloneactions.cpp`
- `indra/newview/alghostcloneactions.h`
- `indra/newview/alghostclonemenu.cpp`
- `indra/newview/alghostclonemenulogic.h`
- `indra/newview/alghoststudio.cpp`
- `indra/newview/alghoststudio.h`
- `indra/newview/alkitpresets.cpp`
- `indra/newview/alpanelsteppedanim.cpp`
- `indra/newview/alpanelsteppedanim.h`
- `indra/newview/alreflectanim.cpp`
- `indra/newview/alreflectanim.h`
- `indra/newview/alreflectmath.h`
- `indra/newview/alsteppedanim.cpp`
- `indra/newview/alsteppedanim.h`
- `indra/newview/alsteppedanimruntime.cpp`
- `indra/newview/alsteppedanimruntime.h`
- `indra/newview/llactormover.cpp`
- `indra/newview/lldirectorcast.cpp`
- `indra/newview/lldirectorcast.h`
- `indra/newview/llfloaterdirector.cpp`
- `indra/newview/llghostavatar.cpp`
- `indra/newview/llghostavatar.h`
- `indra/newview/skins/default/xui/en/panel_avatar_anim_speed.xml`
- `indra/newview/skins/default/xui/en/panel_pose_polish.xml`

## hooks

- {"path": "indra/newview/CMakeLists.txt", "hunks": ["indra/newview/CMakeLists.txt@255,11", "indra/newview/CMakeLists.txt@347,126", "indra/newview/CMakeLists.txt@1220,30", "indra/newview/CMakeLists.txt@1331,186", "indra/newview/CMakeLists.txt@3018,198"]}
- {"path": "indra/newview/app_settings/settings.xml", "hunks": ["indra/newview/app_settings/settings.xml@2,3504", "indra/newview/app_settings/settings.xml@6624,14888"]}
- {"path": "indra/newview/llappviewer.cpp", "hunks": ["indra/newview/llappviewer.cpp@73,36", "indra/newview/llappviewer.cpp@918,26", "indra/newview/llappviewer.cpp@1731,12", "indra/newview/llappviewer.cpp@5383,24", "indra/newview/llappviewer.cpp@6067,30"]}
- {"path": "indra/newview/alviewermenu.cpp", "hunks": ["indra/newview/alviewermenu.cpp@18,37", "indra/newview/alviewermenu.cpp@606,774", "indra/newview/alviewermenu.cpp@1417,79"]}
- {"path": "indra/newview/llcontrolavatar.cpp", "hunks": ["indra/newview/llcontrolavatar.cpp@25,12", "indra/newview/llcontrolavatar.cpp@361,7"]}
- {"path": "indra/newview/llvoavatar.cpp", "hunks": ["indra/newview/llvoavatar.cpp@24,24", "indra/newview/llvoavatar.cpp@1353,7", "indra/newview/llvoavatar.cpp@3384,12", "indra/newview/llvoavatar.cpp@5277,596", "indra/newview/llvoavatar.cpp@7425,14", "indra/newview/llvoavatar.cpp@7453,9", "indra/newview/llvoavatar.cpp@9080,7", "indra/newview/llvoavatar.cpp@11732,27", "indra/newview/llvoavatar.cpp@11791,282"]}
- {"path": "indra/newview/llvoavatar.h", "hunks": ["indra/newview/llvoavatar.h@213,33", "indra/newview/llvoavatar.h@1004,21"]}
- {"path": "indra/newview/llvovolume.cpp", "hunks": ["indra/newview/llvovolume.cpp@27,11", "indra/newview/llvovolume.cpp@630,26"]}
- {"path": "indra/llcharacter/CMakeLists.txt", "hunks": ["indra/llcharacter/CMakeLists.txt@30,9"]}
- {"path": "indra/llcharacter/llkeyframemotion.cpp", "hunks": ["indra/llcharacter/llkeyframemotion.cpp@50,38", "indra/llcharacter/llkeyframemotion.cpp@736,33", "indra/llcharacter/llkeyframemotion.cpp@772,20", "indra/llcharacter/llkeyframemotion.cpp@820,36", "indra/llcharacter/llkeyframemotion.cpp@886,468", "indra/llcharacter/llkeyframemotion.cpp@2964,17", "indra/llcharacter/llkeyframemotion.cpp@3006,13", "indra/llcharacter/llkeyframemotion.cpp@3043,13"]}
- {"path": "indra/llcharacter/llkeyframemotion.h", "hunks": ["indra/llcharacter/llkeyframemotion.h@32,10", "indra/llcharacter/llkeyframemotion.h@226,21", "indra/llcharacter/llkeyframemotion.h@318,49", "indra/llcharacter/llkeyframemotion.h@530,40"]}
- {"path": "indra/llplugin/llpluginclassmedia.cpp", "hunks": ["indra/llplugin/llpluginclassmedia.cpp@198,7", "indra/llplugin/llpluginclassmedia.cpp@832,65", "indra/llplugin/llpluginclassmedia.cpp@1196,76"]}
- {"path": "indra/llplugin/llpluginclassmedia.h", "hunks": ["indra/llplugin/llpluginclassmedia.h@30,10", "indra/llplugin/llpluginclassmedia.h@170,24"]}
- {"path": "indra/newview/llagent.cpp", "hunks": ["indra/newview/llagent.cpp@26,12", "indra/newview/llagent.cpp@293,7", "indra/newview/llagent.cpp@2770,16", "indra/newview/llagent.cpp@3413,11", "indra/newview/llagent.cpp@3437,11"]}
- {"path": "indra/newview/llflexibleobject.cpp", "hunks": ["indra/newview/llflexibleobject.cpp@25,7", "indra/newview/llflexibleobject.cpp@768,11"]}
- {"path": "indra/newview/llpreviewanim.cpp", "hunks": ["indra/newview/llpreviewanim.cpp@27,11", "indra/newview/llpreviewanim.cpp@73,18", "indra/newview/llpreviewanim.cpp@128,45", "indra/newview/llpreviewanim.cpp@180,14", "indra/newview/llpreviewanim.cpp@248,16"]}
- {"path": "indra/newview/llviewerobject.cpp", "hunks": ["indra/newview/llviewerobject.cpp@25,13", "indra/newview/llviewerobject.cpp@965,35", "indra/newview/llviewerobject.cpp@2590,23"]}
- {"path": "indra/newview/llviewerpartsim.cpp", "hunks": ["indra/newview/llviewerpartsim.cpp@25,11", "indra/newview/llviewerpartsim.cpp@280,54", "indra/newview/llviewerpartsim.cpp@378,173", "indra/newview/llviewerpartsim.cpp@574,11", "indra/newview/llviewerpartsim.cpp@674,7", "indra/newview/llviewerpartsim.cpp@702,7", "indra/newview/llviewerpartsim.cpp@839,40", "indra/newview/llviewerpartsim.cpp@901,71", "indra/newview/llviewerpartsim.cpp@999,37", "indra/newview/llviewerpartsim.cpp@1079,18", "indra/newview/llviewerpartsim.cpp@1117,60"]}
- {"path": "indra/newview/llviewerpartsim.h", "hunks": ["indra/newview/llviewerpartsim.h@27,7", "indra/newview/llviewerpartsim.h@61,7", "indra/newview/llviewerpartsim.h@157,11", "indra/newview/llviewerpartsim.h@207,12"]}
- {"path": "indra/newview/llviewerpartsource.h", "hunks": ["indra/newview/llviewerpartsource.h@27,7", "indra/newview/llviewerpartsource.h@79,13"]}

## settings

- `ALPolishLoopSeamEnabled`
- `StepAnimAdaptHysteresis`
- `StepAnimAdaptMinDwell`
- `StepAnimAdaptThreshold`
- `StepAnimAttachFX`
- `StepAnimBackgroundRate`
- `StepAnimBaseFPS`
- `StepAnimBoil`
- `StepAnimBoilAmount`
- `StepAnimBoilTicks`
- `StepAnimClock`
- `StepAnimDebugVerify`
- `StepAnimEase`
- `StepAnimEaseAmount`
- `StepAnimEnabled`
- `StepAnimExcludeCameraMounted`
- `StepAnimFace`
- `StepAnimFastRate`
- `StepAnimFlexi`
- `StepAnimGaze`
- `StepAnimGroupRates`
- `StepAnimImpactHold`
- `StepAnimImpactThreshold`
- `StepAnimImpactTicks`
- `StepAnimIncludeGhosts`
- `StepAnimIncludeSelf`
- `StepAnimLimitDistance`
- `StepAnimMatchRotoInk`
- `StepAnimMaxAvatars`
- `StepAnimMode`
- `StepAnimNearbyRadius`
- `StepAnimOffsetMode`
- `StepAnimPartArms`
- `StepAnimPartAttach`
- `StepAnimPartExtras`
- `StepAnimPartFace`
- `StepAnimPartHands`
- `StepAnimPartHead`
- `StepAnimPartLegs`
- `StepAnimParticles`
- `StepAnimParticlesMigrated`
- `StepAnimPhase`
- `StepAnimPhaseSpread`
- `StepAnimRate`
- `StepAnimRateVariation`
- `StepAnimScope`
- `StepAnimSeed`
- `StepAnimSmear`
- `StepAnimSmearStrength`
- `StepAnimSmearThreshold`
- `StepAnimTextureAnim`
- `StepAnimTimingJitter`
- `StepAnimWhatSteps`

## shaders

None identified by local evidence; not a claim that none is needed.

## xui

- `indra/newview/skins/default/xui/en/director_kit_index.xml`
- `indra/newview/skins/default/xui/en/floater_director_v2.xml`
- `indra/newview/skins/default/xui/en/panel_avatar_anim_speed.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_performance_polish.xml`
- `indra/newview/skins/default/xui/en/panel_pose_polish.xml`

## generated runtime

- `indra/newview/skins/default/xui/en/director_kit_index.xml`
- `indra/newview/skins/default/xui/en/floater_director_v2.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_performance_polish.xml`

## cmake entries

- {"file": "indra/llcharacter/CMakeLists.txt", "line": 35, "text": "alloopseam.h", "source": "indra/llcharacter/alloopseam.h"}
- {"file": "indra/llcharacter/CMakeLists.txt", "line": 17, "text": "llkeyframemotion.cpp", "source": "indra/llcharacter/llkeyframemotion.cpp"}
- {"file": "indra/llcharacter/CMakeLists.txt", "line": 48, "text": "llkeyframemotion.h", "source": "indra/llcharacter/llkeyframemotion.h"}
- {"file": "indra/llcharacter/CMakeLists.txt", "line": 20, "text": "llmotioncontroller.cpp", "source": "indra/llcharacter/llmotioncontroller.cpp"}
- {"file": "indra/llplugin/CMakeLists.txt", "line": 7, "text": "llpluginclassmedia.cpp", "source": "indra/llplugin/llpluginclassmedia.cpp"}
- {"file": "indra/llplugin/CMakeLists.txt", "line": 18, "text": "llpluginclassmedia.h", "source": "indra/llplugin/llpluginclassmedia.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 409, "text": "alavataranimspeed.cpp", "source": "indra/newview/alavataranimspeed.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1415, "text": "alavataranimspeed.h", "source": "indra/newview/alavataranimspeed.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 361, "text": "alcinelightrig.cpp", "source": "indra/newview/alcinelightrig.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 362, "text": "alcinelightrigmanager.cpp", "source": "indra/newview/alcinelightrigmanager.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1177, "text": "aldirectoralloff.h", "source": "indra/newview/aldirectoralloff.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 239, "text": "alfloaterblocked.cpp", "source": "indra/newview/alfloaterblocked.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1412, "text": "alghostanimmirrormath.h", "source": "indra/newview/alghostanimmirrormath.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 418, "text": "alghostcloneactions.cpp # [CloneMenu] Clones page row actions shared with the menus", "source": "indra/newview/alghostcloneactions.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1429, "text": "alghostcloneactions.h", "source": "indra/newview/alghostcloneactions.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 419, "text": "alghostclonemenu.cpp # [CloneMenu] clone right-click menu", "source": "indra/newview/alghostclonemenu.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1431, "text": "alghostclonemenulogic.h", "source": "indra/newview/alghostclonemenulogic.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 410, "text": "alghoststudio.cpp", "source": "indra/newview/alghoststudio.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1416, "text": "alghoststudio.h", "source": "indra/newview/alghoststudio.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 227, "text": "alkitpresets.cpp", "source": "indra/newview/alkitpresets.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 440, "text": "alpanelsteppedanim.cpp", "source": "indra/newview/alpanelsteppedanim.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1228, "text": "alpanelsteppedanim.h", "source": "indra/newview/alpanelsteppedanim.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 408, "text": "alreflectanim.cpp", "source": "indra/newview/alreflectanim.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1414, "text": "alreflectanim.h", "source": "indra/newview/alreflectanim.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1413, "text": "alreflectmath.h", "source": "indra/newview/alreflectmath.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 261, "text": "alsteppedanim.cpp", "source": "indra/newview/alsteppedanim.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 3024, "text": "LL_ADD_INTEGRATION_TEST(alsteppedanim \"alsteppedanim.cpp\" \"${test_libs}\" \"${test_project}\")", "source": "indra/newview/alsteppedanim.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1226, "text": "alsteppedanim.h", "source": "indra/newview/alsteppedanim.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 262, "text": "alsteppedanimruntime.cpp", "source": "indra/newview/alsteppedanimruntime.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1227, "text": "alsteppedanimruntime.h", "source": "indra/newview/alsteppedanimruntime.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 292, "text": "alviewermenu.cpp", "source": "indra/newview/alviewermenu.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 2371, "text": "app_settings/settings.xml", "source": "indra/newview/app_settings/settings.xml"}
- {"file": "indra/newview/CMakeLists.txt", "line": 297, "text": "fsmaniprotatejoint.cpp", "source": "indra/newview/fsmaniprotatejoint.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 298, "text": "fsmaniptranslatejoint.cpp", "source": "indra/newview/fsmaniptranslatejoint.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1286, "text": "fsposingmotion.h", "source": "indra/newview/fsposingmotion.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 354, "text": "llactormover.cpp", "source": "indra/newview/llactormover.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 316, "text": "llagent.cpp", "source": "indra/newview/llagent.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 315, "text": "llaisapi.cpp", "source": "indra/newview/llaisapi.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 331, "text": "llappearancemgr.cpp", "source": "indra/newview/llappearancemgr.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 332, "text": "llappviewer.cpp", "source": "indra/newview/llappviewer.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 488, "text": "llcontrolavatar.cpp", "source": "indra/newview/llcontrolavatar.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 430, "text": "lldirectorcast.cpp", "source": "indra/newview/lldirectorcast.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1444, "text": "lldirectorcast.h", "source": "indra/newview/lldirectorcast.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 537, "text": "llflexibleobject.cpp", "source": "indra/newview/llflexibleobject.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 432, "text": "llfloaterdirector.cpp", "source": "indra/newview/llfloaterdirector.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 604, "text": "llfloaterimcontainer.cpp", "source": "indra/newview/llfloaterimcontainer.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 642, "text": "llfloaterpreferencesgraphicsadvanced.cpp", "source": "indra/newview/llfloaterpreferencesgraphicsadvanced.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 689, "text": "llghostavatar.cpp", "source": "indra/newview/llghostavatar.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1739, "text": "llghostavatar.h", "source": "indra/newview/llghostavatar.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 705, "text": "llhudeffectlookat.cpp", "source": "indra/newview/llhudeffectlookat.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 707, "text": "llhudeffecttrail.cpp", "source": "indra/newview/llhudeffecttrail.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 768, "text": "llmediactrl.cpp", "source": "indra/newview/llmediactrl.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 915, "text": "llpreviewanim.cpp", "source": "indra/newview/llpreviewanim.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 937, "text": "llscenemonitor.cpp", "source": "indra/newview/llscenemonitor.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1057, "text": "llviewerjoint.cpp", "source": "indra/newview/llviewerjoint.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1070, "text": "llviewerobject.cpp", "source": "indra/newview/llviewerobject.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1078, "text": "llviewerpartsim.cpp", "source": "indra/newview/llviewerpartsim.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 2120, "text": "llviewerpartsim.h", "source": "indra/newview/llviewerpartsim.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 2121, "text": "llviewerpartsource.h", "source": "indra/newview/llviewerpartsource.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1096, "text": "llvoavatar.cpp", "source": "indra/newview/llvoavatar.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 2139, "text": "llvoavatar.h", "source": "indra/newview/llvoavatar.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 2140, "text": "llvoavatarself.h", "source": "indra/newview/llvoavatarself.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1106, "text": "llvopartgroup.cpp", "source": "indra/newview/llvopartgroup.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1110, "text": "llvovolume.cpp", "source": "indra/newview/llvovolume.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 2162, "text": "llworld.h", "source": "indra/newview/llworld.h"}

## kit wiring pages

- `move_tab`
- `polish_tab`
- `stepped_tab`

## checks

- `scripts/code_tools/test_avatar_anim_speed.py`

Full include/call edges: ../features.json. Shared hooks: ../hooks.md. Setting values and save coverage: ../settings.csv.

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

- {"path": "indra/newview/CMakeLists.txt", "hunks": ["indra/newview/CMakeLists.txt@242,11", "indra/newview/CMakeLists.txt@334,119", "indra/newview/CMakeLists.txt@1199,14", "indra/newview/CMakeLists.txt@1294,171", "indra/newview/CMakeLists.txt@2965,197"]}
- {"path": "indra/newview/app_settings/settings.xml", "hunks": ["indra/newview/app_settings/settings.xml@2,3455", "indra/newview/app_settings/settings.xml@6575,14788"]}
- {"path": "indra/newview/llappviewer.cpp", "hunks": ["indra/newview/llappviewer.cpp@72,35", "indra/newview/llappviewer.cpp@915,26", "indra/newview/llappviewer.cpp@1728,12", "indra/newview/llappviewer.cpp@5349,24", "indra/newview/llappviewer.cpp@5993,30"]}
- {"path": "indra/newview/alviewermenu.cpp", "hunks": ["indra/newview/alviewermenu.cpp@18,36", "indra/newview/alviewermenu.cpp@604,830", "indra/newview/alviewermenu.cpp@1471,83"]}
- {"path": "indra/newview/llcontrolavatar.cpp", "hunks": ["indra/newview/llcontrolavatar.cpp@25,12", "indra/newview/llcontrolavatar.cpp@361,7"]}
- {"path": "indra/newview/llvoavatar.cpp", "hunks": ["indra/newview/llvoavatar.cpp@24,23", "indra/newview/llvoavatar.cpp@1349,7", "indra/newview/llvoavatar.cpp@3380,12", "indra/newview/llvoavatar.cpp@5227,596", "indra/newview/llvoavatar.cpp@7372,14", "indra/newview/llvoavatar.cpp@7400,9", "indra/newview/llvoavatar.cpp@9022,7", "indra/newview/llvoavatar.cpp@11608,27", "indra/newview/llvoavatar.cpp@11667,282"]}
- {"path": "indra/newview/llvoavatar.h", "hunks": ["indra/newview/llvoavatar.h@213,33", "indra/newview/llvoavatar.h@978,21"]}
- {"path": "indra/newview/llvovolume.cpp", "hunks": ["indra/newview/llvovolume.cpp@27,11", "indra/newview/llvovolume.cpp@622,26"]}
- {"path": "indra/llcharacter/CMakeLists.txt", "hunks": ["indra/llcharacter/CMakeLists.txt@30,9"]}
- {"path": "indra/llcharacter/llkeyframemotion.cpp", "hunks": ["indra/llcharacter/llkeyframemotion.cpp@50,38", "indra/llcharacter/llkeyframemotion.cpp@736,33", "indra/llcharacter/llkeyframemotion.cpp@772,20", "indra/llcharacter/llkeyframemotion.cpp@820,36", "indra/llcharacter/llkeyframemotion.cpp@886,468", "indra/llcharacter/llkeyframemotion.cpp@2964,17", "indra/llcharacter/llkeyframemotion.cpp@3006,13", "indra/llcharacter/llkeyframemotion.cpp@3043,13"]}
- {"path": "indra/llcharacter/llkeyframemotion.h", "hunks": ["indra/llcharacter/llkeyframemotion.h@32,10", "indra/llcharacter/llkeyframemotion.h@226,21", "indra/llcharacter/llkeyframemotion.h@318,49", "indra/llcharacter/llkeyframemotion.h@530,40"]}
- {"path": "indra/llplugin/llpluginclassmedia.cpp", "hunks": ["indra/llplugin/llpluginclassmedia.cpp@198,7", "indra/llplugin/llpluginclassmedia.cpp@832,65", "indra/llplugin/llpluginclassmedia.cpp@1196,76"]}
- {"path": "indra/llplugin/llpluginclassmedia.h", "hunks": ["indra/llplugin/llpluginclassmedia.h@30,10", "indra/llplugin/llpluginclassmedia.h@170,24"]}
- {"path": "indra/newview/llagent.cpp", "hunks": ["indra/newview/llagent.cpp@26,12", "indra/newview/llagent.cpp@293,7", "indra/newview/llagent.cpp@2770,16", "indra/newview/llagent.cpp@3413,11", "indra/newview/llagent.cpp@3437,11"]}
- {"path": "indra/newview/llflexibleobject.cpp", "hunks": ["indra/newview/llflexibleobject.cpp@25,7", "indra/newview/llflexibleobject.cpp@768,11"]}
- {"path": "indra/newview/llpreviewanim.cpp", "hunks": ["indra/newview/llpreviewanim.cpp@27,11", "indra/newview/llpreviewanim.cpp@73,18", "indra/newview/llpreviewanim.cpp@128,45", "indra/newview/llpreviewanim.cpp@180,14", "indra/newview/llpreviewanim.cpp@248,16"]}
- {"path": "indra/newview/llviewerobject.cpp", "hunks": ["indra/newview/llviewerobject.cpp@25,13", "indra/newview/llviewerobject.cpp@965,35", "indra/newview/llviewerobject.cpp@2590,23"]}
- {"path": "indra/newview/llviewerpartsim.cpp", "hunks": ["indra/newview/llviewerpartsim.cpp@25,9", "indra/newview/llviewerpartsim.cpp@278,54", "indra/newview/llviewerpartsim.cpp@376,173", "indra/newview/llviewerpartsim.cpp@572,11", "indra/newview/llviewerpartsim.cpp@672,7", "indra/newview/llviewerpartsim.cpp@700,7", "indra/newview/llviewerpartsim.cpp@841,35", "indra/newview/llviewerpartsim.cpp@898,71", "indra/newview/llviewerpartsim.cpp@996,37", "indra/newview/llviewerpartsim.cpp@1076,18", "indra/newview/llviewerpartsim.cpp@1114,60"]}
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
- {"file": "indra/newview/CMakeLists.txt", "line": 394, "text": "alavataranimspeed.cpp", "source": "indra/newview/alavataranimspeed.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1375, "text": "alavataranimspeed.h", "source": "indra/newview/alavataranimspeed.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 348, "text": "alcinelightrig.cpp", "source": "indra/newview/alcinelightrig.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 349, "text": "alcinelightrigmanager.cpp", "source": "indra/newview/alcinelightrigmanager.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1160, "text": "aldirectoralloff.h", "source": "indra/newview/aldirectoralloff.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 226, "text": "alfloaterblocked.cpp", "source": "indra/newview/alfloaterblocked.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1372, "text": "alghostanimmirrormath.h", "source": "indra/newview/alghostanimmirrormath.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 395, "text": "alghoststudio.cpp", "source": "indra/newview/alghoststudio.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1376, "text": "alghoststudio.h", "source": "indra/newview/alghoststudio.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 214, "text": "alkitpresets.cpp", "source": "indra/newview/alkitpresets.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 425, "text": "alpanelsteppedanim.cpp", "source": "indra/newview/alpanelsteppedanim.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1211, "text": "alpanelsteppedanim.h", "source": "indra/newview/alpanelsteppedanim.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 393, "text": "alreflectanim.cpp", "source": "indra/newview/alreflectanim.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1374, "text": "alreflectanim.h", "source": "indra/newview/alreflectanim.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1373, "text": "alreflectmath.h", "source": "indra/newview/alreflectmath.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 248, "text": "alsteppedanim.cpp", "source": "indra/newview/alsteppedanim.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 2980, "text": "LL_ADD_INTEGRATION_TEST(alsteppedanim \"alsteppedanim.cpp\" \"${test_libs}\" \"${test_project}\")", "source": "indra/newview/alsteppedanim.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1209, "text": "alsteppedanim.h", "source": "indra/newview/alsteppedanim.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 249, "text": "alsteppedanimruntime.cpp", "source": "indra/newview/alsteppedanimruntime.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1210, "text": "alsteppedanimruntime.h", "source": "indra/newview/alsteppedanimruntime.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 279, "text": "alviewermenu.cpp", "source": "indra/newview/alviewermenu.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 2328, "text": "app_settings/settings.xml", "source": "indra/newview/app_settings/settings.xml"}
- {"file": "indra/newview/CMakeLists.txt", "line": 284, "text": "fsmaniprotatejoint.cpp", "source": "indra/newview/fsmaniprotatejoint.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 285, "text": "fsmaniptranslatejoint.cpp", "source": "indra/newview/fsmaniptranslatejoint.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 341, "text": "llactormover.cpp", "source": "indra/newview/llactormover.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 303, "text": "llagent.cpp", "source": "indra/newview/llagent.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 302, "text": "llaisapi.cpp", "source": "indra/newview/llaisapi.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 319, "text": "llappviewer.cpp", "source": "indra/newview/llappviewer.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 472, "text": "llcontrolavatar.cpp", "source": "indra/newview/llcontrolavatar.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 415, "text": "lldirectorcast.cpp", "source": "indra/newview/lldirectorcast.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1403, "text": "lldirectorcast.h", "source": "indra/newview/lldirectorcast.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 521, "text": "llflexibleobject.cpp", "source": "indra/newview/llflexibleobject.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 417, "text": "llfloaterdirector.cpp", "source": "indra/newview/llfloaterdirector.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 588, "text": "llfloaterimcontainer.cpp", "source": "indra/newview/llfloaterimcontainer.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 626, "text": "llfloaterpreferencesgraphicsadvanced.cpp", "source": "indra/newview/llfloaterpreferencesgraphicsadvanced.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 673, "text": "llghostavatar.cpp", "source": "indra/newview/llghostavatar.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1696, "text": "llghostavatar.h", "source": "indra/newview/llghostavatar.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 689, "text": "llhudeffectlookat.cpp", "source": "indra/newview/llhudeffectlookat.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 691, "text": "llhudeffecttrail.cpp", "source": "indra/newview/llhudeffecttrail.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 752, "text": "llmediactrl.cpp", "source": "indra/newview/llmediactrl.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 899, "text": "llpreviewanim.cpp", "source": "indra/newview/llpreviewanim.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 921, "text": "llscenemonitor.cpp", "source": "indra/newview/llscenemonitor.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1041, "text": "llviewerjoint.cpp", "source": "indra/newview/llviewerjoint.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1054, "text": "llviewerobject.cpp", "source": "indra/newview/llviewerobject.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1062, "text": "llviewerpartsim.cpp", "source": "indra/newview/llviewerpartsim.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 2077, "text": "llviewerpartsim.h", "source": "indra/newview/llviewerpartsim.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 2078, "text": "llviewerpartsource.h", "source": "indra/newview/llviewerpartsource.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1080, "text": "llvoavatar.cpp", "source": "indra/newview/llvoavatar.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 2096, "text": "llvoavatar.h", "source": "indra/newview/llvoavatar.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 2097, "text": "llvoavatarself.h", "source": "indra/newview/llvoavatarself.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1090, "text": "llvopartgroup.cpp", "source": "indra/newview/llvopartgroup.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1094, "text": "llvovolume.cpp", "source": "indra/newview/llvovolume.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 2119, "text": "llworld.h", "source": "indra/newview/llworld.h"}

## kit wiring pages

- `move_tab`
- `polish_tab`
- `stepped_tab`

## checks

- `scripts/code_tools/test_avatar_anim_speed.py`

Full include/call edges: ../features.json. Shared hooks: ../hooks.md. Setting values and save coverage: ../settings.csv.

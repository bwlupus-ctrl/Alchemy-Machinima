# capture-sidecar: dependency evidence

Landing layer 6. Check depth/normal/albedo/motion provenance, first-frame invalidity, cuts, resize and capture-off path.

Candidate associations require review. Includes/calls are discovery edges, not a complete call graph or proof of build closure.

## owned

- `indra/newview/alactorchromakey.cpp`
- `indra/newview/alactormaskdiagnostics.cpp`
- `indra/newview/alcinecamobjectlock.h`
- `indra/newview/alcolorchart.cpp`
- `indra/newview/alcolorchart.h`
- `indra/newview/alfloatertemporalcapture.cpp`
- `indra/newview/alfloatertemporalcapture.h`
- `indra/newview/alghostmanipproxy.cpp`
- `indra/newview/alghostmaterialresolver.cpp`
- `indra/newview/alpaneltemporalcapture.cpp`
- `indra/newview/alpaneltemporalcapture.h`
- `indra/newview/alpovblink.cpp`
- `indra/newview/alprojectortracker.cpp`
- `indra/newview/animationexplorer.cpp`
- `indra/newview/animationexplorer.h`
- `indra/newview/app_settings/shaders/class1/alchemy/onLensFiltersF.glsl`
- `indra/newview/app_settings/shaders/class1/deferred/LPMUtil.glsl`
- `indra/newview/app_settings/shaders/class1/deferred/avatarVelocityV.glsl`
- `indra/newview/app_settings/shaders/class1/deferred/motionBlurF.glsl`
- `indra/newview/app_settings/shaders/class1/deferred/projectorVolumetricClassicShadowUtil.glsl`
- `indra/newview/app_settings/shaders/class1/deferred/rigRimWrapF.glsl`
- `indra/newview/app_settings/shaders/class1/deferred/ultimateDiopterGatherF.glsl`
- `indra/newview/app_settings/shaders/class1/deferred/ultimateKaleidoF.glsl`
- `indra/newview/app_settings/shaders/class1/deferred/velocityAlphaF.glsl`
- `indra/newview/app_settings/shaders/class1/deferred/velocityAlphaIndexedF.glsl`
- `indra/newview/app_settings/shaders/class1/deferred/velocityAlphaIndexedV.glsl`
- `indra/newview/app_settings/shaders/class1/deferred/velocityAlphaV.glsl`
- `indra/newview/app_settings/shaders/class1/deferred/velocityCameraF.glsl`
- `indra/newview/app_settings/shaders/class1/deferred/velocityDebugF.glsl`
- `indra/newview/app_settings/shaders/class1/deferred/velocityF.glsl`
- `indra/newview/app_settings/shaders/class1/deferred/velocityV.glsl`
- `indra/newview/app_settings/shaders/class1/deferred/visibleDiffuseSeedF.glsl`
- `indra/newview/app_settings/shaders/class2/deferred/sharedActorFxPbrF.glsl`
- `indra/newview/app_settings/shaders/class3/deferred/projectorVolumetricClassicF.glsl`
- `indra/newview/app_settings/shaders/class3/deferred/projectorVolumetricF.glsl`
- `indra/newview/llactormover.cpp`
- `indra/newview/llactormover.h`
- `indra/newview/llcameraoperator.h`
- `indra/newview/llcinematiccamera.cpp`
- `indra/newview/llclientoutertransform.h`
- `indra/newview/llghostavatar.cpp`
- `indra/newview/llghostavatar.h`
- `indra/newview/llpresentationtime.h`
- `indra/newview/llreshadebridge.cpp`
- `indra/newview/llreshadebridge.h`
- `indra/newview/llreshadebridgeabi.h`
- `indra/newview/lltemporalframecontext.h`
- `indra/newview/skins/default/xui/en/floater_animation_explorer.xml`
- `indra/newview/skins/default/xui/en/floater_director.xml`
- `indra/newview/skins/default/xui/en/floater_temporal_capture.xml`
- `indra/newview/skins/default/xui/en/panel_temporal_capture.xml`
- `reshade-addon/.gitignore`
- `reshade-addon/CMakeLists.txt`
- `reshade-addon/external/reshade/reshade.hpp`
- `reshade-addon/external/reshade/reshade_api.hpp`
- `reshade-addon/external/reshade/reshade_api_device.hpp`
- `reshade-addon/external/reshade/reshade_api_format.hpp`
- `reshade-addon/external/reshade/reshade_api_pipeline.hpp`
- `reshade-addon/external/reshade/reshade_api_resource.hpp`
- `reshade-addon/external/reshade/reshade_events.hpp`
- `reshade-addon/external/reshade/reshade_overlay.hpp`
- `reshade-addon/shaders/SL_Bridge.fxh`
- `reshade-addon/shaders/SL_BridgeDebug.fx`
- `reshade-addon/shaders/SL_GBufferProvider.fx`
- `reshade-addon/src/sl_depth_pin.hpp`
- `reshade-addon/src/sl_reshade_bridge.cpp`
- `reshade-addon/tools/graft_launchpad_v4.py`
- `reshade-addon/tools/patch_dof_v4.py`

## hooks

- {"path": "indra/newview/CMakeLists.txt", "hunks": ["indra/newview/CMakeLists.txt@333,117", "indra/newview/CMakeLists.txt@909,7", "indra/newview/CMakeLists.txt@1288,167", "indra/newview/CMakeLists.txt@1906,8"]}
- {"path": "indra/newview/app_settings/settings.xml", "hunks": ["indra/newview/app_settings/settings.xml@6556,14744", "indra/newview/app_settings/settings.xml@29305,2466"]}
- {"path": "indra/newview/llappviewer.cpp", "hunks": ["indra/newview/llappviewer.cpp@2614,53", "indra/newview/llappviewer.cpp@5338,24"]}
- {"path": "indra/newview/llviewercontrol.cpp", "hunks": ["indra/newview/llviewercontrol.cpp@1207,11", "indra/newview/llviewercontrol.cpp@1279,33"]}
- {"path": "indra/newview/app_settings/settings_alchemy.xml", "hunks": ["indra/newview/app_settings/settings_alchemy.xml@2770,10", "indra/newview/app_settings/settings_alchemy.xml@2781,233"]}
- {"path": "indra/newview/lldrawpool.cpp", "hunks": ["indra/newview/lldrawpool.cpp@71,82", "indra/newview/lldrawpool.cpp@333,32", "indra/newview/lldrawpool.cpp@499,1398"]}
- {"path": "indra/newview/lldrawpool.h", "hunks": ["indra/newview/lldrawpool.h@136,15"]}
- {"path": "indra/newview/lldrawpoolavatar.cpp", "hunks": ["indra/newview/lldrawpoolavatar.cpp@892,158"]}
- {"path": "indra/newview/llspatialpartition.h", "hunks": ["indra/newview/llspatialpartition.h@102,21"]}
- {"path": "indra/newview/llviewerdisplay.cpp", "hunks": ["indra/newview/llviewerdisplay.cpp@102,11", "indra/newview/llviewerdisplay.cpp@1191,82", "indra/newview/llviewerdisplay.cpp@1858,10"]}
- {"path": "indra/newview/llviewerfloaterreg.cpp", "hunks": ["indra/newview/llviewerfloaterreg.cpp@115,17", "indra/newview/llviewerfloaterreg.cpp@538,266"]}
- {"path": "indra/newview/llviewerjoystick.cpp", "hunks": ["indra/newview/llviewerjoystick.cpp@1884,2002"]}
- {"path": "indra/newview/llvoavatar.cpp", "hunks": ["indra/newview/llvoavatar.cpp@899,37", "indra/newview/llvoavatar.cpp@1632,21", "indra/newview/llvoavatar.cpp@6324,49", "indra/newview/llvoavatar.cpp@6545,9"]}
- {"path": "indra/newview/llvoavatar.h", "hunks": ["indra/newview/llvoavatar.h@287,25"]}
- {"path": "indra/newview/llvovolume.cpp", "hunks": ["indra/newview/llvovolume.cpp@1569,18", "indra/newview/llvovolume.cpp@1613,26", "indra/newview/llvovolume.cpp@5690,87", "indra/newview/llvovolume.cpp@5835,10", "indra/newview/llvovolume.cpp@5993,8", "indra/newview/llvovolume.cpp@6090,24", "indra/newview/llvovolume.cpp@6155,15"]}
- {"path": "indra/newview/pipeline.cpp", "hunks": ["indra/newview/pipeline.cpp@125,11", "indra/newview/pipeline.cpp@332,106", "indra/newview/pipeline.cpp@958,88", "indra/newview/pipeline.cpp@1371,61", "indra/newview/pipeline.cpp@1436,15", "indra/newview/pipeline.cpp@1564,106", "indra/newview/pipeline.cpp@1701,30004", "indra/newview/pipeline.cpp@31894,372", "indra/newview/pipeline.cpp@32881,154", "indra/newview/pipeline.cpp@33036,331"]}
- {"path": "indra/newview/pipeline.h", "hunks": ["indra/newview/pipeline.h@85,47", "indra/newview/pipeline.h@709,90", "indra/newview/pipeline.h@802,100", "indra/newview/pipeline.h@1419,31", "indra/newview/pipeline.h@1557,409", "indra/newview/pipeline.h@1992,270", "indra/newview/pipeline.h@2681,194"]}
- {"path": "indra/newview/skins/default/xui/en/floater_lightbox_settings.xml", "hunks": ["indra/newview/skins/default/xui/en/floater_lightbox_settings.xml@368,40", "indra/newview/skins/default/xui/en/floater_lightbox_settings.xml@4360,2151"]}
- {"path": "indra/newview/skins/default/xui/en/floater_preferences_graphics_advanced.xml", "hunks": ["indra/newview/skins/default/xui/en/floater_preferences_graphics_advanced.xml@830,19"]}
- {"path": "indra/newview/skins/default/xui/en/menu_viewer.xml", "hunks": ["indra/newview/skins/default/xui/en/menu_viewer.xml@200,243"]}
- {"path": "indra/newview/app_settings/shaders/class1/deferred/shadowUtil.glsl", "hunks": ["indra/newview/app_settings/shaders/class1/deferred/shadowUtil.glsl@95,154", "indra/newview/app_settings/shaders/class1/deferred/shadowUtil.glsl@256,496"]}
- {"path": "indra/newview/llagent.cpp", "hunks": ["indra/newview/llagent.cpp@70,7", "indra/newview/llagent.cpp@4329,11"]}
- {"path": "indra/newview/lldrawpoolalpha.cpp", "hunks": ["indra/newview/lldrawpoolalpha.cpp@314,439", "indra/newview/lldrawpoolalpha.cpp@1164,157", "indra/newview/lldrawpoolalpha.cpp@1349,116", "indra/newview/lldrawpoolalpha.cpp@1662,65", "indra/newview/lldrawpoolalpha.cpp@1817,27", "indra/newview/lldrawpoolalpha.cpp@1872,34"]}
- {"path": "indra/newview/llviewerpartsim.cpp", "hunks": ["indra/newview/llviewerpartsim.cpp@841,35"]}
- {"path": "indra/newview/llviewershadermgr.cpp", "hunks": ["indra/newview/llviewershadermgr.cpp@339,32", "indra/newview/llviewershadermgr.cpp@374,21", "indra/newview/llviewershadermgr.cpp@437,143", "indra/newview/llviewershadermgr.cpp@1566,18", "indra/newview/llviewershadermgr.cpp@1600,14", "indra/newview/llviewershadermgr.cpp@2153,40", "indra/newview/llviewershadermgr.cpp@2496,28", "indra/newview/llviewershadermgr.cpp@2808,312", "indra/newview/llviewershadermgr.cpp@3920,67", "indra/newview/llviewershadermgr.cpp@5498,299"]}
- {"path": "indra/newview/llviewershadermgr.h", "hunks": ["indra/newview/llviewershadermgr.h@435,34", "indra/newview/llviewershadermgr.h@478,7"]}
- {"path": "indra/llrender/llrender.h", "hunks": ["indra/llrender/llrender.h@479,71"]}
- {"path": "indra/llrender/llrendertarget.h", "hunks": ["indra/llrender/llrendertarget.h@205,35"]}
- {"path": "indra/llrender/llshadermgr.cpp", "hunks": ["indra/llrender/llshadermgr.cpp@2001,363"]}
- {"path": "indra/llrender/llshadermgr.h", "hunks": ["indra/llrender/llshadermgr.h@589,396"]}
- {"path": "indra/llwindow/llwindowwin32.cpp", "hunks": ["indra/llwindow/llwindowwin32.cpp@1624,32", "indra/llwindow/llwindowwin32.cpp@1734,24", "indra/llwindow/llwindowwin32.cpp@1845,16"]}
- {"path": "indra/newview/app_settings/shaders/class1/alchemy/postEffectUtilsF.glsl", "hunks": ["indra/newview/app_settings/shaders/class1/alchemy/postEffectUtilsF.glsl@261,432"]}
- {"path": "indra/newview/app_settings/shaders/class1/deferred/emissiveF.glsl", "hunks": ["indra/newview/app_settings/shaders/class1/deferred/emissiveF.glsl@28,90"]}
- {"path": "indra/newview/lldrawable.cpp", "hunks": ["indra/newview/lldrawable.cpp@1461,42"]}
- {"path": "indra/newview/lldrawpoolterrain.cpp", "hunks": ["indra/newview/lldrawpoolterrain.cpp@720,55"]}
- {"path": "indra/newview/llface.cpp", "hunks": ["indra/newview/llface.cpp@2283,36", "indra/newview/llface.cpp@2413,25"]}
- {"path": "indra/newview/llviewerjointmesh.cpp", "hunks": ["indra/newview/llviewerjointmesh.cpp@171,39"]}
- {"path": "indra/newview/llviewerjointmesh.h", "hunks": ["indra/newview/llviewerjointmesh.h@80,21"]}
- {"path": "indra/newview/llviewerobject.h", "hunks": ["indra/newview/llviewerobject.h@46,7", "indra/newview/llviewerobject.h@154,15", "indra/newview/llviewerobject.h@859,32"]}
- {"path": "indra/newview/llviewertextureanim.cpp", "hunks": ["indra/newview/llviewertextureanim.cpp@237,52"]}
- {"path": "indra/newview/app_settings/shaders/class3/deferred/materialF.glsl", "hunks": ["indra/newview/app_settings/shaders/class3/deferred/materialF.glsl@60,80", "indra/newview/app_settings/shaders/class3/deferred/materialF.glsl@829,51"]}
- {"path": "indra/newview/app_settings/shaders/class1/alchemy/blitWithEffectsF.glsl", "hunks": ["indra/newview/app_settings/shaders/class1/alchemy/blitWithEffectsF.glsl@48,13", "indra/newview/app_settings/shaders/class1/alchemy/blitWithEffectsF.glsl@95,34"]}
- {"path": "indra/newview/app_settings/shaders/class1/deferred/tonemapUtilF.glsl", "hunks": ["indra/newview/app_settings/shaders/class1/deferred/tonemapUtilF.glsl@336,194"]}
- {"path": "indra/newview/app_settings/shaders/class1/environment/waterF.glsl", "hunks": ["indra/newview/app_settings/shaders/class1/environment/waterF.glsl@25,37"]}
- {"path": "indra/newview/app_settings/shaders/class3/environment/waterF.glsl", "hunks": ["indra/newview/app_settings/shaders/class3/environment/waterF.glsl@354,19"]}
- {"path": "indra/newview/lldrawpoolalpha.h", "hunks": ["indra/newview/lldrawpoolalpha.h@58,40", "indra/newview/lldrawpoolalpha.h@111,11"]}
- {"path": "indra/newview/lldrawpoolavatar.h", "hunks": ["indra/newview/lldrawpoolavatar.h@97,14"]}
- {"path": "indra/newview/lldrawpoolbump.cpp", "hunks": ["indra/newview/lldrawpoolbump.cpp@621,36"]}
- {"path": "indra/newview/lldrawpoolbump.h", "hunks": ["indra/newview/lldrawpoolbump.h@76,12"]}
- {"path": "indra/newview/lldrawpoolmaterials.cpp", "hunks": ["indra/newview/lldrawpoolmaterials.cpp@431,101"]}
- {"path": "indra/newview/lldrawpoolmaterials.h", "hunks": ["indra/newview/lldrawpoolmaterials.h@70,12"]}
- {"path": "indra/newview/lldrawpoolpbropaque.cpp", "hunks": ["indra/newview/lldrawpoolpbropaque.cpp@160,73"]}
- {"path": "indra/newview/lldrawpoolpbropaque.h", "hunks": ["indra/newview/lldrawpoolpbropaque.h@41,12"]}
- {"path": "indra/newview/lldrawpoolsimple.cpp", "hunks": ["indra/newview/lldrawpoolsimple.cpp@194,38", "indra/newview/lldrawpoolsimple.cpp@268,130"]}
- {"path": "indra/newview/lldrawpoolsimple.h", "hunks": ["indra/newview/lldrawpoolsimple.h@47,12", "indra/newview/lldrawpoolsimple.h@71,12", "indra/newview/lldrawpoolsimple.h@95,12", "indra/newview/lldrawpoolsimple.h@118,12", "indra/newview/lldrawpoolsimple.h@142,12"]}
- {"path": "indra/newview/lldrawpoolterrain.h", "hunks": ["indra/newview/lldrawpoolterrain.h@56,14"]}
- {"path": "indra/newview/lldrawpooltree.cpp", "hunks": ["indra/newview/lldrawpooltree.cpp@104,57"]}
- {"path": "indra/newview/lldrawpooltree.h", "hunks": ["indra/newview/lldrawpooltree.h@55,13"]}

## settings

- `BDMergeMotionBlur`
- `BDMergeMotionBlurStrength`
- `BDMergeReShadeCutAngle`
- `BDMergeReShadeCutDetect`
- `BDMergeReShadeCutDistance`
- `BDMergeVelocityBuffer`
- `BDMergeVelocityDebug`
- `MachinimaDebugLogging`
- `RenderHDR16BitColor`
- `RenderReShadeDecoupleOnLens`
- `RenderReShadeGBufferDiag`
- `RenderReShadeGBufferPublish`
- `RenderShadowSoftVogel`
- `RenderVisibleDiffuseSidecar`
- `TemporalMode`
- `TronLayer`
- `RenderGLContext10bitSDR`

## shaders

- `indra/newview/app_settings/shaders/class1/alchemy/blitWithEffectsF.glsl`
- `indra/newview/app_settings/shaders/class1/alchemy/onLensFiltersF.glsl`
- `indra/newview/app_settings/shaders/class1/alchemy/postEffectUtilsF.glsl`
- `indra/newview/app_settings/shaders/class1/deferred/LPMUtil.glsl`
- `indra/newview/app_settings/shaders/class1/deferred/avatarVelocityV.glsl`
- `indra/newview/app_settings/shaders/class1/deferred/emissiveF.glsl`
- `indra/newview/app_settings/shaders/class1/deferred/motionBlurF.glsl`
- `indra/newview/app_settings/shaders/class1/deferred/projectorVolumetricClassicShadowUtil.glsl`
- `indra/newview/app_settings/shaders/class1/deferred/rigRimWrapF.glsl`
- `indra/newview/app_settings/shaders/class1/deferred/shadowUtil.glsl`
- `indra/newview/app_settings/shaders/class1/deferred/tonemapUtilF.glsl`
- `indra/newview/app_settings/shaders/class1/deferred/ultimateDiopterGatherF.glsl`
- `indra/newview/app_settings/shaders/class1/deferred/ultimateKaleidoF.glsl`
- `indra/newview/app_settings/shaders/class1/deferred/velocityAlphaF.glsl`
- `indra/newview/app_settings/shaders/class1/deferred/velocityAlphaIndexedF.glsl`
- `indra/newview/app_settings/shaders/class1/deferred/velocityAlphaIndexedV.glsl`
- `indra/newview/app_settings/shaders/class1/deferred/velocityAlphaV.glsl`
- `indra/newview/app_settings/shaders/class1/deferred/velocityCameraF.glsl`
- `indra/newview/app_settings/shaders/class1/deferred/velocityDebugF.glsl`
- `indra/newview/app_settings/shaders/class1/deferred/velocityF.glsl`
- `indra/newview/app_settings/shaders/class1/deferred/velocityV.glsl`
- `indra/newview/app_settings/shaders/class1/deferred/visibleDiffuseSeedF.glsl`
- `indra/newview/app_settings/shaders/class1/environment/waterF.glsl`
- `indra/newview/app_settings/shaders/class2/deferred/sharedActorFxPbrF.glsl`
- `indra/newview/app_settings/shaders/class3/deferred/materialF.glsl`
- `indra/newview/app_settings/shaders/class3/deferred/projectorVolumetricClassicF.glsl`
- `indra/newview/app_settings/shaders/class3/deferred/projectorVolumetricF.glsl`
- `indra/newview/app_settings/shaders/class3/environment/waterF.glsl`
- `reshade-addon/shaders/SL_Bridge.fxh`
- `reshade-addon/shaders/SL_BridgeDebug.fx`
- `reshade-addon/shaders/SL_GBufferProvider.fx`

## xui

- `indra/newview/skins/default/xui/en/director_kit_index.xml`
- `indra/newview/skins/default/xui/en/floater_animation_explorer.xml`
- `indra/newview/skins/default/xui/en/floater_director.xml`
- `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml`
- `indra/newview/skins/default/xui/en/floater_preferences_graphics_advanced.xml`
- `indra/newview/skins/default/xui/en/floater_temporal_capture.xml`
- `indra/newview/skins/default/xui/en/menu_viewer.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_capture_temporal.xml`
- `indra/newview/skins/default/xui/en/panel_temporal_capture.xml`

## generated runtime

- `indra/newview/skins/default/xui/en/director_kit_index.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_capture_temporal.xml`
- `reshade-addon/LAUNCHPAD_INTEGRATION.md`
- `reshade-addon/README.md`

## cmake entries

- {"file": "indra/llrender/CMakeLists.txt", "line": 4, "text": "llcubemap.cpp", "source": "indra/llrender/llcubemap.cpp"}
- {"file": "indra/llrender/CMakeLists.txt", "line": 17, "text": "llimagegl.cpp", "source": "indra/llrender/llimagegl.cpp"}
- {"file": "indra/llrender/CMakeLists.txt", "line": 49, "text": "llimagegl.h", "source": "indra/llrender/llimagegl.h"}
- {"file": "indra/llrender/CMakeLists.txt", "line": 50, "text": "llrender.h", "source": "indra/llrender/llrender.h"}
- {"file": "indra/llrender/CMakeLists.txt", "line": 54, "text": "llrendertarget.h", "source": "indra/llrender/llrendertarget.h"}
- {"file": "indra/llrender/CMakeLists.txt", "line": 23, "text": "llshadermgr.cpp", "source": "indra/llrender/llshadermgr.cpp"}
- {"file": "indra/llrender/CMakeLists.txt", "line": 55, "text": "llshadermgr.h", "source": "indra/llrender/llshadermgr.h"}
- {"file": "indra/llrender/CMakeLists.txt", "line": 27, "text": "llvertexbuffer.cpp", "source": "indra/llrender/llvertexbuffer.cpp"}
- {"file": "indra/llrender/CMakeLists.txt", "line": 60, "text": "llvertexbuffer.h", "source": "indra/llrender/llvertexbuffer.h"}
- {"file": "indra/llui/CMakeLists.txt", "line": 14, "text": "llbutton.cpp", "source": "indra/llui/llbutton.cpp"}
- {"file": "indra/llui/CMakeLists.txt", "line": 39, "text": "llfolderview.cpp", "source": "indra/llui/llfolderview.cpp"}
- {"file": "indra/llui/CMakeLists.txt", "line": 49, "text": "llmenugl.cpp", "source": "indra/llui/llmenugl.cpp"}
- {"file": "indra/llui/CMakeLists.txt", "line": 81, "text": "lltabcontainer.cpp", "source": "indra/llui/lltabcontainer.cpp"}
- {"file": "indra/llui/CMakeLists.txt", "line": 109, "text": "llview.cpp", "source": "indra/llui/llview.cpp"}
- {"file": "indra/llwindow/CMakeLists.txt", "line": 111, "text": "llwindowwin32.cpp", "source": "indra/llwindow/llwindowwin32.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 338, "text": "alactorchromakey.cpp # [ActorKey] WP0/WP1", "source": "indra/newview/alactorchromakey.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 339, "text": "alactormaskdiagnostics.cpp # [ActorMask] WP7", "source": "indra/newview/alactormaskdiagnostics.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1321, "text": "alcinecamobjectlock.h", "source": "indra/newview/alcinecamobjectlock.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 389, "text": "alcolorchart.cpp", "source": "indra/newview/alcolorchart.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1346, "text": "alcolorchart.h", "source": "indra/newview/alcolorchart.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 225, "text": "alfloaterblocked.cpp", "source": "indra/newview/alfloaterblocked.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 414, "text": "alfloatertemporalcapture.cpp", "source": "indra/newview/alfloatertemporalcapture.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1393, "text": "alfloatertemporalcapture.h", "source": "indra/newview/alfloatertemporalcapture.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 388, "text": "alghostmanipproxy.cpp", "source": "indra/newview/alghostmanipproxy.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 342, "text": "alghostmaterialresolver.cpp", "source": "indra/newview/alghostmaterialresolver.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 415, "text": "alpaneltemporalcapture.cpp", "source": "indra/newview/alpaneltemporalcapture.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1394, "text": "alpaneltemporalcapture.h", "source": "indra/newview/alpaneltemporalcapture.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 195, "text": "alpovblink.cpp", "source": "indra/newview/alpovblink.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 398, "text": "alprojectortracker.cpp", "source": "indra/newview/alprojectortracker.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 179, "text": "animationexplorer.cpp", "source": "indra/newview/animationexplorer.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1130, "text": "animationexplorer.h", "source": "indra/newview/animationexplorer.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 2309, "text": "app_settings/settings.xml", "source": "indra/newview/app_settings/settings.xml"}
- {"file": "indra/newview/CMakeLists.txt", "line": 2310, "text": "app_settings/settings_alchemy.xml", "source": "indra/newview/app_settings/settings_alchemy.xml"}
- {"file": "indra/newview/CMakeLists.txt", "line": 283, "text": "fsmaniprotatejoint.cpp", "source": "indra/newview/fsmaniprotatejoint.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 284, "text": "fsmaniptranslatejoint.cpp", "source": "indra/newview/fsmaniptranslatejoint.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 340, "text": "llactormover.cpp", "source": "indra/newview/llactormover.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1298, "text": "llactormover.h", "source": "indra/newview/llactormover.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 302, "text": "llagent.cpp", "source": "indra/newview/llagent.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 301, "text": "llaisapi.cpp", "source": "indra/newview/llaisapi.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 318, "text": "llappviewer.cpp", "source": "indra/newview/llappviewer.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1291, "text": "llcameraoperator.h", "source": "indra/newview/llcameraoperator.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 407, "text": "llcinematiccamera.cpp", "source": "indra/newview/llcinematiccamera.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 481, "text": "lldrawable.cpp", "source": "indra/newview/lldrawable.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1487, "text": "lldrawable.h", "source": "indra/newview/lldrawable.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 482, "text": "lldrawpool.cpp", "source": "indra/newview/lldrawpool.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1488, "text": "lldrawpool.h", "source": "indra/newview/lldrawpool.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 483, "text": "lldrawpoolalpha.cpp", "source": "indra/newview/lldrawpoolalpha.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1489, "text": "lldrawpoolalpha.h", "source": "indra/newview/lldrawpoolalpha.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 484, "text": "lldrawpoolavatar.cpp", "source": "indra/newview/lldrawpoolavatar.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1490, "text": "lldrawpoolavatar.h", "source": "indra/newview/lldrawpoolavatar.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 485, "text": "lldrawpoolbump.cpp", "source": "indra/newview/lldrawpoolbump.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1491, "text": "lldrawpoolbump.h", "source": "indra/newview/lldrawpoolbump.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 486, "text": "lldrawpoolmaterials.cpp", "source": "indra/newview/lldrawpoolmaterials.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1492, "text": "lldrawpoolmaterials.h", "source": "indra/newview/lldrawpoolmaterials.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 487, "text": "lldrawpoolpbropaque.cpp", "source": "indra/newview/lldrawpoolpbropaque.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1493, "text": "lldrawpoolpbropaque.h", "source": "indra/newview/lldrawpoolpbropaque.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 488, "text": "lldrawpoolsimple.cpp", "source": "indra/newview/lldrawpoolsimple.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1494, "text": "lldrawpoolsimple.h", "source": "indra/newview/lldrawpoolsimple.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 490, "text": "lldrawpoolterrain.cpp", "source": "indra/newview/lldrawpoolterrain.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1496, "text": "lldrawpoolterrain.h", "source": "indra/newview/lldrawpoolterrain.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 491, "text": "lldrawpooltree.cpp", "source": "indra/newview/lldrawpooltree.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1497, "text": "lldrawpooltree.h", "source": "indra/newview/lldrawpooltree.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 492, "text": "lldrawpoolwater.cpp", "source": "indra/newview/lldrawpoolwater.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 495, "text": "lldynamictexture.cpp", "source": "indra/newview/lldynamictexture.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 507, "text": "llface.cpp", "source": "indra/newview/llface.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 510, "text": "llfetchedgltfmaterial.cpp", "source": "indra/newview/llfetchedgltfmaterial.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 533, "text": "llfloaterbuy.cpp", "source": "indra/newview/llfloaterbuy.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 534, "text": "llfloaterbuycontents.cpp", "source": "indra/newview/llfloaterbuycontents.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 619, "text": "llfloaterpreferencesgraphicsadvanced.cpp", "source": "indra/newview/llfloaterpreferencesgraphicsadvanced.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 626, "text": "llfloaterreporter.cpp", "source": "indra/newview/llfloaterreporter.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 666, "text": "llghostavatar.cpp", "source": "indra/newview/llghostavatar.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1677, "text": "llghostavatar.h", "source": "indra/newview/llghostavatar.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 682, "text": "llhudeffectlookat.cpp", "source": "indra/newview/llhudeffectlookat.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 684, "text": "llhudeffecttrail.cpp", "source": "indra/newview/llhudeffecttrail.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 727, "text": "lllocalmesh.cpp", "source": "indra/newview/lllocalmesh.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 745, "text": "llmediactrl.cpp", "source": "indra/newview/llmediactrl.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 748, "text": "llmeshrepository.cpp", "source": "indra/newview/llmeshrepository.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1395, "text": "llpresentationtime.h", "source": "indra/newview/llpresentationtime.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 912, "text": "llreshadebridge.cpp", "source": "indra/newview/llreshadebridge.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1909, "text": "llreshadebridge.h", "source": "indra/newview/llreshadebridge.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1910, "text": "llreshadebridgeabi.h", "source": "indra/newview/llreshadebridgeabi.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 914, "text": "llscenemonitor.cpp", "source": "indra/newview/llscenemonitor.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1929, "text": "llselectmgr.h", "source": "indra/newview/llselectmgr.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1944, "text": "llspatialpartition.h", "source": "indra/newview/llspatialpartition.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 952, "text": "llsurface.cpp", "source": "indra/newview/llsurface.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1398, "text": "lltemporalframecontext.h", "source": "indra/newview/lltemporalframecontext.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 966, "text": "lltextureview.cpp", "source": "indra/newview/lltextureview.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 990, "text": "lltoolpie.cpp", "source": "indra/newview/lltoolpie.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1022, "text": "llviewercontrol.cpp", "source": "indra/newview/llviewercontrol.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1024, "text": "llviewerdisplay.cpp", "source": "indra/newview/llviewerdisplay.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1026, "text": "llviewerfloaterreg.cpp", "source": "indra/newview/llviewerfloaterreg.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1034, "text": "llviewerjoint.cpp", "source": "indra/newview/llviewerjoint.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1036, "text": "llviewerjointmesh.cpp", "source": "indra/newview/llviewerjointmesh.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 2039, "text": "llviewerjointmesh.h", "source": "indra/newview/llviewerjointmesh.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1037, "text": "llviewerjoystick.cpp", "source": "indra/newview/llviewerjoystick.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 2050, "text": "llviewerobject.h", "source": "indra/newview/llviewerobject.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1049, "text": "llvieweroctree.cpp", "source": "indra/newview/llvieweroctree.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1055, "text": "llviewerpartsim.cpp", "source": "indra/newview/llviewerpartsim.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1058, "text": "llviewershadermgr.cpp", "source": "indra/newview/llviewershadermgr.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 2062, "text": "llviewershadermgr.h", "source": "indra/newview/llviewershadermgr.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1064, "text": "llviewertextureanim.cpp", "source": "indra/newview/llviewertextureanim.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1073, "text": "llvoavatar.cpp", "source": "indra/newview/llvoavatar.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 2077, "text": "llvoavatar.h", "source": "indra/newview/llvoavatar.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1074, "text": "llvoavatarself.cpp", "source": "indra/newview/llvoavatarself.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 2078, "text": "llvoavatarself.h", "source": "indra/newview/llvoavatarself.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1083, "text": "llvopartgroup.cpp", "source": "indra/newview/llvopartgroup.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1087, "text": "llvovolume.cpp", "source": "indra/newview/llvovolume.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 2090, "text": "llvovolume.h", "source": "indra/newview/llvovolume.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1107, "text": "pipeline.cpp", "source": "indra/newview/pipeline.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 2110, "text": "pipeline.h", "source": "indra/newview/pipeline.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1111, "text": "rlveffects.cpp", "source": "indra/newview/rlveffects.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1114, "text": "rlvhandler.cpp", "source": "indra/newview/rlvhandler.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1117, "text": "rlvlocks.cpp", "source": "indra/newview/rlvlocks.cpp"}
- {"file": "reshade-addon/CMakeLists.txt", "line": 14, "text": "add_library(sl_reshade_bridge SHARED src/sl_reshade_bridge.cpp)", "source": "reshade-addon/src/sl_reshade_bridge.cpp"}

## kit wiring pages

- `temporal_tab`

## checks

None identified by local evidence; not a claim that none is needed.

Full include/call edges: ../features.json. Shared hooks: ../hooks.md. Setting values and save coverage: ../settings.csv.

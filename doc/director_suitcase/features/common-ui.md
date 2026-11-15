# common-ui: dependency evidence

Landing layer 1. Resize and scroll embedded panels; slider wheel settles once; hidden tabs do no expensive work.

Candidate associations require review. Includes/calls are discovery edges, not a complete call graph or proof of build closure.

## owned

- `indra/llui/alcompassdial.cpp`
- `indra/llui/alcompassdial.h`
- `indra/llui/llsliderwheel.cpp`
- `indra/llui/llsliderwheel.h`
- `indra/llui/llsliderwheelcore.cpp`
- `indra/llui/llsliderwheelcore.h`
- `indra/llui/lluifitdiag.cpp`
- `indra/llui/lluifitdiag.h`
- `indra/newview/alcinecamobjectlockui.h`
- `indra/newview/aldirectoralloff.cpp`
- `indra/newview/alenvintensitypresets.cpp`
- `indra/newview/alenvonlydiagnostics.cpp`
- `indra/newview/alfloatercinelightcues.h`
- `indra/newview/alfloatergazecues.h`
- `indra/newview/alfloatertemporalcapture.cpp`
- `indra/newview/alfloaterultimatediopter.cpp`
- `indra/newview/alfloaterultimatediopter.h`
- `indra/newview/alflowgrid.cpp`
- `indra/newview/alflowgrid.h`
- `indra/newview/alkitcard.cpp`
- `indra/newview/alkitcard.h`
- `indra/newview/alkithistory.cpp`
- `indra/newview/alkitlazypage.cpp`
- `indra/newview/alkitpage.cpp`
- `indra/newview/alkitpage.h`
- `indra/newview/alkitpresets.cpp`
- `indra/newview/alkitrow.cpp`
- `indra/newview/alkitshell.cpp`
- `indra/newview/alkitshell.h`
- `indra/newview/allightrigmap.cpp`
- `indra/newview/alpanelactorkey.h`
- `indra/newview/alpanelactormover.cpp`
- `indra/newview/alpanelactormover.h`
- `indra/newview/alpanelatmosphericvolumes.h`
- `indra/newview/alpanelavatarlightresponse.h`
- `indra/newview/alpanelcinecamparams.cpp`
- `indra/newview/alpanelcinelightrig.cpp`
- `indra/newview/alpanelcinelightrig.h`
- `indra/newview/alpaneldanceball.cpp`
- `indra/newview/alpaneldanceball.h`
- `indra/newview/alpaneldirectorswitcher.cpp`
- `indra/newview/alpaneldirectorvcam.cpp`
- `indra/newview/alpanelexposurehub.cpp`
- `indra/newview/alpanelexposurehub.h`
- `indra/newview/alpanelexposurepage.cpp`
- `indra/newview/alpanelexposurepage.h`
- `indra/newview/alpanelflycamrecorder.cpp`
- `indra/newview/alpanelflycamrecorder.h`
- `indra/newview/alpanelgazecues.cpp`
- `indra/newview/alpanelgazecues.h`
- `indra/newview/alpanelghoststudio.cpp`
- `indra/newview/alpanelghoststudio.h`
- `indra/newview/alpanellensgaze.h`
- `indra/newview/alpanellenslooks.cpp`
- `indra/newview/alpanellightcues.cpp`
- `indra/newview/alpanelpatheditor.cpp`
- `indra/newview/alpanelpracticals.cpp`
- `indra/newview/alpanelprojectorshafts.cpp`
- `indra/newview/alpanelprojectorshafts.h`
- `indra/newview/alpanelpropmover.cpp`
- `indra/newview/alpaneltemporalcapture.cpp`
- `indra/newview/alpaneltemporalcapture.h`
- `indra/newview/alpanelweathersettings.cpp`
- `indra/newview/alpresetlook.h`
- `indra/newview/alrigrimcontroller.h`
- `indra/newview/alscrollfit.cpp`
- `indra/newview/alscrollfit.h`
- `indra/newview/alscrollfocus.h`
- `indra/newview/alsettingsundohistory.cpp`
- `indra/newview/alsettingsundohistory.h`
- `indra/newview/alsettingsundohistorymodel.cpp`
- `indra/newview/alsettingsundohistorymodel.h`
- `indra/newview/aluirefresh.h`
- `indra/newview/alundosequence.h`
- `indra/newview/alundovalue.h`
- `indra/newview/alvcammonitorstrip.cpp`
- `indra/newview/alvcammonitorstrip.h`
- `indra/newview/fsfloaterposestand.h`
- `indra/newview/llactormover.cpp`
- `indra/newview/llactormover.h`
- `indra/newview/llfloateractormover.cpp`
- `indra/newview/llfloaterbigpreview.h`
- `indra/newview/llfloaterdirector.cpp`
- `indra/newview/llfloaterdirector.h`
- `indra/newview/llfloaterprismmanager.cpp`
- `indra/newview/llfloaterprismmanager.h`
- `indra/newview/skins/default/xui/en/panel_dance_ball.xml`
- `indra/newview/skins/default/xui/en/widgets/compass_dial.xml`

## hooks

- {"path": "indra/llui/CMakeLists.txt", "hunks": ["indra/llui/CMakeLists.txt@4,7", "indra/llui/CMakeLists.txt@71,8", "indra/llui/CMakeLists.txt@92,7", "indra/llui/CMakeLists.txt@117,7", "indra/llui/CMakeLists.txt@190,12", "indra/llui/CMakeLists.txt@223,7", "indra/llui/CMakeLists.txt@307,7"]}
- {"path": "indra/llui/llfloater.cpp", "hunks": ["indra/llui/llfloater.cpp@58,8", "indra/llui/llfloater.cpp@306,7", "indra/llui/llfloater.cpp@726,16", "indra/llui/llfloater.cpp@775,23", "indra/llui/llfloater.cpp@863,33"]}
- {"path": "indra/llui/llfloater.h", "hunks": ["indra/llui/llfloater.h@380,8", "indra/llui/llfloater.h@566,7", "indra/llui/llfloater.h@587,12"]}
- {"path": "indra/llui/llscrollbar.cpp", "hunks": ["indra/llui/llscrollbar.cpp@35,7", "indra/llui/llscrollbar.cpp@433,9"]}
- {"path": "indra/llui/llscrollcontainer.cpp", "hunks": ["indra/llui/llscrollcontainer.cpp@32,12", "indra/llui/llscrollcontainer.cpp@272,19", "indra/llui/llscrollcontainer.cpp@322,19"]}
- {"path": "indra/llui/llslider.cpp", "hunks": ["indra/llui/llslider.cpp@24,18", "indra/llui/llslider.cpp@60,28", "indra/llui/llslider.cpp@119,39", "indra/llui/llslider.cpp@314,10", "indra/llui/llslider.cpp@363,9", "indra/llui/llslider.cpp@396,230", "indra/llui/llslider.cpp@628,44"]}
- {"path": "indra/llui/llslider.h", "hunks": ["indra/llui/llslider.h@24,21", "indra/llui/llslider.h@65,29", "indra/llui/llslider.h@119,45", "indra/llui/llslider.h@172,19"]}
- {"path": "indra/llui/llsliderctrl.cpp", "hunks": ["indra/llui/llsliderctrl.cpp@37,7", "indra/llui/llsliderctrl.cpp@151,25", "indra/llui/llsliderctrl.cpp@238,82", "indra/llui/llsliderctrl.cpp@431,13", "indra/llui/llsliderctrl.cpp@447,15", "indra/llui/llsliderctrl.cpp@464,29", "indra/llui/llsliderctrl.cpp@496,42"]}
- {"path": "indra/llui/llsliderctrl.h", "hunks": ["indra/llui/llsliderctrl.h@51,19", "indra/llui/llsliderctrl.h@155,17"]}
- {"path": "indra/llui/llspinctrl.cpp", "hunks": ["indra/llui/llspinctrl.cpp@185,29", "indra/llui/llspinctrl.cpp@250,7", "indra/llui/llspinctrl.cpp@294,7", "indra/llui/llspinctrl.cpp@455,20", "indra/llui/llspinctrl.cpp@485,7"]}
- {"path": "indra/llui/llspinctrl.h", "hunks": ["indra/llui/llspinctrl.h@78,20"]}
- {"path": "indra/newview/CMakeLists.txt", "hunks": ["indra/newview/CMakeLists.txt@186,55", "indra/newview/CMakeLists.txt@335,124", "indra/newview/CMakeLists.txt@1145,59", "indra/newview/CMakeLists.txt@1301,184", "indra/newview/CMakeLists.txt@2879,32"]}
- {"path": "indra/newview/app_settings/settings.xml", "hunks": ["indra/newview/app_settings/settings.xml@6617,14888", "indra/newview/app_settings/settings.xml@25897,1573"]}
- {"path": "indra/newview/llappviewer.cpp", "hunks": ["indra/newview/llappviewer.cpp@25,19", "indra/newview/llappviewer.cpp@72,35", "indra/newview/llappviewer.cpp@4687,10", "indra/newview/llappviewer.cpp@6087,8"]}
- {"path": "indra/newview/llfloaterenvironmentadjust.cpp", "hunks": ["indra/newview/llfloaterenvironmentadjust.cpp@27,39", "indra/newview/llfloaterenvironmentadjust.cpp@299,100", "indra/newview/llfloaterenvironmentadjust.cpp@1123,296"]}
- {"path": "indra/newview/llfloaterenvironmentadjust.h", "hunks": ["indra/newview/llfloaterenvironmentadjust.h@31,19", "indra/newview/llfloaterenvironmentadjust.h@60,19"]}
- {"path": "indra/newview/llfloaterhoverheight.cpp", "hunks": ["indra/newview/llfloaterhoverheight.cpp@29,50"]}
- {"path": "indra/newview/llfloaterjoystick.cpp", "hunks": ["indra/newview/llfloaterjoystick.cpp@36,7", "indra/newview/llfloaterjoystick.cpp@112,204", "indra/newview/llfloaterjoystick.cpp@323,79"]}
- {"path": "indra/newview/llfloaterjoystick.h", "hunks": ["indra/newview/llfloaterjoystick.h@27,34", "indra/newview/llfloaterjoystick.h@117,30"]}
- {"path": "indra/newview/llpanelmaininventory.cpp", "hunks": ["indra/newview/llpanelmaininventory.cpp@26,8"]}
- {"path": "indra/newview/llstartup.cpp", "hunks": ["indra/newview/llstartup.cpp@44,9"]}
- {"path": "indra/newview/llviewercontrol.cpp", "hunks": ["indra/newview/llviewercontrol.cpp@101,7"]}
- {"path": "indra/newview/skins/default/xui/en/floater_adjust_environment.xml", "hunks": ["indra/newview/skins/default/xui/en/floater_adjust_environment.xml@547,872"]}

## settings

- `UIFitDiagnostics`
- `UIFitDiagnosticsIgnore`
- `UIFitDiagnosticsTolerancePx`
- `UIRefreshForceAlways`
- `UISliderWheelAdjust`

## shaders

None identified by local evidence; not a claim that none is needed.

## xui

- `indra/newview/skins/default/xui/en/floater_adjust_environment.xml`
- `indra/newview/skins/default/xui/en/panel_dance_ball.xml`
- `indra/newview/skins/default/xui/en/widgets/compass_dial.xml`

## generated runtime

None identified by local evidence; not a claim that none is needed.

## cmake entries

- {"file": "indra/llui/CMakeLists.txt", "line": 7, "text": "alcompassdial.cpp", "source": "indra/llui/alcompassdial.cpp"}
- {"file": "indra/llui/CMakeLists.txt", "line": 120, "text": "alcompassdial.h", "source": "indra/llui/alcompassdial.h"}
- {"file": "indra/llui/CMakeLists.txt", "line": 14, "text": "llbutton.cpp", "source": "indra/llui/llbutton.cpp"}
- {"file": "indra/llui/CMakeLists.txt", "line": 19, "text": "llcombobox.cpp", "source": "indra/llui/llcombobox.cpp"}
- {"file": "indra/llui/CMakeLists.txt", "line": 34, "text": "llfloater.cpp", "source": "indra/llui/llfloater.cpp"}
- {"file": "indra/llui/CMakeLists.txt", "line": 149, "text": "llfloater.h", "source": "indra/llui/llfloater.h"}
- {"file": "indra/llui/CMakeLists.txt", "line": 167, "text": "llmenugl.h", "source": "indra/llui/llmenugl.h"}
- {"file": "indra/llui/CMakeLists.txt", "line": 64, "text": "llscrollbar.cpp", "source": "indra/llui/llscrollbar.cpp"}
- {"file": "indra/llui/CMakeLists.txt", "line": 65, "text": "llscrollcontainer.cpp", "source": "indra/llui/llscrollcontainer.cpp"}
- {"file": "indra/llui/CMakeLists.txt", "line": 72, "text": "llslider.cpp", "source": "indra/llui/llslider.cpp"}
- {"file": "indra/llui/CMakeLists.txt", "line": 196, "text": "llslider.h", "source": "indra/llui/llslider.h"}
- {"file": "indra/llui/CMakeLists.txt", "line": 73, "text": "llsliderctrl.cpp", "source": "indra/llui/llsliderctrl.cpp"}
- {"file": "indra/llui/CMakeLists.txt", "line": 195, "text": "llsliderctrl.h", "source": "indra/llui/llsliderctrl.h"}
- {"file": "indra/llui/CMakeLists.txt", "line": 74, "text": "llsliderwheel.cpp", "source": "indra/llui/llsliderwheel.cpp"}
- {"file": "indra/llui/CMakeLists.txt", "line": 197, "text": "llsliderwheel.h", "source": "indra/llui/llsliderwheel.h"}
- {"file": "indra/llui/CMakeLists.txt", "line": 75, "text": "llsliderwheelcore.cpp", "source": "indra/llui/llsliderwheelcore.cpp"}
- {"file": "indra/llui/CMakeLists.txt", "line": 310, "text": "llsliderwheelcore.cpp # [SliderWheel] pure logic, tests/llsliderwheelcore_test.cpp", "source": "indra/llui/llsliderwheelcore.cpp"}
- {"file": "indra/llui/CMakeLists.txt", "line": 198, "text": "llsliderwheelcore.h", "source": "indra/llui/llsliderwheelcore.h"}
- {"file": "indra/llui/CMakeLists.txt", "line": 76, "text": "llspinctrl.cpp", "source": "indra/llui/llspinctrl.cpp"}
- {"file": "indra/llui/CMakeLists.txt", "line": 202, "text": "llspinctrl.h", "source": "indra/llui/llspinctrl.h"}
- {"file": "indra/llui/CMakeLists.txt", "line": 81, "text": "lltabcontainer.cpp", "source": "indra/llui/lltabcontainer.cpp"}
- {"file": "indra/llui/CMakeLists.txt", "line": 95, "text": "lluifitdiag.cpp", "source": "indra/llui/lluifitdiag.cpp"}
- {"file": "indra/llui/CMakeLists.txt", "line": 226, "text": "lluifitdiag.h", "source": "indra/llui/lluifitdiag.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1335, "text": "alcinecamobjectlockui.h", "source": "indra/newview/alcinecamobjectlockui.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 203, "text": "aldirectoralloff.cpp", "source": "indra/newview/aldirectoralloff.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 357, "text": "alenvintensitypresets.cpp", "source": "indra/newview/alenvintensitypresets.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 356, "text": "alenvonlydiagnostics.cpp", "source": "indra/newview/alenvonlydiagnostics.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1331, "text": "alfloatercinelightcues.h", "source": "indra/newview/alfloatercinelightcues.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1194, "text": "alfloaterexploresounds.h", "source": "indra/newview/alfloaterexploresounds.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1419, "text": "alfloatergazecues.h", "source": "indra/newview/alfloatergazecues.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 422, "text": "alfloatertemporalcapture.cpp", "source": "indra/newview/alfloatertemporalcapture.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 374, "text": "alfloaterultimatediopter.cpp", "source": "indra/newview/alfloaterultimatediopter.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1332, "text": "alfloaterultimatediopter.h", "source": "indra/newview/alfloaterultimatediopter.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 205, "text": "alflowgrid.cpp", "source": "indra/newview/alflowgrid.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1165, "text": "alflowgrid.h", "source": "indra/newview/alflowgrid.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 208, "text": "alkitcard.cpp", "source": "indra/newview/alkitcard.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1168, "text": "alkitcard.h", "source": "indra/newview/alkitcard.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 211, "text": "alkithistory.cpp", "source": "indra/newview/alkithistory.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 212, "text": "alkitlazypage.cpp", "source": "indra/newview/alkitlazypage.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 213, "text": "alkitpage.cpp", "source": "indra/newview/alkitpage.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1176, "text": "alkitpage.h", "source": "indra/newview/alkitpage.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 215, "text": "alkitpresets.cpp", "source": "indra/newview/alkitpresets.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 216, "text": "alkitrow.cpp", "source": "indra/newview/alkitrow.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 218, "text": "alkitshell.cpp", "source": "indra/newview/alkitshell.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1182, "text": "alkitshell.h", "source": "indra/newview/alkitshell.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 226, "text": "allightrigmap.cpp", "source": "indra/newview/allightrigmap.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1434, "text": "alpanelactorkey.h", "source": "indra/newview/alpanelactorkey.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 425, "text": "alpanelactormover.cpp", "source": "indra/newview/alpanelactormover.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1427, "text": "alpanelactormover.h", "source": "indra/newview/alpanelactormover.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1429, "text": "alpanelatmosphericvolumes.h", "source": "indra/newview/alpanelatmosphericvolumes.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1430, "text": "alpanelavatarlightresponse.h", "source": "indra/newview/alpanelavatarlightresponse.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 444, "text": "alpanelcinecamparams.cpp", "source": "indra/newview/alpanelcinecamparams.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 439, "text": "alpanelcinelightrig.cpp", "source": "indra/newview/alpanelcinelightrig.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1452, "text": "alpanelcinelightrig.h", "source": "indra/newview/alpanelcinelightrig.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 446, "text": "alpaneldanceball.cpp", "source": "indra/newview/alpaneldanceball.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1472, "text": "alpaneldanceball.h", "source": "indra/newview/alpaneldanceball.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 448, "text": "alpaneldirectorswitcher.cpp", "source": "indra/newview/alpaneldirectorswitcher.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 221, "text": "alpaneldirectorvcam.cpp", "source": "indra/newview/alpaneldirectorvcam.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 431, "text": "alpanelexposurehub.cpp", "source": "indra/newview/alpanelexposurehub.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1432, "text": "alpanelexposurehub.h", "source": "indra/newview/alpanelexposurehub.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 432, "text": "alpanelexposurepage.cpp", "source": "indra/newview/alpanelexposurepage.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1433, "text": "alpanelexposurepage.h", "source": "indra/newview/alpanelexposurepage.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 451, "text": "alpanelflycamrecorder.cpp", "source": "indra/newview/alpanelflycamrecorder.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1477, "text": "alpanelflycamrecorder.h", "source": "indra/newview/alpanelflycamrecorder.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 434, "text": "alpanelgazecues.cpp", "source": "indra/newview/alpanelgazecues.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1435, "text": "alpanelgazecues.h", "source": "indra/newview/alpanelgazecues.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 407, "text": "alpanelghoststudio.cpp", "source": "indra/newview/alpanelghoststudio.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1401, "text": "alpanelghoststudio.h", "source": "indra/newview/alpanelghoststudio.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1471, "text": "alpanellensgaze.h", "source": "indra/newview/alpanellensgaze.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 435, "text": "alpanellenslooks.cpp", "source": "indra/newview/alpanellenslooks.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 436, "text": "alpanellightcues.cpp", "source": "indra/newview/alpanellightcues.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 452, "text": "alpanelpatheditor.cpp", "source": "indra/newview/alpanelpatheditor.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 437, "text": "alpanelpracticals.cpp", "source": "indra/newview/alpanelpracticals.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 438, "text": "alpanelprojectorshafts.cpp", "source": "indra/newview/alpanelprojectorshafts.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1439, "text": "alpanelprojectorshafts.h", "source": "indra/newview/alpanelprojectorshafts.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 408, "text": "alpanelpropmover.cpp", "source": "indra/newview/alpanelpropmover.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 423, "text": "alpaneltemporalcapture.cpp", "source": "indra/newview/alpaneltemporalcapture.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1422, "text": "alpaneltemporalcapture.h", "source": "indra/newview/alpaneltemporalcapture.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 449, "text": "alpanelweathersettings.cpp", "source": "indra/newview/alpanelweathersettings.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1189, "text": "alrigrimcontroller.h", "source": "indra/newview/alrigrimcontroller.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 235, "text": "alscrollfit.cpp # [ScrollFit]", "source": "indra/newview/alscrollfit.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1392, "text": "alscrollfit.h # [ScrollFit]", "source": "indra/newview/alscrollfit.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1391, "text": "alscrollfocus.h", "source": "indra/newview/alscrollfocus.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 358, "text": "alsettingsundohistory.cpp", "source": "indra/newview/alsettingsundohistory.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1447, "text": "alsettingsundohistory.h", "source": "indra/newview/alsettingsundohistory.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 359, "text": "alsettingsundohistorymodel.cpp", "source": "indra/newview/alsettingsundohistorymodel.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 2906, "text": "alsettingsundohistorymodel.cpp", "source": "indra/newview/alsettingsundohistorymodel.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1448, "text": "alsettingsundohistorymodel.h", "source": "indra/newview/alsettingsundohistorymodel.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1403, "text": "aluirefresh.h # [GhostPanelPerf]", "source": "indra/newview/aluirefresh.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1172, "text": "alundosequence.h", "source": "indra/newview/alundosequence.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 222, "text": "alvcammonitorstrip.cpp", "source": "indra/newview/alvcammonitorstrip.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1186, "text": "alvcammonitorstrip.h", "source": "indra/newview/alvcammonitorstrip.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 2339, "text": "app_settings/settings.xml", "source": "indra/newview/app_settings/settings.xml"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1247, "text": "fsfloaterposestand.h", "source": "indra/newview/fsfloaterposestand.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 342, "text": "llactormover.cpp", "source": "indra/newview/llactormover.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1311, "text": "llactormover.h", "source": "indra/newview/llactormover.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 320, "text": "llappviewer.cpp", "source": "indra/newview/llappviewer.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 525, "text": "llfloaterabout.cpp", "source": "indra/newview/llfloaterabout.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 417, "text": "llfloateractormover.cpp", "source": "indra/newview/llfloateractormover.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1565, "text": "llfloaterbigpreview.h", "source": "indra/newview/llfloaterbigpreview.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1575, "text": "llfloatercamerapresets.h", "source": "indra/newview/llfloatercamerapresets.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 418, "text": "llfloaterdirector.cpp", "source": "indra/newview/llfloaterdirector.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1417, "text": "llfloaterdirector.h", "source": "indra/newview/llfloaterdirector.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 565, "text": "llfloaterenvironmentadjust.cpp", "source": "indra/newview/llfloaterenvironmentadjust.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1593, "text": "llfloaterenvironmentadjust.h", "source": "indra/newview/llfloaterenvironmentadjust.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 584, "text": "llfloaterhoverheight.cpp", "source": "indra/newview/llfloaterhoverheight.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1621, "text": "llfloaterimcontainer.h", "source": "indra/newview/llfloaterimcontainer.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 594, "text": "llfloaterjoystick.cpp", "source": "indra/newview/llfloaterjoystick.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1625, "text": "llfloaterjoystick.h", "source": "indra/newview/llfloaterjoystick.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 628, "text": "llfloaterpreferencesgraphicsadvanced.cpp", "source": "indra/newview/llfloaterpreferencesgraphicsadvanced.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1660, "text": "llfloaterpreferencesgraphicsadvanced.h", "source": "indra/newview/llfloaterpreferencesgraphicsadvanced.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 421, "text": "llfloaterprismmanager.cpp", "source": "indra/newview/llfloaterprismmanager.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1420, "text": "llfloaterprismmanager.h", "source": "indra/newview/llfloaterprismmanager.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 651, "text": "llfloatersnapshot.cpp", "source": "indra/newview/llfloatersnapshot.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1683, "text": "llfloatersnapshot.h", "source": "indra/newview/llfloatersnapshot.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 716, "text": "llinventoryfunctions.cpp", "source": "indra/newview/llinventoryfunctions.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 802, "text": "llpaneleditwater.cpp", "source": "indra/newview/llpaneleditwater.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 833, "text": "llpanelmaininventory.cpp", "source": "indra/newview/llpanelmaininventory.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 860, "text": "llpanelsnapshotlocal.cpp", "source": "indra/newview/llpanelsnapshotlocal.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 862, "text": "llpanelsnapshotpostcard.cpp", "source": "indra/newview/llpanelsnapshotpostcard.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 863, "text": "llpanelsnapshotprofile.cpp", "source": "indra/newview/llpanelsnapshotprofile.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 957, "text": "llstartup.cpp", "source": "indra/newview/llstartup.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1031, "text": "llviewercontrol.cpp", "source": "indra/newview/llviewercontrol.cpp"}

## kit wiring pages

None identified by local evidence; not a claim that none is needed.

## checks

None identified by local evidence; not a claim that none is needed.

Full include/call edges: ../features.json. Shared hooks: ../hooks.md. Setting values and save coverage: ../settings.csv.

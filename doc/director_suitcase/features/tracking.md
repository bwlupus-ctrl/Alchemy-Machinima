# tracking: dependency evidence

Landing layer 7. Track moving actor/group with projector; remove target, cross region and release without stale pointers.

Candidate associations require review. Includes/calls are discovery edges, not a complete call graph or proof of build closure.

## owned

- `indra/newview/alpaneldirectorvcam.cpp`
- `indra/newview/alpanelpracticals.cpp`
- `indra/newview/alprojectortracker.cpp`
- `indra/newview/alprojectortracker.h`
- `indra/newview/alprojectortrackmath.h`
- `indra/newview/alvcammonitorstrip.cpp`
- `indra/newview/lldirectorcast.cpp`
- `indra/newview/llfloaterdirector.cpp`
- `indra/newview/llfloaterdirector.h`

## hooks

- {"path": "indra/newview/CMakeLists.txt", "hunks": ["indra/newview/CMakeLists.txt@333,115", "indra/newview/CMakeLists.txt@1286,162", "indra/newview/CMakeLists.txt@2947,185"]}
- {"path": "indra/newview/llappviewer.cpp", "hunks": ["indra/newview/llappviewer.cpp@72,34", "indra/newview/llappviewer.cpp@1941,24", "indra/newview/llappviewer.cpp@5614,9", "indra/newview/llappviewer.cpp@5969,30", "indra/newview/llappviewer.cpp@6105,7"]}
- {"path": "indra/newview/alviewermenu.cpp", "hunks": ["indra/newview/alviewermenu.cpp@18,36", "indra/newview/alviewermenu.cpp@294,7", "indra/newview/alviewermenu.cpp@304,8"]}
- {"path": "indra/newview/llviewermenu.cpp", "hunks": ["indra/newview/llviewermenu.cpp@25,8", "indra/newview/llviewermenu.cpp@6031,7"]}
- {"path": "indra/newview/llagent.cpp", "hunks": ["indra/newview/llagent.cpp@26,12", "indra/newview/llagent.cpp@1087,17", "indra/newview/llagent.cpp@4783,9"]}
- {"path": "indra/newview/llselectmgr.cpp", "hunks": ["indra/newview/llselectmgr.cpp@29,8", "indra/newview/llselectmgr.cpp@4803,8", "indra/newview/llselectmgr.cpp@4899,8", "indra/newview/llselectmgr.cpp@5071,8", "indra/newview/llselectmgr.cpp@5527,13", "indra/newview/llselectmgr.cpp@5803,10", "indra/newview/llselectmgr.cpp@6029,8", "indra/newview/llselectmgr.cpp@7719,21", "indra/newview/llselectmgr.cpp@7929,7", "indra/newview/llselectmgr.cpp@9149,7", "indra/newview/llselectmgr.cpp@9274,7"]}
- {"path": "indra/newview/llviewerobject.cpp", "hunks": ["indra/newview/llviewerobject.cpp@25,12", "indra/newview/llviewerobject.cpp@2339,33", "indra/newview/llviewerobject.cpp@4597,27"]}
- {"path": "indra/newview/lldrawable.cpp", "hunks": ["indra/newview/lldrawable.cpp@48,8", "indra/newview/lldrawable.cpp@63,20"]}
- {"path": "indra/newview/llviewerobjectlist.cpp", "hunks": ["indra/newview/llviewerobjectlist.cpp@27,7", "indra/newview/llviewerobjectlist.cpp@244,21", "indra/newview/llviewerobjectlist.cpp@1402,8"]}
- {"path": "indra/newview/altoolalign.cpp", "hunks": ["indra/newview/altoolalign.cpp@22,7", "indra/newview/altoolalign.cpp@443,12"]}
- {"path": "indra/newview/llfloatertools.cpp", "hunks": ["indra/newview/llfloatertools.cpp@25,7", "indra/newview/llfloatertools.cpp@987,9"]}
- {"path": "indra/newview/llmaniprotate.cpp", "hunks": ["indra/newview/llmaniprotate.cpp@25,7", "indra/newview/llmaniprotate.cpp@387,13"]}
- {"path": "indra/newview/llmanipscale.cpp", "hunks": ["indra/newview/llmanipscale.cpp@25,7", "indra/newview/llmanipscale.cpp@343,13"]}
- {"path": "indra/newview/llmaniptranslate.cpp", "hunks": ["indra/newview/llmaniptranslate.cpp@29,7", "indra/newview/llmaniptranslate.cpp@322,13"]}
- {"path": "indra/newview/llpanelobject.cpp", "hunks": ["indra/newview/llpanelobject.cpp@28,7", "indra/newview/llpanelobject.cpp@379,7", "indra/newview/llpanelobject.cpp@425,12", "indra/newview/llpanelobject.cpp@1788,23", "indra/newview/llpanelobject.cpp@1862,23", "indra/newview/llpanelobject.cpp@1916,23", "indra/newview/llpanelobject.cpp@2471,10", "indra/newview/llpanelobject.cpp@2503,14", "indra/newview/llpanelobject.cpp@2728,12", "indra/newview/llpanelobject.cpp@2780,12"]}
- {"path": "indra/newview/llpanelvolume.cpp", "hunks": ["indra/newview/llpanelvolume.cpp@25,13", "indra/newview/llpanelvolume.cpp@1833,7"]}
- {"path": "indra/newview/lltoolgrab.cpp", "hunks": ["indra/newview/lltoolgrab.cpp@25,7", "indra/newview/lltoolgrab.cpp@204,55", "indra/newview/lltoolgrab.cpp@279,7", "indra/newview/lltoolgrab.cpp@395,8", "indra/newview/lltoolgrab.cpp@458,35", "indra/newview/lltoolgrab.cpp@574,7", "indra/newview/lltoolgrab.cpp@860,7", "indra/newview/lltoolgrab.cpp@1120,7", "indra/newview/lltoolgrab.cpp@1163,7", "indra/newview/lltoolgrab.cpp@1173,16", "indra/newview/lltoolgrab.cpp@1193,12"]}

## settings

None identified by local evidence; not a claim that none is needed.

## shaders

None identified by local evidence; not a claim that none is needed.

## xui

None identified by local evidence; not a claim that none is needed.

## generated runtime

None identified by local evidence; not a claim that none is needed.

## cmake entries

- {"file": "indra/newview/CMakeLists.txt", "line": 221, "text": "alpaneldirectorvcam.cpp", "source": "indra/newview/alpaneldirectorvcam.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 431, "text": "alpanelpracticals.cpp", "source": "indra/newview/alpanelpracticals.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 400, "text": "alprojectortracker.cpp", "source": "indra/newview/alprojectortracker.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1374, "text": "alprojectortracker.h", "source": "indra/newview/alprojectortracker.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1375, "text": "alprojectortrackmath.h", "source": "indra/newview/alprojectortrackmath.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 277, "text": "altoolalign.cpp", "source": "indra/newview/altoolalign.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 222, "text": "alvcammonitorstrip.cpp", "source": "indra/newview/alvcammonitorstrip.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 280, "text": "alviewermenu.cpp", "source": "indra/newview/alviewermenu.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 304, "text": "llagent.cpp", "source": "indra/newview/llagent.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 320, "text": "llappviewer.cpp", "source": "indra/newview/llappviewer.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 410, "text": "lldirectorcast.cpp", "source": "indra/newview/lldirectorcast.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 483, "text": "lldrawable.cpp", "source": "indra/newview/lldrawable.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 412, "text": "llfloaterdirector.cpp", "source": "indra/newview/llfloaterdirector.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1391, "text": "llfloaterdirector.h", "source": "indra/newview/llfloaterdirector.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 649, "text": "llfloatertools.cpp", "source": "indra/newview/llfloatertools.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 739, "text": "llmaniprotate.cpp", "source": "indra/newview/llmaniprotate.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 740, "text": "llmanipscale.cpp", "source": "indra/newview/llmanipscale.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 741, "text": "llmaniptranslate.cpp", "source": "indra/newview/llmaniptranslate.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 833, "text": "llpanelobject.cpp", "source": "indra/newview/llpanelobject.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 862, "text": "llpanelvolume.cpp", "source": "indra/newview/llpanelvolume.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 930, "text": "llselectmgr.cpp", "source": "indra/newview/llselectmgr.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 986, "text": "lltoolgrab.cpp", "source": "indra/newview/lltoolgrab.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1045, "text": "llviewermenu.cpp", "source": "indra/newview/llviewermenu.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1049, "text": "llviewerobject.cpp", "source": "indra/newview/llviewerobject.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1050, "text": "llviewerobjectlist.cpp", "source": "indra/newview/llviewerobjectlist.cpp"}

## kit wiring pages

None identified by local evidence; not a claim that none is needed.

## checks

None identified by local evidence; not a claim that none is needed.

Full include/call edges: ../features.json. Shared hooks: ../hooks.md. Setting values and save coverage: ../settings.csv.

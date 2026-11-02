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
- `indra/newview/llfloaterdirector.cpp`
- `indra/newview/llfloaterdirector.h`

## hooks

- {"path": "indra/newview/CMakeLists.txt", "hunks": ["indra/newview/CMakeLists.txt@332,115", "indra/newview/CMakeLists.txt@1283,160", "indra/newview/CMakeLists.txt@2942,179"]}
- {"path": "indra/newview/llappviewer.cpp", "hunks": ["indra/newview/llappviewer.cpp@72,33", "indra/newview/llappviewer.cpp@1939,24", "indra/newview/llappviewer.cpp@5612,9", "indra/newview/llappviewer.cpp@5967,21", "indra/newview/llappviewer.cpp@6094,7"]}
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

- {"file": "indra/newview/CMakeLists.txt", "line": 218, "text": "alpaneldirectorvcam.cpp", "source": "indra/newview/alpaneldirectorvcam.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 426, "text": "alpanelpracticals.cpp", "source": "indra/newview/alpanelpracticals.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 396, "text": "alprojectortracker.cpp", "source": "indra/newview/alprojectortracker.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1363, "text": "alprojectortracker.h", "source": "indra/newview/alprojectortracker.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1364, "text": "alprojectortrackmath.h", "source": "indra/newview/alprojectortrackmath.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 274, "text": "altoolalign.cpp", "source": "indra/newview/altoolalign.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 219, "text": "alvcammonitorstrip.cpp", "source": "indra/newview/alvcammonitorstrip.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 277, "text": "alviewermenu.cpp", "source": "indra/newview/alviewermenu.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 301, "text": "llagent.cpp", "source": "indra/newview/llagent.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 317, "text": "llappviewer.cpp", "source": "indra/newview/llappviewer.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 478, "text": "lldrawable.cpp", "source": "indra/newview/lldrawable.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 408, "text": "llfloaterdirector.cpp", "source": "indra/newview/llfloaterdirector.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1380, "text": "llfloaterdirector.h", "source": "indra/newview/llfloaterdirector.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 644, "text": "llfloatertools.cpp", "source": "indra/newview/llfloatertools.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 734, "text": "llmaniprotate.cpp", "source": "indra/newview/llmaniprotate.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 735, "text": "llmanipscale.cpp", "source": "indra/newview/llmanipscale.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 736, "text": "llmaniptranslate.cpp", "source": "indra/newview/llmaniptranslate.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 828, "text": "llpanelobject.cpp", "source": "indra/newview/llpanelobject.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 857, "text": "llpanelvolume.cpp", "source": "indra/newview/llpanelvolume.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 925, "text": "llselectmgr.cpp", "source": "indra/newview/llselectmgr.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 981, "text": "lltoolgrab.cpp", "source": "indra/newview/lltoolgrab.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1040, "text": "llviewermenu.cpp", "source": "indra/newview/llviewermenu.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1044, "text": "llviewerobject.cpp", "source": "indra/newview/llviewerobject.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1045, "text": "llviewerobjectlist.cpp", "source": "indra/newview/llviewerobjectlist.cpp"}

## kit wiring pages

None identified by local evidence; not a claim that none is needed.

## checks

None identified by local evidence; not a claim that none is needed.

Full include/call edges: ../features.json. Shared hooks: ../hooks.md. Setting values and save coverage: ../settings.csv.

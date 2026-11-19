# tracking: dependency evidence

Landing layer 7. Track moving actor/group with projector; remove target, cross region and release without stale pointers.

Candidate associations require review. Includes/calls are discovery edges, not a complete call graph or proof of build closure.

## owned

- `indra/newview/alpaneldirectorvcam.cpp`
- `indra/newview/alpanelpracticals.cpp`
- `indra/newview/alprojectortracker.cpp`
- `indra/newview/alprojectortracker.h`
- `indra/newview/alprojectortrackmath.h`
- `indra/newview/altoolvirtuallightplace.cpp`
- `indra/newview/alvcammonitorstrip.cpp`
- `indra/newview/alvirtuallights.cpp`
- `indra/newview/alvirtuallights.h`
- `indra/newview/lldirectorcast.cpp`
- `indra/newview/llfloaterdirector.cpp`
- `indra/newview/llfloaterdirector.h`

## hooks

- {"path": "indra/newview/CMakeLists.txt", "hunks": ["indra/newview/CMakeLists.txt@347,129", "indra/newview/CMakeLists.txt@1334,191", "indra/newview/CMakeLists.txt@3026,206"]}
- {"path": "indra/newview/llappviewer.cpp", "hunks": ["indra/newview/llappviewer.cpp@73,37", "indra/newview/llappviewer.cpp@1958,26", "indra/newview/llappviewer.cpp@5663,11", "indra/newview/llappviewer.cpp@6069,31", "indra/newview/llappviewer.cpp@6206,7"]}
- {"path": "indra/newview/alviewermenu.cpp", "hunks": ["indra/newview/alviewermenu.cpp@18,37", "indra/newview/alviewermenu.cpp@296,7", "indra/newview/alviewermenu.cpp@306,8"]}
- {"path": "indra/newview/llviewermenu.cpp", "hunks": ["indra/newview/llviewermenu.cpp@25,10", "indra/newview/llviewermenu.cpp@6038,7"]}
- {"path": "indra/newview/llagent.cpp", "hunks": ["indra/newview/llagent.cpp@26,13", "indra/newview/llagent.cpp@1088,20", "indra/newview/llagent.cpp@4787,9"]}
- {"path": "indra/newview/llselectmgr.cpp", "hunks": ["indra/newview/llselectmgr.cpp@29,8", "indra/newview/llselectmgr.cpp@4815,8", "indra/newview/llselectmgr.cpp@4911,8", "indra/newview/llselectmgr.cpp@5083,8", "indra/newview/llselectmgr.cpp@5539,13", "indra/newview/llselectmgr.cpp@5815,10", "indra/newview/llselectmgr.cpp@6041,8", "indra/newview/llselectmgr.cpp@7731,21", "indra/newview/llselectmgr.cpp@7941,7", "indra/newview/llselectmgr.cpp@9190,7", "indra/newview/llselectmgr.cpp@9315,7"]}
- {"path": "indra/newview/llviewerobject.cpp", "hunks": ["indra/newview/llviewerobject.cpp@25,13", "indra/newview/llviewerobject.cpp@2352,33", "indra/newview/llviewerobject.cpp@4614,27"]}
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

- {"file": "indra/newview/CMakeLists.txt", "line": 233, "text": "alpaneldirectorvcam.cpp", "source": "indra/newview/alpaneldirectorvcam.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 454, "text": "alpanelpracticals.cpp", "source": "indra/newview/alpanelpracticals.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 416, "text": "alprojectortracker.cpp", "source": "indra/newview/alprojectortracker.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1430, "text": "alprojectortracker.h", "source": "indra/newview/alprojectortracker.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1431, "text": "alprojectortrackmath.h", "source": "indra/newview/alprojectortrackmath.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 289, "text": "altoolalign.cpp", "source": "indra/newview/altoolalign.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 427, "text": "altoolvirtuallightplace.cpp", "source": "indra/newview/altoolvirtuallightplace.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 234, "text": "alvcammonitorstrip.cpp", "source": "indra/newview/alvcammonitorstrip.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 292, "text": "alviewermenu.cpp", "source": "indra/newview/alviewermenu.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 428, "text": "alvirtuallights.cpp", "source": "indra/newview/alvirtuallights.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1446, "text": "alvirtuallights.h", "source": "indra/newview/alvirtuallights.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 316, "text": "llagent.cpp", "source": "indra/newview/llagent.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 332, "text": "llappviewer.cpp", "source": "indra/newview/llappviewer.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 433, "text": "lldirectorcast.cpp", "source": "indra/newview/lldirectorcast.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 507, "text": "lldrawable.cpp", "source": "indra/newview/lldrawable.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 435, "text": "llfloaterdirector.cpp", "source": "indra/newview/llfloaterdirector.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1457, "text": "llfloaterdirector.h", "source": "indra/newview/llfloaterdirector.h"}
- {"file": "indra/newview/CMakeLists.txt", "line": 673, "text": "llfloatertools.cpp", "source": "indra/newview/llfloatertools.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 763, "text": "llmaniprotate.cpp", "source": "indra/newview/llmaniprotate.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 764, "text": "llmanipscale.cpp", "source": "indra/newview/llmanipscale.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 765, "text": "llmaniptranslate.cpp", "source": "indra/newview/llmaniptranslate.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 857, "text": "llpanelobject.cpp", "source": "indra/newview/llpanelobject.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 886, "text": "llpanelvolume.cpp", "source": "indra/newview/llpanelvolume.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 954, "text": "llselectmgr.cpp", "source": "indra/newview/llselectmgr.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1010, "text": "lltoolgrab.cpp", "source": "indra/newview/lltoolgrab.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1069, "text": "llviewermenu.cpp", "source": "indra/newview/llviewermenu.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1073, "text": "llviewerobject.cpp", "source": "indra/newview/llviewerobject.cpp"}
- {"file": "indra/newview/CMakeLists.txt", "line": 1074, "text": "llviewerobjectlist.cpp", "source": "indra/newview/llviewerobjectlist.cpp"}

## kit wiring pages

None identified by local evidence; not a claim that none is needed.

## checks

None identified by local evidence; not a claim that none is needed.

Full include/call edges: ../features.json. Shared hooks: ../hooks.md. Setting values and save coverage: ../settings.csv.

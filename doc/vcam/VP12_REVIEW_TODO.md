# VP1+VP2 review follow-ups (Opus: BUILD GO)
- [x] Quarantine a loaded saved-projector target that is no longer a projector light; do not reject the scene (`indra/newview/llprismlens.cpp:7319`, test `indra/newview/tests/llprismprojector_test.cpp:223`).
- [x] Keep capture output when `reassignDisplay` leaves a projector consumer (`indra/newview/llprismlens.cpp:2678`, test `indra/newview/tests/llprismprojector_test.cpp:203`).
- [x] On capture deletion, a Gate-subscribed projector follows its own Gate binding (`indra/newview/llprismlens.cpp:8093`, test `indra/newview/tests/llprismprojector_test.cpp:213`).
- [x] Check `prismEnabled()` before projector registry access from `setupSpotLight` (`indra/newview/pipeline.cpp:27833`).
- the new llprismprojector_test target needs a CMake reconfigure to appear.

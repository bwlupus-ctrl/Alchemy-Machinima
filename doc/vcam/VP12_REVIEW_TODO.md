# VP1+VP2 review follow-ups (Opus: BUILD GO). Fix in the VP3 round.
- llprismlens.cpp:6948: a saved projector whose target object is loaded but is no longer a projector light rejects the WHOLE Prism scene load. Quarantine that projector instead.
- llprismlens.cpp:2514: reassignDisplay doesn't check the projector count.
- llprismlens.cpp:7709: a Gate-subscribed projector is deleted with whichever capture it's currently routed to.
- optional: setupSpotLight (pipeline.cpp:27361) should check prismEnabled() before instance() (llprismlens.cpp:5386).
- the new llprismprojector_test target needs a CMake reconfigure to appear.

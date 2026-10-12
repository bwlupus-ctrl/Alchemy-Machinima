# VP1+VP2 review follow-ups
- [x] Already handled before the review: loaded non-projector targets were quarantined without rejecting the scene (`indra/newview/llprismlens.cpp:7351`).
- [x] Already handled before the review: `reassignDisplay` retained output whenever the old capture still had a projector consumer (`indra/newview/llprismlens.cpp:2685`).
- [x] Already handled before the review: deleting a capture preserved and rerouted Gate-subscribed projectors (`indra/newview/llprismlens.cpp:8120`).
- [x] Check `prismEnabled()` before projector registry access from `setupSpotLight` (`indra/newview/pipeline.cpp:27833`).
- The VCam test-pattern and rig-projector functional rows remain `DEFER_INWORLD`: their meaningful oracles require the in-world runner's rendered virtual-camera fixture and initialized rig emitters; the login gate has neither.
- The new llprismprojector_test target needs a CMake reconfigure to appear.

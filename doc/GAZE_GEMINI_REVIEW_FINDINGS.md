# Adversarial review — Gemini's Look-at-Camera / Lens Gaze implementation

Reviewed the uncommitted gaze changes (algazemath.h, llactormover.{h,cpp}, lldirectorcast.{h,cpp},
llprismlens.{h,cpp}, alpanellensgaze.{h,cpp}, panel_lens_gaze.xml, settings.xml, tests) against
doc/LOOKAT_CAMERA_GAZE_ENHANCEMENTS.md.

**Verdict: not ready.** It does not compile, one unit test fails deterministically, the headline per-slot
camera-target feature does not actually work or persist, scrub-safety (the spec's hard requirement) is
broken, and most of the naturalism (reaction delay, anatomical chain) is dead code not wired into production.
The DESIGN intent is largely right — the fixes are integration/wiring, not a redesign.

## P1 — must fix (blockers / core feature broken)

1. **Does not compile — invented API.** `llactormover.cpp:3714` calls
   `LLPresentationTime::presentationTime()`, which does not exist. Use
   `LLPresentationTime::currentFrame().presentation_time` (as aldirectorswitcher.cpp does). This also makes
   `t_sec` (3713) "const not initialized". (2 of the 4 compiler errors.)
2. **Does not compile — missing include.** `llactormover.cpp:3789` uses `gObjectList` without including
   `llviewerobjectlist.h`. Add the include.
3. **Unit test fails deterministically — seed only hashes half the UUID.** `algazemath.h:~103`
   `castSeedFromUUID` reads only bytes 0–7, so UUIDs differing only in the last 8 bytes get identical
   naturalism; test<6> (`seed1 != seed2`) fails. Hash both halves (e.g. `getDigest64()` → `splitMix64`).
4. **Primary per-slot targeting is broken — writes the wrong store.** `alpanellensgaze.cpp:~298` slot/target/
   cast/point/object callbacks mutate `LLActorMover::mGazes` for the host's `mSelected` actors, but Director
   rendering reads `LLDirectorCast::mGazeTarget` and scene serialization saves THAT. So editing a slot's
   target neither affects the checked slot nor persists. Track the active You/A–D slot and commit its full
   target through `LLDirectorCast::getGazeTarget/setGazeTarget`.
5. **Turns EAST instead of the camera on a fresh/legacy selection.** `llactormover.h:~326` — an unauthored
   cast target (newly checked slot, or old scene with `look_at_camera=true` but no `gaze_target`) defaults to
   `MOTION`; with no Move, the tangent fallback is world +X, so the avatar faces east. Default Director
   cast/self targets to `CAMERA` when unauthored (keep `MOTION` only for legacy Actor Mover).
6. **Scrub-safety broken — break applied before the stateful smoothing.** `llactormover.cpp:~3838` applies the
   pure natural-break deflection to `dir` BEFORE `mSmoothDir`'s frame-delta chase, so seeking to a
   presentation time inside a break differs from replaying to it (the rendered break depends on accumulated
   smoothing history). Apply the closed-form break AFTER the stateful base smoothing (or analytically), so the
   random component stays a pure function of (seed, t).

## P2 — significant (features that don't actually work / perf / UI)

7. **Reaction latency is dead code.** `evalReactionDelay` has no production caller (llactormover.cpp:~3622);
   the acquire envelope advances on frame one — no "notices the camera" beat, and breaks aren't suppressed
   during the delay. Wire it: detect acquisition/cut on presentation time, hold for the hashed delay, then
   advance.
8. **Anatomical chain is dead code.** `mBodyTurnThresholdDeg` is read but never consumed and
   `distributeAnatomicalChain` is referenced only by tests (llactormover.cpp:~3723). Production still uses the
   flat torso + fixed head/neck split, and global mode-1 turns toward the render camera, not the resolved
   target. Feed the solved yaw/pitch through the chain and drive the body turn from its threshold.
9. **Chain distribution is wrong even in the pure fn.** `algazemath.h:~381` gives head/neck the whole
   comfortable angle before eyes and adds torso before saturation; at 70° the head gets 38.5° (> its 35°
   limit). Allocate residual sequentially eyes→head→neck→chest→hips, each independently clamped, recruiting
   the next only after saturation.
10. **Release ease never applies to Director gaze.** `llactormover.cpp:~3658` always steps with `active=true`;
    unchecking a slot clears the runtime before a release step can run, so deselection pops. Keep an inactive
    runtime until its envelope steps to zero with `active=false`, then clear.
11. **Saccades fire in sync across a crowd.** `algazemath.h:~249` changes saccade cells at fixed 0.45 s
    multiples for everyone (only the offset is hashed). Derive a per-seed phase + hashed cadence, keeping
    interpolation continuous at each event boundary.
12. **Self ("You") gaze can never enable.** `alpanellensgaze.cpp:~275` passes the user's UUID to
    `setLookAtCamera`, which rejects any id not in the cast; the checkbox resets. Add explicit self state and
    serialize it with `self_gaze_target`.
13. **Hot-path allocations — gate lookup.** `llprismlens.cpp:~6528` `gateOnAirCameraEye` copies a
    `GateSnapshot` (and, active, a full `RegistrySnapshot`) per camera-target avatar per frame. Resolve the
    on-air eye with an O(1) registry lookup or cache one eye per frame/revision.
14. **Hot-path — object bbox rebuilt per subject per frame.** `llactormover.cpp:~3797` walks the whole linkset
    every frame. Cache the agent-space centre once per object per frame (invalidate on transform/link change).
15. **Hot-path — quadratic cast scan.** `lldirectorcast.cpp:~163` `getGazeTarget`→`getMember` linearly scans
    `mCast` per selected avatar per frame. Add an id→member/target index.
16. **Variation not applied on the Actor-Mover path.** `llactormover.cpp:~3735` computes `var_int`/`var_sm`
    then never uses them, so the Variation slider only changes break frequency there. Apply them locally
    (without mutating authored settings or double-applying the Director variation).
17. **Old Director "Ease" control is now dead.** `llactormover.cpp:~3612` — the UI still writes
    `DirectorLookAtCameraEaseTime` but nothing reads it (replaced by acquire/release). Rebind it as a scale
    for both, or remove the control.
18. **Embedded panels clip the new controls.** `panel_lens_gaze.xml` grew to ~262px but
    `floater_actor_mover.xml` and `floater_director.xml` still embed it at height 188; the moved tracking
    controls (y≈192–254) are clipped and can overlap following groups. Update both embed heights + scroll
    extents.

## Positives
- Determinism *approach* is correct: naturalism math is pure closed-form of (seed, t) and the intended clock
  is the scrub-safe `presentation_time` — once #1 compiles and #6 is reordered, scrub-safety holds.
- Render-only; no server writes. Settings + scene keys are additive.

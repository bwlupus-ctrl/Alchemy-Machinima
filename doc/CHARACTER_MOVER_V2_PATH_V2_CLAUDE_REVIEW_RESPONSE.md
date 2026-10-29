# Character Mover V2 — Path V2 Adversarial Review Response

**Candidate:** uncommitted Path V2 block, base `46a532340cb`
**Method:** three adversarial passes (exact geometry; lifecycle/ownership/camera/persistence; editor/transport/UI), each proving the section-8 invariants from code; every MAJOR re-verified by hand against the working-tree files. Full Release link not run (per the gate).

## Gate decision: HOLD — 3 MAJORs, all small, then READY

**No BLOCKER. Compile/link is clean** (the shared `alpathgeometry.h` is fully `inline`, no anon-namespace/ODR hazard — last round's failure mode does not recur; the test uses real `ensure_distance`; CMake closure and all decls/defs verified). The core is proven solid — the zoom-to-space deletion fix, the exact geometry engine, camera finiteness, and v1/v2 migration all hold. Three targeted runtime/persistence correctness bugs must land first; each is a few lines in one function.

### Proven solid (verified)
- **Zoom-to-space deletion fix — proven both sides.** Inv 1 (no active Move outlives erased geometry — all 10 erase/replace sites retire the Move first), Inv 2 (`advancePath` uses `find()` only; missing branch suspends at the live root, no `operator[]`), Inv 3 (missing/degenerate can't place at origin at runtime), Q1 (no ownership leak across clear/stop/scene-replace/undo/redo/actor-loss/flycam/preview transitions — camera released same-frame).
- **Exact geometry.** Inv 4 (`u==1` at `mTotalLength`, proven bitwise), Inv 5 (loop seam snapped to whole turns at every gate; rising helix can never loop — 5 enforcement sites), Inv 10 (arc budget ≤65533 by induction, deterministic), Q2 (circle/helix analytic inversion exact; ellipse table bounded/monotonic; all degenerate inputs floored — no NaN/Inf).
- **Camera finiteness.** Inv 11 (finite global → finite agent write, double-gated in `updateCamera`, keeps last valid frame on invalid).
- **Migration & honesty.** Q4 (v1 waypoint scenes load unchanged — schema-1 skips the `>=2` block), Q3 (no editor order yields a discontinuous loop seam), Q5 (undo/redo/copy round-trip all fields; node metadata rides `std::reverse` correctly), Q6 (one `mShape` per path, no mixed-span — honest), Inv 12 (Play Path calls `mover.start(actor)` directly; ACTION broadcast is a separate `startAll`→DirectorCast path — fully isolated), Inv 9 (structural edits rejected at the engine level, not just UI).

---

## MAJOR — fix before READY (none block compile/link)

### F1 — `reversePath` mirrors a rising helix and teleports it `rise` metres
- **File:** `llactormover.cpp:3064-3069`. The primitive branch does only `mPrimitiveStartDeg += mPrimitiveSweepDeg; mPrimitiveSweepDeg = -mPrimitiveSweepDeg;` — it never touches `mPrimitiveRise` or `mPrimitiveCenterGlobal`.
- **Failure:** reversed curve must be `P(1-u)`. The angle flip gives the correct in-plane θ, but local z stays `rise·u` instead of `rise·(1-u)` → the mirror-chirality spiral **plus** a vertical jump. Trace (r=2, sweep=360, rise=5): reversed start = (2,0,0) vs original end (2,0,5) → **5 m teleport**; `rebuild()→syncPrimitiveNodes()` then rewrites every node onto the wrong curve, dragging dwell/anim/camera metadata with it. Circle/ellipse reverse is exact. (Metadata attachment itself is correct — the geometry is what's wrong; see the Q5 note.)
- **Fix:** in the HELIX case add `mPrimitiveCenterGlobal += LLVector3d(0,0,mPrimitiveRise) * ALPathGeometry::planeRotation(primitive()); mPrimitiveRise = -mPrimitiveRise;` — yields `P(1-u)` exactly. Addresses Inv 6. RUNTIME.

### F2 — `reversePath` pushes `mStartDeg` past the ±36000 load clamp → geometry rotates on reload
- **File:** `llactormover.cpp:3067` (same function). `start' = start + sweep` is not canonicalized; load clamps start to ±36000 (`:1647`).
- **Failure:** start=300, sweep=35900 → reversed 36200 → clamp 36000 on next load → the whole arc rotates −200° (metres at the 10 m radius). Only via reverse→save→load of near-cap sweeps.
- **Fix:** `mPrimitiveStartDeg = fmodf(mPrimitiveStartDeg, 360.f)` after the flip (geometry is 360-periodic; residual error ~4.9e-8 rad/turn, negligible). Addresses Inv 6. RUNTIME.

### M1 — missing `primitive_center` key loads as an origin-centered path (the zoom-to-space symptom via *data*)
- **File:** `llactormover.cpp:1637-1642`. `mPrimitiveCenterGlobal = ll_vector3d_from_sd(data["primitive_center"])` with only an `isFinite()` guard. A **missing** key → `ll_vector3d_from_sd(undefined) = (0,0,0)`, which is finite → committed as a "valid" primitive centered at global origin. (The radii/scalars below have `finite_f32(..., default)` fallbacks — only the center lacks a `has()` check.)
- **Failure:** a hand-edited/truncated schema-2 scene without `primitive_center` loads a circle at region-0 corner; Play walks the actor there — the old bug resurrected through data instead of lifecycle. (v1 could only reach origin via *explicitly authored* origin positions; here an *omitted* key manufactures it.)
- **Fix (one line):** `if (!data.has("primitive_center")) return false;` before `:1637` (optionally also reject a schema-2 primitive whose center XY is exactly 0,0). RUNTIME/persistence.

---

## MINOR (optional hardening; none block anything)
- **m2** `advancePath` missing-geometry guard bypasses `enterSuspend`, leaving `mSuspendTrueGlobal` stale → at worst a 1-frame flicker on auto-resume near a stale anchor. Seed the anchor / call `enterSuspend` in the guard. (`llactormover.cpp:4629`)
- **m3** `clearPath` only cancels a queued staggered start when an active Move exists; clearing inside a group-delay window can let a queued `start()` fire a surprise legacy straight-walk. Cancel the pending start unconditionally. (`:1517`)
- **m4** `setPathShape(WAYPOINTS)` on a pathless actor inserts a default `mPaths[key]` stub then returns false — an `operator[]` entry in a design that bans them (contained, but inconsistent). `find()` first or erase the stub. (`:1767`)
- **m5** helix→circle conversion keeps `mAirborne=true` (the circle flies until the author unticks Airborne). Clear airborne when converting away from helix.
- **Geometry nits (F3-F6):** full-turn seam continuous to ~r·4.9e-8 (F32 `DEG_TO_RAD`), not bit-exact — fine at the 1e-5 test tolerance; `rebuild()` analytic gate lacks the `>1e-9` guard `evalAtDistance` has (behaviorally safe); double-reverse drifts start by one ulp (F2's fmod bounds it); header `evaluate` silently accepts `WAYPOINTS` — add `llassert(isPrimitive(...))`.
- **Editor nits:** `onClickAdd/Insert/Delete` lack a handler-level walking/primitive re-check (the engine catches it regardless); "Loop Close" collapses multi-turn sweeps to one turn while End-combo Loop rounds to nearest integer-turn (both seam-safe, just divergent); the `primitive_sweep_deg` tooltip says "helix" but the control is shared by all three shapes.
- **m6** `getPathCameraPose` allocates a `std::vector` each frame while the path camera drives (pre-existing, not from this diff). **m7** chat-command `clearPath` during a panel preview leaves the (path-independent, validated) preview pose up until actor-switch/close — not a latch on deleted geometry.

---

## The 7 required answers
1. **Can deletion/replacement leave camera/movement ownership alive?** No — Q1 proven across 8 teardown flows; camera releases same-frame.
2. **Is primitive distance mapping exact/bounded where claimed?** Yes — circle/helix analytic inversion is exact, ellipse table bounded & monotonic. (The *reverse operation* on a helix/near-cap arc is wrong — F1/F2 — but the forward distance mapping is exact.)
3. **Can any editor order create a discontinuous Loop seam?** No — sanitized at load, End-combo, primitive-commit, Loop-Close, and shape-conversion.
4. **Does v1/v2 migration preserve default waypoint behavior?** Yes — schema-1 skips the primitive block; v1 fields parse identically.
5. **Can undo/redo/copy/reverse/actor-switch lose/misattach event or camera metadata?** No for *metadata* — full field round-trip, node data rides `std::reverse` correctly. But **reverse of a rising helix produces the wrong geometry** (F1), which then re-anchors nodes onto the wrong curve.
6. **Is the whole-path primitive boundary honest?** Yes — one shape per path, no mixed-span or shape-recognition claims (one cosmetic tooltip nit).
7. **Any reason not to proceed after a clean link?** Land F1 + F2 + M1 first (all small, one-function-each). After that and Codex's re-pass, proceed to the runtime acceptance matrix.

## Minimal correction set
F1 (2 lines in the helix case) + F2 (1 line `fmod`) — both in `reversePath` (`llactormover.cpp:3064-3069`); M1 (1 line `has()` check at `:1637`). Recommend also m2 and m3 as cheap hardening. Then re-run the internal adversarial pass and proceed to the canonical Release build + runtime matrix.

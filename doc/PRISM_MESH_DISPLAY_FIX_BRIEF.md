# Prism/VCam mesh display faces — geometry-fallback fix + visible reject reason

**Problem (confirmed in-world):** a PRISM/VCam **display** face works for a regular **prim** but is refused
for a **mesh** face — the "Add selected face" button greys out. Root cause: `deriveRectangularSurface`
(`indra/newview/llprismlens.cpp`) derives the display quad from the face's **UV layout** and requires
those UVs to be an **affine rectangle** (the restored affine-fit guard: "designated face is not an
affine rectangular UV surface"). Prim box faces always satisfy this; typical mesh faces
(custom/atlassed/skewed UVs) do not, so they are rejected. Also, the manager no longer shows the
reject reason on screen (it was removed to stop status-bar spam), so the user is left guessing.

Baseline: current working tree at HEAD `7670a36ee1e` + the just-built VCam changes (exe 10:50:12).
**Read `GEMINI.md` first. Do NOT build/commit/`git add`. Deliver compile-clean, `/WX`-clean source.**
Claude builds; an Opus sub-agent adversarially reviews to zero must-fix first.

## Fix 1 — Geometry-based fallback in `deriveRectangularSurface` (the real mesh fix)
Keep the existing **UV-affine derivation as the PRIMARY path** so prims and clean-UV faces are
**byte-for-byte unchanged** (no regression to the working prim camera feed). Only when the UV-affine
path would REJECT (i.e. the affine-fit guard fails), **fall back to a UV-INDEPENDENT oriented
bounding quad derived from the flat face geometry.** Any flat face then yields a valid display
rectangle regardless of its UVs.

Read the whole function first (`~615-757`). It currently: gathers UVs, builds an affine UV→position
map from 3 vertices (`basis_b`/`basis_c`, `position_per_u/v`, `world_per_u/v`, `local_per_*`),
derives `surface_origin`/`surface_u_edge`/`surface_v_edge` + world + local variants, runs the
affine-fit guard over every vertex, then calls `assignRectangularSurfaceFrame(...)`.

**Refactor so the affine-fit guard becomes a decision, not a hard reject:**
- Compute the affine basis + run the per-vertex fit check exactly as now. If it PASSES → use the
  UV-derived `surface_*`/`world_*`/`local_basis` and call `assignRectangularSurfaceFrame` (current
  behavior, unchanged).
- If it FAILS → **do not return false.** Instead derive the basis from geometry (below), then call
  `assignRectangularSurfaceFrame` with the geometry-derived basis.

**Geometry fallback — compute the oriented bounding rectangle in SURFACE space, then transform to
world and local via the matrices the function already has** (`surface_matrix`, `model_matrix` are
available in the caller `validateSurfaceGeometry` and passed in / recomputable; if not already
passed, thread the needed inputs — prefer computing in surface space from `surface_positions`, which
`validateSurfaceGeometry` already builds and passes as the coplanar surface-space vertex set):
1. Centroid `C = mean(surface_positions)`.
2. Surface-space plane normal `n`: from the first non-degenerate triangle
   `normalize((surface_positions[b]-surface_positions[a]) x (surface_positions[c]-surface_positions[a]))`
   (mirror the plane-finding loop already used upstream in `validateSurfaceGeometry`).
3. In-plane U axis: take the **longest face edge** direction (max `|surface_positions[i]-
   surface_positions[j]|` over the face's index-buffer edges), project it into the plane, normalize.
   Then `V = normalize(n x U)` and re-orthonormalize `U = normalize(V x n)`. (Longest-edge alignment
   gives a stable axis for a rectangular screen; a degenerate/zero-length result → reject with a
   clear reason, do not divide by zero.)
4. Project every vertex: `u_i=(P_i-C)·U`, `v_i=(P_i-C)·V`; take `u_min/u_max/v_min/v_max`.
5. `surface_origin = C + U*u_min + V*v_min`; `surface_u_edge = U*(u_max-u_min)`;
   `surface_v_edge = V*(v_max-v_min)`.
6. Derive the WORLD basis by transforming these three surface-space vectors through `model_matrix`
   (points via full transform, edges via the linear/rotation part — mirror how the existing code maps
   surface→world), and the LOCAL basis by transforming through `inverse(surface_matrix)` (points +
   edges); guard `surface_matrix` invertibility and finiteness of every result. Fill `local_basis`
   the same way the UV path fills it.
7. Call `assignRectangularSurfaceFrame(surface_origin, surface_u_edge, surface_v_edge,
   world_surface_origin, world_surface_u_edge, world_surface_v_edge, frame, reject_reason)` — its
   existing finiteness / non-degenerate-edge / world-edge-orthogonality checks still gate the result.

**Invariants:** prims and clean-UV faces never enter the fallback (the affine path passes), so they
are unchanged — prove this. The planarity precondition still holds (upstream `validateSurfaceGeometry`
rejects non-planar faces before this function). Non-finite / degenerate geometry must still reject
with a specific reason, never crash/NaN. This is surface-basis derivation feeding the projection, not
the deferred render path — but a wrong basis means a skewed feed, so the geometry math must be
correct. Keep the rigged/animesh rejection as-is (rigged display faces are out of scope).

## Fix 2 — Restore the reject reason on screen, without the spam
In `llfloaterprismmanager.cpp` `refreshSelectionActions()` (called ~4x/sec from the throttled
`refresh()`), the reason for a greyed "Add selected face" must be visible again — but the earlier
version clobbered transient status messages every poll. **Dedup it:** add a member
`std::string mLastSelectionActionStatus;` (or similar). When a CAMERA_FEED capture is selected and the
Add-Display action is disabled with a non-empty reason, call `setStatus(reason)` **only if `reason !=
mLastSelectionActionStatus`**, then store it. When the action becomes allowed (or nothing relevant is
selected), clear `mLastSelectionActionStatus` so the next rejection re-displays. This shows the reason
once on change and never overwrites a fresh success/error message on subsequent identical polls.
(`setStatus` takes `const std::string&` — no `LLStringExplicit` needed; `mReason` is a `std::string`.)

## Deliver
Edited `llprismlens.cpp` (+ `llfloaterprismmanager.cpp`), a per-file changelog (file:line + what/why),
an explicit argument that prims/clean-UV faces are unchanged (fallback not reached), `git diff --check`
clean. Note any input you had to thread into `deriveRectangularSurface` for the fallback. Claude +
Opus review (focus: prim non-regression, geometry-math correctness, no NaN, /WX) then build.
